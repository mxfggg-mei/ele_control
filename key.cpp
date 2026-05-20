/**
 ****************************************************************************************************
 * @file        key.cpp
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

#include "key.h"

/**
* @brief       初始化按键相关IO口
* @param       无
* @retval      无
*/
void key_init(void) 
{
    pinMode(KEY_PIN, INPUT_PULLUP);     /* 设置按键引脚为输入模式，启用内部上拉电阻 */
}

/**
* @brief       按键扫描函数（带去抖动）
* @param       无
* @retval      0: 按键未按下, 1: 按键按下
* @note        使用状态机实现去抖动，避免机械按键抖动导致误触
*/
uint8_t key_scan(void) 
{
    static unsigned long lastDebounceTime = 0;
    static uint8_t lastKeyState = HIGH;
    static uint8_t keyState = HIGH;
    uint8_t reading;
    
    reading = KEY_READ();
    
    if (reading != lastKeyState) {
        lastDebounceTime = millis();
        lastKeyState = reading;
    }
    
    if ((millis() - lastDebounceTime) > KEY_DEBOUNCE_TIME) {
        if (reading != keyState) {
            keyState = reading;
        }
    }
    
    return (keyState == LOW) ? 1 : 0;
}

/**
* @brief       按键事件扫描函数（带去抖动和事件检测）
* @param       无
* @retval      按键事件类型(KEY_EVENT_NONE/PRESS/RELEASE/LONG_PRESS)
* @note        检测按键按下、释放和长按事件
*/
uint8_t key_scan_event(void) 
{
    static unsigned long lastDebounceTime = 0;
    static unsigned long pressStartTime = 0;
    static uint8_t lastKeyState = HIGH;
    static uint8_t keyState = HIGH;
    static bool longPressTriggered = false;
    uint8_t reading;
    uint8_t event = KEY_EVENT_NONE;
    
    reading = KEY_READ();
    
    /* 状态变化检测，重置去抖动定时器 */
    if (reading != lastKeyState) {
        lastDebounceTime = millis();
        lastKeyState = reading;
    }
    
    /* 去抖动处理 */
    if ((millis() - lastDebounceTime) > KEY_DEBOUNCE_TIME) {
        if (reading != keyState) {
            keyState = reading;
            
            if (keyState == LOW) {
                /* 按键按下 */
                pressStartTime = millis();
                longPressTriggered = false;
                event = KEY_EVENT_PRESS;
            } else {
                /* 按键释放 */
                event = KEY_EVENT_RELEASE;
                longPressTriggered = false;  /* 重置长按标志 */
            }
        }
    }
    
    /* 长按检测 */
    if (keyState == LOW && !longPressTriggered) {
        if ((millis() - pressStartTime) > KEY_LONG_PRESS_TIME) {
            longPressTriggered = true;
            event = KEY_EVENT_LONG_PRESS;
        }
    }
    
    return event;
}