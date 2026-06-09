/**
 ****************************************************************************************************
 * @file        rgb.h
 * @author      md
 * @version     V2.0
 * @date        2026-06-08
 * @brief       RGB 万色灯驱动头文件 - 支持序列动画
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#ifndef __RGB_H
#define __RGB_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

/* RGB 灯配置 */
#define RGB_LED_PIN    0        /* RGB 灯控制引脚，连接到 GPIO0 */
#define NUMPIXELS      1        /* 灯珠数量 */

/* 颜色定义（RGB 格式） */
#define RGB_RED        0xFF0000
#define RGB_GREEN      0x00FF00
#define RGB_BLUE       0x0000FF
#define RGB_YELLOW     0xFFFF00
#define RGB_ORANGE     0xFF8800
#define RGB_PURPLE     0xFF00FF
#define RGB_CYAN       0x00FFFF
#define RGB_WHITE      0xFFFFFF
#define RGB_OFF        0x000000

/* 旧模式（保持兼容） */
#define RGB_MODE_OFF       0    /* 关闭 */
#define RGB_MODE_ON        1    /* 常亮 */
#define RGB_MODE_BREATH    2    /* 呼吸灯 */
#define RGB_MODE_FLASH     3    /* 闪烁 */

/* 新模式 */
#define RGB_MODE_ALARM_LED_FAN  10  /* 严重报警紫橙闪烁（LED灯+风扇同时启动） */
#define RGB_MODE_ALARM_LIGHT   11  /* 光照越界报警（橙色呼吸+通讯指示） */
#define RGB_MODE_ALARM_TEMP    12  /* 温度越界报警（紫色呼吸+通讯指示） */

/* 步骤类型 */
#define RGB_STEP_SOLID  0   /* 直接设置颜色并保持 */
#define RGB_STEP_FADE   1   /* 从前一颜色渐变为目标颜色 */

/* 步骤定义 */
typedef struct {
    uint32_t color;      /* 目标颜色 */
    uint16_t duration_ms; /* 持续时间(ms) */
    uint8_t  type;       /* RGB_STEP_SOLID 或 RGB_STEP_FADE */
} rgb_step_t;

/* 函数声明 */
void rgb_init(void);
void rgb_set_color(uint32_t color);
void rgb_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void rgb_off(void);
void rgb_boot_flash(void);                   /* 启动多色快闪（阻塞，在 setup 中调用） */
void rgb_breath(uint32_t color, uint8_t brightness);  /* 阻塞版本，仅用于测试 */
void rgb_update(void);               /* 更新 RGB 状态（在 loop 中调用） */
void rgb_set_mode(uint8_t mode);     /* 设置模式 */
void rgb_update_by_state(wl_status_t wifi_status);  /* 根据设备状态自动选择模式并更新 */
void rgb_task_create(void);          /* 创建 RGB 灯控后台任务（在 setup 中调用） */

#endif /* __RGB_H */
