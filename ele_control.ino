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
#include <WiFi.h>  // 确保已安装 ESP32 开发板支持包
#include <U8g2lib.h>
#include <Wire.h>

// 自定义 SDA 和 SCL 引脚
#define I2C_SDA  4  // 自定义 SDA 引脚
#define I2C_SCL  5  // 自定义 SCL 引脚

// 使用软件 I2C - SH1106 驱动（兼容 SH1116，需要手动偏移）
U8G2_SH1106_128X64_VCOMH0_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ I2C_SCL, /* data=*/ I2C_SDA, /* reset=*/ U8X8_PIN_NONE);
#define OLED_OFFSET 4  // SH1116 屏幕需要 4 像素水平偏移

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
const char* WIFI_SSID = "iPhone_serendipity";     // WiFi名称
const char* WIFI_PASSWORD = "qwerqwer";           // WiFi密码
const uint8_t WIFI_CONNECT_ATTEMPTS = 20;         // WiFi最大连接尝试次数
const unsigned long WIFI_CONNECT_DELAY = 500;     // 每次连接尝试间隔(ms)
const unsigned long WIFI_RECONNECT_INTERVAL = 5000;  // WiFi重连间隔(ms)
const unsigned long LED_BLINK_INTERVAL = 500;     // LED闪烁间隔(ms)
const unsigned long HEARTBEAT_INTERVAL = 10000;    // 心跳打印间隔(ms)
const unsigned long KEY_BLINK_INTERVAL = 200;     // 按键长按LED闪烁间隔(ms)
const long SERIAL_BAUD_RATE = 115200;             // 串口波特率

/* ==================== 状态变量 ==================== */
enum WiFiState {
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_DISCONNECTED
};

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

/* 继电器状态映射表 [bit0=LED, bit1=FAN] */
const uint8_t RELAY_STATE_TABLE[4] = {
    0b00,  // 状态0: LED=OFF, FAN=OFF
    0b01,  // 状态1: LED=ON,  FAN=OFF
    0b10,  // 状态2: LED=OFF, FAN=ON
    0b11   // 状态3: LED=ON,  FAN=ON
};

/* 继电器状态名称表 */
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

/* ==================== OLED 显示相关变量 ==================== */
unsigned long lastOledUpdateMillis = 0;
const unsigned long OLED_UPDATE_INTERVAL = 500;  // OLED 刷新间隔 (ms)
bool g_forceOledUpdate = false;  // 全局强制更新标志

/* ==================== 传感器读取相关变量 ==================== */
unsigned long lastTempReadMillis = 0;
const unsigned long TEMP_READ_INTERVAL = 1000;  // 温度读取间隔 (ms)
unsigned long lastAdcReadMillis = 0;
const unsigned long ADC_READ_INTERVAL = 200;  // ADC 读取间隔 (ms)

/* 模拟传感器数据 */
float lightValue = 0.0;         // 光照值（lux，由 GL5516 读取）
float temperature = 25.5;        // 温度值（由 DS18B20 读取）
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

