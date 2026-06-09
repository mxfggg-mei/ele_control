/**
 ****************************************************************************************************
 * @file        led.h
 * @author      md
 * @version     V1.0
 * @date        2025-12-01
 * @brief       LED 驱动代码
 * @license     Copyright (c)  
 ****************************************************************************************************
 * @attention
 *

 *
 * 修改说明
 * V1.0 20251201
 * 第一次发布
 *
 ****************************************************************************************************
 */

#ifndef __LED_H
#define __LED_H

#include "Arduino.h"

/* 引脚定义 */

#define LED_PIN      6   /* 开发板上LED连接到GPIO6引脚 */
#define FAN_PIN      7   /* 开发板上风机连接到GPIO7引脚 */

/* 宏函数定义 */
#define LED(x)        digitalWrite(LED_PIN, x)
#define LED_TOGGLE()  digitalWrite(LED_PIN, !digitalRead(LED_PIN))

#define FAN(x)       digitalWrite(FAN_PIN, x)
#define FAN_TOGGLE()  digitalWrite(FAN_PIN, !digitalRead(FAN_PIN))


/* 自动控制迟滞（避免临界值附近频繁开关） */
#define TEMP_HYSTERESIS  0.5f   /* 温度迟滞（℃）：温度高于 阈值+迟滞 开风扇，低于 阈值-迟滞 关风扇 */
#define LIGHT_HYSTERESIS 10.0f  /* 光照迟滞（lux）：光照低于 阈值-迟滞 开灯，高于 阈值+迟滞 关灯 */

/* 继电器状态循环表 [bit0=LED, bit1=FAN] */
extern const uint8_t RELAY_STATE_TABLE[4];
extern const char* RELAY_STATE_NAME[4];

/* 函数声明 */
void led_init(void);      /* led引脚初始化函数 */
void fan_init(void);      /* fan引脚初始化函数 */

#endif
