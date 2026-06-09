#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "mqtt.h"
#include "version.h"
#include "key.h"
#include "config.h"
#include "temp.h"

/* MQTT 客户端实例 */
WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

/* Topic 缓冲区 */
static char mqttTopicStatus[64];
static char mqttTopicCommand[64];

/* 当前使用的配置 */
static mqtt_config_t currentMqttConfig;
/* 调试开关 */
bool mqtt_debug_enabled = false;  /* 默认关闭 */

/* 连接状态 */
static bool mqttConnected = false;
static bool mqttNeedPublish = false;  /* 需要立即上报状态标志（按键变化时设置） */
static bool mqttRecentlyPublished = false;  /* 最近主动上报标志（避免重复应答get_status） */
static unsigned long mqttLastPublishTime = 0;  /* 上次上报时间 */

/**
 * @brief       MQTT 连接回调函数
 * @param       无
 * @retval      无
 */
void mqtt_on_connect(bool sessionPresent) {
    mqttConnected = true;
    if (mqtt_debug_enabled) Serial.println("[MQTT] 连接成功");
    
    // 订阅命令 Topic
    if (mqttClient.subscribe(mqttTopicCommand)) {
        if (mqtt_debug_enabled) {
            Serial.print("[MQTT] 已订阅: ");
            Serial.println(mqttTopicCommand);
        }
    } else {
        if (mqtt_debug_enabled) Serial.println("[MQTT] 订阅失败");
    }
}

/**
 * @brief       MQTT 断开连接回调函数
 * @param       无
 * @retval      无
 */
void mqtt_on_disconnect(WiFiClient *client, char *topic, byte *payload, unsigned int length) {
    mqttConnected = false;
    if (mqtt_debug_enabled) Serial.println("[MQTT] 连接断开");
}

/**
 * @brief       MQTT 消息回调函数
 * @param       topic: 消息主题
 * @param       payload: 消息内容
 * @param       length: 消息长度
 * @retval      无
 */
void mqtt_on_message(char* topic, byte* payload, unsigned int length) {
    if (mqtt_debug_enabled) {
        Serial.print("[MQTT] 收到消息: ");
        Serial.print(topic);
        Serial.print(" -> ");
        
        // 打印消息内容
        for (unsigned int i = 0; i < length; i++) {
            Serial.print((char)payload[i]);
        }
        Serial.println();
    }
    
    // 解析 JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        if (mqtt_debug_enabled) {
            Serial.print("[MQTT] JSON 解析失败: ");
            Serial.println(error.f_str());
        }
        return;
    }
    
    // 获取命令
    const char* cmd = doc["cmd"];
    if (!cmd) {
        if (mqtt_debug_enabled) Serial.println("[MQTT] 缺少 cmd 字段");
        return;
    }
    
    // 处理命令
    if (strcmp(cmd, MQTT_CMD_SET_RELAY) == 0) {
        int relay = doc["relay"];
        bool value = doc["value"];
        
        if (mqtt_debug_enabled) {
            Serial.print("[MQTT] 命令: set_relay, relay: ");
            Serial.print(relay);
            Serial.print(", value: ");
            Serial.println(value);
        }
        
        // 受 KEY1 约束
        if (key1_is_on()) {
            if (relay == 3) {
                lightEnabled = value;
                if (mqtt_debug_enabled) {
                    Serial.print("[MQTT] 设置继电器3(LED): ");
                    Serial.println(value ? "ON" : "OFF");
                }
            } else if (relay == 4) {
                fanEnabled = value;
                if (mqtt_debug_enabled) {
                    Serial.print("[MQTT] 设置继电器4(FAN): ");
                    Serial.println(value ? "ON" : "OFF");
                }
            }
            mqtt_publish_status();  // 立即上报新状态
        } else {
            if (mqtt_debug_enabled) Serial.println("[MQTT] 继电器控制无效 - 总闸已关闭");
        }
        
    } else if (strcmp(cmd, MQTT_CMD_SET_MODE) == 0) {
        const char* mode = doc["mode"];
        
        if (mqtt_debug_enabled) {
            Serial.print("[MQTT] 命令: set_mode, mode: ");
            Serial.println(mode);
        }
        
        if (strcmp(mode, "auto") == 0) {
            g_autoMode = true;
            if (mqtt_debug_enabled) Serial.println("[MQTT] 切换到自动模式");
        } else if (strcmp(mode, "manual") == 0) {
            g_autoMode = false;
            if (mqtt_debug_enabled) Serial.println("[MQTT] 切换到手动模式");
        }
        param_save();  // 保存模式
        mqtt_publish_status();  // 立即上报新状态
        
    } else if (strcmp(cmd, MQTT_CMD_GET_STATUS) == 0) {
        if (mqtt_debug_enabled) Serial.println("[MQTT] 命令: get_status");
        // 检查最近是否主动上报过（5秒内）
        unsigned long currentMillis = millis();
        if (mqttRecentlyPublished && (currentMillis - mqttLastPublishTime < 5000)) {
            if (mqtt_debug_enabled) Serial.println("[MQTT] 最近已主动上报，跳过应答");
        } else {
            mqtt_publish_status();  // 立即上报状态
        }
        
    } else if (strcmp(cmd, MQTT_CMD_SET_THRESHOLD) == 0) {
        float temp = doc["temp"];
        float light = doc["light"];
        
        if (mqtt_debug_enabled) {
            Serial.print("[MQTT] 命令: set_threshold, temp: ");
            Serial.print(temp);
            Serial.print(", light: ");
            Serial.println(light);
        }
        
        if (temp > 0) {
            tempThreshold = temp;
        }
        if (light > 0) {
            lightThreshold = light;
        }
        param_save();  // 保存阈值
        mqtt_publish_status();  // 立即上报新状态
        
    } else if (strcmp(cmd, MQTT_CMD_REBOOT) == 0) {
        if (mqtt_debug_enabled) Serial.println("[MQTT] 命令: reboot - 即将重启设备");
        delay(100);
        ESP.restart();
        
    } else {
        if (mqtt_debug_enabled) {
            Serial.print("[MQTT] 未知命令: ");
            Serial.println(cmd);
        }
    }
}

