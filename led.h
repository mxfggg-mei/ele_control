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
#define LED_PIN       1   /* 开发板上LED连接到GPIO1引脚 */
#define LED_PIN2      8   /* 开发板上LED连接到GPIO8引脚 */


/* 宏函数定义 */
#define LED(x)        digitalWrite(LED_PIN, x)
#define LED_TOGGLE()  digitalWrite(LED_PIN, !digitalRead(LED_PIN))

#define LED2(x)       digitalWrite(LED_PIN2, x)
#define LED2_TOGGLE()  digitalWrite(LED_PIN2, !digitalRead(LED_PIN2))


/* 函数声明 */
void led_init(void);      /* led引脚初始化函数 */

#endif
