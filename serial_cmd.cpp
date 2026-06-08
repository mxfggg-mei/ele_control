#include "serial_cmd.h"
#include "config.h"
#include "mqtt.h"
#include "rgb.h"
#include <WiFi.h>
#include <Arduino.h>
#include <string.h>

/* 命令缓冲区 */
#define CMD_BUFFER_SIZE 256
static char cmdBuffer[CMD_BUFFER_SIZE];
static uint16_t cmdIndex = 0;

/* 当前配置 */
static AppWifiConfig currentWifiConfig;
static mqtt_config_t currentMqttConfig;

/* WiFi 交互式配网状态 */
static uint8_t wifiSetupState = 0;  // 0=IDLE, 1=等待选择网络, 2=等待输入密码

/**
 * @brief       初始化串口命令处理
 * @param       无
 * @retval      无
 */
void serial_cmd_init(void) {
    memset(cmdBuffer, 0, sizeof(cmdBuffer));
    cmdIndex = 0;
    // 从已加载的全局配置拷贝本地工作副本，避免重复读取 Flash
    memcpy(&currentMqttConfig, get_mqtt_config(), sizeof(mqtt_config_t));
    memcpy(&currentWifiConfig, get_wifi_config(), sizeof(AppWifiConfig));
    Serial.println("[SerialCmd] 串口命令已初始化");
    Serial.println("[SerialCmd] 输入 'help' 查看可用命令");
}

/**
 * @brief       打印帮助信息
 * @param       无
 * @retval      无
 */
void serial_cmd_print_help(void) {
    Serial.println("\n========== 串口命令帮助 ==========");
    Serial.println("=== WiFi 配置命令（交互式配网）===");
    Serial.println("wifi show                - 显示当前WiFi配置");
    Serial.println("wifi scan                - 扫描并列出附近WiFi网络");
    Serial.println("wifi save                - 手动保存WiFi配置到Flash");
    Serial.println("wifi erase               - 清除Flash中保存的WiFi配置");
    Serial.println("wifi reset               - 重置WiFi配置并断开连接");
    Serial.println("");
    Serial.println("=== MQTT 配置命令 ===");
    Serial.println("mqtt show                - 显示当前MQTT配置");
    Serial.println("mqtt set server <ip>    - 设置MQTT服务器");
    Serial.println("mqtt set port <port>     - 设置MQTT端口");
    Serial.println("mqtt set user <username> - 设置MQTT用户名");
    Serial.println("mqtt set pass <password>- 设置MQTT密码");
    Serial.println("mqtt set id <device_id>  - 设置设备ID");
    Serial.println("mqtt save                - 保存MQTT配置到Flash");
    Serial.println("mqtt erase               - 清除Flash中保存的MQTT配置");
    Serial.println("mqtt reset               - 重置MQTT配置并断开连接（不清除Flash）");
    Serial.println("mqtt reconnect           - 使用新配置重新连接MQTT");
    Serial.println("mqtt debug <on|off>     - 开启/关闭MQTT调试信息");
    Serial.println("");
    Serial.println("=== 系统命令 ===");
    Serial.println("help                     - 显示帮助信息");
    Serial.println("show all                 - 显示所有配置");
    Serial.println("reboot                  - 重启设备");
    Serial.println("====================================\n");
}

/**
 * @brief       处理 WiFi 命令
 * @param       cmd: 命令字符串
 * @retval      无
 */
