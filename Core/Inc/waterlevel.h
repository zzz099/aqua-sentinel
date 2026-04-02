#ifndef __WATERLEVEL_H__
#define __WATERLEVEL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 水位传感器 GPIO 定义 (NPN 信号输出, PA1) */
#define WATERLEVEL_GPIO_PORT   GPIOA
#define WATERLEVEL_GPIO_PIN    GPIO_PIN_1

/**
 * @brief  初始化水位传感器 (GPIO 输入模式, 已在 MX_GPIO_Init 中配置)
 */
void WaterLevel_Init(void);

/**
 * @brief  读取水位传感器状态
 * @return 1: 检测到水位 (NPN 导通, 低电平)
 *         0: 未检测到水位 (高电平)
 */
uint8_t WaterLevel_Read(void);

#ifdef __cplusplus
}
#endif

#endif /* __WATERLEVEL_H__ */
