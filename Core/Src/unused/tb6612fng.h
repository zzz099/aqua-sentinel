#ifndef __TB6612FNG_H__
#define __TB6612FNG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ======================== TB6612FNG 双路直流电机驱动 (纯 PWM 模式) ========
 *
 *  硬件接线:
 *    AIN1, BIN1, STBY  — 硬件接 3.3V 高电平
 *    AIN2, BIN2        — 硬件接 GND
 *    PWMA, PWMB        — TIM PWM 输出
 *
 *  工作原理:
 *    IN1=H, IN2=L 为固定正转方向, 通过 PWM 占空比控制启停和功率:
 *      PWM > 0  →  设备运行
 *      PWM = 0  →  设备停止
 *
 * ==================================================================== */

/* ---------- 电机通道标识 ---------- */
typedef enum {
    TB6612_MOTOR_A = 0,
    TB6612_MOTOR_B = 1
} TB6612_Motor_t;

/* ---------- TB6612FNG 完整配置 / 句柄 ---------- */
typedef struct {
    /* PWM 定时器 (需支持 PWM 输出, 如 TIM8 等) */
    TIM_HandleTypeDef *htim;
    uint32_t           pwmChannelA;   /* HAL 通道宏, 如 TIM_CHANNEL_1 */
    uint32_t           pwmChannelB;   /* HAL 通道宏, 如 TIM_CHANNEL_2 */
} TB6612_Handle_t;

/* ======================== API ======================== */

/**
 * @brief  初始化 TB6612FNG 驱动
 *         - 启动 PWM 输出 (占空比 0)
 * @param  h  已填充好配置的句柄指针
 * @note   调用前需确保 TIM 已由 CubeMX 初始化完毕
 * @note   方向引脚 (AIN1/BIN1) 和 STBY 已硬件接高电平, 无需软件控制
 */
void TB6612_Init(TB6612_Handle_t *h);

/**
 * @brief  设置指定通道的 PWM 占空比 (控制设备功率)
 * @param  h      句柄指针
 * @param  motor  TB6612_MOTOR_A 或 TB6612_MOTOR_B
 * @param  speed  占空比 0~100 (%), 0 = 停止
 */
void TB6612_SetMotor(TB6612_Handle_t *h, TB6612_Motor_t motor, uint8_t speed);

/**
 * @brief  停止指定通道 (PWM 占空比设为 0)
 * @param  h      句柄指针
 * @param  motor  TB6612_MOTOR_A 或 TB6612_MOTOR_B
 */
void TB6612_Coast(TB6612_Handle_t *h, TB6612_Motor_t motor);

/**
 * @brief  同时停止两路通道
 */
void TB6612_StopAll(TB6612_Handle_t *h);


#ifdef __cplusplus
}
#endif

#endif /* __TB6612FNG_H__ */