/**
 * @brief       MQTT 初始化
 * @param       无
 * @retval      无
 */
void mqtt_init(void) {
    // 从已加载的全局配置拷贝，避免重复读取 Flash
    memcpy(&currentMqttConfig, get_mqtt_config(), sizeof(mqtt_config_t));
    
    mqttClient.setServer(currentMqttConfig.server, currentMqttConfig.port);
    mqttClient.setCallback(mqtt_on_message);
    
    snprintf(mqttTopicStatus, sizeof(mqttTopicStatus), 
             "%s%s%s", MQTT_TOPIC_STATUS_PREFIX, currentMqttConfig.deviceId, MQTT_TOPIC_STATUS_SUFFIX);
    snprintf(mqttTopicCommand, sizeof(mqttTopicCommand), 
             "%s%s%s", MQTT_TOPIC_COMMAND_PREFIX, currentMqttConfig.deviceId, MQTT_TOPIC_COMMAND_SUFFIX);
    
    Serial.print("[MQTT] 状态 Topic: ");
    Serial.println(mqttTopicStatus);
    if (mqtt_debug_enabled) {
        Serial.print("[MQTT] 命令 Topic: ");
        Serial.println(mqttTopicCommand);
    }
}

/**
 * @brief       MQTT 连接
 * @param       无
 * @retval      无
 */
void mqtt_connect(void) {
    if (mqttClient.connected()) {
        return;
    }
    
    // 检查 MQTT 配置是否有效
    if (currentMqttConfig.server[0] == '\0' || currentMqttConfig.port == 0) {
        if (mqtt_debug_enabled) Serial.println("[MQTT] 未配置，跳过连接（使用 'mqtt set server/port' 配置）");
        return;
    }
    
    if (mqtt_debug_enabled) Serial.print("[MQTT] 正在连接...");
    
    const char* deviceId = currentMqttConfig.deviceId;
    const char* username = currentMqttConfig.username;
    const char* password = currentMqttConfig.password;
    
    if (mqttClient.connect(deviceId, username, password)) {
        mqttConnected = true;
        Serial.println("[MQTT] 连接成功");
        
        // 连接成功自动保存 MQTT 配置到 Flash
        config_save(&currentMqttConfig);
        
        // 订阅命令 Topic
        mqttClient.subscribe(mqttTopicCommand);
        if (mqtt_debug_enabled) {
            Serial.print("[MQTT] 已订阅: ");
            Serial.println(mqttTopicCommand);
        }
    } else {
        Serial.print("[MQTT] 连接失败，错误码: ");
        Serial.println(mqttClient.state());
        mqttConnected = false;
    }
}

/**
 * @brief       MQTT 循环处理
 * @param       无
 * @retval      无
 */
