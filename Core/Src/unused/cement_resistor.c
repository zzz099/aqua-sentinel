#include "cement_resistor.h"

/* ---------- 内部 TB6612 句柄 ---------- */
static TB6612_Handle_t g_cement_resistor_drv;
static bool s_cement_resistor_on;

void cement_resistor_init(TIM_HandleTypeDef *htim)
{
    /*
     * 第二块 TB6612FNG 接线:
     *   AIN1_2, BIN1_2, STBY_2 → 硬件接 3.3V 高电平
     *   AIN2_2, BIN2_2         → 硬件接 GND
     *   PWMA → PC8 (TIM8_CH3)   (水泥电阻)
     *   PWMB → PC9 (TIM8_CH4)   (风扇)
     */
    g_cement_resistor_drv.htim        = htim;
    g_cement_resistor_drv.pwmChannelA = TIM_CHANNEL_3;
    g_cement_resistor_drv.pwmChannelB = TIM_CHANNEL_4;

    TB6612_Init(&g_cement_resistor_drv);
    s_cement_resistor_on = false;
}

TB6612_Handle_t *cement_resistor_get_driver(void)
{
    return &g_cement_resistor_drv;
}

void start_cement_resistor(void)
{
    TB6612_SetMotor(&g_cement_resistor_drv, CEMENT_RESISTOR_MOTOR_CHANNEL, CEMENT_RESISTOR_DEFAULT_POWER);
    s_cement_resistor_on = true;
}

void off_cement_resistor(void)
{
    TB6612_Coast(&g_cement_resistor_drv, CEMENT_RESISTOR_MOTOR_CHANNEL);
    s_cement_resistor_on = false;
}

bool cement_resistor_is_on(void)
{
    return s_cement_resistor_on;
}
