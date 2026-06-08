/**
 ****************************************************************************************************
 * @file        temp.cpp
 * @author      md
 * @version     V2.0
 * @date        2026-06-08
 * @brief       DS18B20 温度传感器驱动 - 独立 FreeRTOS 任务轮询
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#include "temp.h"
#include <Arduino.h>

/* 单总线对象 */
static OneWire oneWire(DS18B20_PIN);

/* DS18B20 对象 */
static DallasTemperature sensors(&oneWire);

/* 全局温度值，由后台任务更新 */
float g_temperature = 25.5f;

/**
 * @brief       DS18B20 初始化
 * @param       无
 * @retval      无
 */
void temp_init(void)
{
    Serial.println("[TEMP] 初始化 DS18B20...");
    
    sensors.begin();
    
    // 获取传感器数量
    uint8_t sensorCount = sensors.getDeviceCount();
    
    if (sensorCount > 0) {
        Serial.print("[TEMP] 发现 ");
        Serial.print(sensorCount);
        Serial.println(" 个 DS18B20 传感器");
        
        // 打印传感器地址
        DeviceAddress addr;
        sensors.getAddress(addr, 0);
        Serial.print("[TEMP] 传感器地址：");
        for (uint8_t i = 0; i < 8; i++) {
            Serial.print(addr[i], HEX);
            if (i < 7) Serial.print(":");
        }
        Serial.println();
    } else {
        Serial.println("[TEMP] ❌ 未发现 DS18B20 传感器！");
    }
}

/**
 * @brief       读取温度（立即返回缓存值，不阻塞）
 * @param       *temp: 温度值输出指针
 * @retval      TEMP_OK: 读取成功
 *              TEMP_ERROR: 读取失败
 */
uint8_t temp_read(float *temp)
{
    *temp = g_temperature;
    return TEMP_OK;
}

/**
 * @brief       后台温度轮询任务
 * @param       pvParameters: 未使用
 * @retval      无
 * @note        每 ~1 秒读取一次 DS18B20，更新 g_temperature。
 *              转换期间任务挂起，不占用 CPU。
 */
static void temp_task(void *pvParameters)
{
    while (1) {
        /* 发起温度转换（立即返回） */
        sensors.setWaitForConversion(false);
        sensors.requestTemperatures();
        
        /* 等待转换完成（750ms），让出 CPU 给其他任务 */
        vTaskDelay(pdMS_TO_TICKS(850));// 等待转换完成，850ms 是 DS18B20 最大转换时间
        
        /* 读取转换结果 */
        float val = sensors.getTempCByIndex(0);
        if (!(val == -127.0f || val < -50.0f || val > 125.0f)) {
            g_temperature = val;
        }
        
        /* 剩余等待，凑整 1 秒周期 */
        vTaskDelay(pdMS_TO_TICKS(150));// 等待 150ms，凑整 1 秒周期
    }
}

/**
 * @brief       创建温度轮询后台任务
 * @param       无
 * @retval      无
 * @note        在 setup() 中调用一次
 */
void temp_task_create(void)
{
    xTaskCreate(
        temp_task,          /* 任务函数 */
        "tempTask",         /* 任务名称 */
        4096,               /* 栈大小（字节） */
        NULL,               /* 参数 */
        1,                  /* 优先级（低） */
        NULL                /* 任务句柄（不需要） */
    );
    Serial.println("[TEMP] 后台温度轮询任务已创建");
}
