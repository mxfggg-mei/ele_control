/**
 ****************************************************************************************************
 * @file        adc.cpp
 * @author      md
 * @version     V1.0
 * @date        2026-05-20
 * @brief       ADC 光照强度读取实现
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#include "adc.h"

/**
 * @brief       ADC 初始化
 * @param       无
 * @retval      无
 */
void adc_init(void)
{
    Serial.println("[ADC] 初始化 ADC...");
    
    // 配置 ADC 引脚
    pinMode(ADC_PIN, ANALOG);
    
    // 配置 ADC 衰减（ESP32-C3 需要设置衰减以扩大测量范围）
    analogReadResolution(ADC_RESOLUTION);
    analogSetAttenuation(ADC_ATTENUATION);
    
    Serial.print("[ADC] ADC 引脚：GPIO");
    Serial.println(ADC_PIN);
    Serial.print("[ADC] 分辨率：");
    Serial.print(ADC_RESOLUTION);
    Serial.println(" 位");
    Serial.print("[ADC] 参考电压：");
    Serial.print(ADC_REF_VOLTAGE);
    Serial.println(" V");
}

/**
 * @brief       读取 ADC 原始值
 * @param       无
 * @retval      ADC 原始值（0-4095）
 */
uint16_t adc_read_raw(void)
{
    return analogRead(ADC_PIN);
}

/**
 * @brief       读取电压值
 * @param       无
 * @retval      电压值（单位：V）
 */
float adc_read_voltage(void)
{
    uint16_t adcValue = adc_read_raw();
    float voltage = adcValue * (ADC_REF_VOLTAGE / pow(2, ADC_RESOLUTION));
    
    return voltage;
}