void mqtt_loop(void) {
    static unsigned long lastReconnectAttempt = 0;
    unsigned long currentMillis = millis();
    
    if (!mqttClient.connected()) {
        // 每 5 秒尝试重连
        if (currentMillis - lastReconnectAttempt >= 5000) {
            lastReconnectAttempt = currentMillis;
            mqtt_connect();
        }
    } else {
        // 处理 MQTT 消息（接收命令）
        mqttClient.loop();
        
        // 检查是否需要立即上报（如按键状态变化）
        if (mqttNeedPublish) {
            mqttNeedPublish = false;
            mqtt_publish_status();
        }
        
        // 5秒后重置主动上报标志，允许应答后续的get_status查询
        if (mqttRecentlyPublished && (currentMillis - mqttLastPublishTime >= 5000)) {
            mqttRecentlyPublished = false;
        }
    }
}

/**
 * @brief       检查 MQTT 是否连接
 * @param       无
 * @retval      true: 已连接, false: 未连接
 */
bool mqtt_is_connected(void) {
    return mqttClient.connected();
}

/**
 * @brief       发布设备状态
 * @param       无
 * @retval      无
 */
void mqtt_publish_status(void) {
    if (!mqttClient.connected()) {
        return;
    }
    
    // 构建 JSON 消息
    StaticJsonDocument<256> doc;
    doc["temperature"] = g_temperature;
    doc["light"] = (int)lightValue;
    doc["temp_threshold"] = tempThreshold;
    doc["light_threshold"] = lightThreshold;
    doc["mode"] = g_autoMode ? "auto" : "manual";
    doc["key1_lock"] = key1_is_on();
    // 上报实际输出状态（受总闸约束）
    bool key1On = key1_is_on();
    doc["relay3"] = key1On && lightEnabled;
    doc["relay4"] = key1On && fanEnabled;
    
    // 序列化 JSON
    char payload[256];
    serializeJson(doc, payload, sizeof(payload));
    
    // 发布消息
    if (mqttClient.publish(mqttTopicStatus, payload)) {
        if (mqtt_debug_enabled) {
            Serial.print("[MQTT] 发布状态: ");
            Serial.println(payload);
        }
        // 记录上报时间和标志
        mqttLastPublishTime = millis();
        mqttRecentlyPublished = true;
    } else {
        if (mqtt_debug_enabled) Serial.println("[MQTT] 发布失败");
    }
}
/**
 * @brief       设置阈值
 * @param       temp: 温度阈值
 * @param       light: 光照阈值
 * @retval      无
 */
void mqtt_set_threshold(float temp, float light) {
    if (temp > 0) {
        tempThreshold = temp;
    }
    if (light > 0) {
        lightThreshold = light;
    }
    param_save();  // 保存阈值
}

/**
 * @brief       请求立即发布设备状态（按键状态变化时调用）
 * @param       无
 * @retval      无
 */
void mqtt_request_publish(void) {
    mqttNeedPublish = true;
}

/**
 * @brief       使用新配置重新连接MQTT
 * @param       config: 新的MQTT配置
 * @retval      无
 */
void mqtt_reconnect_with_config(const mqtt_config_t* config) {
    if (config == NULL) {
        if (mqtt_debug_enabled) Serial.println("[MQTT] 错误: 配置指针为空");
        return;
    }
    
    if (mqtt_debug_enabled) Serial.println("[MQTT] 正在使用新配置重新连接...");
    
    memcpy(&currentMqttConfig, config, sizeof(mqtt_config_t));
    
    mqttClient.disconnect();
    
    mqttClient.setServer(currentMqttConfig.server, currentMqttConfig.port);
    
    snprintf(mqttTopicStatus, sizeof(mqttTopicStatus), 
             "%s%s%s", MQTT_TOPIC_STATUS_PREFIX, currentMqttConfig.deviceId, MQTT_TOPIC_STATUS_SUFFIX);
    snprintf(mqttTopicCommand, sizeof(mqttTopicCommand), 
             "%s%s%s", MQTT_TOPIC_COMMAND_PREFIX, currentMqttConfig.deviceId, MQTT_TOPIC_COMMAND_SUFFIX);
    
    if (mqtt_debug_enabled) {
        Serial.print("[MQTT] 新状态 Topic: ");
        Serial.println(mqttTopicStatus);
        Serial.print("[MQTT] 新命令 Topic: ");
        Serial.println(mqttTopicCommand);
    }
    
    mqtt_connect();
}

/**
 * @brief       重置MQTT配置并断开连接（重置内部配置为默认值，阻止自动重连）
 * @param       无
 * @retval      无
 */
void mqtt_reset(void) {
    config_set_default(&currentMqttConfig);
    mqttClient.disconnect();
    if (mqtt_debug_enabled) Serial.println("[MQTT] 配置已重置，已断开连接");
}