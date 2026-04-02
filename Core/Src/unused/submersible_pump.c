#include "submersible_pump.h"

/* ---------- 内部 TB6612 句柄 ---------- */
static TB6612_Handle_t g_pump_drv;
static bool s_submersible_pump_on;

void submersible_pump_init(TIM_HandleTypeDef *htim)
{
    /*
     * 第一块 TB6612FNG 接线:
     *   AIN1_1, BIN1_1, STBY_1 → 硬件接 3.3V 高电平
     *   AIN2_1, BIN2_1         → 硬件接 GND
     *   PWMA → PC6 (TIM8_CH1)   (增氧泵)
     *   PWMB → PC7 (TIM8_CH2)   (潜水泵)
     */
    g_pump_drv.htim         = htim;
    g_pump_drv.pwmChannelA  = TIM_CHANNEL_1;
    g_pump_drv.pwmChannelB  = TIM_CHANNEL_2;

    TB6612_Init(&g_pump_drv);
    s_submersible_pump_on = false;
}

TB6612_Handle_t *submersible_pump_get_driver(void)
{
    return &g_pump_drv;
}

void start_submersible_pump(void)
{
    TB6612_SetMotor(&g_pump_drv, PUMP_MOTOR_CHANNEL, PUMP_DEFAULT_SPEED);
    s_submersible_pump_on = true;
}

void off_submersible_pump(void)
{
    TB6612_Coast(&g_pump_drv, PUMP_MOTOR_CHANNEL);
    s_submersible_pump_on = false;
}

bool submersible_pump_is_on(void)
{
    return s_submersible_pump_on;
}
