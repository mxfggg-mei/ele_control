#include "config.h"
#include "version.h"
#include "mqtt.h"
#include <Arduino.h>

/* Preferences 命名空间 */
#define PREF_NAMESPACE "mqtt_config"

/* 键名 */
#define PREF_KEY_SERVER    "server"
#define PREF_KEY_PORT      "port"
#define PREF_KEY_USERNAME  "username"
#define PREF_KEY_PASSWORD  "password"
#define PREF_KEY_DEVICE_ID "device_id"

/* 默认配置 */
#define DEFAULT_SERVER     MQTT_SERVER
#define DEFAULT_PORT       MQTT_PORT
#define DEFAULT_USERNAME   MQTT_USERNAME
#define DEFAULT_PASSWORD   MQTT_PASSWORD
#define DEFAULT_DEVICE_ID  DEVICE_ID

/* 全局配置实例 */
static mqtt_config_t g_config;
static Preferences prefs;

/**
 * @brief       初始化配置管理
 * @param       无
 * @retval      无
 */
void config_init(void) {
    prefs.begin(PREF_NAMESPACE, false);
    config_load(&g_config);
    prefs.end();
}

/**
 * @brief       加载配置
 * @param       config: 配置结构体指针
 * @retval      无
 */
void config_load(mqtt_config_t* config) {
    if (!prefs.begin(PREF_NAMESPACE, true)) {
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
    Serial.println("[Config] 配置加载完成");
    config_print(config);
}

/**
 * @brief       保存配置
 * @param       config: 配置结构体指针
 * @retval      无
 */
void config_save(const mqtt_config_t* config) {
    if (!prefs.begin(PREF_NAMESPACE, false)) {
        Serial.println("[Config] 无法打开Preferences，保存失败");
        return;
    }
    
    prefs.putString(PREF_KEY_SERVER, config->server);
    prefs.putUShort(PREF_KEY_PORT, config->port);
    prefs.putString(PREF_KEY_USERNAME, config->username);
    prefs.putString(PREF_KEY_PASSWORD, config->password);
    prefs.putString(PREF_KEY_DEVICE_ID, config->deviceId);
    
    prefs.end();
    Serial.println("[Config] 配置保存完成");
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
    Serial.println("[Config] 已设置为默认配置");
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
    Serial.println(config->password);
    Serial.print("  设备ID: ");
    Serial.println(config->deviceId);
}