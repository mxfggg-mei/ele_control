/**
 ****************************************************************************************************
 * @file        rgb.h
 * @author      md
 * @version     V1.0
 * @date        2026-05-20
 * @brief       RGB 万色灯驱动头文件
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#ifndef __RGB_H
#define __RGB_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

/* RGB 灯配置 */
#define RGB_LED_PIN    0        /* RGB 灯控制引脚，连接到 GPIO0 */
#define NUMPIXELS      1        /* 灯珠数量 */

/* 颜色定义（RGB 格式） */
#define RGB_RED        0xFF0000
#define RGB_GREEN      0x00FF00
#define RGB_BLUE       0x0000FF
#define RGB_YELLOW     0xFFFF00
#define RGB_PURPLE     0xFF00FF
#define RGB_CYAN       0x00FFFF
#define RGB_WHITE      0xFFFFFF
#define RGB_OFF        0x000000

/* RGB 工作模式 */
#define RGB_MODE_OFF       0    /* 关闭 */
#define RGB_MODE_ON        1    /* 常亮 */
#define RGB_MODE_BREATH     2    /* 呼吸灯 */
#define RGB_MODE_FLASH      3    /* 闪烁 */

/* 函数声明 */
void rgb_init(void);
void rgb_set_color(uint32_t color);
void rgb_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void rgb_off(void);
void rgb_breath(uint32_t color, uint8_t brightness);  /* 阻塞版本，仅用于测试 */

/* 非阻塞控制函数（推荐在主循环中使用） */
void rgb_update(void);  /* 更新 RGB 状态 */
void rgb_set_mode(uint8_t mode);  /* 设置模式 */

#endif /* __RGB_H */
