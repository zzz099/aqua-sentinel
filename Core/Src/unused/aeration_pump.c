#include "aeration_pump.h"

/* ---------- 共享的 TB6612 句柄指针 ---------- */
static TB6612_Handle_t *g_aeration_drv;
static bool s_aeration_on;

void aeration_pump_init(TB6612_Handle_t *drv)
{
    g_aeration_drv = drv;
    s_aeration_on = false;
}

void start_aeration_pump(void)
{
    TB6612_SetMotor(g_aeration_drv, AERATION_MOTOR_CHANNEL, AERATION_DEFAULT_SPEED);
    s_aeration_on = true;
}

void off_aeration_pump(void)
{
    TB6612_Coast(g_aeration_drv, AERATION_MOTOR_CHANNEL);
    s_aeration_on = false;
}

bool aeration_pump_is_on(void)
{
    return s_aeration_on;
}
