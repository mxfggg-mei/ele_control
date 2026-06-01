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
#define KEY_PIN1       20   /* KEY1: 自锁开关，连接到 GPIO20（总开关，下拉电阻，高电平=ON） */
#define KEY_PIN2       21   /* KEY2: 自复位按键，连接到 GPIO21（短按/长按，下拉电阻，高电平=按下） */

/* 按键类型定义 */
#define KEY_TYPE_LOCK      1    /* 自锁按键（两态保持） */
#define KEY_TYPE_MOMENTARY 2    /* 自复位按键（按下导通） */

/* 去抖动配置 */
#define KEY_DEBOUNCE_TIME    50   /* 按键去抖动时间 (ms) */
#define KEY_LONG_PRESS_TIME  2000 /* 长按时间阈值 (ms) */

/* 按键事件类型 */
#define KEY_EVENT_NONE       0    /* 无事件 */
#define KEY_EVENT_SHORT_PRESS 1   /* 短按事件 */
#define KEY_EVENT_LONG_PRESS  2   /* 长按事件 */

/* 按键状态 */
#define KEY_STATE_OFF        0    /* 自锁开关 OFF（断开） */
#define KEY_STATE_ON         1    /* 自锁开关 ON（闭合） */

/* 宏函数定义 */
#define KEY1_READ()    digitalRead(KEY_PIN1)
#define KEY2_READ()    digitalRead(KEY_PIN2)

/* 函数声明 */
void key1_init(void);              /* KEY1 初始化 */
uint8_t key1_get_state(void);      /* KEY1 状态查询 */
bool key1_is_on(void);             /* KEY1 是否 ON */

void key2_init(void);              /* KEY2 初始化 */
void key2_isr_handler(void);       /* KEY2 中断处理函数 */
uint8_t key2_get_event(void);      /* 获取 KEY2 事件（非阻塞） */
bool key2_is_pressed(void);        /* KEY2 是否按下 */

#endif