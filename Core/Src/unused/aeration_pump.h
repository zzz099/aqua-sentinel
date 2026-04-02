#ifndef __AERATION_PUMP_H__
#define __AERATION_PUMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tb6612fng.h"
#include <stdbool.h>

/* 增氧泵使用第一块 TB6612FNG 的 Motor A 通道 (PWMA → PC6, TIM8_CH1) */
#define AERATION_MOTOR_CHANNEL TB6612_MOTOR_A
#define AERATION_DEFAULT_SPEED 100 /* 占空比: 100=运行, 0=停止 */

/**
 * @brief  初始化增氧泵 (需在潜水泵初始化之后调用, 共享同一 TB6612 芯片)
 * @param  drv  已初始化的 TB6612 句柄指针 (由 submersible_pump_init 创建)
 */
void aeration_pump_init(TB6612_Handle_t *drv);

/**
 * @brief  启动增氧泵
 */
void start_aeration_pump(void);

/**
 * @brief  关闭增氧泵
 */
void off_aeration_pump(void);

/**
 * @brief  获取增氧泵当前状态
 */
bool aeration_pump_is_on(void);

#ifdef __cplusplus
}
#endif

#endif /* __AERATION_PUMP_H__ */