void handle_wifi_command(const char* cmd) {
    if (strcmp(cmd, "wifi show") == 0) {
        wifi_config_print(&currentWifiConfig);
    }
    else if (strcmp(cmd, "wifi scan") == 0) {
        Serial.println("[WiFi] 正在扫描网络...");
        int n = WiFi.scanNetworks(false, true);
        if (n == 0) {
            Serial.println("[WiFi] 未扫描到网络");
            wifiSetupState = 0;
        } else {
            Serial.print("[WiFi] 扫描到 ");
            Serial.print(n);
            Serial.println(" 个网络:");
            for (int i = 0; i < n; i++) {
                Serial.print("  [");
                Serial.print(i);
                Serial.print("] ");
                Serial.print(WiFi.SSID(i));
                bool isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
                Serial.println(isOpen ? "  [开放]" : "  [加密]");
            }
            wifiSetupState = 1;
            Serial.println(">>> 请输入序号(0~" + String(n - 1) + ")选择网络:");
        }
    }
    else if (strcmp(cmd, "wifi reset") == 0) {
        wifi_config_set_default(&currentWifiConfig);
        memcpy(get_wifi_config(), &currentWifiConfig, sizeof(AppWifiConfig));
        wifiSetupState = 0;
        WiFi.disconnect();  // 断开 WiFi 连接
        // RGB 状态由 rgb_update_by_state() 在 loop 中自动更新为 WiFi 未连接灯效
        Serial.println("[WiFi] 配置已重置，WiFi 已断开");
    }
    else if (strcmp(cmd, "wifi save") == 0) {
        memcpy(get_wifi_config(), &currentWifiConfig, sizeof(AppWifiConfig));
        wifi_config_save(get_wifi_config());
    }
    else {
        Serial.println("[SerialCmd] 未知WiFi命令，输入 'help' 查看帮助");
    }
}

/**
 * @brief       处理 MQTT 命令
 * @param       cmd: 命令字符串
 * @retval      无
 */
void handle_mqtt_command(const char* cmd) {
    if (strcmp(cmd, "mqtt show") == 0) {
        config_print(&currentMqttConfig);
    }
    else if (strcmp(cmd, "mqtt save") == 0) {
        config_save(&currentMqttConfig);
        Serial.println("[SerialCmd] MQTT配置已保存");
    }
    else if (strcmp(cmd, "mqtt erase") == 0) {
        config_erase();
        Serial.println("[SerialCmd] 已清除Flash中的MQTT配置，重启后生效");
    }
    else if (strcmp(cmd, "mqtt reset") == 0) {
        config_set_default(&currentMqttConfig);
        mqtt_reset();
        Serial.println("[SerialCmd] MQTT配置已重置，已断开连接");
    }
    else if (strcmp(cmd, "mqtt reconnect") == 0) {
        Serial.print("[SerialCmd] 正在使用新配置重新连接MQTT...");
        mqtt_reconnect_with_config(&currentMqttConfig);
        // mqtt_reconnect_with_config 内部已调用 mqtt_connect()，连接结果实时打印
    }
    else if (strncmp(cmd, "mqtt set server ", 16) == 0) {
        const char* server = cmd + 16;
        strncpy(currentMqttConfig.server, server, sizeof(currentMqttConfig.server) - 1);
        currentMqttConfig.server[sizeof(currentMqttConfig.server) - 1] = '\0';
        Serial.print("[SerialCmd] MQTT服务器已设置为: ");
        Serial.println(currentMqttConfig.server);
    }
    else if (strncmp(cmd, "mqtt set port ", 14) == 0) {
        uint16_t port = atoi(cmd + 14);
        if (port > 0 && port <= 65535) {
            currentMqttConfig.port = port;
            Serial.print("[SerialCmd] MQTT端口已设置为: ");
            Serial.println(currentMqttConfig.port);
        } else {
            Serial.println("[SerialCmd] 错误: 端口范围无效 (1-65535)");
        }
    }
    else if (strncmp(cmd, "mqtt set user ", 14) == 0) {
        const char* user = cmd + 14;
        strncpy(currentMqttConfig.username, user, sizeof(currentMqttConfig.username) - 1);
        currentMqttConfig.username[sizeof(currentMqttConfig.username) - 1] = '\0';
        Serial.print("[SerialCmd] MQTT用户名已设置为: ");
        Serial.println(currentMqttConfig.username);
    }
    else if (strncmp(cmd, "mqtt set pass ", 14) == 0) {
        const char* pass = cmd + 14;
        strncpy(currentMqttConfig.password, pass, sizeof(currentMqttConfig.password) - 1);
        currentMqttConfig.password[sizeof(currentMqttConfig.password) - 1] = '\0';
        Serial.println("[SerialCmd] MQTT密码已设置");
    }
    else if (strncmp(cmd, "mqtt set id ", 12) == 0) {
        const char* id = cmd + 12;
        strncpy(currentMqttConfig.deviceId, id, sizeof(currentMqttConfig.deviceId) - 1);
        currentMqttConfig.deviceId[sizeof(currentMqttConfig.deviceId) - 1] = '\0';
        Serial.print("[SerialCmd] 设备ID已设置为: ");
        Serial.println(currentMqttConfig.deviceId);
    }
    else if (strcmp(cmd, "mqtt debug on") == 0) {
        extern bool mqtt_debug_enabled;
        mqtt_debug_enabled = true;
        Serial.println("[SerialCmd] MQTT调试信息已开启");
    }
    else if (strcmp(cmd, "mqtt debug off") == 0) {
        extern bool mqtt_debug_enabled;
        mqtt_debug_enabled = false;
        Serial.println("[SerialCmd] MQTT调试信息已关闭");
    }
    else {
        Serial.println("[SerialCmd] 未知MQTT命令，输入 'help' 查看帮助");
    }
}

