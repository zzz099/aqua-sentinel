# STM32H743 鱼缸控制系统复查报告

日期：2026-04-02

## 结论

当前代码相对 `final_code_review.md` 中的大部分高优问题已经做了有效修复，尤其是这些点已经明显改善：

- MQTT 下发 JSON 解析已补上 `strstr` 判空，避免了异常帧直接触发空指针解引用。
- `cJSON` 清理路径已经改成单出口，`root` 在清理后会置 `NULL`。
- MQTT 接收链路已经拆成 `queueMqttMsg` 和 `queueCloudCmd` 两条队列，不再把 AT 响应和云端下发混在同一个队列里。
- `cloud_Temperature` 已从 `double` 改为 `float`，降低了 Cortex-M7 上的非原子读写风险。
- UI 告警判断已经改为读取 `FishCtrl_CopyThresholds()` 快照，不再依赖旧的 fallback 常量。
- `aquarium_light_on/off()` 已同步更新 `g_led_state`。
- `OxygenPump_Ctrl(0)` 已改为 `HAL_TIM_PWM_Stop()`，此前 UI 误判的问题已经一起缓解。
- ADC 常量已收敛到 `Core/Inc/adc_config.h`，D-Cache 失效也已移动到采样任务统一处理。
- LED 跨午夜逻辑、`TaskUiManager` 自删、AT24C 页写恢复等修复都已落地。

但这次复查后，仍然确认有 **6 个值得继续处理的问题**。其中前 2 个会直接影响运行时行为，建议优先修。二次核验时又额外发现了 **4 个未覆盖的问题**（编号 7-10），合计 **10 个待处理项**。

---

## 仍存在的问题

### 1. WiFi 掉线后的恢复仍然没有重建 MQTT 会话

- **文件**
  - `Core/Src/task_mqtt.c:128-138`
  - `Core/Src/task_mqtt.c:142-191`
  - `Core/Src/AT_MQTT_OS.c:202-220`
- **现状**
  - 心跳失败时，外层循环会重新走 `MQTT_Init()`，这一点是对的。
  - 但如果只是 `MQTT_GetWiFiState()` 失败，当前代码只调用 `MQTT_ConnectWiFi()` 重新连 WiFi，然后继续上报。
  - 这条路径没有重新执行 `AT+MQTTUSERCFG` / `AT+MQTTCONN` / 订阅恢复。
- **风险**
  - ESP01 恢复到 WiFi 后，MQTT 会话和订阅很可能已经失效，云端上报/下发会继续失败，但任务仍然按“已恢复”继续运行。
- **建议**
  - 把“WiFi 断开”也视为需要完整重建 MQTT 链路的状态，直接跳回 `MQTT_Init()`，而不是只做 `MQTT_ConnectWiFi()`。

### 2. 云端手动控制仍然绕过自动/手动仲裁，自动控制会在 3 秒内覆盖云端操作

- **文件**
  - `Core/Src/task_mqtt.c:234-288`
  - `Core/Src/freertos.c:373-487`
- **现状**
  - UI 手动控制在自动模式下已经被禁止。
  - 但 MQTT 下发仍然会在自动模式开启时直接调用 `Heater_Ctrl()`、`Fan_Ctrl()`、`SubmersiblePump_Ctrl()`、`OxygenPump_Ctrl()`、`aquarium_light_on/off()`。
- **风险**
  - 云端“手动关泵/关灯/关加热”后，`TaskAutoFishCtrl` 仍会按当前自动策略在下一个周期覆盖该操作。
  - 增氧泵最明显，因为自动任务内部还有独立的 `aeration_running` 时序状态机，云端操作不会同步这个状态机。
- **建议**
  - 云端命令到达时，要么显式切到手动模式，要么为每个设备引入“手动覆盖”状态，并同步自动控制状态机。
  - 你当前新增的华为云属性 `device_operating_state` 是一个可行方案，建议定义为：
    - `0 = 自动控制模式`
    - `1 = 手动控制模式`
  - 推荐做法是：
    - 手机端只有在 `device_operating_state = 1` 时才允许下发设备控制命令；
    - 固件端在处理 `led_ctrl` / `fan_ctrl` / `res_ctrl` / `aeration_pump_ctrl` / `submersible_pump_ctrl` 前，仍然必须再次检查当前模式，自动模式下直接拒绝执行；
    - 本地 UI 切换自动/手动时，也要同步上报 `device_operating_state`，避免本地状态和云端显示不一致。
  - 需要特别注意：当前固件里的 `g_auto_ctrl_enabled = 1` 表示“自动模式”，而你新定义的云端属性是 `1 = 手动模式`，两者语义相反，落地时必须显式做一次映射，不能直接把云端值赋给 `g_auto_ctrl_enabled`。

### 3. “Restore defaults” 按钮没有持久化，而且在运行中直接绕过阈值互斥保护

