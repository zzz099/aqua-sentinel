#ifndef __GT911_PORT_H
#define __GT911_PORT_H

#include "main.h"
#include "gt911.h" // 引入你下载的官方驱动头文件

// 暴露给外部使用的触摸屏对象和初始化函数
extern GT911_Object_t gt911_obj;
extern uint8_t g_touch_initialized;

void Touch_Init(void);
void Touch_Get_Coord(void);

#endif