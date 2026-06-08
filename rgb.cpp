/**
 ****************************************************************************************************
 * @file        rgb.cpp
 * @author      md
 * @version     V2.0
 * @date        2026-06-08
 * @brief       RGB 万色灯驱动实现 - 支持序列动画
 * @license     Copyright (c) 
 ****************************************************************************************************
 */

#include "rgb.h"
#include "mqtt.h"
#include <WiFi.h>

/* 外部变量声明（来自主控文件） */
extern bool lightEnabled;
extern bool fanEnabled;

/* NeoPixel 对象 */
static Adafruit_NeoPixel pixels(NUMPIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

/* RGB 状态变量 */
static uint8_t rgb_current_mode = RGB_MODE_OFF;
static uint32_t rgb_current_color = RGB_OFF;
static unsigned long rgb_last_update = 0;
static bool rgb_flash_state = false;

/* 序列动画状态 */
static uint8_t seq_step_index = 0;       /* 当前步骤索引 */
static unsigned long seq_step_start = 0;  /* 当前步骤开始时间(ms) */
static uint32_t seq_fade_start_color = 0; /* 渐变起始颜色 */

/* ==================== 序列定义 ==================== */

/* 1. 严重报警：紫橙交替闪烁 */
/* PURPLE(200) → ORANGE(200) */
static const rgb_step_t seq_alarm_flash[] = {
    {RGB_PURPLE, 200, RGB_STEP_SOLID},
    {RGB_ORANGE, 200, RGB_STEP_SOLID},
};

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
    
    rgb_off();
}

/**
 * @brief       设置颜色（使用预定义颜色）
 * @param       color: 颜色值（RGB 格式，如 RGB_RED）
 * @retval      无
 */
void rgb_set_color(uint32_t color)
{
    rgb_current_color = color;
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
    rgb_current_color = pixels.Color(r, g, b);
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
}

/**
 * @brief       关闭灯
 * @param       无
 * @retval      无
 */
void rgb_off(void)
{
    rgb_current_color = RGB_OFF;
    pixels.clear();
    pixels.show();
}

/**
 * @brief       设置 RGB 模式
 * @param       mode: 模式（RGB_MODE_OFF/ON/BREATH/FLASH/ALARM_*）
 * @retval      无
 */
void rgb_set_mode(uint8_t mode)
{
    /* 模式没变，不重置序列 */
    if (mode == rgb_current_mode) {
        return;
    }
    
    rgb_current_mode = mode;
    rgb_last_update = millis();
    seq_step_index = 0;
    seq_step_start = millis();
    seq_fade_start_color = RGB_OFF;
    
    if (mode == RGB_MODE_OFF) {
        rgb_off();
    } else if (mode == RGB_MODE_ON) {
        /* 保持当前颜色 */
    }
}

/**
 * @brief       对颜色分量进行线性插值
 * @param       c1: 起始颜色
 * @param       c2: 目标颜色
 * @param       t:  插值系数 (0.0 ~ 1.0)
 * @retval      插值后的颜色
 */
