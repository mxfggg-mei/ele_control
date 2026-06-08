/**
 ****************************************************************************************************
 * @file        key.cpp
 * @author      md
 * @version     V3.0
 * @date        2025-12-01
 * @brief       按键驱动代码 - KEY1 轮询 + KEY2 中断
 * @license     Copyright (c) 
 ****************************************************************************************************
 * @attention
 *
 * 修改说明
 * V3.0 20251201 - KEY1 轮询 + KEY2 中断模式
 * V2.0 20251201 - 支持自锁和自恢复按键
 * V1.0 20251201 - 第一次发布
 *
 ****************************************************************************************************
 */

#include "key.h"

/* ==================== KEY1: 自锁开关（总开关） ==================== */
/* 硬件自锁，直接读取物理状态即可 */
static uint8_t key1_state = KEY_STATE_OFF;  // KEY1 状态
static unsigned long key1_lastDebounceTime = 0;
static uint8_t key1_lastReading = LOW;

/* ==================== KEY2: 自复位按键（中断模式） ==================== */
static unsigned long key2_pressStartTime = 0;
static bool key2_longPressTriggered = false;
static volatile bool key2_pressed = false;  // 当前是否按下（volatile 保证 ISR 安全）
static unsigned long key2_lastInterruptTime = 0;  // 中断去抖时间

/**
 * @brief       KEY1 初始化（自锁总开关）
 * @param       无
 * @retval      无
 * @note        KEY1 是硬件自锁开关，保持两种状态
 */
void key1_init(void)
{
    pinMode(KEY_PIN1, INPUT_PULLDOWN);
    
    // 读取初始状态
    key1_lastReading = KEY1_READ();
    key1_state = (key1_lastReading == HIGH) ? KEY_STATE_ON : KEY_STATE_OFF;
    key1_lastDebounceTime = millis();
}

/**
 * @brief       KEY1 状态查询
 * @param       无
 * @retval      KEY_STATE_ON 或 KEY_STATE_OFF
 */
uint8_t key1_get_state(void)
{
    uint8_t reading = KEY1_READ();
    
    // 检测电平变化
    if (reading != key1_lastReading) {
        // 电平变化，重置计时器
        key1_lastDebounceTime = millis();
        key1_lastReading = reading;
    }
    
    // 等待稳定时间后更新状态
    if ((millis() - key1_lastDebounceTime) > KEY_DEBOUNCE_TIME) {
        // 电平稳定，更新状态
        key1_state = (reading == HIGH) ? KEY_STATE_ON : KEY_STATE_OFF;
    }
    
    return key1_state;
}

/**
 * @brief       检查 KEY1 是否处于 ON 状态
 * @param       无
 * @retval      true: ON（允许继电器输出）
 *              false: OFF（强制断开继电器）
 */
bool key1_is_on(void)
{
    return (key1_get_state() == KEY_STATE_ON);
}

/**
 * @brief       KEY2 初始化（自复位按键，中断模式）
 * @param       无
 * @retval      无
 */
void key2_init(void)
{
    // 配置引脚为输入下拉
    pinMode(KEY_PIN2, INPUT_PULLDOWN);
    
    // 附加中断：仅上升沿触发（按键按下）
    attachInterrupt(digitalPinToInterrupt(KEY_PIN2), key2_isr_handler, RISING);
    
    // 初始化变量
    key2_pressStartTime = 0;
    key2_longPressTriggered = false;
    key2_pressed = false;
}

/**
 * @brief       KEY2 中断处理函数（ISR）
 * @param       无
 * @retval      无
 * @note        当按键按下时触发（上升沿）
 */
void key2_isr_handler(void)
{
    unsigned long currentTime = millis();
    
    // 去抖：忽略 KEY_DEBOUNCE_TIME 内的重复中断
    if (currentTime - key2_lastInterruptTime < KEY_DEBOUNCE_TIME) {
        return;
    }
    
    // 再次验证：确保是真正的按下
    if (digitalRead(KEY_PIN2) != HIGH) {
        return;
    }
    
    key2_pressStartTime = currentTime;
    key2_pressed = true;
    key2_longPressTriggered = false;
    key2_lastInterruptTime = currentTime;
    
    Serial.print("[KEY2 ISR] 按下 @");
    Serial.println(currentTime);
}

/**
 * @brief       检查 KEY2 是否按下
 * @param       无
 * @retval      true: 按下，false: 未按下
 */
bool key2_is_pressed(void)
{
    return key2_pressed;
}

/**
 * @brief       获取 KEY2 事件（非阻塞，增强防误触发）
 * @param       无
 * @retval      KEY_EVENT_SHORT_PRESS: 短按（<1s）
 *              KEY_EVENT_LONG_PRESS: 长按（≥1s）
 *              KEY_EVENT_NONE: 无事件
 * @note        在 loop() 中调用，检测按键释放后的事件
 */
/**
 * @brief       获取 KEY2 事件（非阻塞，优化长按检测）
 * @param       无
 * @retval      KEY_EVENT_SHORT_PRESS: 短按（<1s）
 *              KEY_EVENT_LONG_PRESS: 长按（≥1s）
 *              KEY_EVENT_NONE: 无事件
 * @note        在 loop() 中调用，按键释放时产生事件
 */
uint8_t key2_get_event(void)
{
    uint8_t event = KEY_EVENT_NONE;
    
    // 检查是否按下
    if (key2_pressed) {
        unsigned long pressDuration = millis() - key2_pressStartTime;
        
        // 检查是否释放（读取 GPIO 状态）
        if (digitalRead(KEY_PIN2) == LOW) {
            // 按键已释放
            Serial.print("[KEY2] 释放检测：");
            Serial.print(pressDuration);
            Serial.println("ms");
            
            // 如果未触发长按，产生短按事件
            if (!key2_longPressTriggered) {
                if (pressDuration >= KEY_DEBOUNCE_TIME) {
                    event = KEY_EVENT_SHORT_PRESS;
                    Serial.println("[KEY2] >> 短按事件");
                } else {
                    Serial.println("[KEY2] 忽略抖动");
                }
            } else {
                Serial.println("[KEY2] 已触发长按，忽略释放");
            }
            
            // 重置状态
            key2_pressed = false;
            key2_longPressTriggered = false;
            
        } else {
            // 按键仍按住，检查长按
            if (!key2_longPressTriggered && pressDuration >= KEY_LONG_PRESS_TIME) {
                key2_longPressTriggered = true;
                event = KEY_EVENT_LONG_PRESS;
                Serial.print("[KEY2] >> 长按触发 @");
                Serial.println(pressDuration);
            }
        }
    }
    
    return event;
}