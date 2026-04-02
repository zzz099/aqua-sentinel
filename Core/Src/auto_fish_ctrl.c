/**
 * @file   auto_fish_ctrl.c
 * @brief  自动鱼缸控制 —— 阀值管理 / EEPROM 持久化 / 设备状态变量
 *
 * 功能概述：
 *   - 维护自动控制阀值结构体（线程安全读写）
 *   - 阀值持久化到 EEPROM，上电后恢复
 *   - 全局设备状态变量及报警标志定义
 */

#include "auto_fish_ctrl.h"
#include "AT24C256.h"
#include <cmsis_os2.h>
#include <string.h>
#include <math.h>

/* ============ 自动控制总开关 (1=自动, 0=手动; 上电默认自动) ============ */
volatile uint8_t g_auto_ctrl_enabled     = 1;

/* ============ 全局设备运行状态变量定义 ============ */
volatile uint8_t g_led_state             = 0;
volatile uint8_t g_heater_state          = 0;
volatile uint8_t g_fan_state             = 0;
volatile uint8_t g_oxygenpump_state      = 0;
volatile uint8_t g_submersiblepump_state = 0;

/* ============ 屏幕报警标志变量定义 ============ */
volatile uint8_t g_alarm_tds_high = 0;
volatile uint8_t g_alarm_ph_high  = 0;
volatile uint8_t g_alarm_ph_low   = 0;

/* ============ 内部阈值实例 ============ */
static FishCtrl_Thresholds_t s_thresholds;
static osMutexId_t s_thresholds_mutex = NULL;
static const osMutexAttr_t s_thresholds_mutex_attr = {
  .name = "thresholdsMtx"
};

/**
 * @brief  初始化系统默认阈值 (上电时调用一次)
 */
void FishCtrl_InitDefaultThresholds(void) {
  if (s_thresholds_mutex == NULL) {
    s_thresholds_mutex = osMutexNew(&s_thresholds_mutex_attr);
  }
  s_thresholds.temp_upper_limit       = 23.0f;      // 水温上限 23°C
  s_thresholds.temp_lower_limit       = 20.0f;      // 水温下限 20°C
  s_thresholds.turbidity_on_thresh    = 50.0f;      // 浊度 >50 NTU 开过滤
  s_thresholds.turbidity_off_thresh   = 20.0f;      // 浊度 <20 NTU 关过滤
  s_thresholds.tds_high_thresh        = 500.0f;     // TDS >500 ppm 报警
  s_thresholds.ph_low_thresh          = 6.5f;       // PH <6.5 报警
  s_thresholds.ph_high_thresh         = 8.5f;       // PH >8.5 报警
  s_thresholds.pressure_low_thresh_pa = 99000.0f;   // 气压 <990 hPa 视为低气压
  s_thresholds.aeration_on_ms         = 300000U;    // 增氧泵开 5 分钟
  s_thresholds.aeration_off_ms        = 1800000U;   // 增氧泵关 30 分钟
  s_thresholds.led_hour_on            = 8U;         // 08:00 开灯
  s_thresholds.led_hour_off           = 20U;        // 20:00 关灯
}

/**
 * @brief  获取当前阈值的指针 (只读)
 */
const FishCtrl_Thresholds_t *FishCtrl_GetThresholds(void) {
  return &s_thresholds;
}

FishCtrl_Thresholds_t *FishCtrl_GetThresholdsMut(void) {
  return &s_thresholds;
}

void FishCtrl_CopyThresholds(FishCtrl_Thresholds_t *dst) {
  if (s_thresholds_mutex) osMutexAcquire(s_thresholds_mutex, osWaitForever);
  memcpy(dst, &s_thresholds, sizeof(FishCtrl_Thresholds_t));
  if (s_thresholds_mutex) osMutexRelease(s_thresholds_mutex);
}

void FishCtrl_UpdateThresholds(const FishCtrl_Thresholds_t *src) {
  if (s_thresholds_mutex) osMutexAcquire(s_thresholds_mutex, osWaitForever);
  memcpy(&s_thresholds, src, sizeof(FishCtrl_Thresholds_t));
  FishCtrl_SaveToEEPROM();
  if (s_thresholds_mutex) osMutexRelease(s_thresholds_mutex);
}

/* ============ EEPROM 阈值持久化 ============ */
#define EEPROM_ADDR_THRESHOLDS  0x0100  /**< 阀值结构体起始地址 */
#define EEPROM_THRESHOLDS_MAGIC 0xA5    /**< 有效性标记字节 */

/**
 * @brief  将当前阀值结构体写入 EEPROM
 * @note   先写数据再写 magic，保证掉电安全：
 *         若写数据期间掉电，magic 不会被更新，下次启动不会误读半写数据。
 */

void FishCtrl_SaveToEEPROM(void) {
  /* 先写结构体内容，再写 magic，保证掉电安全：
   * 如果写结构体期间掉电，magic 不会被更新，下次启动不会误读半写数据 */
  if (AT24C_WriteArray(EEPROM_ADDR_THRESHOLDS + 1,
                       (uint8_t *)&s_thresholds, sizeof(s_thresholds)) != HAL_OK)
    return;
  uint8_t magic = EEPROM_THRESHOLDS_MAGIC;
  AT24C_WriteArray(EEPROM_ADDR_THRESHOLDS, &magic, 1);
}

/**
 * @brief  从 EEPROM 恢复阀值
 * @note   读取后进行完整的有效性校验（NaN / 范围），无效则保持默认值。
 */
void FishCtrl_LoadFromEEPROM(void) {
  uint8_t magic = 0;
  if (AT24C_ReadArray(EEPROM_ADDR_THRESHOLDS, &magic, 1) != HAL_OK)
    return;
  if (magic != EEPROM_THRESHOLDS_MAGIC)
    return;

  FishCtrl_Thresholds_t tmp;
  if (AT24C_ReadArray(EEPROM_ADDR_THRESHOLDS + 1,
                      (uint8_t *)&tmp, sizeof(tmp)) != HAL_OK)
    return;

  /* 完整的有效性校验：浮点值不能是 NaN，且范围需合理 */
  if (isnan(tmp.temp_upper_limit) || isnan(tmp.temp_lower_limit) ||
      isnan(tmp.tds_high_thresh) || isnan(tmp.ph_low_thresh) ||
      isnan(tmp.ph_high_thresh) || isnan(tmp.turbidity_on_thresh) ||
      isnan(tmp.turbidity_off_thresh) || isnan(tmp.pressure_low_thresh_pa))
    return;

  /* 时间区间和阈值上下界合法性检查 */
  if (tmp.led_hour_on >= 24U || tmp.led_hour_off >= 24U)
    return;
  if (tmp.aeration_on_ms == 0U || tmp.aeration_off_ms == 0U)
    return;
  if (tmp.temp_lower_limit < -10.0f || tmp.temp_upper_limit > 50.0f)
    return;
  if (tmp.ph_low_thresh < 0.0f || tmp.ph_high_thresh > 14.0f)
    return;

  if (s_thresholds_mutex) osMutexAcquire(s_thresholds_mutex, osWaitForever);
  memcpy(&s_thresholds, &tmp, sizeof(s_thresholds));
  if (s_thresholds_mutex) osMutexRelease(s_thresholds_mutex);
}
