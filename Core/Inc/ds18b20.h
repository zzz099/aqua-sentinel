#ifndef __DS18B20_H
#define __DS18B20_H

#include "main.h"

// 包含 TIM 定义，因为我们需要用到 htim6

/***************** HAL库移植版 ******************
 * 原始作者     : 辰哥
 * 移植修改     : Gemini AI
 * 平台         : STM32F103RCT6 (HAL Library)
 * 时间基准     : TIM6
 **********************************************/

// DS18B20引脚定义 (根据原代码保持为 PA6)
#define DS18B20_PORT        GPIOA
#define DS18B20_PIN         GPIO_PIN_7
#define DS18B20_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

// 输出状态定义
#define DS18B20_MODE_OUT 1
#define DS18B20_MODE_IN  0

// 控制引脚电平
#define DS18B20_Low     HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET)
#define DS18B20_High    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET)

// 函数声明
uint8_t DS18B20_Init(void);          // 初始化
short DS18B20_Get_Temp(void);        // 获取温度
void DS18B20_Start(void);            // 开始转换
void DS18B20_Write_Byte(uint8_t dat);// 写字节
uint8_t DS18B20_Read_Byte(void);     // 读字节
uint8_t DS18B20_Read_Bit(void);      // 读位
void DS18B20_Mode(uint8_t mode);     // 模式切换
uint8_t DS18B20_Check(void);         // 检测是否存在
void DS18B20_Rst(void);              // 复位

// 微秒延时函数声明
void delay_us(uint16_t us);

#endif