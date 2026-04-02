# Aqua Sentinel — STM32H743 智能鱼缸控制系统

基于 STM32H743IIT6 微控制器的 IoT 智能鱼缸自动化控制系统。系统通过多种传感器实时监测水质参数，自动控制加热器、水泵、风扇和 LED 照明等设备，并通过 ESP01S WiFi 模块与华为云 IoT 平台进行 MQTT 数据通信。配备 800×480 RGB LCD 触摸屏，使用 LVGL 图形库提供丰富的用户界面。

## 功能特性

- **多传感器实时监测** — 水温 (DS18B20)、气压 (BMP280)、浊度、TDS、pH 值、水位
- **智能设备控制** — 加热棒、增氧泵、潜水泵、风扇、LED 灯光，支持阈值自动控制与滞回逻辑
- **云端数据同步** — 通过 ESP01S (ESP8266) WiFi 模块连接华为云 IoT 平台，MQTT 协议上报数据与接收远程指令
- **触摸屏交互界面** — 800×480 电容触摸屏 (GT911 控制器)，LVGL 图形库驱动，实时数据展示与参数设置
- **数据持久化存储** — AT24C256 EEPROM 保存控制阈值，W25Q64 QSPI Flash 存储中文字库
- **FreeRTOS 多任务架构** — 5 个并发任务，互斥锁保证线程安全

## 硬件架构

### 主控

| 项目 | 规格 |
|------|------|
| MCU | STM32H743IIT6 (ARM Cortex-M7) |
| 主频 | 480 MHz |
| 内部 Flash | 2 MB |
| 内部 RAM | 864 KB |
| 外部 SDRAM | 32 MB (FMC 接口，用于 LCD 帧缓冲) |

### 传感器

| 传感器 | 接口 | 测量参数 |
|--------|------|----------|
| DS18B20 | 1-Wire (PA4 GPIO) | 水温 (-55~125°C, ±0.5°C) |
| BMP280 | I2C2 (地址 0x76) | 气压 (300~1100 hPa)、环境温度 |
| 浊度传感器 | ADC1 INP6 (PA6) | 水体浊度 (NTU) |
| TDS 传感器 | ADC1 INP0 (PC0) | 总溶解固体 (ppm) |
| pH 传感器 | ADC1 INP4 (PC4) | 酸碱度 (0-14) |
| 水位传感器 | GPIO (PA1) | 水位高低 (开关量) |

### 执行器

| 设备 | 控制方式 | 引脚 | 功能 |
|------|----------|------|------|
| 增氧泵 | TIM8 PWM CH1 | PC6 | 水体增氧，低压自动增强 |
| 潜水泵 | TIM8 PWM CH2 | PC7 | 水体过滤循环 |
| 加热棒 | TIM8 PWM CH3 | PC8 | 水温恒温控制 |
| 风扇 | GPIO | PC9 | 环境降温 |
| LED 灯 | GPIO 继电器 | PA10 | RTC 定时照明 |

### 显示与交互

| 项目 | 规格 |
|------|------|
| LCD | 800×480 RGB TFT (LTDC 驱动) |
| 色深 | RGB565 (16 位) |
| 像素时钟 | 33 MHz (60 Hz 刷新率) |
| 触摸 | GT911 电容触摸 (I2C) |
| 图形加速 | DMA2D 硬件加速 |
| 背光 | PWM 亮度调节 |

### 通信模块

| 项目 | 规格 |
|------|------|
| WiFi 模块 | ESP01S (ESP8266) |
| 通信接口 | USART2 (115200 baud, AT 指令) |
| 云平台 | 华为云 IoT |
| 协议 | MQTT 3.1.1 (端口 1883) |

## 外设使用

| 外设 | 用途 |
|------|------|
| USART1 | 调试串口 (printf 重定向) |
| USART2 | ESP01S WiFi 模块通信 |
| I2C2 | BMP280 气压传感器 |
| I2C4 | AT24C256 EEPROM |
| ADC1 + DMA | 浊度 / TDS / pH 模拟量采集 |
| TIM6 | 微秒级延时 (DS18B20 时序) |
| TIM8 | PWM 输出 (增氧泵/潜水泵/加热棒) |
| RTC | 实时时钟 (LED 定时控制) |
| LTDC | RGB LCD 显示控制 |
| DMA2D | 2D 图形加速 |
| QSPI | W25Q64JV Flash (字库存储，内存映射读取) |
| FMC | 外部 SDRAM (帧缓冲) |

## 软件架构

### FreeRTOS 任务

| 任务 | 函数 | 栈大小 | 周期 | 功能 |
|------|------|--------|------|------|
| MQTT 任务 | `Start_MQTT()` | 4 KB | 10 ms | WiFi/MQTT 连接管理、数据上报、云端指令处理 |
| 传感器任务 | `TaskReadSensorData()` | 4 KB | 100 ms | ADC 采集、DS18B20/BMP280 读取、EEPROM 持久化 |
| UI 管理任务 | `TaskUiManager()` | 2 KB | 一次性 | LVGL 初始化后自删除 |
| LVGL 处理任务 | `TaskLvglHandler()` | 4 KB | 1-30 ms | LVGL 定时器处理、屏幕刷新、触摸输入 |
| 自动控制任务 | `TaskAutoFishCtrl()` | 4 KB | 2-3 s | 设备自动控制逻辑 (加热/水泵/风扇/灯光) |