void updateOledDisplay(void);

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
            rgb_set_mode(RGB_MODE_BREATH);
            rgb_set_color(RGB_RED);
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            DEBUG_PRINTLN("[WiFi] 获得IP地址: " + WiFi.localIP().toString());
            wifiState = WIFI_CONNECTED;
            rgb_set_mode(RGB_MODE_BREATH);
            rgb_set_color(RGB_GREEN);
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            DEBUG_PRINTLN("[WiFi] IP地址丢失");
            wifiState = WIFI_DISCONNECTED;
            lastBlinkMillis = millis();
            rgb_set_mode(RGB_MODE_BREATH);
            rgb_set_color(RGB_RED);
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
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    DEBUG_PRINT("[WiFi] 正在连接: " + String(WIFI_SSID) + "...");
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
    
    // WiFi 连接成功后连接 MQTT
    mqtt_connect();
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
    
    // 初始化RGB为红色呼吸状态（未联网）
    rgb_set_mode(RGB_MODE_BREATH);
    rgb_set_color(RGB_RED);
    
    startWiFiConnection();

    config_init();
    serial_cmd_init();
    mqtt_init();

    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setContrast(0x60);  // SH1116 最佳对比度设置（0x00-0xFF）
    u8g2.setPowerSave(0);    // 开启显示
    
    Serial.println("OLED SH1116 display initialized");
    
    // 初始化温度传感器
    temp_init();
    
    // 初始化 ADC
    adc_init();
    
    // RGB 灯测试：快速彩虹色循环（减少阻塞时间）
    Serial.println("[RGB] 测试 RGB 灯...");
    rgb_set_color(RGB_RED);
    //delay(100);
    rgb_set_color(RGB_GREEN);
    //delay(100);
    rgb_set_color(RGB_BLUE);
    //delay(100);
    rgb_off();
    

    //u8g2.clearBuffer();
    //u8g2.drawFrame(0, 0, 120, 60);  // 画边框
    //u8g2.setCursor(10, 30);
    //delay(100);
    /* 调试用 TEST 画面 - 正式使用时注释掉
    u8g2.print("TEST");
    u8g2.sendBuffer();
    //*/
    
    /* 开机画面 */
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);  // 12像素GB2312字体
    
    // 第1行：欢迎（居中）
    u8g2.setCursor(OLED_OFFSET + 0, 16);
    u8g2.print("欢迎使用化工智能控制");
    
    // 第2行：设备ID
    u8g2.setCursor(OLED_OFFSET, 36);
    u8g2.print("ID:");
    u8g2.print(DEVICE_ID);
    
    // 第3行：版本
    u8g2.setCursor(OLED_OFFSET, 56);
    u8g2.print("V");
    u8g2.print(VERSION);
    
    u8g2.sendBuffer();
    delay(2000);  // 显示2秒
    
    Serial.println("OLED display content updated");
    Serial.println("System ready!");
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
            Serial.printf("[Heartbeat] TEMP: %.1f℃  |  LIGHT: %.1f lux\n", temperature, lightValue);
        }
    }
    
    /* 读取温度传感器（非阻塞，每隔 1 秒读取一次） */
    if (currentMillis - lastTempReadMillis >= TEMP_READ_INTERVAL) {
        lastTempReadMillis = currentMillis;
        float tempValue;
        if (temp_read(&tempValue) == TEMP_OK) {
            temperature = tempValue;
        }
    }
    
    /* 读取光照强度（lux，每隔 200ms 读取一次） */
    if (currentMillis - lastAdcReadMillis >= ADC_READ_INTERVAL) {
        lastAdcReadMillis = currentMillis;
        lightValue = adc_read_lux();
    }
    
    /* 自动模式下的传感器控制 */
    if (g_autoMode) {
        // 自动模式：根据传感器值自动控制
        // LIGHT < 阈值 → 打开 LED（光照低时开灯）
        bool newLightState = (lightValue < lightThreshold);
        // TEMP > 阈值 → 打开 FAN
        bool newFanState = (temperature > tempThreshold);
        
        if (newLightState != lightEnabled || newFanState != fanEnabled) {
            lightEnabled = newLightState;
            fanEnabled = newFanState;
            
            Serial.print("[Auto] 自动控制 - 光照: ");
            Serial.print(lightValue, 1);
            Serial.print("/");
            Serial.print(lightThreshold, 1);
            Serial.print(" lux (低亮开灯) → LED: ");
            Serial.println(lightEnabled ? "ON" : "OFF");
            
            Serial.print("[Auto] 自动控制 - 温度: ");
            Serial.print(temperature);
            Serial.print("/");
            Serial.print(tempThreshold);
            Serial.print(" → FAN: ");
            Serial.println(fanEnabled ? "ON" : "OFF");
            
            setOledForceUpdate();  // 强制更新 OLED
            mqtt_request_publish();  // 立即上报状态
        }
    }
    
    /* 处理按键事件（手动模式下控制，自动模式下仅 KEY2 长按切换模式） */
    handleKeyEvent();
    
    /* MQTT 循环处理 */
    mqtt_loop();
    
    /* 更新 RGB 状态 */
    rgb_update();
    
    /* 更新 OLED 显示 */
    updateOledDisplay();
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
            Serial.println("[Key1] 总开关 ON - 允许继电器输出");
            // 直接由 updateLedState() 根据内部状态恢复继电器
        } else {
            Serial.println("[Key1] 总开关 OFF - 强制断开继电器");
            // 强制关闭所有继电器
            lightEnabled = false;
            fanEnabled = false;
            LED(LOW);
            FAN(LOW);
        }
        setOledForceUpdate();  // 立即更新 OLED
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
                        
                        setOledForceUpdate();
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
                        // 切换到自动模式
                       // wifiState = WIFI_IDLE;
                       // startWiFiConnection();
                        Serial.println("[Key2] 长按 - 切换到自动模式");
                    } else {
                        // 切换到手动模式
                       // wifiState = WIFI_DISCONNECTED;
                       // WiFi.disconnect();
                        Serial.println("[Key2] 长按 - 切换到手动模式");
                    }
                    mqtt_request_publish();  // 立即上报状态
                } else {
                    Serial.println("[Key2] 长按无效 - 总闸已关闭");
                }
                break;
                
            default:
                break;
        }
        
        /* 设置强制更新标志，让 OLED 立即刷新 */
        setOledForceUpdate();
    }
}

