#ifndef __LV_UI_H__
#define __LV_UI_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lv_ui_create(void);
bool app_lvgl_lock_timeout(uint32_t timeout_ms);
void app_lvgl_lock(void);
void app_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __LV_UI_H__ */