- **文件**
  - `Core/Src/lv_ui.c:677-684`
  - `Core/Src/auto_fish_ctrl.c:32-48`
  - `Core/Src/auto_fish_ctrl.c:67-71`
- **现状**
  - `ui_restore_defaults_event_cb()` 直接调用 `FishCtrl_InitDefaultThresholds()`。
  - 这个函数会直接写 `s_thresholds`，但不会调用 `FishCtrl_UpdateThresholds()`，因此也不会写回 EEPROM。
- **风险**
  - 用户点击“Restore defaults”后，当前界面虽然变成默认值，但重启后仍会从 EEPROM 载入旧阈值。
  - 另外，这个运行时写入路径没有走 `s_thresholds_mutex`，会重新引入阈值并发访问的竞争窗口。
- **建议**
  - 将“恢复默认值”改为先构造一个默认阈值结构体，再调用 `FishCtrl_UpdateThresholds()` 一次性更新并持久化。

### 4. 新增的阈值 EEPROM 存储格式不具备掉电安全性，且校验过弱

- **文件**
  - `Core/Src/auto_fish_ctrl.c:78-82`
  - `Core/Src/auto_fish_ctrl.c:85-102`
- **现状**
  - 保存时先写 `magic`，再写结构体内容。
  - 读取时只校验 `magic` 和少数字段是否为 `NaN`。
- **风险**
  - 如果在写 `magic` 后、写完整个结构体前掉电，下次启动会把一份“半写入”的阈值块当成有效配置。
  - 当前校验没有覆盖 `ph_high_thresh`、`pressure_low_thresh_pa`、`aeration_on_ms`、`aeration_off_ms`、`led_hour_on/off` 等字段，损坏数据可能被直接接受。
- **建议**
  - 至少改成“先写结构体，再写 magic/版本号”。
  - 更稳妥的做法是增加 CRC 或校验和，并对小时范围、时间区间和阈值上下界做完整合法性检查。

### 5. `sprintf` 风险只修了一半：虽然改成了 `snprintf`，但多数调用点仍未检查截断

- **文件**
  - `Core/Src/AT_MQTT_OS.c:265`
  - `Core/Src/AT_MQTT_OS.c:342`
  - `Core/Src/AT_MQTT_OS.c:361`
  - `Core/Src/AT_MQTT_OS.c:378`
  - `Core/Src/AT_MQTT_OS.c:412`
  - `Core/Src/AT_MQTT_OS.c:416`
- **现状**
  - 当前实现已经把不少 `sprintf` 改成了 `snprintf`。
  - 但这些调用点没有检查返回值，发生截断时仍会把被截断的 AT 指令继续发给模块。
- **风险**
  - 这类问题不会再直接造成本地栈/缓冲区溢出，但会变成“命令被悄悄截断后发送”，表现为偶发连接失败、上报失败或 request_id 应答异常。
- **建议**
  - 对所有 `snprintf` 统一检查：`ret < 0 || ret >= sizeof(buf)` 时直接返回 `HAL_ERROR`，不要继续发送。

### 6. UART ISR 仍然会在队列满时静默丢帧

- **文件**
  - `Core/Src/AT_MQTT_OS.c:534`
  - `Core/Src/AT_MQTT_OS.c:538`
- **现状**
  - `xQueueSendFromISR()` 的返回值没有检查。
  - `queueMqttMsg` / `queueCloudCmd` 的长度都只有 5。
- **风险**
  - AT 返回或云端命令突发时，一旦队列打满，当前帧会被直接丢弃，且没有任何日志。
  - 这会表现为“偶发收不到云端命令”或“AT 等待超时”，定位会比较困难。
- **建议**
  - 至少检查 `xQueueSendFromISR()` 返回值并计数/打印丢帧。
  - 如果现场流量比较大，应考虑增大队列长度，或改成环形缓冲区 + 行解析状态机。

---
## 额外发现的问题

以下问题在原始复查中未被覆盖，由二次核验时发现。

### 7. AT24C256 写入操作不检查返回值，EEPROM 写入链路无可靠性保证

- **文件**
  - `Core/Src/AT24C256.c:82`
  - `Core/Src/AT24C256.c:93`
  - `Core/Src/AT24C256.c:105`
  - `Core/Src/AT24C256.c:112-114`（`AT24C_ReadArray` 同样不检查）
- **现状**
  - `AT24C_WriteArray()` 内部连续调用 `Write()`（即 `HAL_I2C_Mem_Write()`），但**完全忽略返回值**。
  - `AT24C_ReadArray()` 返回类型是 `void`，调用者无从得知读取是否成功。
- **风险**
  - 若 I2C 总线出现瞬态故障（应答超时、总线仲裁失败等），`FishCtrl_SaveToEEPROM()` 会静默失败，用户以为阈值已保存但实际上 EEPROM 未写入。
  - 配合问题 4（magic 先写、结构体后写），即使 I2C 只失败一次也可能导致下次启动读回不一致的阈值。