### 任务间通信

- **消息队列**: `queueMqttMsg` (MQTT 接收消息)、`queueCloudCmd` (云端下行指令)
- **互斥锁**: `mutex_printfHandle` (线程安全 printf)、`mutex_lvglHandle` (LVGL 访问保护)
- **全局变量**: 传感器数据 (`g_Temperature`, `g_TDS_Value`, `g_PH_Value` 等)、设备状态、告警标志

### 目录结构

```
├── Core/
│   ├── Inc/                    # 头文件
│   │   ├── main.h              # GPIO 定义、外设句柄声明
│   │   ├── app_runtime.h       # 全局传感器数据、MQTT 状态
│   │   ├── auto_fish_ctrl.h    # 自动控制阈值与设备状态
│   │   ├── lv_ui.h             # LVGL 界面对象
│   │   ├── AT_MQTT_OS.h        # ESP01S AT 指令封装
│   │   ├── task_mqtt.h         # MQTT 任务接口
│   │   ├── task_sensor.h       # 传感器任务接口
│   │   └── ...                 # 其他外设与传感器驱动头文件
│   └── Src/                    # 源文件
│       ├── main.c              # 入口，外设初始化，FreeRTOS 启动
│       ├── freertos.c          # 任务创建与应用初始化
│       ├── task_mqtt.c         # MQTT 数据上报与云端指令处理
│       ├── task_sensor.c       # 传感器数据采集
│       ├── auto_fish_ctrl.c    # 设备自动控制逻辑
│       ├── device_ctrl.c       # PWM/GPIO 设备驱动
│       ├── lv_ui.c             # LVGL 界面布局与事件处理
│       ├── AT_MQTT_OS.c        # ESP01S AT 指令状态机
│       ├── cJSON.c             # JSON 解析库
│       ├── ds18b20.c           # 温度传感器驱动
│       ├── bmp280function.c    # 气压传感器驱动
│       ├── lcd_rgb.c           # RGB LCD 驱动
│       ├── touch_800x480.c     # GT911 触摸驱动
│       ├── qspi_w25q64.c       # QSPI Flash 驱动
│       ├── AT24C256.c          # EEPROM 驱动
│       └── ...                 # 其他外设驱动与工具文件
├── Drivers/
│   ├── STM32H7xx_HAL_Driver/   # STM32 HAL 库
│   └── CMSIS/                  # ARM CMSIS 核心支持
├── LVGL/                       # LVGL 图形库源码与示例
├── Middlewares/
│   └── Third_Party/FreeRTOS/   # FreeRTOS v10.3.1 内核
├── CMakeLists.txt              # CMake 构建配置
├── CMakePresets.json           # CMake 预设
├── STM32H743XX_FLASH.ld        # 链接脚本
├── startup_stm32h743xx.s       # 启动汇编文件
└── aqua_sentinel.ioc           # STM32CubeMX 工程配置
```

## 构建方法

### 环境要求

- [ARM GCC 工具链](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm) (arm-none-eabi-gcc)
- [CMake](https://cmake.org/) ≥ 3.22
- [Ninja](https://ninja-build.org/) 或 Make

### 编译步骤

```bash
# 配置 (使用 CMake Presets)
cmake --preset default

# 或手动配置
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build build
```

编译产物位于 `build/` 目录，生成 `.elf`、`.hex`、`.bin` 固件文件。

### 烧录

使用 ST-Link 或 J-Link 烧录器，通过 STM32CubeProgrammer 或 OpenOCD 将固件下载到目标板。

## 配置说明

### WiFi 与 MQTT

WiFi 及 MQTT 连接凭据存放在 `mqtt_secrets.h` 文件中 (不纳入版本控制)。请创建该文件并填入以下内容：

```c
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"
#define MQTT_BROKER     "your_mqtt_broker_address"
#define MQTT_PORT       "1883"
#define MQTT_CLIENT_ID  "your_device_id"
#define MQTT_USERNAME   "your_username"
#define MQTT_PASSWORD   "your_password"
```

### 自动控制阈值

设备控制阈值通过触摸屏界面设置，自动保存到 EEPROM。主要参数包括：

- 水温目标值与死区范围
- 浊度触发阈值
- 增氧泵工作周期
- LED 灯光定时开关时间

## 依赖库

| 库 | 版本 | 用途 |
|----|------|------|
| STM32H7xx HAL | - | 硬件抽象层 |
| CMSIS | v5.x | ARM Cortex-M 核心支持 |
| FreeRTOS | v10.3.1 | 实时操作系统 |
| LVGL | v8.x | 图形用户界面库 |
| cJSON | - | JSON 解析 (嵌入式) |

## 许可证

本项目基于 [MIT License](LICENSE.txt) 开源。Copyright © 2026 Temperature6。
