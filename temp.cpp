/**
 ****************************************************************************************************
 * @file        temp.cpp
 * @author      md
 * @version     V1.0
 * @date        2026-05-20
 * @brief       DS18B20 温度传感器驱动实现
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#include "temp.h"

/* 单总线对象 */
static OneWire oneWire(DS18B20_PIN);

/* DS18B20 对象 */
static DallasTemperature sensors(&oneWire);

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
 * @brief       读取温度（异步非阻塞）
 * @param       *temp: 温度值输出指针（摄氏度）
 * @retval      TEMP_OK: 读取成功
 *              TEMP_ERROR: 读取失败
 */
uint8_t temp_read(float *temp)
{
    // 发送温度转换指令
    sensors.requestTemperatures();
    
    // 注意：DS18B20 需要 750ms 转换时间
    // 这里不阻塞等待，由主程序控制读取频率
    // 下次读取时，上一次的温度已经转换完成
    
    // 读取温度（第一个传感器）
    float tempValue = sensors.getTempCByIndex(0);
    
    // 检查读取是否成功（-127 表示读取失败）
    if (tempValue == -127.0 || tempValue < -50.0 || tempValue > 125.0) {
        //Serial.println("[TEMP] ❌ 温度读取失败");
        return TEMP_ERROR;
    }
    
    // 输出温度值
    *temp = tempValue;
    
    // 调试打印
    Serial.print("[TEMP] 温度：");
    Serial.print(*temp);
    Serial.println(" ℃");
    
    return TEMP_OK;
}
