/**
 ******************************************************************************
 * @file     ele_control.ino
 * @author   md
 * @version  V3.0
 * @date     2025-12-01
 * @brief    LED灯实验 - WiFi连接状态监测
 ******************************************************************************
 */

#include "led.h"
#include "key.h"
#include "temp.h"
#include "adc.h"
#include "rgb.h"
#include "mqtt.h"
#include "version.h"
#include "config.h"
#include "serial_cmd.h"
#include "oled.h"
#include <WiFi.h>  // 确保已安装 ESP32 开发板支持包

/* ==================== 调试开关 ==================== */
#define DEBUG_ENABLE  1

#if DEBUG_ENABLE
  #define DEBUG_PRINT(x)    Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

/* ==================== 配置常量 ==================== */
const uint8_t WIFI_CONNECT_ATTEMPTS = 20;         // WiFi最大连接尝试次数
const unsigned long WIFI_CONNECT_DELAY = 500;     // 每次连接尝试间隔(ms)
const unsigned long WIFI_RECONNECT_INTERVAL = 5000;  // WiFi重连间隔(ms)
const unsigned long LED_BLINK_INTERVAL = 500;     // LED闪烁间隔(ms)
const unsigned long HEARTBEAT_INTERVAL = 10000;    // 心跳打印间隔(ms)
const unsigned long KEY_BLINK_INTERVAL = 200;     // 按键长按LED闪烁间隔(ms)
const long SERIAL_BAUD_RATE = 115200;             // 串口波特率

/* ==================== 状态变量 ==================== */
WiFiState wifiState = WIFI_IDLE;
uint8_t connectAttempt = 0;
unsigned long lastHeartbeatMillis = 0;
unsigned long lastBlinkMillis = 0;
unsigned long lastConnectMillis = 0;
unsigned long lastReconnectMillis = 0;

/* 自动/手动模式变量 */
bool g_autoMode = true;  // 默认自动模式，由 KEY2 长按切换

/* 继电器状态循环控制变量 */
uint8_t g_relayState = 0;  // 0:00  1:01  2:10  3:11

/* 继电器状态映射表和名称表定义（声明在 led.h） */
const uint8_t RELAY_STATE_TABLE[4] = {
    0b00,  // 状态0: LED=OFF, FAN=OFF
    0b01,  // 状态1: LED=ON,  FAN=OFF
    0b10,  // 状态2: LED=OFF, FAN=ON
    0b11   // 状态3: LED=ON,  FAN=ON
};

const char* RELAY_STATE_NAME[4] = {
    "00 - LED:OFF  FAN:OFF",
    "01 - LED:ON   FAN:OFF",
    "10 - LED:OFF  FAN:ON",
    "11 - LED:ON   FAN:ON"
};

/* ==================== 传感器阈值设置 ==================== */
float lightThreshold = 300.0;   // 光照阈值（lux），超过此值打开LED
float tempThreshold = 28.0;     // 温度阈值（℃），超过此值打开风扇

/* ==================== 按键相关变量 ==================== */
bool keyLongPressBlink = false;
unsigned long lastKeyBlinkMillis = 0;

/* ==================== 传感器读取相关变量 ==================== */
unsigned long lastAdcReadMillis = 0;
const unsigned long ADC_READ_INTERVAL = 200;  // ADC 读取间隔 (ms)

/* 模拟传感器数据 */
float lightValue = 0.0;         // 光照值（lux，由 GL5516 读取）
bool lightEnabled = false;
bool fanEnabled = false;

/* ==================== 函数声明 ==================== */
void updateLedState(void);
void handleWiFiEvent(WiFiEvent_t event);
void startWiFiConnection(void);
void continueWiFiConnection(void);
void handleWiFiConnected(void);
void handleWiFiDisconnected(void);
void handleKeyEvent(void);

/**
 * @brief    WiFi事件回调函数
 * @param    event: WiFi事件类型
 * @retval   无
 */
void handleWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_READY:
            DEBUG_PRINTLN("[WiFi] 准备就绪");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            DEBUG_PRINTLN("[WiFi] 已连接到AP");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            DEBUG_PRINTLN("[WiFi] 连接断开");
            wifiState = WIFI_DISCONNECTED;
            lastBlinkMillis = millis();
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            handleWiFiConnected();
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            DEBUG_PRINTLN("[WiFi] IP地址丢失");
            wifiState = WIFI_DISCONNECTED;
            lastBlinkMillis = millis();
            break;
        default:
            break;
    }
}

/**
 * @brief    开始WiFi连接
 * @param    无
 * @retval   无
 */
void startWiFiConnection() {
    AppWifiConfig* wifiCfg = get_wifi_config();
    if (!wifi_config_is_valid(wifiCfg)) {
        DEBUG_PRINTLN("[WiFi] 未配置WiFi，使用串口命令 'wifi scan' 配网");
        wifiState = WIFI_DISCONNECTED;
        return;
    }
    WiFi.begin(wifiCfg->ssid, wifiCfg->password);
    DEBUG_PRINT("[WiFi] 正在连接: ");
    DEBUG_PRINTLN(wifiCfg->ssid);
    wifiState = WIFI_CONNECTING;
    connectAttempt = 0;
    lastConnectMillis = millis();
}

/**
 * @brief    继续WiFi连接（非阻塞）
 * @param    无
 * @retval   无
 */
void continueWiFiConnection() {
    unsigned long currentMillis = millis();
    
    if (wifiState == WIFI_CONNECTING) {
        if (currentMillis - lastConnectMillis >= WIFI_CONNECT_DELAY) {
            lastConnectMillis = currentMillis;
            
            if (WiFi.status() == WL_CONNECTED) {
                handleWiFiConnected();
            } else {
                connectAttempt++;
                DEBUG_PRINT(".");
                
                if (connectAttempt >= WIFI_CONNECT_ATTEMPTS) {
                    DEBUG_PRINTLN("\n[WiFi] 连接失败！");
                    wifiState = WIFI_DISCONNECTED;
                    lastBlinkMillis = currentMillis; // 重置闪烁定时器
                }
            }
        }
    }
}

/**
 * @brief    处理 WiFi 连接成功
 * @param    无
 * @retval   无
 */
void handleWiFiConnected(void) {
    if (wifiState == WIFI_CONNECTED) {
        return;
    }
    
    DEBUG_PRINTLN("\n[WiFi] 连接成功！");
    DEBUG_PRINTLN("[WiFi] IP 地址：" + WiFi.localIP().toString());
    DEBUG_PRINTLN("[WiFi] 信号强度：" + String(WiFi.RSSI()) + " dBm");
    wifiState = WIFI_CONNECTED;
    
    // 连接成功自动保存配置到 Flash
    wifi_config_save(get_wifi_config());
    
    // MQTT 连接交由 mqtt_loop() 处理（等待网络栈稳定后自动连接）
}

/**
 * @brief    更新 LED 状态（灯光控制）
 * @param    无
 * @retval   无
 * @note     LED 引脚（GPIO6）控制继电器，RGB 灯（GPIO0）作为指示灯
 * @note     继电器电平定义：HIGH=ON(吸合), LOW=OFF(断开)
 */
void updateLedState(void) {
    // 控制实际继电器（仅在 KEY1 ON 时有效）
    if (key1_is_on()) {
        LED(lightEnabled ? HIGH : LOW);
        FAN(fanEnabled ? HIGH : LOW);
    } else {
        // KEY1 OFF，强制关闭继电器
        LED(LOW);
        FAN(LOW);
    }
}

/**
 * @brief    处理WiFi断开和重连
 * @param    无
 * @retval   无
 */
void handleWiFiDisconnected(void) {
    unsigned long currentMillis = millis();
    
    if (wifiState == WIFI_DISCONNECTED) {
        if (currentMillis - lastReconnectMillis >= WIFI_RECONNECT_INTERVAL) {
            lastReconnectMillis = currentMillis;
            DEBUG_PRINTLN("[WiFi] 尝试重连...");
            startWiFiConnection();
        }
    }
}

/**
 * @brief    初始化函数
 * @param    无
 * @retval   无
 */
