/**
 * @file   app_runtime.h
 * @brief  全局运行时变量 & 互斥锁声明
 *
 * 所有任务共享的传感器数据、MQTT 状态、互斥锁及线程安全宏均在此头文件统一声明。
 */

#ifndef __APP_RUNTIME_H__
#define __APP_RUNTIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os.h"
#include "bmp280function.h"

/* ============ 全局传感器变量 ============ */
extern float g_Turbidity_NTU;
extern float g_Turbidity_Voltage;
extern float g_TDS_Value;
extern float g_PH_Value;
extern short g_Temperature;
extern float cloud_Temperature;
extern float g_BMP280_Temperature;
extern float g_BMP280_Pressure;
extern BMP280ObjectType g_bmp280;

/* ============ MQTT 状态 ============ */
extern int g_MQTT_Status;               /* -1=WAIT, 0=OK, 1=ERROR */
extern uint8_t g_WiFi_Connected;

/* ============ 互斥量 ============ */
extern osMutexId_t mutex_printfHandle;
extern osMutexId_t mutex_lvglHandle;

/* ============ 线程安全 printf ============ */
#define osPrintf(...)                                                          \
  do {                                                                         \
    osMutexAcquire(mutex_printfHandle, osWaitForever);                         \
    printf(__VA_ARGS__);                                                       \
    osMutexRelease(mutex_printfHandle);                                        \
  } while (0)

/* ============ LVGL 锁 ============ */
bool app_lvgl_lock_timeout(uint32_t timeout_ms);
void app_lvgl_lock(void);
void app_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_RUNTIME_H__ */
