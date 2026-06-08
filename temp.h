/**
 ****************************************************************************************************
 * @file        temp.h
 * @author      md
 * @version     V2.0
 * @date        2026-06-08
 * @brief       DS18B20 温度传感器驱动头文件 - 独立任务轮询版
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

/* 全局温度值（由独立任务持续更新，主线程直接读） */
extern float g_temperature;

/* 函数声明 */
void temp_init(void);
uint8_t temp_read(float *temp);
void temp_task_create(void);  /* 创建后台温度轮询任务 */

#endif /* __TEMP_H */