void setup() {
    led_init();
    fan_init();
    key1_init();  // KEY1: 自锁按键
    key2_init();  // KEY2: 自恢复按键
    rgb_init();   // RGB 灯初始化
    Serial.begin(SERIAL_BAUD_RATE);
    
    //delay(100);
    
    // 打印版本信息
    Serial.println("\n========================================");
    Serial.println("      ESP32-C3 启动成功！");
    Serial.print("  版本：");
    Serial.println(VERSION);
    Serial.print("  项目：");
    Serial.println(PROJECT_NAME);
    Serial.println("========================================\n");
    
    WiFi.onEvent(handleWiFiEvent);
    WiFi.mode(WIFI_STA);
    
    // 先加载配置，再启动 WiFi 连接
    // RGB 状态由 rgb_update_by_state() 在 loop 中自动管理
    config_init();
    param_load();   // 恢复工作参数（模式/阈值/继电器状态）
    // 恢复继电器输出状态
    {
        uint8_t stateBits = RELAY_STATE_TABLE[g_relayState];
        lightEnabled = (stateBits & 0b01) != 0;
        fanEnabled   = (stateBits & 0b10) != 0;
    }
    // 手动模式下，上电不自动打开灯和风扇，强制全关
    if (!g_autoMode) {
        g_relayState = 0;
        lightEnabled = false;
        fanEnabled   = false;
    }
    serial_cmd_init();
    startWiFiConnection();
    mqtt_init();

    // 初始化 OLED 显示（包含开机画面）
    oled_init();
    
    // 初始化温度传感器
    temp_init();
    temp_task_create();  // 创建后台温度轮询任务
    
    // 创建 RGB 灯控后台任务（独立任务驱动呼吸灯）
    rgb_task_create();
    
    // 初始化 ADC
    adc_init();
    
    // RGB 灯测试：启动多色快闪（红→绿→蓝→白）
    rgb_boot_flash();
}

/**
 * @brief    主循环函数
 * @param    无
 * @retval   无
 */
void loop() {
    unsigned long currentMillis = millis();
    
    continueWiFiConnection();
    updateLedState();
    handleWiFiDisconnected();
    serial_cmd_process();
    
    if (wifiState == WIFI_CONNECTED) {
        if (currentMillis - lastHeartbeatMillis >= HEARTBEAT_INTERVAL) {
            lastHeartbeatMillis = currentMillis;
            if (mqtt_debug_enabled) {
                Serial.printf("[Heartbeat] TEMP: %.1f℃  |  LIGHT: %.1f lux\n", g_temperature, lightValue);
            }
        }
    }
    
    /* 读取光照强度（lux，每隔 200ms 读取一次） */
    if (currentMillis - lastAdcReadMillis >= ADC_READ_INTERVAL) {
        lastAdcReadMillis = currentMillis;
        lightValue = adc_read_lux();
    }
    
    /* 自动模式下的传感器控制（带迟滞，避免临界值频繁开关） */
    if (g_autoMode && key1_is_on()) {
        bool oldLight = lightEnabled;
        bool oldFan = fanEnabled;
        
        // 光照控制：低于阈值-迟滞 开灯，高于阈值+迟滞 关灯
        if (!lightEnabled && lightValue < lightThreshold) {
            lightEnabled = true;
        } else if (lightEnabled && lightValue > lightThreshold + LIGHT_HYSTERESIS) {
            lightEnabled = false;
        }
        // 温度控制：高于阈值+迟滞 开风扇，低于阈值-迟滞 关风扇
        if (!fanEnabled && g_temperature > tempThreshold) {
            fanEnabled = true;
        } else if (fanEnabled && g_temperature < tempThreshold - TEMP_HYSTERESIS) {
            fanEnabled = false;
        }
        
        if (oldLight != lightEnabled || oldFan != fanEnabled) {
            Serial.print("[Auto] 自动控制 - 光照: ");
            Serial.print(lightValue, 1);
            Serial.print("/");
            Serial.print(lightThreshold, 1);
            Serial.print(" lux → LED: ");
            Serial.println(lightEnabled ? "ON" : "OFF");
            
            Serial.print("[Auto] 自动控制 - 温度: ");
            Serial.print(g_temperature, 1);
            Serial.print("/");
            Serial.print(tempThreshold, 1);
            Serial.print(" ℃ → FAN: ");
            Serial.println(fanEnabled ? "ON" : "OFF");
            
            oled_force_update();  // 强制更新 OLED
            mqtt_request_publish();  // 立即上报状态
        }
    }
    
    /* 处理按键事件（手动模式下控制，自动模式下仅 KEY2 长按切换模式） */
    handleKeyEvent();
    
    /* MQTT 循环处理 */
    mqtt_loop();
    
    /* RGB 状态由独立后台任务 rgbTask 自动更新，此处不再调用 */
    
    /* 更新 OLED 显示 */
    oled_update();
}

