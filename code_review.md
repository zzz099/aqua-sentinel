# STM32H743 鱼缸控制系统 - 代码审查与优化建议

## 一、线程安全与并发问题

### 1. 全局传感器变量缺少同步保护
- **文件**: `Core/Src/freertos.c` (L133-L141)
- **问题**: `g_Turbidity_NTU`、`g_TDS_Value`、`g_PH_Value`、`cloud_Temperature` 等全局变量被 `TaskReadSensorData`(写)、`Start_MQTT`(读)、`TaskAutoFishCtrl`(读)、`lv_ui.c`(读) 多个任务同时访问，没有互斥保护。尤其 `double cloud_Temperature` 是 64 位的，在 ARM Cortex-M7 上读写**不是原子的**，可能读到半更新的值。
- **建议**: 使用互斥锁保护，或将 `double` 改为 `float`，或封装带锁的读写函数。

### 2. `FishCtrl_Thresholds_t` 的并发读写未加锁
- **文件**: `Core/Src/auto_fish_ctrl.c`、`Core/Src/lv_ui.c`
- **问题**: `FishCtrl_GetThresholdsMut()` 返回可写指针供 UI 任务修改阈值，同时 `TaskAutoFishCtrl` 持续读取同一结构体。UI 同时修改多个字段时，自动控制任务可能读到不一致的状态。
- **建议**: 加读写锁或使用拷贝快照方式访问。

### 3. `aquarium_light.c` 中 `s_light_on` 与 `g_led_state` 信息冗余
- **文件**: `Core/Src/aquarium_light.c`、`Core/Src/freertos.c`
- **问题**: `aquarium_light.c` 有私有变量 `s_light_on` 追踪灯状态，`auto_fish_ctrl.h` 又有 `g_led_state`。`aquarium_light_on/off` 不会更新 `g_led_state`，需要任务自行维护，容易出现不同步。
- **建议**: 统一使用一个状态变量，或在 `aquarium_light_on/off` 内部同步更新 `g_led_state`。

---

## 二、安全与健壮性问题

### 4. MQTT 凭据硬编码在头文件中
- **文件**: `Core/Inc/AT_MQTT_OS.h` (L12-L17)
- **问题**: WiFi 密码和华为云三元组密钥直接作为 `#define` 硬编码。如果代码上传到公开仓库会泄露凭据。
- **建议**: 将敏感信息提取到单独的配置文件（如 `mqtt_secrets.h`）并加入 `.gitignore`。

### 5. `MQTT_HandleRequestID` 存在缓冲区越界写入
- **文件**: `Core/Src/AT_MQTT_OS.c`
- **问题**: `request_id_len = sizeof(char) * 36 + 1 = 37`，`request_id[request_id_len] = 0` 写入的是第 37 个字节（索引 37），但数组只有 37 字节（索引 0~36），这是一个**越界写入 (off-by-one)**。
- **建议**: 应改为 `request_id[request_id_len - 1] = '\0'`。

### 6. `sprintf` 应替换为 `snprintf`
- **文件**: `Core/Src/AT_MQTT_OS.c`
- **问题**: `MQTT_ConnectWiFi`、`MQTT_ReportIntVal`、`MQTT_ReportDoubleVal` 等函数使用 `sprintf(TempBuff, ...)` 写入固定大小缓冲区，无长度保护，payload 较长时可能溢出。
- **建议**: 统一使用 `snprintf` 并检查返回值。

### 7. cJSON 解析后 `cJSON_Delete(root)` 的位置不当
- **文件**: `Core/Src/freertos.c` (L637)
- **问题**: `cJSON_Delete(root)` 放在 `if (strstr(...MQTT_SUBRECV_KEYWORD))` 块外面，但 `root` 的赋值在 `if` 块内部。如果收到非命令消息，`root` 可能是上一次循环残留的已删除指针或 NULL，再次 `cJSON_Delete(root)` 会导致 **double free**。
- **建议**: 每次解析完立即释放 `root`，并在循环头部重置 `root = NULL`。

---

## 三、资源与性能优化

### 8. ADC DMA Cache 失效操作重复执行
- **文件**: `Core/Src/turbidity.c`、`Core/Src/tds.c`、`Core/Src/ph_sensor.c`
- **问题**: `SCB_InvalidateDCache_by_Addr` 在三个传感器读取函数中各调用一次，而这三个函数在同一个循环周期内依次调用，操作的是同一个 `adc_dma_buffer`。
- **建议**: 在 `TaskReadSensorData` 中读取前统一做一次 Cache Invalidate 即可，各传感器函数内部不再重复调用。

### 9. TDS 传感器使用冒泡排序
- **文件**: `Core/Src/tds.c` (L28-L37)
- **问题**: 对 64 个样本做冒泡排序，时间复杂度 O(n²)。
- **建议**: 改用插入排序（对小数组更快）或快速选择算法找中位数 O(n)。

### 10. DS18B20 的 `delay_us` 使用 TIM6 轮询
- **文件**: `Core/Src/ds18b20.c` (L8-L18)
- **问题**: `delay_us` 直接轮询 TIM6 计数器实现微秒延时，在 FreeRTOS 任务中调用时不会让出 CPU（busy-wait）。
- **建议**: 此为 1-Wire 协议必要操作（微秒级时序），当前任务优先级为 `osPriorityLow` 影响可控。如需优化可适当延长采集间隔减少 CPU 占用。