/**
 * @brief    设置 OLED 强制更新标志
 * @param    无
 * @retval   无
 */
void setOledForceUpdate(void) {
    g_forceOledUpdate = true;  // 设置强制更新标志
}

/**
 * @brief    更新 OLED 显示内容
 * @param    无
 * @retval   无
 * @note     显示四行信息：模式/总闸、光照、温度、设备状态
 */
void updateOledDisplay(void) {
    static unsigned long lastUpdateMillis = 0;
    unsigned long currentMillis = millis();
    
    // 检查是否需要强制更新（通过全局标志）
    extern bool g_forceOledUpdate;
    
    if (!g_forceOledUpdate && (currentMillis - lastUpdateMillis < OLED_UPDATE_INTERVAL)) {
        return;
    }
    
    g_forceOledUpdate = false;  // 重置强制更新标志
    lastUpdateMillis = currentMillis;
    
    u8g2.clearBuffer();
    
    // 使用支持中英文混合的字体（unifont包含完整ASCII + 常用汉字）
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);  // GB2312小字体
    
    char buffer[64];
    
    // 第 1 行：模式:自动/手动  主:开/关
    u8g2.setCursor(OLED_OFFSET, 14);
    snprintf(buffer, sizeof(buffer), "模式:%s 总闸:%s", 
             g_autoMode ? "自动" : "手动",
             key1_is_on() ? "开" : "关");
    u8g2.print(buffer);
    
    // 第 2 行：光:1234/300
    u8g2.setCursor(OLED_OFFSET, 30);
    snprintf(buffer, sizeof(buffer), "光强:%.0f/%.0f", lightValue, lightThreshold);
    u8g2.print(buffer);
    
    // 第 3 行：温:25.5/28.0
    u8g2.setCursor(OLED_OFFSET, 46);
    snprintf(buffer, sizeof(buffer), "温度:%.1f/%.1f", temperature, tempThreshold);
    u8g2.print(buffer);
    
    // 第 4 行：灯:开 风:关（受总闸控制）
    u8g2.setCursor(OLED_OFFSET, 62);
    // 总闸关闭时，强制显示关闭状态
    bool key1State = key1_is_on();
    snprintf(buffer, sizeof(buffer), "灯光:%s 风扇:%s", 
             (key1State && lightEnabled) ? "开" : "关",
             (key1State && fanEnabled) ? "开" : "关");
    u8g2.print(buffer);
    
    u8g2.sendBuffer();
}