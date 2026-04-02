/**
 * @file   aquarium_light.c
 * @brief  水族箱照明灯控制 (GPIO 继电器)
 */

#include "aquarium_light.h"
#include "auto_fish_ctrl.h"

static bool s_light_on;

void aquarium_light_init(void)
{
    aquarium_light_off();
}

void aquarium_light_on(void)
{
    HAL_GPIO_WritePin(AQUARIUM_LIGHT_PORT, AQUARIUM_LIGHT_PIN,
                      AQUARIUM_LIGHT_ACTIVE_STATE);
    s_light_on = true;
    g_led_state = 1;
}

void aquarium_light_off(void)
{
    HAL_GPIO_WritePin(AQUARIUM_LIGHT_PORT, AQUARIUM_LIGHT_PIN,
                      (AQUARIUM_LIGHT_ACTIVE_STATE == GPIO_PIN_SET) ? GPIO_PIN_RESET
                                                                    : GPIO_PIN_SET);
    s_light_on = false;
    g_led_state = 0;
}

bool aquarium_light_is_on(void)
{
    return s_light_on;
}
