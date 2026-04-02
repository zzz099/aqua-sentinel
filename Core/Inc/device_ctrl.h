/**
 * @file   device_ctrl.h
 * @brief  执行设备开关控制函数声明
 */

#ifndef __DEVICE_CTRL_H
#define __DEVICE_CTRL_H

#include <stdint.h>
#include "auto_fish_ctrl.h"

/*
 * 设备启停控制函数声明
 * 参数 state: 1 开启, 0 关闭
 * 调用时会自动更新 auto_fish_ctrl.h 中声明的全局设备状态变量
 */

void OxygenPump_Ctrl(uint8_t state);
void SubmersiblePump_Ctrl(uint8_t state);
void Heater_Ctrl(uint8_t state);
void Fan_Ctrl(uint8_t state);

#endif /* __DEVICE_CTRL_H */
