#ifndef __PH_SENSOR_H__
#define __PH_SENSOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "adc_config.h"

/* -------- PH 传感器默认校准参数 -------- */
/*
 * 典型 PH 模块 (PH-4502C 等)：
 *   pH 7.0  ≈ 2.50 V   (中性)
 *   pH 4.0  ≈ 3.04 V   (酸性)
 *   斜率 slope = (pH2 - pH1) / (V2 - V1) = (4.0 - 7.0) / (3.04 - 2.50) = -5.56
 *   截距 offset = pH1 - slope * V1 = 7.0 - (-5.56) * 2.50 = 20.89
 *
 * pH = slope * voltage + offset
 */
#define PH_DEFAULT_SLOPE    (-5.56f)
#define PH_DEFAULT_OFFSET   (20.89f)
#define PH_VALUE_MIN        0.0f
#define PH_VALUE_MAX        14.0f

/* -------- 函数声明 -------- */

/**
 * @brief  从 DMA buffer 中提取指定通道的均值电压
 * @param  channel_index  通道索引 (ADC_CH_WATERLEVEL 或 ADC_CH_PH)
 * @return 均值电压值 (V)
 */
float ADC_GetChannelVoltage(uint8_t channel_index);

/**
 * @brief  初始化 PH 传感器 (使用默认校准参数)
 */
void PH_Init(void);

/**
 * @brief  设置 PH 校准参数
 * @param  slope   斜率
 * @param  offset  截距
 */
void PH_SetCalibration(float slope, float offset);

/**
 * @brief  从 DMA buffer 读取 PH 通道电压
 * @return PH 传感器电压 (V)
 */
float PH_ReadVoltage(void);

/**
 * @brief  读取 PH 值 (经过校准转换)
 * @return PH 值 (0.0 ~ 14.0)
 */
float PH_ReadValue(void);

#ifdef __cplusplus
}
#endif

#endif /* __PH_SENSOR_H__ */
