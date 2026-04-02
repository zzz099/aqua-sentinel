/**
 * @file lv_port_disp.h
 * LVGL display port for STM32H743 + LTDC + SDRAM framebuffer (800x480 RGB565)
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void lv_port_disp_init(void);
uint32_t lv_port_disp_get_fps(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISP_H */
