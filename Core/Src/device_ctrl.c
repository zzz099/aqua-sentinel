/**
 * @file   device_ctrl.c
 * @brief  执行设备开关控制 (PWM / GPIO)
 *
 * 硬件连接说明：
 *   - 增氧泵     : TB6612FNG #1 PWMA → PC6 / TIM8_CH1
 *   - 潜水泵     : TB6612FNG #1 PWMB → PC7 / TIM8_CH2
 *   - 水泥电阻(加热): TB6612FNG #2 PWMA → PC8 / TIM8_CH3
 *   - 风扇       : TB6612FNG #2 PWMB → PC9 / GPIO 高低电平
 *
 * TIM8 参数: PSC=479, ARR=9999 → PWM 频率 = 240 MHz / 480 / 10000 = 50 Hz
 */

#include "device_ctrl.h"
#include "tim.h"

/* ---------- PWM 通道 & 占空比 ---------- */
#define OXYGEN_PUMP_CHANNEL       TIM_CHANNEL_1
#define SUBMERSIBLE_PUMP_CHANNEL  TIM_CHANNEL_2
#define HEATER_CHANNEL            TIM_CHANNEL_3

#define OXYGEN_PUMP_DUTY          10000   /**< 100% 占空比 */
#define SUBMERSIBLE_PUMP_DUTY     10000   /**< 100% 占空比 */
#define HEATER_DUTY               10000   /**< 100% 占空比 */

/* ================================================================
 *  设备控制函数
 *  参数 state: 1=开启, 0=关闭
 *  调用时自动更新 auto_fish_ctrl.h 中声明的全局设备状态变量
 * ================================================================ */

/** @brief  控制增氧泵 (TIM8_CH1 PWM) */
void OxygenPump_Ctrl(uint8_t state) {
  if (state) {
    HAL_TIM_PWM_Start(&htim8, OXYGEN_PUMP_CHANNEL);
    __HAL_TIM_SET_COMPARE(&htim8, OXYGEN_PUMP_CHANNEL, OXYGEN_PUMP_DUTY);
    g_oxygenpump_state = 1;
  } else {
    HAL_TIM_PWM_Stop(&htim8, OXYGEN_PUMP_CHANNEL);
    g_oxygenpump_state = 0;
  }
}

/** @brief  控制潜水泵 (TIM8_CH2 PWM) */
void SubmersiblePump_Ctrl(uint8_t state) {
  if (state) {
    HAL_TIM_PWM_Start(&htim8, SUBMERSIBLE_PUMP_CHANNEL);
    __HAL_TIM_SET_COMPARE(&htim8, SUBMERSIBLE_PUMP_CHANNEL, SUBMERSIBLE_PUMP_DUTY);
    g_submersiblepump_state = 1;
  } else {
    HAL_TIM_PWM_Stop(&htim8, SUBMERSIBLE_PUMP_CHANNEL);
    g_submersiblepump_state = 0;
  }
}

/** @brief  控制加热器/水泥电阻 (TIM8_CH3 PWM) */
void Heater_Ctrl(uint8_t state) {
  if (state) {
    HAL_TIM_PWM_Start(&htim8, HEATER_CHANNEL);
    __HAL_TIM_SET_COMPARE(&htim8, HEATER_CHANNEL, HEATER_DUTY);
    g_heater_state = 1;
  } else {
    HAL_TIM_PWM_Stop(&htim8, HEATER_CHANNEL);
    g_heater_state = 0;
  }
}

/** @brief  控制风扇 (PC9 GPIO 高低电平) */
void Fan_Ctrl(uint8_t state) {
  if (state) {
    HAL_GPIO_WritePin(Fan_GPIO_Port, Fan_Pin, GPIO_PIN_SET);
    g_fan_state = 1;
  } else {
    HAL_GPIO_WritePin(Fan_GPIO_Port, Fan_Pin, GPIO_PIN_RESET);
    g_fan_state = 0;
  }
}
