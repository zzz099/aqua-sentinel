#include "tb6612fng.h"

/* -------------------- 内部辅助 -------------------- */

static uint32_t GetPWMChannel(TB6612_Handle_t *h, TB6612_Motor_t motor)
{
    return (motor == TB6612_MOTOR_A) ? h->pwmChannelA : h->pwmChannelB;
}

/**
 * @brief  设置 PWM 占空比
 * @param  h        句柄
 * @param  channel  TIM_CHANNEL_x
 * @param  percent  0~100
 */
static void SetPWMDuty(TB6612_Handle_t *h, uint32_t channel, uint8_t percent)
{
    if (percent > 100) percent = 100;

    uint32_t arr     = __HAL_TIM_GET_AUTORELOAD(h->htim);
    uint32_t compare = (uint32_t)((uint64_t)arr * percent / 100);

    __HAL_TIM_SET_COMPARE(h->htim, channel, compare);
}

/* -------------------- 公共 API -------------------- */

void TB6612_Init(TB6612_Handle_t *h)
{
    /*
     * PWM 通道已在 main() → MX_TIM8_Init() 之后立即启动 (占空比=0),
     * 此处只需确保占空比为 0, 无需再次调用 HAL_TIM_PWM_Start.
     * (重复调用 HAL_TIM_PWM_Start 会因通道已处于 BUSY 状态而返回 HAL_ERROR)
     */
    SetPWMDuty(h, h->pwmChannelA, 0);
    SetPWMDuty(h, h->pwmChannelB, 0);

    /* 方向引脚 (AIN1/BIN1) 和 STBY 已硬件接高电平, 无需软件控制 */
    /* AIN2/BIN2 已硬件接 GND, 方向固定为正转 */
}

void TB6612_SetMotor(TB6612_Handle_t *h, TB6612_Motor_t motor, uint8_t speed)
{
    uint32_t ch = GetPWMChannel(h, motor);
    SetPWMDuty(h, ch, speed);
}

void TB6612_Coast(TB6612_Handle_t *h, TB6612_Motor_t motor)
{
    SetPWMDuty(h, GetPWMChannel(h, motor), 0);
}

void TB6612_StopAll(TB6612_Handle_t *h)
{
    TB6612_Coast(h, TB6612_MOTOR_A);
    TB6612_Coast(h, TB6612_MOTOR_B);
}
