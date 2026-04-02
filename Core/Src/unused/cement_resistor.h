#ifndef __CEMENT_RESISTOR_H__
#define __CEMENT_RESISTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tb6612fng.h"
#include <stdbool.h>

/* 水泥电阻使用第二块 TB6612FNG 的 Motor A 通道 (PWMA → PC8, TIM8_CH3) */
#define CEMENT_RESISTOR_MOTOR_CHANNEL   TB6612_MOTOR_A
#define CEMENT_RESISTOR_DEFAULT_POWER   100  /* 占空比: 100=运行, 0=停止 */

/**
 * @brief  初始化水泥电阻 (内部完成第二块 TB6612FNG 的配置与初始化)
 * @param  htim  PWM 定时器句柄 (当前工程使用 &htim8)
 * @note   调用前需确保 TIM 已由 CubeMX 初始化完毕
 */
void cement_resistor_init(TIM_HandleTypeDef *htim);

/**
 * @brief  获取第二块 TB6612 句柄 (供风扇共享同一芯片)
 */
TB6612_Handle_t *cement_resistor_get_driver(void);

/**
 * @brief  启动水泥电阻 (PWM=100%)
 */
void start_cement_resistor(void);

/**
 * @brief  关闭水泥电阻 (PWM=0%)
 */
void off_cement_resistor(void);

/**
 * @brief  获取水泥电阻当前状态
 */
bool cement_resistor_is_on(void);

#ifdef __cplusplus
}
#endif

#endif /* __CEMENT_RESISTOR_H__ */
