#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Preferences.h>

/* 配置参数最大长度 */
#define CONFIG_MAX_SERVER_LEN     64
#define CONFIG_MAX_USERNAME_LEN   32
#define CONFIG_MAX_PASSWORD_LEN   32
#define CONFIG_MAX_DEVICE_ID_LEN  32

/* 配置结构体 */
typedef struct {
    char server[CONFIG_MAX_SERVER_LEN];
    uint16_t port;
    char username[CONFIG_MAX_USERNAME_LEN];
    char password[CONFIG_MAX_PASSWORD_LEN];
    char deviceId[CONFIG_MAX_DEVICE_ID_LEN];
} mqtt_config_t;

/* 函数声明 */
void config_init(void);
void config_load(mqtt_config_t* config);
void config_save(const mqtt_config_t* config);
void config_set_default(mqtt_config_t* config);
void config_print(const mqtt_config_t* config);

#endif /* __CONFIG_H__ */