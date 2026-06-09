# 项目架构分析：`ele_control`（化工智能控制系统）

## 基本信息
- **平台**：ESP32-C3
- **版本**：V1.9
- **功能**：基于传感器的智能继电器控制系统，支持光照和温度自动/手动控制，具备 WiFi + MQTT 远程管理能力

---

## 一、模块总览

```
┌──────────────────────────────────────────────────────────────┐
│                     ele_control.ino (主程序)                    │
│              setup() → loop() → handleKeyEvent()               │
└──────┬───────┬───────┬───────┬───────┬──────┬───────┬────────┘
       │       │       │       │       │      │       │
       ▼       ▼       ▼       ▼       ▼      ▼       ▼
    ┌─────┐┌─────┐┌─────┐┌─────┐┌─────┐┌─────┐┌──────────┐
    │config││ led ││ key ││ adc ││ mqtt││ rgb ││serial_cmd│
    └──┬──┘└─────┘└─────┘└─────┘└──┬──┘└──┬──┘└──────────┘
       │                           │      │
       ▼                           ▼      ▼
    ┌─────┐                    ┌──────┐┌─────┐
    │ temp│                    │ oled ││rgb  │ (独立RTOS任务)
    └─────┘                    └──────┘└─────┘
```

---

## 二、模块详细说明

| 模块 | 文件 | 功能 | 依赖 |
|------|------|------|------|
| **主程序** | `ele_control.ino` | 程序入口，状态机调度，WiFi事件处理，按键事件分发，自动控制逻辑 | 所有模块 |
| **配置管理** | `config.h/cpp` | WiFi/MQTT 配置读写（Preferences/Flash），工作参数持久化 | Preferences |
| **LED/继电器** | `led.h/cpp` | GPIO6(LED)、GPIO7(FAN) 引脚初始化和宏定义，迟滞常量 | - |
| **按键驱动** | `key.h/cpp` | KEY1(GPIO20,自锁)轮询 + KEY2(GPIO21,自复位)中断 | - |
| **ADC光照** | `adc.h/cpp` | GPIO1 读取 GL5516 光照传感器，反相ADC平方公式转lux | - |
| **温度传感器** | `temp.h/cpp` | DS18B20(GPIO10)，独立 FreeRTOS 任务每秒轮询 | OneWire, DallasTemperature |
| **RGB指示灯** | `rgb.h/cpp` | GPIO0 NeoPixel，独立 FreeRTOS 任务 33fps，多序列动画 | Adafruit_NeoPixel |
| **OLED显示** | `oled.h/cpp` | SH1106 128x64 I2C(GPIO4/5)，100ms 非阻塞刷新 | U8g2lib |
| **MQTT通信** | `mqtt.h/cpp` | 远程控制(set_relay/set_mode/set_threshold/reboot)，状态上报 | PubSubClient, ArduinoJson |
| **串口命令** | `serial_cmd.h/cpp` | 交互式配网(WiFi scan)、MQTT 参数配置、设备管理 | - |
| **版本信息** | `version.h` | 版本号 V1.9，设备ID PCT_100_888，项目名称 | - |

---

## 三、硬件引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| GPIO0 | RGB NeoPixel | 状态指示灯 |
| GPIO1 | ADC 输入 | GL5516 光照传感器（分压电路） |
| GPIO4 | I2C SDA | OLED 显示屏 |
| GPIO5 | I2C SCL | OLED 显示屏 |
| GPIO6 | LED 继电器 | 灯光控制（HIGH=吸合） |
| GPIO7 | FAN 继电器 | 风扇控制（HIGH=吸合） |
| GPIO10 | OneWire | DS18B20 温度传感器 |
| GPIO20 | KEY1 | 自锁总开关（INPUT_PULLDOWN） |
| GPIO21 | KEY2 | 自复位按键（中断模式） |

---

## 四、程序执行流程

### setup() 初始化流程

```
setup()
  ├── led_init() / fan_init()     → GPIO6/7 输出低电平
  ├── key1_init() / key2_init()   → KEY1轮询 + KEY2中断(RISING)
  ├── rgb_init()                  → NeoPixel 初始化
  ├── Serial.begin(115200)        → 串口启动
  ├── WiFi.onEvent()              → 注册WiFi事件回调
  ├── config_init()               → 从Flash加载WiFi/MQTT配置
  ├── param_load()                → 恢复工作参数(模式/阈值/继电器)
  ├── serial_cmd_init()           → 串口命令就绪
  ├── startWiFiConnection()       → 开始WiFi连接
  ├── mqtt_init()                 → 初始化MQTT客户端
  ├── oled_init()                 → OLED开机画面
  ├── temp_init()                 → DS18B20初始化
  ├── temp_task_create()          → 创建温度轮询任务(优先级1)
  ├── rgb_task_create()           → 创建RGB灯控任务(优先级2)
  ├── adc_init()                  → ADC初始化
  └── rgb_boot_flash()            → 启动多色快闪
```

