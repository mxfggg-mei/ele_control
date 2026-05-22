/**
 ****************************************************************************************************
 * @file        adc.h
 * @author      md
 * @version     V1.0
 * @date        2026-05-20
 * @brief       ADC 光照强度读取头文件
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#ifndef __ADC_H
#define __ADC_H

#include <Arduino.h>

/* ADC 引脚定义 */
#define ADC_PIN        1   /* ADC 输入引脚，连接到 GPIO0（光照传感器） */

/* ADC 配置 */
#define ADC_ATTENUATION    ADC_11db          /* 衰减系数（扩大测量范围） */
#define ADC_RESOLUTION     12                /* ADC 分辨率（12 位：0-4095） */
#define ADC_REF_VOLTAGE    3.3               /* 参考电压（V） */

/* 函数声明 */
void adc_init(void);
uint16_t adc_read_raw(void);
float adc_read_voltage(void);

#endif /* __ADC_H */
