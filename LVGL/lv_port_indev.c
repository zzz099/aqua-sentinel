/**
 * @file lv_port_indev.c
 * LVGL input device port for GT911 capacitive touch (800x480)
 */

#include "lv_port_indev.h"
#include "touch_800x480.h"

static lv_indev_drv_t indev_drv;

static void touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    Touch_Scan();
    (void)drv;
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    if (touchInfo.flag && touchInfo.num > 0) {
        last_x = touchInfo.x[0];
        last_y = touchInfo.y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = last_x;
    data->point.y = last_y;
    data->continue_reading = false;
}

void lv_port_indev_init(void)
{
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}
