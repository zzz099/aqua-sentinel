/**
 * @file   tds.c
 * @brief  TDS (总溶解固体) 传感器驱动
 *
 * 传感器规格：输出 0~2.3 V，测量范围 0~1000 ppm，精度 ±5% F.S. (25°C)。
 * 采用中位值滤波 + 温度补偿以提高精度。
 */

#include "tds.h"
#include "adc_config.h"

/**
  * @brief  计算 TDS 值 (带温度补偿 + 中位值滤波)
  */
float TDS_Get_Value_DMA(float temperature)
{
    uint32_t total_len = SAMPLES_PER_CHANNEL * ADC_CHANNEL_COUNT;
    float filter_buf[SAMPLES_PER_CHANNEL];
    int count = 0;

    // D-Cache 失效已由 TaskReadSensorData 统一执行，此处不再重复

    /* TDS 接在 ADC_CHANNEL_10 (Rank 3)，对应索引 2, 5, 8, ... */
    for(uint32_t i = 2; i < total_len; i += ADC_CHANNEL_COUNT)
    {
        filter_buf[count++] = (float)adc_dma_buffer[i] * ADC_VREF / ADC_RESOLUTION;
    }

    /* 插入排序：小数组下比冒泡排序快约 2× */
    for(int j = 1; j < SAMPLES_PER_CHANNEL; j++)
    {
        float key = filter_buf[j];
        int i = j - 1;
        while(i >= 0 && filter_buf[i] > key)
        {
            filter_buf[i + 1] = filter_buf[i];
            i--;
        }
        filter_buf[i + 1] = key;
    }

    /* 去掉最大最小各约 1/4，取中间平均值 */
    float sum_voltage = 0;
    int start_idx = SAMPLES_PER_CHANNEL / 4;
    int end_idx = SAMPLES_PER_CHANNEL - start_idx;

    for(int i = start_idx; i < end_idx; i++)
    {
        sum_voltage += filter_buf[i];
    }
    float averageVoltage = sum_voltage / (float)(end_idx - start_idx);

    /* 温度补偿：参考温度 25°C，补偿系数 0.02/°C */
    if (temperature < 0.1f) temperature = 25.0f;
    float compensationCoefficient = 1.0f + 0.02f * (temperature - 25.0f);
    float compensationVoltage = averageVoltage / compensationCoefficient;

    /* 线性映射：TDS = (V_compensated / 2.3) × 1000 */
    float tdsValue = (compensationVoltage / 2.3f) * 1000.0f;

    if(tdsValue < 0) tdsValue = 0;

    return tdsValue;
}
