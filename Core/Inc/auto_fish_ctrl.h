/**
 * @file   auto_fish_ctrl.h
 * @brief  自动鱼缸控制——阀值结构体、设备状态、报警标志声明
 */

#ifndef __AUTO_FISH_CTRL_H__
#define __AUTO_FISH_CTRL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============ 自动控制阈值结构体 ============ */
typedef struct {
  float temp_upper_limit;        // 温度上限 (°C)
  float temp_lower_limit;        // 温度下限 (°C)
  float turbidity_on_thresh;     // 浊度开启潜水泵阈值 (NTU)
  float turbidity_off_thresh;    // 浊度关闭潜水泵阈值 (NTU)
  float tds_high_thresh;         // TDS 报警上限 (ppm)
  float ph_low_thresh;           // PH 报警下限
  float ph_high_thresh;          // PH 报警上限
  float pressure_low_thresh_pa;  // 气压低阈值 (Pa), 低于此值增氧泵加速
  uint32_t aeration_on_ms;       // 增氧泵开启持续时间 (ms)
  uint32_t aeration_off_ms;      // 增氧泵关闭间隔时间 (ms)
  uint8_t led_hour_on;           // LED 开灯时间 (小时, 24h制)
  uint8_t led_hour_off;          // LED 关灯时间 (小时, 24h制)
} FishCtrl_Thresholds_t;

/* ============ 自动控制总开关 (1=自动, 0=手动) ============ */
extern volatile uint8_t g_auto_ctrl_enabled;

/* ============ 全局设备运行状态 (0=未运行, 1=正在运行) ============ */
extern volatile uint8_t g_led_state;
extern volatile uint8_t g_heater_state;
extern volatile uint8_t g_fan_state;
extern volatile uint8_t g_oxygenpump_state;
extern volatile uint8_t g_submersiblepump_state;

/* ============ 屏幕报警标志 (0=正常, 1=报警) ============ */
extern volatile uint8_t g_alarm_tds_high;
extern volatile uint8_t g_alarm_ph_high;
extern volatile uint8_t g_alarm_ph_low;

/* ============ 函数声明 ============ */

/**
 * @brief  初始化系统默认阈值 (上电时调用一次)
 */
void FishCtrl_InitDefaultThresholds(void);

/**
 * @brief  获取当前阈值的指针 (只读)
 * @return 指向内部阈值结构体的常量指针
 */
const FishCtrl_Thresholds_t *FishCtrl_GetThresholds(void);

/**
 * @brief  获取当前阈值的可写指针 (供 UI 编辑)
 * @return 指向内部阈值结构体的可写指针
 */
FishCtrl_Thresholds_t *FishCtrl_GetThresholdsMut(void);

/**
 * @brief  线程安全地将阈值拷贝到调用者提供的缓冲区
 */
void FishCtrl_CopyThresholds(FishCtrl_Thresholds_t *dst);

/**
 * @brief  线程安全地用调用者提供的值更新内部阈值
 */
void FishCtrl_UpdateThresholds(const FishCtrl_Thresholds_t *src);

/**
 * @brief  将当前阈值持久化到 EEPROM
 */
void FishCtrl_SaveToEEPROM(void);

/**
 * @brief  从 EEPROM 恢复阈值（无有效数据则保持默认值）
 */
void FishCtrl_LoadFromEEPROM(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUTO_FISH_CTRL_H__ */
