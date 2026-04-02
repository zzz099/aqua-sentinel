# STM32H743 鱼缸控制系统复查结果

本次复查基于 `Core/Src`、`Core/Inc` 中当前实际运行路径做了二次确认，重点看了 MQTT 通信、任务并发、自动控制、UI 状态同步、ADC DMA 采样和 EEPROM 存储。下面只保留我认为值得优先处理的问题，避免重复记录纯样式类意见。

## 总结

- 最高优先级问题集中在 MQTT 接收链路：异常报文防护不足、`cJSON` 生命周期管理错误、AT 响应队列与云端下行复用。
- 控制链路里有几处“状态源分裂”问题：LED 和增氧泵的实际状态、UI 展示状态、自动控制状态并不总是一致。
- UI 设置页已经允许改阈值，但部分运行时逻辑仍然使用写死常量，导致“改了阈值，告警逻辑却没变”。

## P0 / 必须先修

### 1. MQTT 下发解析缺少边界检查，异常报文可直接触发空指针解引用

- 文件：`Core/Src/freertos.c:530-532`
- 现状：
  - `json_text = strstr(subrecv_text, ",{") + 1;`
  - `*(strstr(json_text, "\r\n")) = 0;`
- 问题：
  - 这里默认 `subrecv_text` 一定含有 `",{` 和 `"\r\n"`。
  - 只要 ESP01 返回格式异常、报文被截断、队列中混入其他 AT 行，就可能对 `NULL` 做 `+ 1` 或解引用，直接 HardFault。
- 建议：
  - 先保存 `payload_begin` / `payload_end`，判空后再处理。
  - 异常帧直接丢弃并打印一次错误日志，不要进入 JSON 解析。

### 2. `cJSON` 生命周期管理有缺陷，同时存在 stale pointer、double free 和泄漏风险

- 文件：`Core/Src/freertos.c:389`, `Core/Src/freertos.c:544-563`, `Core/Src/freertos.c:627`
- 现状：
  - `root` 在循环外定义，循环内复用。
  - 多个 `continue` 会绕过 `cJSON_Delete(root)`。
  - `cJSON_Delete(root)` 后没有立刻把 `root = NULL`。
- 问题：
  - 解析成功后如果走到 `paras` / `state` / `command_name` 校验失败分支，会泄漏当前 `root`。
  - 某次成功释放后，如果下一条消息不是 `MQTTSUBRECV`，循环尾部仍可能再次 `cJSON_Delete(root)`，形成对旧指针的重复释放。
- 建议：
  - 每次进入消息处理前先 `root = NULL`。
  - 用单出口清理模式处理，例如 `goto cleanup`。
  - 在 `cleanup` 中统一 `cJSON_Delete(root); root = NULL;`。

### 3. `MQTT_HandleRequestID` 有明确的越界写，而且没有校验 request_id 长度

- 文件：`Core/Src/AT_MQTT_OS.c:379-394`
- 现状：
  - `request_id_len = MQTT_REQUEST_ID_LEN + 1`
  - `request_id[request_id_len] = 0;`
- 问题：
  - 这里写的是索引 `37`，但分配长度也是 `37`，有效索引只到 `36`，属于典型 off-by-one。
  - 同时 `memcpy(..., request_id_len - 1)` 直接拷 36 字节，没有确认源串是否真的有 36 字节。
- 建议：
  - 改成 `request_id[request_id_len - 1] = '\0';`
  - 在 `memcpy` 前校验 `request_id=` 后面的剩余长度，并只拷贝到下一个分隔符。

## P1 / 高优先级

### 4. 同一个队列同时承载“同步 AT 响应”和“异步云端下发”，`xQueueReset()` 会丢消息

- 文件：`Core/Src/AT_MQTT_OS.c:72-76`, `Core/Src/AT_MQTT_OS.c:284-302`, `Core/Src/AT_MQTT_OS.c:503-520`
- 现状：
  - `queueMqttMsg` 既装 ESP01 的 AT 返回行，也装 `MQTTSUBRECV` 下发命令。
  - `MQTT_RecoverUART()` 和 `MQTT_SendRetCmd()` 都会直接 `xQueueReset(queueMqttMsg)`。
  - UART ISR 中 `xQueueSendFromISR()` 的返回值没有检查，队列满时会静默丢帧。
- 问题：
  - 心跳、WiFi 检查、上报确认、NTP 请求过程中，只要有云端命令同时到达，就可能被 reset 掉。
  - 这会表现成“云端偶发收不到命令”“某些 ACK/下发莫名消失”，非常难排查。
- 建议：
  - 至少拆成两个逻辑通道：AT 命令响应队列、异步下发消息队列。
  - 更稳妥的做法是用单一串口接收状态机，把行分类后再投递到不同队列。

### 5. 心跳失败后没有真正回到初始化流程，MQTT 任务会永久卡在降级状态

- 文件：`Core/Src/freertos.c:417-429`, `Core/Src/freertos.c:447`, `Core/Src/freertos.c:513`
- 现状：
  - 心跳失败后只把 `status = HAL_ERROR`，然后 `MQTT_RecoverUART()` + `osDelay(3000)`。
  - 后续上传逻辑都以 `status == HAL_OK` 为前提。