/**
 * @brief       处理单条命令
 * @param       cmd: 命令字符串
 * @retval      无
 */
void process_single_command(const char* cmd) {
    if (strlen(cmd) == 0) {
        if (wifiSetupState == 2) {
            // 空密码
            wifiSetupState = 0;
            Serial.print("[WiFi] 正在连接 ");
            Serial.print(currentWifiConfig.ssid);
            Serial.println("...");
            wifi_reconnect_with_config(&currentWifiConfig);
        }
        return;
    }
    
    Serial.print("[SerialCmd] 收到命令: ");
    Serial.println(cmd);
    
    if (strcmp(cmd, "help") == 0) {
        serial_cmd_print_help();
    }
    else if (strcmp(cmd, "show all") == 0) {
        Serial.println("\n--- WiFi 配置 ---");
        wifi_config_print(&currentWifiConfig);
        Serial.println("\n--- MQTT 配置 ---");
        config_print(&currentMqttConfig);
    }
    else if (strncmp(cmd, "wifi ", 5) == 0) {
        handle_wifi_command(cmd);
    }
    else if (strncmp(cmd, "mqtt ", 5) == 0) {
        handle_mqtt_command(cmd);
    }
    else if (strcmp(cmd, "reboot") == 0) {
        Serial.println("[System] 正在重启...");
        delay(100);
        ESP.restart();
    }
    else if (wifiSetupState == 1 && isdigit(cmd[0])) {
        int n = WiFi.scanComplete();
        int index = atoi(cmd);
        if (n >= 0 && index >= 0 && index < n) {
            String ssid = WiFi.SSID(index);
            strncpy(currentWifiConfig.ssid, ssid.c_str(), sizeof(currentWifiConfig.ssid) - 1);
            currentWifiConfig.ssid[sizeof(currentWifiConfig.ssid) - 1] = '\0';
            Serial.print("[WiFi] 已选择: ");
            Serial.println(currentWifiConfig.ssid);
            wifiSetupState = 2;
            Serial.println(">>> 请输入密码(直接回车为空密码):");
        } else {
            Serial.println("[WiFi] 序号无效");
        }
    }
    else if (wifiSetupState == 2) {
        strncpy(currentWifiConfig.password, cmd, sizeof(currentWifiConfig.password) - 1);
        currentWifiConfig.password[sizeof(currentWifiConfig.password) - 1] = '\0';
        wifiSetupState = 0;
        
        // 同步到全局配置（连接成功时自动保存到 Flash）
        memcpy(get_wifi_config(), &currentWifiConfig, sizeof(AppWifiConfig));
        
        Serial.print("[WiFi] 正在连接 ");
        Serial.print(currentWifiConfig.ssid);
        Serial.println("...");
        wifi_reconnect_with_config(&currentWifiConfig);
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

/**
 * @brief       使用指定配置重新连接 WiFi
 * @param       config: WiFi 配置指针
 * @retval      无
 */
void wifi_reconnect_with_config(const AppWifiConfig* config) {
    if (config->ssid[0] == '\0') {
        Serial.println("[WiFi] 错误: SSID 未设置");
        return;
    }
    
    Serial.print("[WiFi] 正在连接: ");
    Serial.println(config->ssid);
    
    WiFi.disconnect();
    delay(100);
    
    if (config->password[0] == '\0') {
        WiFi.begin(config->ssid);
    } else {
        WiFi.begin(config->ssid, config->password);
    }
}