#ifndef __TDS_H
#define __TDS_H

#include "main.h"

// 必须与 freertos.c 中的定义一致
extern uint16_t adc_dma_buffer[];

float TDS_Get_Value_DMA(float temperature);

#endif