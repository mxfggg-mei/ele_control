/**
 ****************************************************************************************************
 * @file        rgb.cpp
 * @author      md
 * @version     V1.0
 * @date        2026-05-20
 * @brief       RGB 万色灯驱动实现
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#include "rgb.h"

/* NeoPixel 对象 */
static Adafruit_NeoPixel pixels(NUMPIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

/* RGB 状态变量 */
static uint8_t rgb_current_mode = RGB_MODE_OFF;
static uint32_t rgb_current_color = RGB_OFF;
static unsigned long rgb_last_update = 0;
static uint8_t rgb_brightness = 0;
static bool rgb_flash_state = false;

/**
 * @brief       RGB 灯初始化
 * @param       无
 * @retval      无
 */
void rgb_init(void)
{
    Serial.println("[RGB] 初始化 RGB 灯...");
    
    pixels.begin();
    pixels.clear();
    pixels.show();
    
    Serial.print("[RGB] 引脚：GPIO");
    Serial.println(RGB_LED_PIN);
    Serial.print("[RGB] 灯珠数量：");
    Serial.println(NUMPIXELS);
    
    // 启动测试：快速闪烁白色（减少延时）
    //rgb_set_color(RGB_WHITE);
    //delay(100);
    rgb_off();
}

/**
 * @brief       设置颜色（使用预定义颜色）
 * @param       color: 颜色值（RGB 格式，如 RGB_RED）
 * @retval      无
 */
void rgb_set_color(uint32_t color)
{
    pixels.setPixelColor(0, color);
    pixels.show();
}

/**
 * @brief       设置颜色（自定义 RGB 值）
 * @param       r: 红色分量（0-255）
 * @param       g: 绿色分量（0-255）
 * @param       b: 蓝色分量（0-255）
 * @retval      无
 */
void rgb_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = pixels.Color(r, g, b);
    pixels.setPixelColor(0, color);
    pixels.show();
}

/**
 * @brief       关闭灯
 * @param       无
 * @retval      无
 */
void rgb_off(void)
{
    pixels.clear();
    pixels.show();
}

/**
 * @brief       设置 RGB 模式
 * @param       mode: 模式（RGB_MODE_OFF/ON/BREATH/FLASH）
 * @retval      无
 */
void rgb_set_mode(uint8_t mode)
{
    rgb_current_mode = mode;
    rgb_last_update = millis();
    
    if (mode == RGB_MODE_OFF) {
        rgb_off();
    }
}

/**
 * @brief       更新 RGB 状态（非阻塞）
 * @param       无
 * @retval      无
 * @note        在 loop() 中调用，实现非阻塞控制
 */
void rgb_update(void)
{
    unsigned long currentMillis = millis();
    
    switch (rgb_current_mode) {
        case RGB_MODE_ON:
            rgb_set_color(rgb_current_color);
            break;
            
        case RGB_MODE_FLASH:
            if (currentMillis - rgb_last_update >= 500) {
                rgb_last_update = currentMillis;
                rgb_flash_state = !rgb_flash_state;
                
                if (rgb_flash_state) {
                    rgb_set_color(rgb_current_color);
                } else {
                    rgb_off();
                }
            }
            break;
            
        case RGB_MODE_BREATH:
            if (currentMillis - rgb_last_update >= 40) {
                rgb_last_update = currentMillis;
                rgb_brightness++;
                if (rgb_brightness >= 255) {
                    rgb_brightness = 0;
                }
                // 使用正弦波实现平滑呼吸效果，范围10-240避免完全熄灭和过亮
                float breath_f = 10.0 + 115.0 * (1.0 + sin(rgb_brightness * 2 * PI / 255.0));
                uint8_t breath = (uint8_t)breath_f;
                
                // 提取RGB分量并应用呼吸亮度（使用浮点数计算提高精度）
                uint8_t r = (rgb_current_color >> 16) & 0xFF;
                uint8_t g = (rgb_current_color >> 8) & 0xFF;
                uint8_t b = rgb_current_color & 0xFF;
                
                r = (uint8_t)(r * breath_f / 255.0);
                g = (uint8_t)(g * breath_f / 255.0);
                b = (uint8_t)(b * breath_f / 255.0);
                
                pixels.setPixelColor(0, pixels.Color(r, g, b));
                pixels.show();
            }
            break;
            
        case RGB_MODE_OFF:
        default:
            rgb_off();
            break;
    }
}

/**
 * @brief       呼吸灯效果（阻塞版本，仅用于测试）
 * @param       color: 颜色值
 * @param       brightness: 亮度（0-255）
 * @retval      无
 * @note        此函数会阻塞程序执行，建议在主循环中使用非阻塞方式
 */
void rgb_breath(uint32_t color, uint8_t brightness)
{
    // 提取 RGB 分量
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    // 使用正弦波实现平滑呼吸效果
    for (uint8_t i = 0; i < 255; i++) {
        float breath_f = 10.0 + 115.0 * (1.0 + sin(i * 2 * PI / 255.0));
        uint8_t breath = (uint8_t)breath_f;
        
        pixels.setPixelColor(0, pixels.Color(
            (uint8_t)(r * breath_f / 255.0),
            (uint8_t)(g * breath_f / 255.0),
            (uint8_t)(b * breath_f / 255.0)
        ));
        pixels.show();
        //delay(10);
    }
}