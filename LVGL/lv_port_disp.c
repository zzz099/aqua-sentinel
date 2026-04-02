/**
 * @file lv_port_disp.c
 * LVGL display port for STM32H743 + LTDC + SDRAM framebuffer (800x480 RGB565)
 *
 * Strategy: two full-screen framebuffers in SDRAM + LVGL full_refresh.
 * LVGL redraws the entire screen into the back buffer each frame, then
 * the LTDC layer address is swapped during the next vertical blanking period.
 * This avoids the dirty-area synchronisation artefacts (ghosting / overlap)
 * that occur with direct_mode when scenes change frequently (e.g. benchmark).
 */

#include "lv_port_disp.h"
#include "lcd_rgb.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* Display resolution */
#define DISP_HOR_RES  800
#define DISP_VER_RES  480
#define FB_PIXELS     (DISP_HOR_RES * DISP_VER_RES)
#define FB_BYTES      (FB_PIXELS * sizeof(lv_color_t))  /* 768 000 */

/*
 * SDRAM layout (32 MB starting at 0xC0000000):
 *   0xC0000000 : Framebuffer 0  (768 KB)
 *   0xC00C0000 : Framebuffer 1  (768 KB)
 *   0xC0180000 : (free gap)
 *   0xC0400000 : LVGL lv_mem pool (8 MB, configured in lv_conf.h)
 */
#define FB0_ADDR  0xC0000000U
#define FB1_ADDR  0xC00C0000U

extern LTDC_HandleTypeDef hltdc;

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;
static uint32_t           s_fps_value;
static uint32_t           s_fps_window_start;
static uint32_t           s_fps_frame_count;
static uint32_t           s_front_fb_addr = FB0_ADDR;
static uint32_t           s_back_fb_addr  = FB1_ADDR;

/**
 * Swap the LTDC display address to @p fb_addr at the next vertical blanking.
 * Blocks until the hardware reload completes (or 100 ms timeout).
 */
static void ltdc_request_buffer_swap(uint32_t fb_addr)
{
    uint32_t tick_start = HAL_GetTick();

    if (fb_addr == s_front_fb_addr) {
        return;
    }

    HAL_LTDC_SetAddress_NoReload(&hltdc, fb_addr, 0);
    LTDC->SRCR = LTDC_SRCR_VBR;

    while ((LTDC->SRCR & LTDC_SRCR_VBR) != 0U) {
        if ((HAL_GetTick() - tick_start) > 100U) {
            break;
        }
    }

    if ((LTDC->SRCR & LTDC_SRCR_VBR) == 0U) {
        s_front_fb_addr = fb_addr;
        s_back_fb_addr = (fb_addr == FB0_ADDR) ? FB1_ADDR : FB0_ADDR;
    }
}

/**
 * LVGL flush callback.
 * In full_refresh mode this is called exactly once per frame with the
 * entire screen area.  We just swap the LTDC layer address.
 */
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)area;

    ltdc_request_buffer_swap((uint32_t)color_p);

    lv_disp_flush_ready(drv);
}

static void disp_monitor(lv_disp_drv_t *drv, uint32_t time, uint32_t px)
{
    uint32_t now;
    uint32_t elapsed;

    (void)drv;
    (void)time;
    (void)px;

    now = lv_tick_get();
    if (s_fps_window_start == 0U) {
        s_fps_window_start = now;
    }

    ++s_fps_frame_count;
    elapsed = lv_tick_elaps(s_fps_window_start);
    if (elapsed >= 1000U) {
        s_fps_value = (s_fps_frame_count * 1000U + (elapsed / 2U)) / elapsed;
        s_fps_frame_count = 0U;
        s_fps_window_start = now;
    }
}

void lv_port_disp_init(void)
{
    /* Clear both framebuffers */
    memset((void *)FB0_ADDR, 0, FB_BYTES);
    memset((void *)FB1_ADDR, 0, FB_BYTES);

    s_fps_value = 0U;
    s_fps_window_start = lv_tick_get();
    s_fps_frame_count = 0U;
    s_front_fb_addr = FB0_ADDR;
    s_back_fb_addr = FB1_ADDR;

    lv_disp_draw_buf_init(&draw_buf,
                          (lv_color_t *)FB0_ADDR,
                          (lv_color_t *)FB1_ADDR,
                          FB_PIXELS);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res      = DISP_HOR_RES;
    disp_drv.ver_res      = DISP_VER_RES;
    disp_drv.flush_cb     = disp_flush;
    disp_drv.monitor_cb   = disp_monitor;
    disp_drv.draw_buf     = &draw_buf;
    disp_drv.full_refresh = 1;  /* Redraw entire screen each frame → no ghost/overlap */

    lv_disp_drv_register(&disp_drv);
}

uint32_t lv_port_disp_get_fps(void)
{
    return s_fps_value;
}
