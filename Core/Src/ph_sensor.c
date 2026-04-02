/**
 * @file   ph_sensor.c
 * @brief  PH 传感器驱动 + ADC 通用通道电压读取
 *
 * PH 传感器采用线性标定模型：PH = slope × V + offset
 * 输出钳位到 [0, 14] 范围。
 */

#include "ph_sensor.h"
#include "adc.h"

/* -------- ADC DMA buffer 由 freertos.c 定义并放置于 RAM_D2 -------- */

/* -------- PH 校准参数 (运行时可修改) -------- */
static float ph_slope  = PH_DEFAULT_SLOPE;
static float ph_offset = PH_DEFAULT_OFFSET;

/* ========== ADC DMA 公共函数 ========== */

/**
 * @brief  从 DMA buffer 中提取指定通道的均值电压
 *         buffer 排列: [CH3_0, CH4_0, CH10_0, CH3_1, CH4_1, CH10_1, ... ]
 */
float ADC_GetChannelVoltage(uint8_t channel_index)
{
    float sum = 0.0f;
    uint32_t count = 0;
    uint32_t total_len = ADC_DMA_BUF_LEN;

    /* D-Cache 失效已由 TaskReadSensorData 统一执行，此处不再重复 */

    for (uint32_t i = channel_index; i < total_len; i += ADC_CHANNEL_COUNT)
    {
        sum += (float)adc_dma_buffer[i] * ADC_VREF / ADC_RESOLUTION;
        count++;
    }

    if (count == 0) return 0.0f;
    return sum / (float)count;
}

/* ========== PH 传感器函数 ========== */

void PH_Init(void)
{
    ph_slope  = PH_DEFAULT_SLOPE;
    ph_offset = PH_DEFAULT_OFFSET;
}

void PH_SetCalibration(float slope, float offset)
{
    ph_slope  = slope;
    ph_offset = offset;
}

float PH_ReadVoltage(void)
{
    return ADC_GetChannelVoltage(ADC_CH_PH);
}

float PH_ReadValue(void)
{
    float voltage = PH_ReadVoltage();
    float ph = ph_slope * voltage + ph_offset;

    /* 限幅到 0~14 */
    if (ph < PH_VALUE_MIN) ph = PH_VALUE_MIN;
    if (ph > PH_VALUE_MAX) ph = PH_VALUE_MAX;

    return ph;
}


