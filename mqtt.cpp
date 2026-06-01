#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "mqtt.h"
#include "version.h"
#include "key.h"

/* 外部变量声明 */
extern float temperature;
extern float lightValue;
extern bool g_autoMode;
extern bool lightEnabled;
extern bool fanEnabled;
extern float tempThreshold;
extern float lightThreshold;

/* MQTT 客户端实例 */
WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

/* Topic 缓冲区 */
static char mqttTopicStatus[64];
static char mqttTopicCommand[64];

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
    Serial.println("[MQTT] 连接成功");
    
    // 订阅命令 Topic
    if (mqttClient.subscribe(mqttTopicCommand)) {
        Serial.print("[MQTT] 已订阅: ");
        Serial.println(mqttTopicCommand);
    } else {
        Serial.println("[MQTT] 订阅失败");
    }
}

/**
 * @brief       MQTT 断开连接回调函数
 * @param       无
 * @retval      无
 */
void mqtt_on_disconnect(WiFiClient *client, char *topic, byte *payload, unsigned int length) {
    mqttConnected = false;
    Serial.println("[MQTT] 连接断开");
}

/**
 * @brief       MQTT 消息回调函数
 * @param       topic: 消息主题
 * @param       payload: 消息内容
 * @param       length: 消息长度
 * @retval      无
 */
void mqtt_on_message(char* topic, byte* payload, unsigned int length) {
    Serial.print("[MQTT] 收到消息: ");
    Serial.print(topic);
    Serial.print(" -> ");
    
    // 打印消息内容
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    
    // 解析 JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        Serial.print("[MQTT] JSON 解析失败: ");
        Serial.println(error.f_str());
        return;
    }
    
    // 获取命令
    const char* cmd = doc["cmd"];
    if (!cmd) {
        Serial.println("[MQTT] 缺少 cmd 字段");
        return;
    }
    
    // 处理命令
    if (strcmp(cmd, MQTT_CMD_SET_RELAY) == 0) {
        int relay = doc["relay"];
        bool value = doc["value"];
        
        Serial.print("[MQTT] 命令: set_relay, relay: ");
        Serial.print(relay);
        Serial.print(", value: ");
        Serial.println(value);
        
        // 受 KEY1 约束
        if (key1_is_on()) {
            if (relay == 3) {
                lightEnabled = value;
                Serial.print("[MQTT] 设置继电器3(LED): ");
                Serial.println(value ? "ON" : "OFF");
            } else if (relay == 4) {
                fanEnabled = value;
                Serial.print("[MQTT] 设置继电器4(FAN): ");
                Serial.println(value ? "ON" : "OFF");
            }
            mqtt_publish_status();  // 立即上报新状态
        } else {
            Serial.println("[MQTT] 继电器控制无效 - 总闸已关闭");
        }
        
    } else if (strcmp(cmd, MQTT_CMD_SET_MODE) == 0) {
        const char* mode = doc["mode"];
        
        Serial.print("[MQTT] 命令: set_mode, mode: ");
        Serial.println(mode);
        
        if (strcmp(mode, "auto") == 0) {
            g_autoMode = true;
            Serial.println("[MQTT] 切换到自动模式");
        } else if (strcmp(mode, "manual") == 0) {
            g_autoMode = false;
            Serial.println("[MQTT] 切换到手动模式");
        }
        mqtt_publish_status();  // 立即上报新状态
        
    } else if (strcmp(cmd, MQTT_CMD_GET_STATUS) == 0) {
        Serial.println("[MQTT] 命令: get_status");
        // 检查最近是否主动上报过（5秒内）
        unsigned long currentMillis = millis();
        if (mqttRecentlyPublished && (currentMillis - mqttLastPublishTime < 5000)) {
            Serial.println("[MQTT] 最近已主动上报，跳过应答");
        } else {
            mqtt_publish_status();  // 立即上报状态
        }
        
    } else if (strcmp(cmd, MQTT_CMD_SET_THRESHOLD) == 0) {
        float temp = doc["temp"];
        float light = doc["light"];
        
        Serial.print("[MQTT] 命令: set_threshold, temp: ");
        Serial.print(temp);
        Serial.print(", light: ");
        Serial.println(light);
        
        if (temp > 0) {
            tempThreshold = temp;
        }
        if (light > 0) {
            lightThreshold = light;
        }
        mqtt_publish_status();  // 立即上报新状态
        
    } else if (strcmp(cmd, MQTT_CMD_REBOOT) == 0) {
        Serial.println("[MQTT] 命令: reboot - 即将重启设备");
        delay(100);
        ESP.restart();
        
    } else {
        Serial.print("[MQTT] 未知命令: ");
        Serial.println(cmd);
    }
}

/**
 * @brief       MQTT 初始化
 * @param       无
 * @retval      无
 */
void mqtt_init(void) {
    // 设置服务器地址和端口
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    
    // 设置回调函数
    mqttClient.setCallback(mqtt_on_message);
    
    // 构建 Topic
    snprintf(mqttTopicStatus, sizeof(mqttTopicStatus), 
             "%s%s%s", MQTT_TOPIC_STATUS_PREFIX, DEVICE_ID, MQTT_TOPIC_STATUS_SUFFIX);
    snprintf(mqttTopicCommand, sizeof(mqttTopicCommand), 
             "%s%s%s", MQTT_TOPIC_COMMAND_PREFIX, DEVICE_ID, MQTT_TOPIC_COMMAND_SUFFIX);
    
    Serial.print("[MQTT] 状态 Topic: ");
    Serial.println(mqttTopicStatus);
    Serial.print("[MQTT] 命令 Topic: ");
    Serial.println(mqttTopicCommand);
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
    
    Serial.print("[MQTT] 正在连接...");
    
    // 尝试连接
    if (mqttClient.connect(DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
        mqttConnected = true;
        Serial.println("成功");
        
        // 订阅命令 Topic
        mqttClient.subscribe(mqttTopicCommand);
        Serial.print("[MQTT] 已订阅: ");
        Serial.println(mqttTopicCommand);
    } else {
        Serial.print("失败，错误码: ");
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
    doc["temperature"] = temperature;
    doc["light"] = (int)lightValue;
    doc["mode"] = g_autoMode ? "auto" : "manual";
    doc["key1_lock"] = key1_is_on();
    doc["relay3"] = lightEnabled;
    
    // 序列化 JSON
    char payload[256];
    serializeJson(doc, payload, sizeof(payload));
    
    // 发布消息
    if (mqttClient.publish(mqttTopicStatus, payload)) {
        Serial.print("[MQTT] 发布状态: ");
        Serial.println(payload);
        // 记录上报时间和标志
        mqttLastPublishTime = millis();
        mqttRecentlyPublished = true;
    } else {
        Serial.println("[MQTT] 发布失败");
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
}

/**
 * @brief       请求立即发布设备状态（按键状态变化时调用）
 * @param       无
 * @retval      无
 */
void mqtt_request_publish(void) {
    mqttNeedPublish = true;
}