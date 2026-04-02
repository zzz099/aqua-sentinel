
#ifndef __SUBMERSIBLE_PUMP_H__
#define __SUBMERSIBLE_PUMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tb6612fng.h"
#include <stdbool.h>

/* 潜水泵使用第一块 TB6612FNG 的 Motor B 通道 (PWMB → PC7, TIM8_CH2) */
#define PUMP_MOTOR_CHANNEL   TB6612_MOTOR_B
#define PUMP_DEFAULT_SPEED   100   /* 占空比 0~100 (%) */

/**
 * @brief  初始化潜水泵 (内部完成第一块 TB6612FNG 配置与初始化)
 * @param  htim  PWM 定时器句柄 (当前工程使用 &htim8)
 * @note   调用前需确保 TIM 已由 CubeMX 初始化完毕
 */
void submersible_pump_init(TIM_HandleTypeDef *htim);

/**
 * @brief  获取内部 TB6612 句柄 (供增氧泵共享同一芯片)
 */
TB6612_Handle_t *submersible_pump_get_driver(void);

/**
 * @brief  启动潜水泵 (默认满速)
 */
void start_submersible_pump(void);

/**
 * @brief  关闭潜水泵
 */
void off_submersible_pump(void);

/**
 * @brief  获取潜水泵当前状态
 */
bool submersible_pump_is_on(void);

#ifdef __cplusplus
}
#endif

#endif /* __SUBMERSIBLE_PUMP_H__ */