### loop() 主循环流程

```
loop()  (每个周期)
  ├── continueWiFiConnection()    → 非阻塞WiFi连接状态机
  ├── updateLedState()            → 根据KEY1状态控制继电器
  ├── handleWiFiDisconnected()    → 断线重连(5s间隔)
  ├── serial_cmd_process()        → 处理串口命令
  ├── 心跳打印 (10s间隔, WiFi已连时)
  ├── adc_read_lux() (200ms间隔)  → 读取光照值
  ├── 自动模式传感器控制 (带迟滞)
  │   ├── 光照 < 阈值 → 开灯
  │   ├── 光照 > 阈值+10lux → 关灯
  │   ├── 温度 > 阈值 → 开风扇
  │   └── 温度 < 阈值-0.5℃ → 关风扇
  ├── handleKeyEvent()            → 按键事件处理
  │   ├── KEY1: 总闸ON/OFF
  │   ├── KEY2短按: 手动模式切换继电器状态(00→01→10→11)
  │   └── KEY2长按(≥1s): 切换自动/手动模式
  ├── mqtt_loop()                 → MQTT消息处理与重连
  └── oled_update()               → OLED非阻塞刷新
```

---

## 五、RTOS 任务架构

| 任务名 | 优先级 | 栈大小 | 周期 | 功能 |
|--------|--------|--------|------|------|
| `tempTask` | 1 (低) | 4096 | ~1s | DS18B20温度轮询，更新 `g_temperature` |
| `rgbTask` | 2 (中) | 4096 | ~30ms | RGB灯效自动选择与动画渲染 |
| `loop()` | 主任务 | - | 事件驱动 | 主循环，处理WiFi/按键/MQTT/OLED/ADC |

---

## 六、数据流与全局状态

```
┌─────────────────────────────────────────────────┐
│                 全局共享变量 (config.h)             │
├──────────────┬──────────────┬────────────────────┤
│ wifiState    │ g_autoMode   │ lightValue          │
│ lightEnabled │ fanEnabled   │ g_relayState        │
│ lightThreshold│ tempThreshold│ g_temperature(temp) │
└──────────────┴──────────────┴────────────────────┘
         ▲              ▲              ▲
    ┌────┴────┐   ┌─────┴─────┐   ┌───┴────┐
    │ 主程序   │   │ MQTT命令  │   │ RTOS   │
    │ 按键控制 │   │ 远程控制  │   │ 任务   │
    └─────────┘   └───────────┘   └────────┘
```

---

## 七、控制模式

| 模式 | 触发方式 | 行为 |
|------|----------|------|
| **自动模式**（默认） | KEY2长按切换 | 根据光照/温度传感器阈值自动控制继电器 |
| **手动模式** | KEY2长按切换 | KEY2短按循环切换 00→01→10→11 四种状态 |
| **总闸关闭** | KEY1自锁OFF | 强制断开所有继电器，不受任何模式影响 |

### 继电器状态编码

| 状态值 | 二进制 | LED(灯) | FAN(风扇) |
|--------|--------|---------|-----------|
| 0 | 0b00 | OFF | OFF |
| 1 | 0b01 | ON | OFF |
| 2 | 0b10 | OFF | ON |
| 3 | 0b11 | ON | ON |

### 传感器阈值与迟滞

| 参数 | 阈值 | 迟滞 |
|------|------|------|
| 光照 (lux) | 默认 300.0 | ±10.0 |
| 温度 (℃) | 默认 28.0 | ±0.5 |

---

## 八、RGB 指示灯状态体系

RGB灯根据**继电器状态 × WiFi × MQTT**组合自动显示不同灯效，优先级从高到低：

| 优先级 | 场景 | 灯效 |
|--------|------|------|
| 1 | 严重报警（灯+风扇都开） | 紫呼吸+橙呼吸循环 + 通讯指示 |
| 2 | 光照越界（仅灯开） | 橙色呼吸 + 通讯指示 |
| 3 | 温度越界（仅风扇开） | 紫色呼吸 + 通讯指示 |
| 4 | 总闸断开 + WiFi断 | 红色呼吸 + 白灯三闪 |
| 5 | 总闸断开 + MQTT断 | 蓝色呼吸 + 白灯三闪 |
| 6 | 总闸断开 + 正常 | 绿色呼吸 + 白灯三闪 |
| 7 | WiFi断（无报警） | 红色呼吸 |
| 8 | MQTT断（无报警） | 蓝色呼吸 |
| 9 | 一切正常 | 绿色呼吸 |

