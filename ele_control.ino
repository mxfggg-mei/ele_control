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
#include "version.h"
#include <WiFi.h>
#include <U8g2lib.h>
#include <Wire.h>

// 自定义 SDA 和 SCL 引脚
#define I2C_SDA  4  // 自定义 SDA 引脚
#define I2C_SCL  5  // 自定义 SCL 引脚

// 使用软件 I2C
U8G2_SH1106_128X64_VCOMH0_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ I2C_SCL, /* data=*/ I2C_SDA, /* reset=*/ U8X8_PIN_NONE);

//U8G2_SH1106_128X64_VCOMH0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

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
const unsigned long HEARTBEAT_INTERVAL = 1000;    // 心跳打印间隔(ms)
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

/* ==================== 按键相关变量 ==================== */
bool keyLongPressBlink = false;
unsigned long lastKeyBlinkMillis = 0;

/* ==================== OLED 显示相关变量 ==================== */
unsigned long lastOledUpdateMillis = 0;
const unsigned long OLED_UPDATE_INTERVAL = 500;  // OLED 刷新间隔 (ms)

/* 模拟传感器数据 */
float lightValue = 1234.0;
float lightThreshold = 1000.0;
float temperature = 25.5;
float tempThreshold = 40.0;
bool lightEnabled = true;
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
            lastBlinkMillis = millis(); // 重置闪烁定时器
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            DEBUG_PRINTLN("[WiFi] 获得IP地址: " + WiFi.localIP().toString());
            wifiState = WIFI_CONNECTED;
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            DEBUG_PRINTLN("[WiFi] IP地址丢失");
            wifiState = WIFI_DISCONNECTED;
            lastBlinkMillis = millis(); // 重置闪烁定时器
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
        LED(0);
        return;
    }
    
    DEBUG_PRINTLN("\n[WiFi] 连接成功！");
    DEBUG_PRINTLN("[WiFi] IP 地址：" + WiFi.localIP().toString());
    DEBUG_PRINTLN("[WiFi] 信号强度：" + String(WiFi.RSSI()) + " dBm");
    wifiState = WIFI_CONNECTED;
    LED(0);
}

/**
 * @brief    更新LED状态
 * @param    无
 * @retval   无
 */
void updateLedState(void) {
    unsigned long currentMillis = millis();
    
    switch (wifiState) {
        case WIFI_CONNECTED:
            LED(0);
            break;
        case WIFI_CONNECTING:
        case WIFI_DISCONNECTED:
            if (currentMillis - lastBlinkMillis >= LED_BLINK_INTERVAL) {
                lastBlinkMillis = currentMillis;
                LED_TOGGLE();
            }
            break;
        default:
            break;
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
    key_init();
    Serial.begin(SERIAL_BAUD_RATE);
    
    delay(100);
    
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
    startWiFiConnection();

    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setContrast(0x80);  // 设置对比度
    
    Serial.println("OLED display initialized");
    

    u8g2.clearBuffer();
    u8g2.drawFrame(0, 0, 120, 60);  // 画边框
    u8g2.setCursor(10, 30);

    delay(1000);
    /* 调试用 TEST 画面 - 正式使用时注释掉
    u8g2.print("TEST");
    u8g2.sendBuffer();
    //*/
    
    /* 启动画面 */
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(4, 20);
    u8g2.print("LIGHT SH1106 OLED");

    u8g2.setCursor(4, 40);
    u8g2.print("LIGHT 0.96 inch");

    u8g2.setCursor(4, 55);
    u8g2.print("LIGHT U8g2 Driver");
    u8g2.sendBuffer();
    Serial.println("OLED display content updated");
    Serial.println("System ready!");
    delay(1000);
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
    
    if (wifiState == WIFI_CONNECTED) {
        if (currentMillis - lastHeartbeatMillis >= HEARTBEAT_INTERVAL) {
            lastHeartbeatMillis = currentMillis;
            Serial.println("[Heartbeat] Hello World!");
        }
    }
    
    /* 处理按键事件 */
    handleKeyEvent();
    
    /* 更新 OLED 显示 */
    updateOledDisplay();
}

/**
 * @brief    处理按键事件
 * @param    无
 * @retval   无
 */
void handleKeyEvent(void) {
    uint8_t keyEvent = key_scan_event();
    unsigned long currentMillis = millis();
    static uint8_t lastKeyEvent = KEY_EVENT_NONE;
    
    /* 只在事件变化时打印，避免重复输出 */
    if (keyEvent != lastKeyEvent) {
        lastKeyEvent = keyEvent;
        
        switch (keyEvent) {
            case KEY_EVENT_PRESS:
                keyLongPressBlink = false;
                LED2(0);
                Serial.println("[Key] 按键按下");
                break;
                
            case KEY_EVENT_RELEASE:
                keyLongPressBlink = false;
                LED2(1);
                Serial.println("[Key] 按键释放");
                break;
                
            case KEY_EVENT_LONG_PRESS:
                keyLongPressBlink = true;
                lastKeyBlinkMillis = currentMillis;
                Serial.println("[Key] 按键长按 - LED闪烁");
                break;
                
            case KEY_EVENT_NONE:
                break;
        }
    }
    
    /* 处理长按闪烁 */
    if (keyLongPressBlink) {
        if (currentMillis - lastKeyBlinkMillis >= KEY_BLINK_INTERVAL) {
            lastKeyBlinkMillis = currentMillis;
            LED2_TOGGLE();
        }
    }
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
    
    if (currentMillis - lastUpdateMillis < OLED_UPDATE_INTERVAL) {
        return;
    }
    lastUpdateMillis = currentMillis;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tr);  // 6x13 像素字体
    
    char buffer[64];
    
    u8g2.setCursor(4, 12);  // x=4 补偿显示偏移
    snprintf(buffer, sizeof(buffer), "MODE:%s  MAIN:%s", 
             wifiState == WIFI_CONNECTED ? "AUTO" : "HAND",
             wifiState == WIFI_CONNECTED ? "ON" : "OFF");
    u8g2.print(buffer);
    
    //u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(4, 26);
    snprintf(buffer, sizeof(buffer), "LIGHT:%d/%d", 
             (int)lightValue, (int)lightThreshold);
    u8g2.print(buffer);
    
    u8g2.setCursor(4, 40);
    snprintf(buffer, sizeof(buffer), "TEMP:%.1f/%.1f", 
             temperature, tempThreshold);
    u8g2.print(buffer);
    
    u8g2.setCursor(4, 54);
    snprintf(buffer, sizeof(buffer), "LED:%s  FAN:%s", 
             lightEnabled ? "ON" : "OFF",
             fanEnabled ? "ON" : "OFF");
    u8g2.print(buffer);
    
    u8g2.sendBuffer();
}