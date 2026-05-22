/**
 ****************************************************************************************************
 * @file        temp.h
 * @author      md
 * @version     V1.0
 * @date        2026-05-20
 * @brief       DS18B20 温度传感器驱动头文件
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#ifndef __TEMP_H
#define __TEMP_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* DS18B20 引脚定义 */
#define DS18B20_PIN    10   /* DS18B20 数据引脚，连接到 GPIO10 */

/* 温度读取状态 */
#define TEMP_OK        0   /* 读取成功 */
#define TEMP_ERROR     1   /* 读取失败 */

/* 函数声明 */
void temp_init(void);
uint8_t temp_read(float *temp);

#endif /* __TEMP_H */