通讯指示：
- WiFi+MQTT正常 → 加绿灯闪烁
- WiFi正常、MQTT异常 → 加蓝灯闪烁
- WiFi+MQTT都异常 → 加红灯闪烁

---

## 九、MQTT 通信协议

### Topic 格式

| 类型 | 格式 | 说明 |
|------|------|------|
| 状态上报 | `chemctrl/{deviceId}/status` | 设备定时/事件触发上报 |
| 命令下发 | `chemctrl/{deviceId}/command` | 远程控制命令 |

### 命令列表

| 命令 | 参数 | 功能 |
|------|------|------|
| `set_relay` | `relay: 3/4`, `value: true/false` | 控制LED(3)或FAN(4)继电器 |
| `set_mode` | `mode: "auto"/"manual"` | 切换自动/手动模式 |
| `get_status` | - | 立即上报设备状态 |
| `set_threshold` | `temp: float`, `light: float` | 设置温度/光照阈值 |
| `reboot` | - | 重启设备 |

### 状态上报 JSON 格式

```json
{
  "temperature": 26.5,
  "light": 350,
  "temp_threshold": 28.0,
  "light_threshold": 300.0,
  "mode": "auto",
  "key1_lock": true,
  "relay3": false,
  "relay4": true
}
```

---

## 十、串口命令

| 分类 | 命令 | 功能 |
|------|------|------|
| **WiFi** | `wifi show` | 显示当前WiFi配置 |
| | `wifi scan` | 扫描并交互式配网 |
| | `wifi save` | 保存WiFi配置到Flash |
| | `wifi erase` | 清除WiFi配置 |
| | `wifi reset` | 重置并断开WiFi |
| **MQTT** | `mqtt show` | 显示MQTT配置 |
| | `mqtt set server <ip>` | 设置MQTT服务器 |
| | `mqtt set port <port>` | 设置MQTT端口 |
| | `mqtt set user <name>` | 设置用户名 |
| | `mqtt set pass <pwd>` | 设置密码 |
| | `mqtt set id <id>` | 设置设备ID |
| | `mqtt save` | 保存MQTT配置 |
| | `mqtt reconnect` | 使用新配置重连 |
| | `mqtt debug on/off` | 开关MQTT调试 |
| **设备** | `device show` | 显示设备ID |
| | `device set id <id>` | 设置设备ID |
| **系统** | `show all` | 显示所有配置 |
| | `reboot` | 重启设备 |
| | `help` | 帮助信息 |

---

## 十一、架构特点总结

1. **非阻塞设计**：主循环全部使用 `millis()` 定时，无 `delay()` 阻塞
2. **RTOS 多任务**：温度和 RGB 各自独立 FreeRTOS 任务，互不干扰
3. **Flash 持久化**：WiFi/MQTT/工作参数均通过 Preferences 保存，重启恢复
4. **交互式配网**：串口 `wifi scan` → 选择序号 → 输入密码，无需硬编码
5. **迟滞控制**：温度 ±0.5℃、光照 ±10lux 迟滞，避免临界抖动
6. **RGB 序列动画引擎**：支持 SOLID（直接设置）和 FADE（线性渐变）两种步骤类型，支持通讯状态叠加闪烁
7. **多层安全约束**：总闸(KEY1) → 自动/手动模式 → 继电器状态，层层约束
8. **状态变更即时上报**：按键变化、模式切换、阈值修改均立即通过 MQTT 上报

---

## 十二、文件清单

```
ele_control/
├── ele_control.ino    # 主程序入口
├── version.h          # 版本信息
├── config.h/cpp       # 配置管理（WiFi/MQTT/工作参数）
├── led.h/cpp          # LED/风扇继电器驱动
├── key.h/cpp          # 按键驱动（KEY1轮询 + KEY2中断）
├── adc.h/cpp          # ADC 光照传感器读取
├── temp.h/cpp         # DS18B20 温度传感器（RTOS任务）
├── rgb.h/cpp          # RGB NeoPixel 指示灯（RTOS任务）
├── oled.h/cpp         # OLED 显示驱动
├── mqtt.h/cpp         # MQTT 通信模块
└── serial_cmd.h/cpp   # 串口命令处理
```
