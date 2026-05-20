/**
 ****************************************************************************************************
 * @file        led.cpp
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

#include "led.h"

/**
* @brief       初始化LED相关IO口
* @param       无
* @retval      无
*/
void led_init(void) 
{
    pinMode(LED_PIN, OUTPUT);     /* 设置led引脚为输出模式 */
    digitalWrite(LED_PIN, HIGH);  /* 结合原理图设计,实物LED获得高电平会熄灭 */
    
    pinMode(LED_PIN2, OUTPUT);    /* 设置led2引脚为输出模式 */
    digitalWrite(LED_PIN2, HIGH); /* LED2初始状态为熄灭 */
}


