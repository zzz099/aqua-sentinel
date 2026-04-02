#ifndef __COOLING_FAN_H__
#define __COOLING_FAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tb6612fng.h"
#include <stdbool.h>

/* 风扇使用第二块 TB6612FNG 的 Motor B 通道 (PWMB → PC9, TIM8_CH4) */
#define COOLING_FAN_MOTOR_CHANNEL   TB6612_MOTOR_B
#define COOLING_FAN_DEFAULT_SPEED   100  /* 占空比 0~100 (%) */

/**
 * @brief  初始化风扇 (需在水泥电阻初始化之后调用, 共享同一 TB6612 芯片)
 * @param  drv  已初始化的 TB6612 句柄指针 (由 cement_resistor_init 创建)
 */
void cooling_fan_init(TB6612_Handle_t *drv);

/**
 * @brief  启动风扇 (PWM=100%)
 */
void start_cooling_fan(void);

/**
 * @brief  关闭风扇 (PWM=0%)
 */
void off_cooling_fan(void);

/**
 * @brief  获取风扇当前状态
 */
bool cooling_fan_is_on(void);

#ifdef __cplusplus
}
#endif

#endif /* __COOLING_FAN_H__ */
