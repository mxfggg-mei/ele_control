/**
 ****************************************************************************************************
 * @file        key.h
 * @author      md
 * @version     V1.0
 * @date        2025-12-01
 * @brief       按键驱动代码
 * @license     Copyright (c)  
 ****************************************************************************************************
 * @attention
 *
 * 修改说明
 * V1.0 20251201
 * 第一次发布
 *
 ****************************************************************************************************
 */

#ifndef __KEY_H
#define __KEY_H

#include "Arduino.h"

/* 引脚定义 */
#define KEY_PIN       0   /* 按键连接到GPIO0引脚 */

/* 去抖动配置 */
#define KEY_DEBOUNCE_TIME    20   /* 按键去抖动时间(ms) */
#define KEY_LONG_PRESS_TIME  1000 /* 长按时间阈值(ms) */

/* 按键事件类型 */
#define KEY_EVENT_NONE       0    /* 无事件 */
#define KEY_EVENT_PRESS      1    /* 按下事件 */
#define KEY_EVENT_RELEASE    2    /* 释放事件 */
#define KEY_EVENT_LONG_PRESS 3    /* 长按事件 */

/* 宏函数定义 */
#define KEY_READ()    digitalRead(KEY_PIN)

/* 函数声明 */
void key_init(void);              /* 按键引脚初始化函数 */
uint8_t key_scan(void);           /* 按键扫描函数，返回按键状态(0/1) */
uint8_t key_scan_event(void);     /* 按键事件扫描函数，返回事件类型 */

#endif