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
static bool key2_pressed = false;  // 当前是否按下

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
    
    // 附加中断：上升沿触发（按键按下）
    attachInterrupt(digitalPinToInterrupt(KEY_PIN2), key2_isr_handler, RISING);//
    
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
    static unsigned long lastInterruptTime = 0;
    unsigned long currentTime = millis();
    
    // 去抖：忽略 KEY_DEBOUNCE_TIME 内的重复中断
    if (currentTime - lastInterruptTime < KEY_DEBOUNCE_TIME) {
        return;
    }
    
    // 重要：验证按键是否真的按下（GPIO 应该是 HIGH）
    if (digitalRead(KEY_PIN2) != HIGH) {
        // 不是真正的按下，可能是干扰
        return;
    }
    
    lastInterruptTime = currentTime;
    
    // 记录按下时间
    key2_pressStartTime = currentTime;
    key2_pressed = true;
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
uint8_t key2_get_event(void)
{
    uint8_t event = KEY_EVENT_NONE;
    
    // 检查是否按下
    if (key2_pressed) {
        unsigned long pressDuration = millis() - key2_pressStartTime;
        
        // 检查是否释放（读取当前引脚状态）
        uint8_t currentReading = digitalRead(KEY_PIN2);
        
        if (currentReading == LOW) {
            // 按键已释放
            
            // 验证：按下时间必须大于去抖时间，防止误触发
            if (pressDuration < KEY_DEBOUNCE_TIME) {
                Serial.print("[KEY2] 异常：按下时间过短 (");
                Serial.print(pressDuration);
                Serial.println("ms)，忽略");
                key2_pressed = false;
                return KEY_EVENT_NONE;
            }
            
            key2_pressed = false;
            
            // 防止异常值
            if (pressDuration > 10000) {
                pressDuration = 0;
            }
            
            Serial.print("[KEY2] 按下时长：");
            Serial.print(pressDuration);
            Serial.print(" ms (阈值：");
            Serial.print(KEY_DEBOUNCE_TIME);
            Serial.print("-");
            Serial.print(KEY_LONG_PRESS_TIME);
            Serial.println("ms)");
            
            // 判断事件类型
            if (pressDuration >= KEY_LONG_PRESS_TIME) {
                // 长按释放
                Serial.println("[KEY2] 长按已触发，释放时不产生事件");
                event = KEY_EVENT_NONE;
            } else {
                // 短按
                event = KEY_EVENT_SHORT_PRESS;
                Serial.println("[KEY2] >> 短按事件触发");
            }
            
            key2_longPressTriggered = false;
        } else {
            // 按键仍按住，检查长按
            if (!key2_longPressTriggered && pressDuration >= KEY_LONG_PRESS_TIME) {
                key2_longPressTriggered = true;
                event = KEY_EVENT_LONG_PRESS;
                Serial.println("[KEY2] >> 长按事件触发");
            }
        }
    }
    
    return event;
}