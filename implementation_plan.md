# STM32 鱼缸自动控制任务实施计划

## 问题概述

为 STM32H743 鱼缸工程添加 **自动控制任务 (`AutoFishCtrl`)**，根据传感器反馈数据与阈值的比较来自动控制执行设备，同时追踪设备运行状态并上报华为云。

**五个执行设备**：LED照明灯、增氧泵、潜水泵（过滤）、加热器（水泥电阻）、风扇

**传感器**：水温（DS18B20）、浊度、TDS、PH、大气压（BMP280）、水位

**特殊逻辑**：
- 无溶解氧传感器 → 增氧泵采用 **定时开关 + 低气压加速** 策略
- 无换水设备 → TDS/PH 超阈值仅在屏幕上 **报警提示**
- 上电时使用一套 **系统默认阈值**

---

## Proposed Changes

### 1. 新模块：auto_fish_ctrl

#### [NEW] [auto_fish_ctrl.h](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Inc/auto_fish_ctrl.h)

定义：
- **阈值结构体** `FishCtrl_Thresholds_t`：温度目标/死区、浊度开启/关闭阈值、TDS上限、PH上下限、气压低阈值、增氧泵开/关时间、LED开关时段
- **设备状态全局变量**（`extern`）：
  - `g_heater_state`（0/1）
  - `g_fan_state`（0/1）
  - `g_oxygenpump_state`（0/1）
  - `g_submersiblepump_state`（0/1）
  - `g_led_state`（0/1）
- **屏幕报警标志**（`extern`）：
  - `g_alarm_tds_high`、`g_alarm_ph_high`、`g_alarm_ph_low`
- `FishCtrl_GetThresholds()` 和 `FishCtrl_InitDefaultThresholds()` 函数声明

#### [NEW] [auto_fish_ctrl.c](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/auto_fish_ctrl.c)

实现：
- **默认阈值初始化** `FishCtrl_InitDefaultThresholds()`：
  - 温度目标 26°C，死区 ±0.5°C
  - 浊度开启 50 NTU，浊度关闭 20 NTU
  - TDS 上限 500 ppm
  - PH 范围 6.5~8.5
  - 气压低阈值 99000 Pa（990 hPa）
  - 增氧泵默认开 5 分钟，关 30 分钟
  - LED 照明时间 08:00~20:00
- 获取阈值函数 `FishCtrl_GetThresholds()`

> [!NOTE]
> 阈值参数与 [lv_ui.c](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/lv_ui.c) 中已有的 `k_fallback_settings` 保持一致。后续可以通过 UI 编辑或云端下发来更新阈值。

---

### 2. 修改 device_ctrl

#### [MODIFY] [device_ctrl.h](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Inc/device_ctrl.h)

- 添加 `#include <stdbool.h>` 和获取各设备运行状态的查询函数声明

#### [MODIFY] [device_ctrl.c](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/device_ctrl.c)

