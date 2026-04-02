/**
 * @file   waterlevel.c
 * @brief  水位传感器驱动 (NPN 开关量输出, PA1)
 */

#include "waterlevel.h"

/** @brief  初始化水位传感器 (GPIO 已在 MX_GPIO_Init 中配置) */
void WaterLevel_Init(void)
{
}

/**
 * @brief  读取水位传感器状态
 * @retval 1=检测到水位 (NPN 导通, 低电平)  0=未检测到水位
 */
uint8_t WaterLevel_Read(void)
{
    return (HAL_GPIO_ReadPin(WATERLEVEL_GPIO_PORT, WATERLEVEL_GPIO_PIN)
            == GPIO_PIN_RESET) ? 1 : 0;
}