- 问题：
  - 一旦进入 `HAL_ERROR`，当前任务没有重新调用 `MQTT_Init()` 建链。
  - 结果是 MQTT 上传长期停摆，只剩下局部 UART 恢复。
- 建议：
  - 把“建链成功后的主循环”改成可重入状态机，掉线后跳回完整 init 流程。
  - 至少在心跳失败后重新执行：AT 同步、WiFi、MQTTCONN、订阅恢复。

### 6. 共享运行时状态和阈值缺少同步，`double cloud_Temperature` 风险尤其明显

- 文件：`Core/Src/freertos.c:117-124`, `Core/Src/freertos.c:733-753`, `Core/Src/freertos.c:904-908`, `Core/Src/lv_ui.c:767-790`, `Core/Src/auto_fish_ctrl.c:18-47`
- 现状：
  - 传感器任务写全局值，MQTT/UI/自动控制任务并发读取。
  - 阈值结构体通过 `FishCtrl_GetThresholdsMut()` 直接暴露给 UI 改写。
- 问题：
  - `float` 在 Cortex-M7 上通常问题不大，但 `double cloud_Temperature` 不是天然原子。
  - UI 修改阈值的过程中，自动控制任务可能读到一半旧值、一半新值。
- 建议：
  - 最小改动方案：把 `cloud_Temperature` 改成 `float`，并对阈值访问做“整结构体拷贝”。
  - 更稳妥的方案：给运行时快照和阈值配置分别加 mutex。

### 7. 自动/手动控制存在状态机脱节，增氧泵最明显

- 文件：`Core/Src/freertos.c:886-1008`, `Core/Src/freertos.c:592-597`, `Core/Src/lv_ui.c:1185-1192`
- 现状：
  - `TaskAutoFishCtrl` 用局部变量 `aeration_running` 管理增氧泵时序。
  - UI 手动控制和云端控制会直接调用 `OxygenPump_Ctrl()`。
  - 自动模式关闭时，时序状态机只是暂停，但不会和实际设备状态重新对齐。
- 问题：
  - 手动或云端改过增氧泵后，`aeration_running` 仍保留旧值。
  - 重新打开自动模式后，状态机可能立刻反向覆盖用户操作，或者基于错误状态继续计时。
- 建议：
  - 手动/云控操作执行后，要同步更新自动控制状态机的参考状态和时间戳。
  - 或者在切回自动模式时显式重建一次状态机状态。

## P2 / 中优先级

### 8. UI 告警逻辑没有使用当前阈值，而是仍然依赖写死 fallback 常量

- 文件：`Core/Src/lv_ui.c:173-186`, `Core/Src/lv_ui.c:776-780`
- 现状：
  - 设置页修改的是 `FishCtrl_Thresholds_t`。
  - 运行时告警判断却仍然使用 `k_fallback_settings.tds_high_thresh / ph_high_thresh / ph_low_thresh`。
- 问题：
  - 用户改了 TDS / pH 阈值后，自动控制和设置页显示的是新值，但概览页告警判断还是旧值。
  - 这会造成“设置已修改但 UI 告警不一致”的明显功能错乱。
- 建议：
  - 告警判断统一读 `FishCtrl_GetThresholds()`。
  - `k_fallback_settings` 如果不再承担真实逻辑，应移除或只保留 demo 数据用途。

### 9. LED 状态有两套来源，云端控制路径不会同步 `g_led_state`

- 文件：`Core/Src/freertos.c:570-576`, `Core/Src/freertos.c:1025-1031`, `Core/Src/aquarium_light.c:3-27`
- 现状：
  - `aquarium_light.c` 维护 `s_light_on`。
  - 自动控制和 MQTT 上报依赖 `g_led_state`。
  - 云端 `led_ctrl` 只调用 `aquarium_light_on/off()`，没有更新 `g_led_state`。
- 问题：
  - UI 看的是 `s_light_on`，自动控制看的是 `g_led_state`，两者可能分叉。
  - 云端开关 LED 后，MQTT 上报和定时控制逻辑可能还以为灯保持旧状态。
- 建议：
  - 统一一个状态源。
  - 最直接的修法是在 `aquarium_light_on/off()` 内部同步 `g_led_state`。

### 10. `OxygenPump_Ctrl(0)` 只把比较值拉到 0，没有停 PWM；UI 会把它误判为“仍在运行”

- 文件：`Core/Src/device_ctrl.c:75-84`, `Core/Src/lv_ui.c:788`
- 现状：
  - `OxygenPump_Ctrl()` 无论开关都先 `HAL_TIM_PWM_Start()`。
  - UI 的 `rt->aeration_on` 通过 `ui_is_tim8_channel_enabled(TIM_CHANNEL_1)` 判断。
- 问题：
  - 第一次开过增氧泵后，通道使能位会一直保持打开。
  - 即使占空比已经是 0，UI 仍可能显示“增氧泵开启”。