static uint32_t color_lerp(uint32_t c1, uint32_t c2, float t)
{
    uint8_t r1 = (c1 >> 16) & 0xFF;
    uint8_t g1 = (c1 >> 8) & 0xFF;
    uint8_t b1 = c1 & 0xFF;
    uint8_t r2 = (c2 >> 16) & 0xFF;
    uint8_t g2 = (c2 >> 8) & 0xFF;
    uint8_t b2 = c2 & 0xFF;
    
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
    
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/**
 * @brief       执行序列动画的一帧（非阻塞）
 * @param       currentMillis: 当前时间戳
 * @param       seq: 序列步骤数组
 * @param       step_count: 步骤数量
 * @retval      无
 */
static void run_sequence(unsigned long currentMillis, const rgb_step_t* seq, uint8_t step_count)
{
    if (step_count == 0) return;
    
    const rgb_step_t* step = &seq[seq_step_index];
    unsigned long elapsed = currentMillis - seq_step_start;
    
    if (elapsed >= step->duration_ms) {
        /* 当前步骤完成，进入下一步 */
        seq_step_index++;
        if (seq_step_index >= step_count) {
            seq_step_index = 0;  /* 循环播放 */
        }
        seq_step_start = currentMillis;
        seq_fade_start_color = rgb_current_color;  /* 记录渐变起点 */
        elapsed = 0;
        step = &seq[seq_step_index];
    }
    
    if (step->type == RGB_STEP_SOLID) {
        /* 直接设置颜色 */
        rgb_set_color(step->color);
    } else {
        /* RGB_STEP_FADE: 线性渐变 */
        float progress = (float)elapsed / step->duration_ms;
        if (progress > 1.0f) progress = 1.0f;
        uint32_t color = color_lerp(seq_fade_start_color, step->color, progress);
        rgb_set_color(color);
    }
}

/**
 * @brief       启动多色快闪（红→绿→蓝→白，各100ms，阻塞）
 * @param       无
 * @retval      无
 * @note        在 setup() 结尾调用，完成后灯灭，由 loop 接管
 */
void rgb_boot_flash(void)
{
    const uint32_t colors[] = {RGB_RED, RGB_YELLOW, RGB_GREEN, RGB_CYAN, RGB_BLUE, RGB_PURPLE,RGB_RED, RGB_YELLOW, RGB_GREEN, RGB_CYAN, RGB_BLUE, RGB_PURPLE, RGB_WHITE};
    for (uint8_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        rgb_set_color(colors[i]);
        delay(100);
    }
    rgb_off();
}

/**
 * @brief       根据设备状态自动选择模式并更新 RGB
 * @param       无
 * @retval      无
 * @note        在 loop() 中调用，优先级从高到低：
 *              1. 严重报警（LED灯+风扇同时启动）→ 紫橙闪烁
 *              2. 光照越界报警（LED灯亮）        → 橙色呼吸
 *              3. 温度越界报警（风扇启动）        → 紫色呼吸
 *              4. WiFi未连接                     → 红色呼吸
 *              5. MQTT未连接（WiFi已连接）        → 蓝色呼吸
 *              6. 所有状态正常                   → 绿色呼吸
 */
void rgb_update_by_state(wl_status_t wifi_status)
{
    if (lightEnabled && fanEnabled) {
        /* 严重报警：紫橙闪烁 */
        if (rgb_current_mode != RGB_MODE_ALARM_LED_FAN) {
            rgb_set_mode(RGB_MODE_ALARM_LED_FAN);
        }
        run_sequence(millis(), seq_alarm_flash, sizeof(seq_alarm_flash) / sizeof(rgb_step_t));
    } else if (lightEnabled) {
        /* 光照越界：橙色呼吸 */
        if (rgb_current_mode != RGB_MODE_BREATH || rgb_current_color != RGB_ORANGE) {
            rgb_set_mode(RGB_MODE_BREATH);
            rgb_set_color(RGB_ORANGE);
        }
        rgb_update();
    } else if (fanEnabled) {
        /* 温度越界：紫色呼吸 */
        if (rgb_current_mode != RGB_MODE_BREATH || rgb_current_color != RGB_PURPLE) {
            rgb_set_mode(RGB_MODE_BREATH);
            rgb_set_color(RGB_PURPLE);
        }
        rgb_update();
    } else if (wifi_status != WL_CONNECTED) {
        /* WiFi断开：红色呼吸 */
        if (rgb_current_mode != RGB_MODE_BREATH || rgb_current_color != RGB_RED) {
            rgb_set_mode(RGB_MODE_BREATH);
            rgb_set_color(RGB_RED);
        }
        rgb_update();
    } else if (!mqtt_is_connected()) {
        /* MQTT断开：蓝色呼吸 */
        if (rgb_current_mode != RGB_MODE_BREATH || rgb_current_color != RGB_BLUE) {
            rgb_set_mode(RGB_MODE_BREATH);
            rgb_set_color(RGB_BLUE);
        }
        rgb_update();
    } else {
        /* 一切正常：绿色呼吸 */
        if (rgb_current_mode != RGB_MODE_BREATH || rgb_current_color != RGB_GREEN) {
            rgb_set_mode(RGB_MODE_BREATH);
            rgb_set_color(RGB_GREEN);
        }
        rgb_update();
    }
}

/**
 * @brief       更新 RGB 状态（非阻塞）
 * @param       无
 * @retval      无
 * @note        在 loop() 中调用，实现非阻塞控制
 *              支持 OFF / ON / BREATH / FLASH 模式
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
            {
                /* 纯时间驱动呼吸：不依赖累加器，每帧用 millis() 算亮度 */
                float t = (float)(currentMillis % 3000) / 3000.0;  /* 周期 3 秒 */
                float brightness = 5.0 + 250.0 * (1.0 + sin(t * 2 * PI)) / 2.0;
                uint8_t r = (rgb_current_color >> 16) & 0xFF;
                uint8_t g = (rgb_current_color >> 8) & 0xFF;
                uint8_t b = rgb_current_color & 0xFF;
                r = (uint8_t)((uint16_t)r * (uint16_t)brightness / 255);
                g = (uint8_t)((uint16_t)g * (uint16_t)brightness / 255);
                b = (uint8_t)((uint16_t)b * (uint16_t)brightness / 255);
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
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    for (uint8_t i = 0; i < 255; i++) {
        float breath_f = 10.0 + 115.0 * (1.0 + sin(i * 2 * PI / 255.0));
        
        pixels.setPixelColor(0, pixels.Color(
            (uint8_t)(r * breath_f / 255.0),
            (uint8_t)(g * breath_f / 255.0),
            (uint8_t)(b * breath_f / 255.0)
        ));
        pixels.show();
    }
}
