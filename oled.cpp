/**
 ****************************************************************************************************
 * @file        oled.cpp
 * @author      md
 * @version     V1.0
 * @date        2026-06-09
 * @brief       OLED 显示驱动独立模块 - 非阻塞刷新
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#include "oled.h"
#include "key.h"
#include "mqtt.h"
#include "config.h"
#include "temp.h"
#include "version.h"
#include <U8g2lib.h>
#include <Wire.h>

/* ==================== 硬件配置 ==================== */
#define I2C_SDA       4
#define I2C_SCL       5
#define OLED_OFFSET   4          /* SH1116 水平偏移像素 */

/* ==================== U8g2 对象 ==================== */
static U8G2_SH1106_128X64_VCOMH0_F_SW_I2C u8g2(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE);

/* ==================== 内部状态 ==================== */
static unsigned long lastUpdateMillis = 0;
static const unsigned long UPDATE_INTERVAL = 100;  /* 刷新间隔 (ms) */
static bool forceUpdate = false;

/**
 * @brief       初始化 OLED 并显示开机画面
 * @param       无
 * @retval      无
 */
void oled_init(void)
{
    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setContrast(0x60);      /* SH1116 最佳对比度 */
    u8g2.setPowerSave(0);        /* 开启显示 */

    /* 开机画面 */
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);

    u8g2.setCursor(OLED_OFFSET + 0, 16);
    u8g2.print("欢迎使用化工智能控制");

    u8g2.setCursor(OLED_OFFSET, 36);
    u8g2.print("ID:");
    u8g2.print(get_mqtt_config()->deviceId);

    u8g2.setCursor(OLED_OFFSET, 56);
    u8g2.print("V");
    u8g2.print(VERSION);

    u8g2.sendBuffer();

    delay(200);  /* 短暂停留后进入主循环 */
}

/**
 * @brief       强制下一次更新（忽略刷新间隔）
 * @param       无
 * @retval      无
 */
void oled_force_update(void)
{
    forceUpdate = true;
}

/**
 * @brief       更新 OLED 显示内容（非阻塞）
 * @param       无
 * @retval      无
 * @note        每隔 UPDATE_INTERVAL ms 刷新一次，
 *              调用 oled_force_update() 可立即刷新。
 *              显示内容：WiFi/MQTT 状态、模式、光照、温度、继电器输出。
 */
void oled_update(void)
{
    unsigned long now = millis();

    if (!forceUpdate && (now - lastUpdateMillis < UPDATE_INTERVAL)) {
        return;
    }

    forceUpdate = false;
    lastUpdateMillis = now;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);

    char buf[64];

    /* 第 1 行：WiFi 和 MQTT 状态 */
    u8g2.setCursor(OLED_OFFSET, 11);
    snprintf(buf, sizeof(buf), "WiFi: %s  MQTT: %s",
             (wifiState == 2) ? "\u221A" : "\u00D7",
             mqtt_is_connected() ? "\u221A" : "\u00D7");
    u8g2.print(buf);

    /* 第 2 行：模式 + 总闸 */
    u8g2.setCursor(OLED_OFFSET, 24);
    snprintf(buf, sizeof(buf), "模式: %s  总闸: %s",
             g_autoMode ? "自动" : "手动",
             key1_is_on() ? "开" : "关");
    u8g2.print(buf);

    /* 第 3 行：光照 */
    u8g2.setCursor(OLED_OFFSET, 37);
    snprintf(buf, sizeof(buf), "光强: %.0f/%.0f", lightValue, lightThreshold);
    u8g2.print(buf);

    /* 第 4 行：温度 */
    u8g2.setCursor(OLED_OFFSET, 50);
    snprintf(buf, sizeof(buf), "温度: %.1f/%.1f", g_temperature, tempThreshold);
    u8g2.print(buf);

    /* 第 5 行：灯光 + 风扇 */
    u8g2.setCursor(OLED_OFFSET, 63);
    bool k1 = key1_is_on();
    snprintf(buf, sizeof(buf), "灯光: %s  风扇: %s",
             (k1 && lightEnabled) ? "开" : "关",
             (k1 && fanEnabled) ? "开" : "关");
    u8g2.print(buf);

    u8g2.sendBuffer();
}