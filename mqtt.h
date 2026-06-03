#ifndef __MQTT_H__
#define __MQTT_H__

#include "config.h"

/* ==================== MQTT 默认配置 ==================== */
#define MQTT_SERVER      "47.98.170.180"  /* MQTT 服务器地址 */
#define MQTT_PORT        8081             /* MQTT 端口 */
#define MQTT_USERNAME    "dzdx_emqx"      /* MQTT 用户名 */
#define MQTT_PASSWORD    "Jp4!sQ7$"       /* MQTT 密码 */

/* ==================== Topic 定义 ==================== */
#define MQTT_TOPIC_STATUS_PREFIX    "chemctrl/"
#define MQTT_TOPIC_STATUS_SUFFIX    "/status"
#define MQTT_TOPIC_COMMAND_PREFIX   "chemctrl/"
#define MQTT_TOPIC_COMMAND_SUFFIX   "/command"

/* ==================== 命令类型 ==================== */
#define MQTT_CMD_SET_RELAY      "set_relay"
#define MQTT_CMD_SET_MODE       "set_mode"
#define MQTT_CMD_GET_STATUS     "get_status"
#define MQTT_CMD_SET_THRESHOLD  "set_threshold"
#define MQTT_CMD_REBOOT         "reboot"

/* ==================== 函数声明 ==================== */
void mqtt_init(void);                          /* MQTT 初始化 */
void mqtt_connect(void);                       /* MQTT 连接 */
void mqtt_loop(void);                          /* MQTT 循环处理 */
bool mqtt_is_connected(void);                  /* 检查 MQTT 是否连接 */
void mqtt_publish_status(void);                /* 发布设备状态 */
void mqtt_request_publish(void);               /* 请求立即发布状态 */
void mqtt_set_threshold(float temp, float light);  /* 设置阈值 */
void mqtt_reconnect_with_config(const mqtt_config_t* config);  /* 使用新配置重新连接 */

#endif /* __MQTT_H__ */