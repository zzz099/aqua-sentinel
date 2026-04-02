/**
 * @file   adc_config.h
 * @brief  ADC DMA 公共常量定义 (3 通道: Turbidity / PH / TDS)
 */

#ifndef __ADC_CONFIG_H__
#define __ADC_CONFIG_H__

#include "main.h"

/* -------- ADC DMA 公共定义 (3 通道共享: Turbidity, PH, TDS) -------- */
#define ADC_CHANNEL_COUNT       3       /* 通道数: CH3(Turbidity), CH4(PH), CH10(TDS) */
#define SAMPLES_PER_CHANNEL     64      /* 每通道采样次数(用于均值滤波) */
#define ADC_SAMPLE_COUNT        SAMPLES_PER_CHANNEL  /* 别名, 兼容旧代码 */
#define ADC_DMA_BUF_LEN         (ADC_CHANNEL_COUNT * SAMPLES_PER_CHANNEL)
#define ADC_TOTAL_BUF_SIZE      ADC_DMA_BUF_LEN      /* 别名, 兼容旧代码 */

/* DMA buffer 中通道偏移 (对应 Rank 顺序) */
#define ADC_CH_TURBIDITY        0       /* Rank1 — ADC_CHANNEL_3  Turbidity */
#define ADC_CH_PH               1       /* Rank2 — ADC_CHANNEL_4  PH */
#define ADC_CH_TDS              2       /* Rank3 — ADC_CHANNEL_10 TDS */

/* ADC 参考电压 & 分辨率 */
#define ADC_VREF                3.3f
#define ADC_RESOLUTION          65536.0f   /* 16-bit: 2^16 */

/* DMA buffer (在 freertos.c 中定义, 放置于 RAM_D2 以配合 DMA) */
extern uint16_t adc_dma_buffer[];

#endif /* __ADC_CONFIG_H__ */
