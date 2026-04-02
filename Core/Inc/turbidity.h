 #ifndef __TURBIDITY_H
#define __TURBIDITY_H

#include "main.h"


// 必须确保这个宏定义在头文件或此处与 freertos.c 一致
// 这里我们为了简单，直接复用逻辑。注意：外部变量 adc_dma_buffer 在 main.h 或 turbidity.h 声明过
#include "adc_config.h"


// 声明外部变量，对应 main.c 中定义的缓冲区大小和数组
//#define TURBIDITY_BUFFER_SIZE 50
//extern uint16_t adc_dma_buffer[TURBIDITY_BUFFER_SIZE];

float Turbidity_Get_Voltage_DMA(void);
float Turbidity_Get_NTU_DMA(void);

#endif