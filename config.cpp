#include "config.h"
#include "version.h"
#include "mqtt.h"
#include <Arduino.h>

/* Preferences 命名空间 */
#define PREF_NAMESPACE_WIFI  "wifi_config"
#define PREF_NAMESPACE_MQTT "mqtt_config"
#define PREF_NAMESPACE_WORK "work_param"

/* WiFi 键名 */
#define PREF_KEY_WIFI_SSID   "ssid"
#define PREF_KEY_WIFI_PASS   "password"

/* MQTT 键名 */
#define PREF_KEY_SERVER    "server"
#define PREF_KEY_PORT      "port"
#define PREF_KEY_USERNAME  "username"
#define PREF_KEY_PASSWORD  "password"
#define PREF_KEY_DEVICE_ID "device_id"

/* 默认 WiFi 配置 */
#define DEFAULT_WIFI_SSID     ""
#define DEFAULT_WIFI_PASSWORD ""

/* 默认 MQTT 配置 */
#define DEFAULT_SERVER     MQTT_SERVER
#define DEFAULT_PORT       MQTT_PORT
#define DEFAULT_USERNAME   MQTT_USERNAME
#define DEFAULT_PASSWORD   MQTT_PASSWORD
#define DEFAULT_DEVICE_ID  DEVICE_ID

/* 全局配置实例 */
static AppWifiConfig g_wifi_config;
static mqtt_config_t g_mqtt_config;
static Preferences prefs;

/* ==================== WiFi 配置函数 ==================== */

/**
 * @brief       加载 WiFi 配置
 * @param       config: WiFi 配置结构体指针
 * @retval      无
 */
void wifi_config_load(AppWifiConfig* config) {
    if (!prefs.begin(PREF_NAMESPACE_WIFI, true)) {
        Serial.println("[WiFi Config] 无法打开Preferences，使用默认配置");
        wifi_config_set_default(config);
        return;
    }
    
    size_t len;
    len = prefs.getString(PREF_KEY_WIFI_SSID, config->ssid, sizeof(config->ssid));
    if (len == 0) {
        strncpy(config->ssid, DEFAULT_WIFI_SSID, sizeof(config->ssid));
    }
    
    len = prefs.getString(PREF_KEY_WIFI_PASS, config->password, sizeof(config->password));
    if (len == 0) {
        strncpy(config->password, DEFAULT_WIFI_PASSWORD, sizeof(config->password));
    }
    
    prefs.end();
}

/**
 * @brief       保存 WiFi 配置到 Flash
 * @param       config: WiFi 配置结构体指针
 * @retval      无
 */
void wifi_config_save(const AppWifiConfig* config) {
    if (!prefs.begin(PREF_NAMESPACE_WIFI, false)) {
        Serial.println("[WiFi Config] 无法打开Preferences，保存失败");
        return;
    }
    
    prefs.putString(PREF_KEY_WIFI_SSID, config->ssid);
    prefs.putString(PREF_KEY_WIFI_PASS, config->password);
    prefs.end();
    Serial.println("[WiFi Config] WiFi 配置已保存");
}

/**
 * @brief       清除 Flash 中保存的 WiFi 配置
 * @param       无
 * @retval      无
 */
void wifi_config_erase(void) {
    if (!prefs.begin(PREF_NAMESPACE_WIFI, false)) {
        Serial.println("[WiFi Config] 无法打开Preferences，清除失败");
        return;
    }
    prefs.clear();
    prefs.end();
    Serial.println("[WiFi Config] 已清除Flash中的WiFi配置");
}

/**
 * @brief       设置 WiFi 默认配置
 * @param       config: WiFi 配置结构体指针
 * @retval      无
 */
void wifi_config_set_default(AppWifiConfig* config) {
    config->ssid[0] = '\0';
    config->password[0] = '\0';
}

/**
 * @brief       打印 WiFi 配置
 * @param       config: WiFi 配置结构体指针
 * @retval      无
 */
void wifi_config_print(const AppWifiConfig* config) {
    Serial.println("[WiFi Config] 当前配置:");
    Serial.print("  SSID: ");
    Serial.println(config->ssid[0] ? config->ssid : "(未设置)");
    Serial.print("  密码: ");
    Serial.println(config->password[0] ? "******" : "(未设置)");
}

/**
 * @brief       检查 WiFi 配置是否有效
 * @param       config: WiFi 配置结构体指针
 * @retval      true: 有效，false: 无效
 */
bool wifi_config_is_valid(const AppWifiConfig* config) {
    return (config->ssid[0] != '\0');
}

/* ==================== MQTT 配置函数 ==================== */

/**
 * @brief       初始化配置管理
 * @param       无
 * @retval      无
 */
void config_init(void) {
    // 各配置加载函数自行管理 prefs.begin()/end()
    config_load(&g_mqtt_config);
    wifi_config_load(&g_wifi_config);
    Serial.println("[Config] 配置加载完成");
}

/**
 * @brief       加载配置
 * @param       config: 配置结构体指针
 * @retval      无
 */
