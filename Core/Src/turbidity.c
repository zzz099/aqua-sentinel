/**
 * @file   turbidity.c
 * @brief  光学浊度传感器驱动
 *
 * 传感器规格：光学透射式，模拟量输出 0~4.5 V，测量范围 0~1000±30 NTU。
 * 电压越高 → 透光率越高 → NTU 越低。
 * 硬件分压电路将 0~4.5 V 映射到 0~3.3 V 后接入 ADC。
 */

#include "turbidity.h"
#include <stdint.h>

/**
  * @brief  基于DMA缓冲区的均值滤波获取电压
  * @note   该函数不会阻塞 CPU，计算极快
  * @return 滤波后的电压值 (V)
  */
float Turbidity_Get_Voltage_DMA(void)
{
    uint32_t sum = 0;
    uint32_t total_len = SAMPLES_PER_CHANNEL * ADC_CHANNEL_COUNT;

    // D-Cache 失效已由 TaskReadSensorData 统一执行，此处不再重复

    /* Turbidity 接在 ADC_CHANNEL_3 (Rank 1)，对应索引 0, 3, 6, ... */
    for(uint32_t i = 0; i < total_len; i += ADC_CHANNEL_COUNT)
    {
        sum += adc_dma_buffer[i];
    }

    float avg_adc = (float)sum / SAMPLES_PER_CHANNEL;
    return avg_adc * (ADC_VREF / ADC_RESOLUTION);
}

/**
 * @brief  将电压转换为浊度值 (NTU)
 * @note   NTU = (1 - V_sensor / 4.5) × 1000，结果钳位到 [0, 1000]
 * @return 浊度值 (NTU)
 */
float Turbidity_Get_NTU_DMA(void)
{
    float voltage = Turbidity_Get_Voltage_DMA();

    /* 还原传感器实际电压 (4.5 V 满量程，硬件分压到 3.3 V) */
    float sensor_voltage = voltage * (4.5f / 3.3f);
    float ntu_val = (1.0f - sensor_voltage / 4.5f) * 1000.0f;

    /* 钳位到有效范围 */
    if (ntu_val < 0.0f)    ntu_val = 0.0f;
    if (ntu_val > 1000.0f) ntu_val = 1000.0f;

    return ntu_val;
}