/**
 * @brief    处理按键事件
 * @param    无
 * @retval   无
 */
void handleKeyEvent(void) {
    unsigned long currentMillis = millis();
    
    /* ==================== KEY1: 自锁总开关 ==================== */
    /* KEY1 控制总闸：ON 允许继电器输出，OFF 强制断开继电器 */
    static bool lastKey1State = false;
    bool key1On = key1_is_on();
    
    if (key1On != lastKey1State) {
        lastKey1State = key1On;
        
        if (key1On) {
            Serial.println("[Key1] 总开关 ON");
            // 直接由 updateLedState() 根据内部状态恢复继电器
        } else {
            Serial.println("[Key1] 总开关 OFF");
            // 强制关闭所有继电器
            lightEnabled = false;
            fanEnabled = false;
            LED(LOW);
            FAN(LOW);
        }
        oled_force_update();  // 立即更新 OLED
        mqtt_request_publish();  // 立即上报状态
    }
    
    /* ==================== KEY2: 自复位按键（中断模式） ==================== */
    uint8_t key2Event = key2_get_event();  // 使用新的中断模式函数
    
    // 调试：打印接收到的事件
    if (key2Event != KEY_EVENT_NONE) {
        Serial.print("[KEY2_MAIN] 收到事件：");
        if (key2Event == KEY_EVENT_SHORT_PRESS) {
            Serial.println("短按");
        } else if (key2Event == KEY_EVENT_LONG_PRESS) {
            Serial.println("长按");
        }
    }
    
    if (key2Event != KEY_EVENT_NONE) {
        switch (key2Event) {
            case KEY_EVENT_SHORT_PRESS:
                // 短按：仅在手动模式下有效，循环切换继电器状态 00->01->10->11->00
                if (!g_autoMode) {
                    if (key1On) {
                        g_relayState = (g_relayState + 1) % 4;
                        
                        uint8_t stateBits = RELAY_STATE_TABLE[g_relayState];
                        lightEnabled = (stateBits & 0b01) != 0;
                        fanEnabled = (stateBits & 0b10) != 0;
                        
                        Serial.print("[Key2] 状态 ");
                        Serial.println(RELAY_STATE_NAME[g_relayState]);
                        
                        param_save();   // 保存继电器状态
                        oled_force_update();
                        mqtt_request_publish();
                    } else {
                        Serial.println("[Key2] 短按无效 - 总闸已关闭");
                    }
                } else {
                    Serial.println("[Key2] 短按无效 - 当前为自动模式");
                }
                break;
                
            case KEY_EVENT_LONG_PRESS:
                // 长按：切换自动/手动模式（仅在总闸开启时有效）
                if (key1On) {
                    g_autoMode = !g_autoMode;
                    
                    if (g_autoMode) {
                        Serial.println("[Key2] 长按 - 切换到自动模式");
                    } else {
                        Serial.println("[Key2] 长按 - 切换到手动模式");
                    }
                    param_save();   // 保存模式
                    mqtt_request_publish();  // 立即上报状态
                } else {
                    Serial.println("[Key2] 长按无效 - 总闸已关闭");
                }
                break;
                
            default:
                break;
        }
        
        /* 设置强制更新标志，让 OLED 立即刷新 */
        oled_force_update();
    }
}