void config_load(mqtt_config_t* config) {
    if (!prefs.begin(PREF_NAMESPACE_MQTT, true)) {
        Serial.println("[Config] 无法打开Preferences，使用默认配置");
        config_set_default(config);
        return;
    }
    
    size_t len;
    
    len = prefs.getString(PREF_KEY_SERVER, config->server, sizeof(config->server));
    if (len == 0) {
        Serial.println("[Config] 未找到服务器配置，使用默认值");
        strncpy(config->server, DEFAULT_SERVER, sizeof(config->server));
    }
    
    config->port = prefs.getUShort(PREF_KEY_PORT, DEFAULT_PORT);
    
    len = prefs.getString(PREF_KEY_USERNAME, config->username, sizeof(config->username));
    if (len == 0) {
        strncpy(config->username, DEFAULT_USERNAME, sizeof(config->username));
    }
    
    len = prefs.getString(PREF_KEY_PASSWORD, config->password, sizeof(config->password));
    if (len == 0) {
        strncpy(config->password, DEFAULT_PASSWORD, sizeof(config->password));
    }
    
    len = prefs.getString(PREF_KEY_DEVICE_ID, config->deviceId, sizeof(config->deviceId));
    if (len == 0) {
        strncpy(config->deviceId, DEFAULT_DEVICE_ID, sizeof(config->deviceId));
    }
    
    prefs.end();
}

/**
 * @brief       保存配置
 * @param       config: 配置结构体指针
 * @retval      无
 */
void config_save(const mqtt_config_t* config) {
    if (!prefs.begin(PREF_NAMESPACE_MQTT, false)) {
        Serial.println("[Config] 无法打开Preferences，保存失败");
        return;
    }
    
    prefs.putString(PREF_KEY_SERVER, config->server);
    prefs.putUShort(PREF_KEY_PORT, config->port);
    prefs.putString(PREF_KEY_USERNAME, config->username);
    prefs.putString(PREF_KEY_PASSWORD, config->password);
    prefs.putString(PREF_KEY_DEVICE_ID, config->deviceId);
    
    prefs.end();
    Serial.println("[Config] MQTT 配置已保存");
}

/**
 * @brief       清除 Flash 中保存的 MQTT 配置
 * @param       无
 * @retval      无
 */
void config_erase(void) {
    if (!prefs.begin(PREF_NAMESPACE_MQTT, false)) {
        Serial.println("[Config] 无法打开Preferences，清除失败");
        return;
    }
    prefs.clear();
    prefs.end();
    Serial.println("[Config] 已清除Flash中的MQTT配置");
}

/**
 * @brief       设置默认配置
 * @param       config: 配置结构体指针
 * @retval      无
 */
void config_set_default(mqtt_config_t* config) {
    strncpy(config->server, DEFAULT_SERVER, sizeof(config->server));
    config->port = DEFAULT_PORT;
    strncpy(config->username, DEFAULT_USERNAME, sizeof(config->username));
    strncpy(config->password, DEFAULT_PASSWORD, sizeof(config->password));
    strncpy(config->deviceId, DEFAULT_DEVICE_ID, sizeof(config->deviceId));
}

/* ==================== 工作参数持久化 ==================== */

/**
 * @brief       保存工作参数到 Flash
 * @param       无
 * @retval      无
 */
void param_save(void) {
    if (!prefs.begin(PREF_NAMESPACE_WORK, false)) {
        Serial.println("[Param] 无法打开Preferences，保存失败");
        return;
    }
    prefs.putUChar("autoMode", g_autoMode ? 1 : 0);
    prefs.putUChar("relaySt", g_relayState);
    prefs.putFloat("tempThr", tempThreshold);
    prefs.putFloat("lightThr", lightThreshold);
    prefs.end();
}

/**
 * @brief       从 Flash 加载工作参数
 * @param       无
 * @retval      无
 */
void param_load(void) {
    if (!prefs.begin(PREF_NAMESPACE_WORK, true)) {
        return;  /* 首次启动，无保存数据 */
    }
    g_autoMode   = prefs.getUChar("autoMode", 1) != 0;
    g_relayState = prefs.getUChar("relaySt", 0);
    tempThreshold = prefs.getFloat("tempThr", 28.0f);
    lightThreshold = prefs.getFloat("lightThr", 300.0f);
    prefs.end();
    
    Serial.print("[Param] 已恢复: 模式=");
    Serial.print(g_autoMode ? "自动" : "手动");
    Serial.print(" 状态=");
    Serial.print(g_relayState);
    Serial.print(" 温度阈值=");
    Serial.print(tempThreshold, 1);
    Serial.print(" 光照阈值=");
    Serial.println(lightThreshold, 1);
}

/**
 * @brief       清除 Flash 中的工作参数
 * @param       无
 * @retval      无
 */
void param_erase(void) {
    if (!prefs.begin(PREF_NAMESPACE_WORK, false)) {
        return;
    }
    prefs.clear();
    prefs.end();
    Serial.println("[Param] 已清除Flash中的工作参数");
}

/**
 * @brief       打印配置
 * @param       config: 配置结构体指针
 * @retval      无
 */
void config_print(const mqtt_config_t* config) {
    Serial.println("[Config] 当前MQTT配置:");
    Serial.print("  服务器: ");
    Serial.println(config->server);
    Serial.print("  端口: ");
    Serial.println(config->port);
    Serial.print("  用户名: ");
    Serial.println(config->username);
    Serial.print("  密码: ");
    //Serial.println(config->password);
    Serial.print("  设备ID: ");
    Serial.println(config->deviceId);
}

/**
 * @brief       检查 MQTT 配置是否有效
 * @param       config: 配置结构体指针
 * @retval      true: 有效，false: 无效
 */
bool config_is_valid(const mqtt_config_t* config) {
    return (config->server[0] != '\0' && config->port > 0);
}

/**
 * @brief       获取全局 WiFi 配置
 * @param       无
 * @retval      WiFi 配置指针
 */
AppWifiConfig* get_wifi_config(void) {
    return &g_wifi_config;
}

/**
 * @brief       获取全局 MQTT 配置
 * @param       无
 * @retval      MQTT 配置指针
 */
mqtt_config_t* get_mqtt_config(void) {
    return &g_mqtt_config;
}