#include "serial_cmd.h"
#include "config.h"
#include "mqtt.h"
#include <Arduino.h>
#include <string.h>

/* 命令缓冲区 */
#define CMD_BUFFER_SIZE 256
static char cmdBuffer[CMD_BUFFER_SIZE];
static uint16_t cmdIndex = 0;

/* 当前配置 */
static mqtt_config_t currentConfig;

/**
 * @brief       初始化串口命令处理
 * @param       无
 * @retval      无
 */
void serial_cmd_init(void) {
    memset(cmdBuffer, 0, sizeof(cmdBuffer));
    cmdIndex = 0;
    config_load(&currentConfig);
    Serial.println("[SerialCmd] 串口命令处理已初始化");
    Serial.println("[SerialCmd] 输入 'help' 查看可用命令");
}

/**
 * @brief       打印帮助信息
 * @param       无
 * @retval      无
 */
void serial_cmd_print_help(void) {
    Serial.println("\n========== 串口命令帮助 ==========");
    Serial.println("help                    - 显示帮助信息");
    Serial.println("config                  - 显示当前MQTT配置");
    Serial.println("set server <ip>         - 设置MQTT服务器地址");
    Serial.println("set port <port>         - 设置MQTT端口");
    Serial.println("set username <user>     - 设置MQTT用户名");
    Serial.println("set password <pass>     - 设置MQTT密码");
    Serial.println("set deviceid <id>       - 设置设备ID");
    Serial.println("save                    - 保存配置到Flash");
    Serial.println("reset                   - 恢复默认配置");
  Serial.println("reconnect                - 使用新配置重新连接MQTT");
    Serial.println("====================================\n");
}

/**
 * @brief       处理设置命令
 * @param       cmd: 命令字符串
 * @retval      无
 */
void handle_set_command(const char* cmd) {
    char* param = strchr(cmd, ' ');
    if (param == NULL) {
        Serial.println("[SerialCmd] 错误: 缺少参数");
        return;
    }
    param++;  // 跳过空格
    
    if (strncmp(cmd, "set server", 10) == 0) {
        strncpy(currentConfig.server, param, sizeof(currentConfig.server) - 1);
        currentConfig.server[sizeof(currentConfig.server) - 1] = '\0';
        Serial.print("[SerialCmd] 服务器地址已设置为: ");
        Serial.println(currentConfig.server);
    } 
    else if (strncmp(cmd, "set port", 8) == 0) {
        uint16_t port = atoi(param);
        if (port > 0 && port <= 65535) {
            currentConfig.port = port;
            Serial.print("[SerialCmd] 端口已设置为: ");
            Serial.println(currentConfig.port);
        } else {
            Serial.println("[SerialCmd] 错误: 端口范围无效 (1-65535)");
        }
    }
    else if (strncmp(cmd, "set username", 12) == 0) {
        strncpy(currentConfig.username, param, sizeof(currentConfig.username) - 1);
        currentConfig.username[sizeof(currentConfig.username) - 1] = '\0';
        Serial.print("[SerialCmd] 用户名已设置为: ");
        Serial.println(currentConfig.username);
    }
    else if (strncmp(cmd, "set password", 12) == 0) {
        strncpy(currentConfig.password, param, sizeof(currentConfig.password) - 1);
        currentConfig.password[sizeof(currentConfig.password) - 1] = '\0';
        Serial.println("[SerialCmd] 密码已设置");
    }
    else if (strncmp(cmd, "set deviceid", 12) == 0) {
        strncpy(currentConfig.deviceId, param, sizeof(currentConfig.deviceId) - 1);
        currentConfig.deviceId[sizeof(currentConfig.deviceId) - 1] = '\0';
        Serial.print("[SerialCmd] 设备ID已设置为: ");
        Serial.println(currentConfig.deviceId);
    }
    else {
        Serial.println("[SerialCmd] 错误: 未知命令");
    }
}

/**
 * @brief       处理单条命令
 * @param       cmd: 命令字符串
 * @retval      无
 */
void process_single_command(const char* cmd) {
    if (strlen(cmd) == 0) {
        return;
    }
    
    Serial.print("[SerialCmd] 收到命令: ");
    Serial.println(cmd);
    
    if (strcmp(cmd, "help") == 0) {
        serial_cmd_print_help();
    }
    else if (strcmp(cmd, "config") == 0) {
        config_print(&currentConfig);
    }
    else if (strncmp(cmd, "set ", 4) == 0) {
        handle_set_command(cmd);
    }
    else if (strcmp(cmd, "save") == 0) {
        config_save(&currentConfig);
        Serial.println("[SerialCmd] 配置已保存，请使用 'reconnect' 命令重新连接MQTT");
    }
    else if (strcmp(cmd, "reset") == 0) {
        config_set_default(&currentConfig);
        Serial.println("[SerialCmd] 已恢复默认配置，请使用 'save' 保存或 'reconnect' 重新连接");
    }
    else if (strcmp(cmd, "reconnect") == 0) {
        mqtt_reconnect_with_config(&currentConfig);
    }
    else {
        Serial.println("[SerialCmd] 错误: 未知命令，输入 'help' 查看帮助");
    }
}

/**
 * @brief       处理串口命令（在loop中调用）
 * @param       无
 * @retval      无
 */
void serial_cmd_process(void) {
    while (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (cmdIndex > 0) {
                cmdBuffer[cmdIndex] = '\0';
                process_single_command(cmdBuffer);
                cmdIndex = 0;
            }
        }
        else if (cmdIndex < CMD_BUFFER_SIZE - 1) {
            cmdBuffer[cmdIndex++] = c;
        }
    }
}