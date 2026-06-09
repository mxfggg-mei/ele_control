#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Preferences.h>

/* ==================== WiFi 状态枚举（全局共享） ==================== */
enum WiFiState {
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_DISCONNECTED
};

/* ==================== 全局工作变量声明（所有模块共享） ==================== */
extern WiFiState wifiState;
extern bool  g_autoMode;
extern uint8_t g_relayState;
extern float lightValue;
extern float lightThreshold;
extern float tempThreshold;
extern bool  lightEnabled;
extern bool  fanEnabled;

/* 配置参数最大长度 */
#define CONFIG_MAX_SERVER_LEN     64
#define CONFIG_MAX_USERNAME_LEN   32
#define CONFIG_MAX_PASSWORD_LEN   32
#define CONFIG_MAX_DEVICE_ID_LEN 32
#define CONFIG_MAX_WIFI_SSID_LEN 32
#define CONFIG_MAX_WIFI_PASS_LEN 32

/* WiFi 配置结构体 */
typedef struct {
    char ssid[CONFIG_MAX_WIFI_SSID_LEN];
    char password[CONFIG_MAX_WIFI_PASS_LEN];
} AppWifiConfig;

/* MQTT 配置结构体 */
typedef struct {
    char server[CONFIG_MAX_SERVER_LEN];
    uint16_t port;
    char username[CONFIG_MAX_USERNAME_LEN];
    char password[CONFIG_MAX_PASSWORD_LEN];
    char deviceId[CONFIG_MAX_DEVICE_ID_LEN];
} mqtt_config_t;

/* WiFi 配置函数 */
void wifi_config_load(AppWifiConfig* config);
void wifi_config_save(const AppWifiConfig* config);
void wifi_config_erase(void);
void wifi_config_set_default(AppWifiConfig* config);
void wifi_config_print(const AppWifiConfig* config);
bool wifi_config_is_valid(const AppWifiConfig* config);

/* 获取全局配置 */
AppWifiConfig* get_wifi_config(void);

/* 工作参数（模式/阈值/继电器状态，重启后恢复） */
typedef struct {
    uint8_t  autoMode;         /* 0=manual, 1=auto */
    uint8_t  relayState;       /* 0-3 继电器状态 */
    float tempThreshold;
    float lightThreshold;
} work_param_t;

void param_save(void);
void param_load(void);
void param_erase(void);

/* MQTT 配置函数 */
void config_init(void);
void config_load(mqtt_config_t* config);
void config_save(const mqtt_config_t* config);
void config_erase(void);
void config_set_default(mqtt_config_t* config);
void config_print(const mqtt_config_t* config);
bool config_is_valid(const mqtt_config_t* config);
mqtt_config_t* get_mqtt_config(void);

#endif /* __CONFIG_H__ */