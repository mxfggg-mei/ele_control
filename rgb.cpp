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
#include "key.h"
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

/* 当前激活的序列指针（用于检测序列变化时自动重置状态） */
static const rgb_step_t* active_seq = NULL;

/* ==================== 序列定义 ==================== */

/* ==================== 严重报警（紫呼吸+橙呼吸循环） ==================== */

/* 无通讯指示 */
static const rgb_step_t seq_critical_alarm[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_PURPLE, 1400, RGB_STEP_FADE},   /* 渐紫 1.4s */
    {RGB_OFF, 1400, RGB_STEP_FADE},      /* 渐暗 1.4s */
    {RGB_ORANGE, 1400, RGB_STEP_FADE},   /* 渐橙 1.4s */
    {RGB_OFF, 1400, RGB_STEP_FADE},      /* 渐暗 1.4s */
};

/* WiFi+MQTT正常 → 加绿灯闪烁 */
static const rgb_step_t seq_critical_comm_ok[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_PURPLE, 1200, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_ORANGE, 1200, RGB_STEP_FADE},
    {RGB_OFF, 1000, RGB_STEP_FADE},
    {RGB_GREEN, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* WiFi正常MQTT异常 → 加蓝灯闪烁 */
static const rgb_step_t seq_critical_comm_mqtt_down[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_PURPLE, 1200, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_ORANGE, 1200, RGB_STEP_FADE},
    {RGB_OFF, 1000, RGB_STEP_FADE},
    {RGB_BLUE, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* WiFi异常MQTT异常 → 加红灯闪烁 */
static const rgb_step_t seq_critical_comm_both_down[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_PURPLE, 1200, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_ORANGE, 1200, RGB_STEP_FADE},
    {RGB_OFF, 1000, RGB_STEP_FADE},
    {RGB_RED, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* ==================== 光照越界（橙色呼吸） ==================== */

/* 无通讯指示 */
static const rgb_step_t seq_light_alarm[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_ORANGE, 1400, RGB_STEP_FADE},
    {RGB_OFF, 1400, RGB_STEP_FADE},
};

/* WiFi+MQTT正常 → 加绿灯闪烁 */
static const rgb_step_t seq_light_comm_ok[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_ORANGE, 1400, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_GREEN, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* WiFi正常MQTT异常 → 加蓝灯闪烁 */
static const rgb_step_t seq_light_comm_mqtt_down[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_ORANGE, 1400, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_BLUE, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* WiFi异常MQTT异常 → 加红灯闪烁 */
static const rgb_step_t seq_light_comm_both_down[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_ORANGE, 1400, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_RED, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* ==================== 温度越界（紫色呼吸） ==================== */

/* 无通讯指示 */
static const rgb_step_t seq_temp_alarm[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_PURPLE, 1400, RGB_STEP_FADE},
    {RGB_OFF, 1400, RGB_STEP_FADE},
};

/* WiFi+MQTT正常 → 加绿灯闪烁 */
static const rgb_step_t seq_temp_comm_ok[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_PURPLE, 1400, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_GREEN, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* WiFi正常MQTT异常 → 加蓝灯闪烁 */
static const rgb_step_t seq_temp_comm_mqtt_down[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_PURPLE, 1400, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_BLUE, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* WiFi异常MQTT异常 → 加红灯闪烁 */
static const rgb_step_t seq_temp_comm_both_down[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_PURPLE, 1400, RGB_STEP_FADE},
    {RGB_OFF, 1200, RGB_STEP_FADE},
    {RGB_RED, 100, RGB_STEP_SOLID},
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* ==================== 总闸断开（呼吸+白灯闪三下） ==================== */

/* 7. WiFi未连 + 总闸断开：红色呼吸 + 白灯闪三下 */
static const rgb_step_t seq_sw_off_wifi_down[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_RED, 1200, RGB_STEP_FADE},       /* 渐红 1.2s */
    {RGB_OFF, 1000, RGB_STEP_FADE},       /* 渐暗 1.0s */
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第1下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第2下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第3下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* 8. WiFi连MQTT未连 + 总闸断开：蓝色呼吸 + 白灯闪三下 */
static const rgb_step_t seq_sw_off_mqtt_down[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_BLUE, 1200, RGB_STEP_FADE},      /* 渐蓝 1.2s */
    {RGB_OFF, 1000, RGB_STEP_FADE},       /* 渐暗 1.0s */
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第1下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第2下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第3下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
};

/* 9. 总闸断开 + 一切正常：绿色呼吸 + 白灯闪三下 */
static const rgb_step_t seq_sw_off_all_ok[] = {
    {RGB_OFF, 1, RGB_STEP_SOLID},
    {RGB_GREEN, 1200, RGB_STEP_FADE},     /* 渐绿 1.2s */
    {RGB_OFF, 1000, RGB_STEP_FADE},       /* 渐暗 1.0s */
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第1下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第2下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
    {RGB_WHITE_HALF, 100, RGB_STEP_SOLID},     /* 白灯(半亮) 闪第3下 */
    {RGB_OFF, 100, RGB_STEP_SOLID},
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
 * @param       wifi_status: 当前 WiFi 状态（由调用者传入缓存值）
 * @retval      无
 * @note        优先级从高到低：
 *              1. 严重报警（LED+风扇都开）       → 紫呼吸+橙呼吸循环
 *                 - WiFi+MQTT正常  → 加绿灯闪烁
 *                 - WiFi正常MQTT异常 → 加蓝灯闪烁
 *                 - WiFi+MQTT异常  → 加红灯闪烁
 *              2. 光照越界（LED开、风扇未开）    → 橙色呼吸
 *                 - WiFi+MQTT正常  → 加绿灯闪烁
 *                 - WiFi正常MQTT异常 → 加蓝灯闪烁
 *                 - WiFi+MQTT异常  → 加红灯闪烁
 *              3. 温度越界（风扇开、LED未亮）    → 紫色呼吸
 *                 - WiFi+MQTT正常  → 加绿灯闪烁
 *                 - WiFi正常MQTT异常 → 加蓝灯闪烁
 *                 - WiFi+MQTT异常  → 加红灯闪烁
 *              4. WiFi未连接（无报警）           → 红色呼吸
 *              5. WiFi连但MQTT未连（无报警）     → 蓝色呼吸
 *              6. 一切正常                       → 绿色呼吸
 */
void rgb_update_by_state(wl_status_t wifi_status)
{
    unsigned long now = millis();
    bool wifiOk = (wifi_status == WL_CONNECTED);
    bool mqttOk = mqtt_is_connected();

    /* ---- 辅助宏：运行序列，自动检测序列指针变化并重置状态 ---- */
#define RUN_SEQ(seq) do { \
    const rgb_step_t* _s = (seq); \
    uint8_t _n = sizeof(seq) / sizeof(rgb_step_t); \
    if (_s != active_seq) { \
        seq_step_index = 0; \
        seq_step_start = now; \
        seq_fade_start_color = RGB_OFF; \
        active_seq = _s; \
    } \
    run_sequence(now, _s, _n); \
} while(0)

    /* ==================== 报警状态（序列驱动） ==================== */
    if (lightEnabled && fanEnabled) {
        /* 1. 严重报警：紫呼吸+橙呼吸循环 + 通讯指示 */
        if (rgb_current_mode != RGB_MODE_ALARM_LED_FAN) {
            rgb_set_mode(RGB_MODE_ALARM_LED_FAN);
            active_seq = NULL;
        }
        if (wifiOk && mqttOk) {
            RUN_SEQ(seq_critical_comm_ok);
        } else if (wifiOk && !mqttOk) {
            RUN_SEQ(seq_critical_comm_mqtt_down);
        } else if (!wifiOk && !mqttOk) {
            RUN_SEQ(seq_critical_comm_both_down);
        } else {
            RUN_SEQ(seq_critical_alarm);
        }
        return;
    }

    if (lightEnabled) {
        /* 2. 光照越界：橙色呼吸 + 通讯指示 */
        if (rgb_current_mode != RGB_MODE_ALARM_LIGHT) {
            rgb_set_mode(RGB_MODE_ALARM_LIGHT);
            active_seq = NULL;
        }
        if (wifiOk && mqttOk) {
            RUN_SEQ(seq_light_comm_ok);
        } else if (wifiOk && !mqttOk) {
            RUN_SEQ(seq_light_comm_mqtt_down);
        } else if (!wifiOk && !mqttOk) {
            RUN_SEQ(seq_light_comm_both_down);
        } else {
            RUN_SEQ(seq_light_alarm);
        }
        return;
    }

    if (fanEnabled) {
        /* 3. 温度越界：紫色呼吸 + 通讯指示 */
        if (rgb_current_mode != RGB_MODE_ALARM_TEMP) {
            rgb_set_mode(RGB_MODE_ALARM_TEMP);
            active_seq = NULL;
        }
        if (wifiOk && mqttOk) {
            RUN_SEQ(seq_temp_comm_ok);
        } else if (wifiOk && !mqttOk) {
            RUN_SEQ(seq_temp_comm_mqtt_down);
        } else if (!wifiOk && !mqttOk) {
            RUN_SEQ(seq_temp_comm_both_down);
        } else {
            RUN_SEQ(seq_temp_alarm);
        }
        return;
    }

    /* ==================== 非报警状态（呼吸灯驱动） ==================== */
    bool key1On = key1_is_on();

    if (!key1On) {
        /* 总闸断开：使用呼吸+白灯闪三下序列 */
        if (!wifiOk) {
            /* 7. WiFi未连 + 总闸断开：红色呼吸 + 白灯闪三下 */
            if (rgb_current_mode != RGB_MODE_BREATH_SW_OFF) {
                rgb_set_mode(RGB_MODE_BREATH_SW_OFF);
                active_seq = NULL;// 重置序列指针，确保下一次序列播放
            }
            RUN_SEQ(seq_sw_off_wifi_down);
        } else if (!mqttOk) {
            /* 8. WiFi连MQTT未连 + 总闸断开：蓝色呼吸 + 白灯闪三下 */
            if (rgb_current_mode != RGB_MODE_BREATH_SW_OFF) {
                rgb_set_mode(RGB_MODE_BREATH_SW_OFF);//
                active_seq = NULL;
            }
            RUN_SEQ(seq_sw_off_mqtt_down);
        } else {
            /* 9. 总闸断开 + 一切正常：绿色呼吸 + 白灯闪三下 */
            if (rgb_current_mode != RGB_MODE_BREATH_SW_OFF) {
                rgb_set_mode(RGB_MODE_BREATH_SW_OFF);
                active_seq = NULL;
            }
            RUN_SEQ(seq_sw_off_all_ok);
        }
    } else {
        /* 总闸闭合：原呼吸灯逻辑 */
        if (!wifiOk) {
            /* 4. WiFi断开（无报警）：红色呼吸 */
            if (rgb_current_mode != RGB_MODE_BREATH || rgb_current_color != RGB_RED) {
                rgb_set_mode(RGB_MODE_BREATH);
                rgb_set_color(RGB_RED);
            }
            rgb_update();
        } else if (!mqttOk) {
            /* 5. WiFi连但MQTT未连（无报警）：蓝色呼吸 */
            if (rgb_current_mode != RGB_MODE_BREATH || rgb_current_color != RGB_BLUE) {
                rgb_set_mode(RGB_MODE_BREATH);
                rgb_set_color(RGB_BLUE);
            }
            rgb_update();
        } else {
            /* 6. 一切正常：绿色呼吸 */
            if (rgb_current_mode != RGB_MODE_BREATH || rgb_current_color != RGB_GREEN) {
                rgb_set_mode(RGB_MODE_BREATH);
                rgb_set_color(RGB_GREEN);
            }
            rgb_update();
        }
    }

#undef RUN_SEQ
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

/**
 * @brief       后台 RGB 灯控任务
 * @param       pvParameters: 未使用
 * @retval      无
 * @note        每 ~30ms 更新一次 RGB 状态，实现平滑呼吸/闪烁效果。
 *              读取外部全局变量（lightEnabled、fanEnabled）和 WiFi/MQTT 状态，
 *              自动选择合适的灯效。
 */
static void rgb_task(void *pvParameters)
{
    while (1) {
        wl_status_t wifiStatus = WiFi.status();
        rgb_update_by_state(wifiStatus);
        vTaskDelay(pdMS_TO_TICKS(30));  /* ~33fps，呼吸足够平滑 */
    }
}

/**
 * @brief       创建 RGB 灯控后台任务
 * @param       无
 * @retval      无
 * @note        在 setup() 中调用一次
 */
void rgb_task_create(void)
{
    xTaskCreate(
        rgb_task,           /* 任务函数 */
        "rgbTask",          /* 任务名称 */
        4096,               /* 栈大小（字节） */
        NULL,               /* 参数 */
        2,                  /* 优先级（略高于温度任务，保证呼吸平滑） */
        NULL                /* 任务句柄（不需要） */
    );
    Serial.println("[RGB] 后台呼吸灯任务已创建");
}
