#include "cooling_fan.h"

/* ---------- 共享的 TB6612 句柄指针 ---------- */
static TB6612_Handle_t *g_cooling_fan_drv;
static bool s_cooling_fan_on;

void cooling_fan_init(TB6612_Handle_t *drv)
{
    g_cooling_fan_drv = drv;
    s_cooling_fan_on = false;
}

void start_cooling_fan(void)
{
    TB6612_SetMotor(g_cooling_fan_drv, COOLING_FAN_MOTOR_CHANNEL, COOLING_FAN_DEFAULT_SPEED);
    s_cooling_fan_on = true;
}

void off_cooling_fan(void)
{
    TB6612_Coast(g_cooling_fan_drv, COOLING_FAN_MOTOR_CHANNEL);
    s_cooling_fan_on = false;
}

bool cooling_fan_is_on(void)
{
    return s_cooling_fan_on;
}