- 建议：
  - 关闭时调用 `HAL_TIM_PWM_Stop()`，或改成按比较值与状态变量联合判断。
  - 如果保留“PWM 打开 + CCR=0”模式，UI 就不能再只看 `CCER` 位。

### 11. `sprintf` 仍在多个 AT/MQTT 路径中使用，固定缓冲区存在溢出风险

- 文件：`Core/Src/AT_MQTT_OS.c:254`, `Core/Src/AT_MQTT_OS.c:331`, `Core/Src/AT_MQTT_OS.c:350`, `Core/Src/AT_MQTT_OS.c:367`, `Core/Src/AT_MQTT_OS.c:397`, `Core/Src/AT_MQTT_OS.c:401`
- 问题：
  - `TempBuff` 虽然有 512 字节，但 SSID、密码、payload、topic 都是变量长度。
  - 当前代码对格式化结果没有边界保护。
- 建议：
  - 全部切换为 `snprintf`。
  - 对返回值做长度检查，超限时直接返回错误。

### 12. ADC DMA 缓存失效和电压计算被重复做了多次

- 文件：`Core/Src/freertos.c:741-752`, `Core/Src/turbidity.c:14-18`, `Core/Src/turbidity.c:33`, `Core/Src/tds.c:16-19`, `Core/Src/ph_sensor.c:72-73`
- 现状：
  - 一个采样周期里，浊度电压、浊度 NTU、TDS、PH 都会各自失效一次 D-Cache。
  - `Turbidity_Get_NTU_DMA()` 还会再次调用 `Turbidity_Get_Voltage_DMA()`。
- 问题：
  - 功能上没错，但同一块 DMA buffer 在一个周期里被重复失效和遍历。
  - 对 H7 这种 cache 体系来说，这类重复操作没有必要。
- 建议：
  - 在 `TaskReadSensorData` 里统一失效一次 cache，再分发各通道结果。
  - 浊度电压和 NTU 可以共用同一次均值结果。

### 13. EEPROM 写入路径按字节写 + 每字节延时，当前实现每分钟都会执行一次较重的阻塞写

- 文件：`Core/Src/AT24C256.c:103-106`, `Core/Src/freertos.c:778-788`
- 现状：
  - `AT24C_WriteArray()` 最终逐字节调用 `AT24C_WriteByte()`。
  - `AT24C_WriteByte()` 每次都会 `HAL_Delay(AT_REST_TIME)`。
  - 传感器任务每 60 秒写 6 个 `double`，即 48 个字节。
- 问题：
  - 这意味着一次保存至少有数百毫秒级的阻塞时间。
  - 同时 EEPROM 页写优势完全没用上，写放大和磨损都更重。
- 建议：
  - 恢复页写实现，按页拆包。
  - 如果只是“掉电前留最近数据”，可以只在值变化明显或用户触发时写。

## P3 / 低优先级

### 14. `TaskUiManager` 初始化后空转，占着 8 KB 栈和一个高优先级任务

- 文件：`Core/Src/freertos.c:148-151`, `Core/Src/freertos.c:798-835`
- 问题：
  - UI 初始化完成后，真正的刷新由 `LvglHandler` 和 `lv_timer` 驱动。
  - `TaskUiManager` 后续只是在 `for (;;)` 中 `osDelay(500)`。
- 建议：
  - 初始化完成后自删任务。
  - 或者把 UI 定时更新职责明确迁入这个任务，避免空转。

### 15. ADC 采样参数在多个文件重复定义，后续改硬件配置时容易失配

- 文件：`Core/Src/freertos.c:73-76`, `Core/Inc/turbidity.h:11-13`, `Core/Src/tds.c:3-5`, `Core/Inc/ph_sensor.h:10-13`
- 问题：
  - `ADC_CHANNEL_COUNT`、样本数、DMA buffer 长度分散在多个文件。
  - 只要 ADC 顺序或样本数变了，极容易出现某个模块忘改的问题。
- 建议：
  - 收敛到单一头文件，例如 `adc_runtime_config.h`。

### 16. MQTT/WiFi/云平台凭据仍然硬编码在头文件中

- 文件：`Core/Inc/AT_MQTT_OS.h:10-21`
- 问题：
  - 这部分如果进入公开仓库或被分享给他人，就是直接泄露。
- 建议：
  - 拆到本地私有配置头文件，纳入 `.gitignore`。
  - 至少在提交前做一层占位符替换。

## 我认为最值得先做的 5 件事

1. 修掉 MQTT 异常报文解析的空指针风险和 `cJSON` 清理逻辑。
2. 修掉 `MQTT_HandleRequestID()` 越界写，并把所有 `sprintf` 改为 `snprintf`。
3. 重构 `queueMqttMsg` 的用法，不再让同步 AT 响应和异步下发共用一个 reset 型队列。
4. 统一 LED / 增氧泵 / UI / 自动控制的状态源，去掉“一个设备两套状态”的情况。
5. 让 UI 告警逻辑和自动控制逻辑都基于同一份阈值配置。