- 在每个控制函数中，控制设备开/关时同步更新对应的全局设备状态变量
- 例如：[OxygenPump_Ctrl(1)](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/device_ctrl.c#20-31) → `g_oxygenpump_state = 1`

---

### 3. 修改 FreeRTOS 任务管理

#### [MODIFY] [freertos.c](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/freertos.c)

**新增内容**：
- `#include "auto_fish_ctrl.h"`
- 新任务定义 `AutoFishCtrlHandle` + `AutoFishCtrl_attributes`（优先级 `osPriorityNormal`，栈 `1024*4`）
- 新任务函数 `TaskAutoFishCtrl(void *argument)`，核心控制逻辑：
  1. 调用 `FishCtrl_InitDefaultThresholds()` 初始化阈值
  2. 进入无限循环（周期约 2~3 秒），执行以下判断：

  **加热器控制**（回滞/死区逻辑）：
  - 水温 < 目标温度 - 死区 → 开启加热器
  - 水温 > 目标温度 + 死区 → 关闭加热器

  **风扇控制**（基于 BMP280 环境温度）：
  - 环境温度 > 30°C → 开启风扇散热
  - 环境温度 < 28°C → 关闭风扇

  **潜水泵（过滤）控制**（基于浊度）：
  - 浊度 > turbidity_on_thresh → 开启潜水泵
  - 浊度 < turbidity_off_thresh → 关闭潜水泵

  **增氧泵控制**（定时 + 气压策略）：
  - 使用 `HAL_GetTick()` 实现定时开关循环
  - 当气压低于 `pressure_low_thresh_pa` 时，将关闭间隔减半（低气压水中溶氧降低需增加增氧）

  **LED 照明控制**（基于 RTC 时间）：
  - 读取 RTC 当前小时
  - 在 `led_hour_on ~ led_hour_off` 时段内自动开灯
  - 其他时段自动关灯

  **TDS / PH 报警**（仅设置报警标志，不控制设备）：
  - TDS > tds_high_thresh → 设 `g_alarm_tds_high = 1`
  - PH > ph_high_thresh  → 设 `g_alarm_ph_high = 1`
  - PH < ph_low_thresh   → 设 `g_alarm_ph_low = 1`

**删除/替换**：
- 移除 [TaskReadSensorData](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/freertos.c#550-715) 中开机时的硬编码全开设备调用（第 629~632 行的 [OxygenPump_Ctrl(1)](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/device_ctrl.c#20-31) 等），由自动控制任务来决定设备开关

**在 [MX_FREERTOS_Init](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/freertos.c#235-287) 中**：
- 注册新任务 `osThreadNew(TaskAutoFishCtrl, ...)`

---

### 4. 修改 MQTT 上报

#### [MODIFY] [freertos.c](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/freertos.c)（Start_MQTT 函数）

在现有 `payload_fmt` 模板中增加 5 个设备状态字段：

```
\\,\\\"led_state\\\":%d
\\,\\\"heater_state\\\":%d
\\,\\\"fan_state\\\":%d
\\,\\\"oxygenpump_state\\\":%d
\\,\\\"submersiblepump_state\\\":%d
```

对应华为云属性列表中的 `led_state`、`heater_state`、`fan_state`、`oxygenpump_state`、`submersiblepump_state`，值为 0（未运行）或 1（正在运行）。

---

### 5. 更新 UI 显示

#### [MODIFY] [lv_ui.c](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/lv_ui.c)

- 在 [ui_get_runtime()](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/lv_ui.c#695-766) 函数中，将设备状态从 [ui_is_tim8_channel_enabled()](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/lv_ui.c#214-232) 改为直接读取全局变量 `g_heater_state` / `g_fan_state` / `g_oxygenpump_state` / `g_submersiblepump_state` / `g_led_state`
- 同时读取报警标志 `g_alarm_tds_high`、`g_alarm_ph_high`、`g_alarm_ph_low` 来更新告警显示

> [!IMPORTANT]
> PWM 占空比按照用户在 [device_ctrl.c](file:///d:/Fish_tank/h743_demo_clouddata_app/Core/Src/device_ctrl.c) 中已有的 100% 设置不做修改。

---

## Verification Plan

### Manual Verification

由于这是 STM32 嵌入式工程（使用 Keil/STM32CubeIDE 编译），无法在 PC 上运行单元测试。需要用户在硬件上验证：

1. **编译验证**：请用 STM32CubeIDE / Keil 编译工程，确认无编译错误
2. **上电验证**：观察串口日志，确认 `AutoFishCtrl` 任务正常启动并输出控制日志
3. **温度控制验证**：将 DS18B20 放入冷水/热水，观察加热器是否按阈值自动开关
4. **增氧泵定时验证**：观察增氧泵是否按 5 分钟开 / 30 分钟关的周期运行
5. **MQTT 上报验证**：在华为云 IoTDA 控制台查看 `heater_state`、`fan_state` 等属性是否正确上报为 0/1
6. **屏幕报警验证**：当 TDS 或 PH 超阈值时，观察 LCD 是否显示报警信息