- **建议**
  - `AT24C_WriteArray()` 和 `AT24C_ReadArray()` 改为返回 `HAL_StatusTypeDef`，内部每次 `Write()`/`Read()` 失败时立即返回错误。
  - `FishCtrl_SaveToEEPROM()` / `FishCtrl_LoadFromEEPROM()` 据此处理失败路径。

### 8. `FishCtrl_SaveToEEPROM()` 在 mutex 外调用，可能写入不一致的阈值

- **文件**
  - `Core/Src/auto_fish_ctrl.c:72-75`
- **现状**
  ```c
  void FishCtrl_UpdateThresholds(const FishCtrl_Thresholds_t *src) {
    if (s_thresholds_mutex) osMutexAcquire(s_thresholds_mutex, osWaitForever);
    memcpy(&s_thresholds, src, sizeof(FishCtrl_Thresholds_t));
    if (s_thresholds_mutex) osMutexRelease(s_thresholds_mutex);
    FishCtrl_SaveToEEPROM();   // ← 在 mutex 已释放后才执行
  }
  ```
  - `FishCtrl_SaveToEEPROM()` 直接读取 `s_thresholds`，但此时 mutex 已释放。
- **风险**
  - 如果两个线程几乎同时调用 `FishCtrl_UpdateThresholds()`（例如 UI 编辑 + 云端阈值下发），第二个线程可能在第一个线程 EEPROM 写入期间修改了 `s_thresholds`，导致 EEPROM 写入混合数据。
- **建议**
  - 将 `FishCtrl_SaveToEEPROM()` 调用移到 mutex 保护内；或在 mutex 内先拷贝到临时变量，释放 mutex 后再用临时变量写 EEPROM。

### 9. UART ISR 中调用 `strstr()` 遍历 512 字节缓冲区，影响中断实时性

- **文件**
  - `Core/Src/AT_MQTT_OS.c:533`
- **现状**
  ```c
  if (strstr(RecvBuff, MQTT_SUBRECV_KEYWORD))
  ```
  - 在 UART 接收中断服务函数 `MQTT_HandleUARTInterrupt()` 中，每收到 `'\n'` 时调用 `strstr()` 在最大 512 字节的 `RecvBuff` 中搜索 `"MQTTSUBRECV"` 关键字。
- **风险**
  - `strstr()` 最坏情况需要线性遍历整个缓冲区。ISR 执行时间偏长会延迟其他中断的响应，尤其在高波特率、大负载时可能导致其他外设丢中断。
- **建议**
  - 在 ISR 中只做入队操作，将分流判断移到任务侧。
  - 或者在接收过程中实时匹配关键字前缀（状态机方式），避免在 ISR 中做全缓冲区扫描。

### 10. `MQTT_HandleRequestID()` 对固定长度字符串使用堆分配，长期运行有碎片化风险

- **文件**
  - `Core/Src/AT_MQTT_OS.c:393`
- **现状**
  - 每次收到云端命令时，`MQTT_HandleRequestID()` 都调用 `pvPortMalloc(37)` 分配 `request_id`，用完后 `vPortFree()`。
- **风险**
  - 长期运行（数天/数周），频繁的小块 malloc/free 会导致 FreeRTOS 堆碎片化。虽然 FreeRTOS 堆配置为 131KB，短期内不会耗尽，但属于不必要的风险。
- **建议**
  - 改为栈分配 `char request_id[MQTT_REQUEST_ID_LEN + 1];`，完全消除堆碎片化风险。

---
## 其他备注

- `Save config` 按钮当前只是弹一个 `"Settings saved"` 的 toast，实际不执行保存逻辑：`Core/Src/lv_ui.c:670-675`、`Core/Src/lv_ui.c:1445-1450`。如果设计上是“编辑即自动保存”，建议把按钮文案改掉，避免误导。
- `FishCtrl_GetThresholdsMut()` 仍保留在头文件中：`Core/Inc/auto_fish_ctrl.h:58`。当前虽然没有被调用，但它仍然提供了绕开互斥保护直接改内部结构体的入口，后续最好移除。

---

## 验证说明

- 本次复查以源码静态核对为主，重点检查了 `task_mqtt.c`、`AT_MQTT_OS.c`、`freertos.c`、`lv_ui.c`、`auto_fish_ctrl.c`、`task_sensor.c`、`AT24C256.c` 等当前实际路径。
- 我尝试做本地构建校验，但当前环境下无法完整复现一次新的成功构建：
  - 现有 `build` / `cmake-build-stm32-debug` 目录在 `cmake --build` 时都会报 `CMakeCache.txt` 无法写入（permission denied）。
  - 另起一个全新构建目录时，当前 shell 环境又缺少可直接调用的 `Ninja`。
- 因此，这份报告对“逻辑修复是否正确”的判断是可信的，但不把“我本地重新构建通过”作为结论的一部分。