### 11. `OxygenPump_Ctrl` 每次调用都执行 `HAL_TIM_PWM_Start`
- **文件**: `Core/Src/device_ctrl.c` (L22)
- **问题**: 即使关闭增氧泵（设 compare = 0），也会先调用 `HAL_TIM_PWM_Start`。而 `SubmersiblePump_Ctrl` 和 `Heater_Ctrl` 在关闭时调用 `HAL_TIM_PWM_Stop`。行为不一致。
- **建议**: `OxygenPump_Ctrl` 关闭时也应 `HAL_TIM_PWM_Stop`，确保输出引脚不会有漏电流或意外脉冲。

---

## 四、架构与代码质量

### 12. ADC 通道索引、采样数等常量在多处重复定义
- **文件**: `freertos.c`、`turbidity.h`、`tds.c`、`ph_sensor.h`
- **问题**: `SAMPLES_PER_CHANNEL`、`ADC_CHANNEL_COUNT` 在 4 个文件中各自独立定义。修改 ADC 配置时很容易漏改导致数据错位。
- **建议**: 统一在 `adc.h` 或新建 `adc_config.h` 中定义，其他文件引用。

### 13. `freertos.c` 文件过于臃肿
- **文件**: `Core/Src/freertos.c` (超 1000 行)
- **问题**: 承载了所有任务逻辑（MQTT 循环、传感器采集、UI 管理、LVGL Handler、自动控制）。
- **建议**: 将各任务实现拆分到独立的 `.c` 文件中，`freertos.c` 仅保留任务创建和 RTOS 初始化。

### 14. MQTT 传感器数据上报用整数拼接模拟浮点
- **文件**: `Core/Src/freertos.c` (L460-L475)
- **问题**: 将所有传感器值手动拆分为 `int_part.dec_part` 格式。但 CMakeLists.txt 已链接 `-u _printf_float`，完全可以直接用 `%.1f` 格式化。
- **建议**: 直接使用浮点格式化，简化代码。

### 15. MQTT 心跳检测后未完成重连循环
- **文件**: `Core/Src/freertos.c` (L430-L445)
- **问题**: 心跳失败后只恢复了 UART 并 delay 3 秒，但没有重新调 `MQTT_Init()` 进行完整初始化。`status` 保持 `HAL_ERROR`，后续数据上报被跳过且永远不会恢复。
- **建议**: 检测到 ESP01 掉线后执行完整的重连流程（重新调用 `MQTT_Init`）。

### 16. `lv_ui.c` 中 `ui_fallback_settings_t` 与 `FishCtrl_Thresholds_t` 结构重复
- **文件**: `Core/Src/lv_ui.c` (L130-L143)
- **问题**: `ui_fallback_settings_t` 字段与 `FishCtrl_Thresholds_t` 完全一样，`k_fallback_settings` 默认值也和 `FishCtrl_InitDefaultThresholds()` 一一对应。改阈值需同步修改两处。
- **建议**: 直接复用 `FishCtrl_Thresholds_t`，或移除 fallback 结构体。

---

## 五、功能层面的改进建议

### 17. EEPROM 只存传感器历史，不存阈值配置
- **问题**: 用户在 UI 上修改阈值后，掉电重启会恢复默认值。
- **建议**: 将 `FishCtrl_Thresholds_t` 在修改后持久化到 EEPROM，上电时恢复。

### 18. 自动控制与云端下发命令可能冲突
- **问题**: `TaskAutoFishCtrl` 每 3 秒根据传感器数据控制设备，而 MQTT 任务收到云端命令也直接操作设备。云端手动关了增氧泵，3 秒后自动控制又会开回来。
- **建议**: 收到云端命令时暂时切换为手动模式（`g_auto_ctrl_enabled = 0`），或对单个设备设置手动覆盖标志。

### 19. LED 定时跨午夜的情况未处理
- **文件**: `Core/Src/freertos.c` (L1022)
- **问题**: LED 判断逻辑为 `hour >= led_hour_on && hour < led_hour_off`。如果设置开灯 20:00、关灯 8:00（跨午夜），则逻辑会一直返回 false，灯永远不亮。
- **建议**: 增加 `led_hour_on > led_hour_off` 场景的处理逻辑。

### 20. TaskUiManager 循环体为空
- **文件**: `Core/Src/freertos.c` (L830-L840)
- **问题**: `TaskUiManager` 的 `for(;;)` 循环只有一个 `osDelay(500)` 和注释掉的示例代码。实际 UI 更新由 `lv_ui.c` 内部的 LVGL refresh_timer 驱动，该任务初始化完成后空转浪费 8KB 栈空间。
- **建议**: 初始化完成后删除任务释放栈空间，或将 UI 刷新逻辑迁入此任务。

---

## 优先级建议

| 优先级 | 编号 | 类型 |
|--------|------|------|
| **P0 必须修复** | #5 | 缓冲区越界写入（Bug） |
| **P0 必须修复** | #7 | cJSON double free（Bug） |
| **P1 高优先级** | #1 | 全局变量线程安全 |
| **P1 高优先级** | #15 | MQTT 断线无法重连 |
| **P1 高优先级** | #18 | 自动/手动控制冲突 |
| **P1 高优先级** | #6 | sprintf 缓冲区溢出风险 |
| **P2 中优先级** | #2, #3 | 阈值并发、状态冗余 |
| **P2 中优先级** | #4 | 凭据硬编码 |
| **P2 中优先级** | #17 | 阈值掉电丢失 |
| **P2 中优先级** | #19 | LED 跨午夜 |
| **P3 低优先级** | #8~#14, #16, #20 | 性能优化、架构改进 |
