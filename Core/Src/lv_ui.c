#include "lv_ui.h"

#include "AT_MQTT_OS.h"
#include "aquarium_light.h"
#include "auto_fish_ctrl.h"
#include "app_runtime.h"
#include "device_ctrl.h"
#include "lvgl.h"
#include "rtc.h"
#include "tim.h"
#include "waterlevel.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_montserrat_14)
LV_FONT_DECLARE(lv_font_montserrat_16)
LV_FONT_DECLARE(lv_font_montserrat_20)
LV_FONT_DECLARE(lv_font_montserrat_24)
LV_FONT_DECLARE(lv_font_montserrat_30)

enum {
  UI_SENSOR_TEMP = 0,
  UI_SENSOR_TURBIDITY,
  UI_SENSOR_TDS,
  UI_SENSOR_PH,
  UI_SENSOR_LEVEL,
  UI_SENSOR_PRESSURE,
  UI_SENSOR_COUNT
};

enum {
  UI_DEVICE_HEATER = 0,
  UI_DEVICE_FAN,
  UI_DEVICE_AERATION,
  UI_DEVICE_FILTER,
  UI_DEVICE_LED,
  UI_DEVICE_COUNT
};

enum {
  UI_SETTING_TEMP_LOWER_LIMIT = 0,
  UI_SETTING_TEMP_UPPER_LIMIT,
  UI_SETTING_TURBIDITY_ON,
  UI_SETTING_TURBIDITY_OFF,
  UI_SETTING_TDS_HIGH,
  UI_SETTING_PH_LOW,
  UI_SETTING_PH_HIGH,
  UI_SETTING_PRESSURE_LOW,
  UI_SETTING_PRESSURE_EFFECT,
  UI_SETTING_COUNT
};

typedef struct {
  lv_obj_t *overview_screen;
  lv_obj_t *settings_screen;
  lv_obj_t *clock_time[2];
  lv_obj_t *clock_date[2];
  lv_obj_t *quality_pill;
  lv_obj_t *quality_pill_label;
  lv_obj_t *sensor_value[UI_SENSOR_COUNT];
  lv_obj_t *alarm_pill;
  lv_obj_t *alarm_pill_label;
  lv_obj_t *alarm_icon_box;
  lv_obj_t *alarm_title;
  lv_obj_t *alarm_headline;
  lv_obj_t *alarm_metric_value[3];
  lv_obj_t *alert_label;
  lv_obj_t *device_button[UI_DEVICE_COUNT];
  lv_obj_t *device_state[UI_DEVICE_COUNT];
  lv_obj_t *auto_mode_button;
  lv_obj_t *auto_mode_label;
  lv_obj_t *setting_value[UI_SETTING_COUNT];
  lv_obj_t *settings_note;
  lv_obj_t *editor_backdrop;
  lv_obj_t *editor_textarea;
  lv_obj_t *editor_keyboard;
  lv_obj_t *toast;
  lv_obj_t *toast_label;
  lv_timer_t *refresh_timer;
  lv_timer_t *toast_timer;
  uint32_t editing_setting;
} ui_ctx_t;

typedef struct {
  bool use_demo_data;
  bool water_ok;
  bool alerts_present;
  bool tds_high;
  bool ph_high;
  bool ph_low;
  bool water_level_low;
  float temperature_c;
  float turbidity_ntu;
  float tds_ppm;
  float ph_value;
  float pressure_pa;
  float env_temperature_c;
  uint8_t alert_count;
  bool heater_on;
  bool fan_on;
  bool aeration_on;
  bool filter_on;
  bool led_on;
  bool wifi_connected;
  int mqtt_status;
  char alert_text[96];
} ui_runtime_t;

static ui_ctx_t s_ui;

static const lv_color_t k_color_bg_top = LV_COLOR_MAKE(0x18, 0x37, 0x4b);
static const lv_color_t k_color_bg_bottom = LV_COLOR_MAKE(0x08, 0x17, 0x24);
static const lv_color_t k_color_panel = LV_COLOR_MAKE(0x10, 0x29, 0x3c);
static const lv_color_t k_color_panel_border = LV_COLOR_MAKE(0x36, 0x56, 0x68);
static const lv_color_t k_color_card = LV_COLOR_MAKE(0x17, 0x30, 0x44);
static const lv_color_t k_color_chip = LV_COLOR_MAKE(0x1b, 0x34, 0x47);
static const lv_color_t k_color_text_main = LV_COLOR_MAKE(0xf2, 0xfb, 0xff);
static const lv_color_t k_color_text_soft = LV_COLOR_MAKE(0xc1, 0xd8, 0xe6);
static const lv_color_t k_color_text_faint = LV_COLOR_MAKE(0x89, 0xa3, 0xb6);
static const lv_color_t k_color_aqua = LV_COLOR_MAKE(0x70, 0xec, 0xff);
static const lv_color_t k_color_mint = LV_COLOR_MAKE(0x8f, 0xf7, 0xca);
static const lv_color_t k_color_warm = LV_COLOR_MAKE(0xff, 0xb5, 0x5f);
static const lv_color_t k_color_coral = LV_COLOR_MAKE(0xff, 0x8d, 0x72);
static const lv_color_t k_color_purple = LV_COLOR_MAKE(0x9f, 0xa5, 0xff);
static const lv_color_t k_color_steel = LV_COLOR_MAKE(0xb9, 0xd0, 0xe8);
static const lv_color_t k_color_good = LV_COLOR_MAKE(0x9d, 0xfb, 0xd5);

#define UI_SETTING_NONE ((uint32_t)UINT32_MAX)
#define UI_SCREEN_LOAD_ANIM      LV_SCR_LOAD_ANIM_NONE
#define UI_SCREEN_LOAD_ANIM_TIME 0U
#define UI_SCREEN_LOAD_DELAY     0U

typedef struct {
  const char *title;
  const char *unit;
  float min_value;
  float max_value;
  uint8_t decimals;
  bool editable;
} ui_setting_meta_t;

static const ui_setting_meta_t k_setting_meta[UI_SETTING_COUNT] = {
    [UI_SETTING_TEMP_LOWER_LIMIT] = {"Temp Lower", "C", 0.0f, 40.0f, 1, true},
    [UI_SETTING_TEMP_UPPER_LIMIT] = {"Temp Upper", "C", 0.0f, 40.0f, 1, true},
    [UI_SETTING_TURBIDITY_ON] = {"Turbidity Start", "NTU", 0.0f, 500.0f, 0, true},
    [UI_SETTING_TURBIDITY_OFF] = {"Turbidity Stop", "NTU", 0.0f, 500.0f, 0, true},
    [UI_SETTING_TDS_HIGH] = {"TDS High", "ppm", 0.0f, 3000.0f, 0, true},
    [UI_SETTING_PH_LOW] = {"pH Low", "", 0.0f, 14.0f, 1, true},
    [UI_SETTING_PH_HIGH] = {"pH High", "", 0.0f, 14.0f, 1, true},
    [UI_SETTING_PRESSURE_LOW] = {"Pressure Low", "hPa", 800.0f, 1200.0f, 0, true},
    [UI_SETTING_PRESSURE_EFFECT] = {"Pressure Effect", "", 0.0f, 0.0f, 0, false},
};

static void ui_refresh_settings_values(void);
static void ui_close_setting_editor(void);
static void ui_update_auto_mode_visual(void);

static bool ui_setting_edit_supported(void) {
  return true;
}

static float ui_get_fallback_setting_value(uint32_t setting_id) {
  FishCtrl_Thresholds_t th_snap;
  FishCtrl_CopyThresholds(&th_snap);
  const FishCtrl_Thresholds_t *th = &th_snap;
  switch (setting_id) {
  case UI_SETTING_TEMP_LOWER_LIMIT:
    return th->temp_lower_limit;
  case UI_SETTING_TEMP_UPPER_LIMIT:
    return th->temp_upper_limit;
  case UI_SETTING_TURBIDITY_ON:
    return th->turbidity_on_thresh;
  case UI_SETTING_TURBIDITY_OFF:
    return th->turbidity_off_thresh;
  case UI_SETTING_TDS_HIGH:
    return th->tds_high_thresh;
  case UI_SETTING_PH_LOW:
    return th->ph_low_thresh;
  case UI_SETTING_PH_HIGH:
    return th->ph_high_thresh;
  case UI_SETTING_PRESSURE_LOW:
    return th->pressure_low_thresh_pa / 100.0f;
  default:
    return 0.0f;
  }
}

static bool ui_is_tim8_channel_enabled(uint32_t channel) {
  if (htim8.Instance == NULL) {
    return false;
  }

  switch (channel) {
  case TIM_CHANNEL_1:
    return (htim8.Instance->CCER & TIM_CCER_CC1E) != 0U;
  case TIM_CHANNEL_2:
    return (htim8.Instance->CCER & TIM_CCER_CC2E) != 0U;
  case TIM_CHANNEL_3:
    return (htim8.Instance->CCER & TIM_CCER_CC3E) != 0U;
  case TIM_CHANNEL_4:
    return (htim8.Instance->CCER & TIM_CCER_CC4E) != 0U;
  default:
    return false;
  }
}

static void ui_append_alert_text(char *buf, size_t buf_size, const char *text) {
  size_t used;

  if (buf == NULL || buf_size == 0U || text == NULL || text[0] == '\0') {
    return;
  }

  used = strlen(buf);
  if (used >= (buf_size - 1U)) {
    return;
  }

  (void)snprintf(buf + used, buf_size - used, "%s%s", (used == 0U) ? "" : " | ",
                 text);
}

static const ui_setting_meta_t *ui_get_setting_meta(uint32_t setting_id) {
  if (setting_id >= UI_SETTING_COUNT) {
    return NULL;
  }
  return &k_setting_meta[setting_id];
}

static float ui_get_setting_edit_value(uint32_t setting_id) {
  return ui_get_fallback_setting_value(setting_id);
}

static void ui_format_setting_input_value(uint32_t setting_id, char *buf,
                                          size_t buf_size) {
  const ui_setting_meta_t *meta = ui_get_setting_meta(setting_id);

  if (buf == NULL || buf_size == 0U || meta == NULL) {
    return;
  }

  (void)snprintf(buf, buf_size, "%.*f", meta->decimals,
                 ui_get_setting_edit_value(setting_id));
}

static bool ui_apply_setting_value(uint32_t setting_id, float value,
                                   char *err_buf, size_t err_buf_size) {
  const ui_setting_meta_t *meta = ui_get_setting_meta(setting_id);
  FishCtrl_Thresholds_t th_copy;
  FishCtrl_CopyThresholds(&th_copy);
  FishCtrl_Thresholds_t *th = &th_copy;

  if (meta == NULL) {
    if (err_buf != NULL && err_buf_size > 0U) {
      (void)snprintf(err_buf, err_buf_size, "Unknown setting");
    }
    return false;
  }

  if (value < meta->min_value || value > meta->max_value) {
    if (err_buf != NULL && err_buf_size > 0U) {
      (void)snprintf(err_buf, err_buf_size, "%s out of range", meta->title);
    }
    return false;
  }

  switch (setting_id) {
  case UI_SETTING_TEMP_LOWER_LIMIT:
    th->temp_lower_limit = value;
    break;
  case UI_SETTING_TEMP_UPPER_LIMIT:
    th->temp_upper_limit = value;
    break;
  case UI_SETTING_TURBIDITY_ON:
    th->turbidity_on_thresh = value;
    break;
  case UI_SETTING_TURBIDITY_OFF:
    th->turbidity_off_thresh = value;
    break;
  case UI_SETTING_TDS_HIGH:
    th->tds_high_thresh = value;
    break;
  case UI_SETTING_PH_LOW:
    th->ph_low_thresh = value;
    break;
  case UI_SETTING_PH_HIGH:
    th->ph_high_thresh = value;
    break;
  case UI_SETTING_PRESSURE_LOW:
    th->pressure_low_thresh_pa = value * 100.0f;
    break;
  default:
    if (err_buf != NULL && err_buf_size > 0U) {
      (void)snprintf(err_buf, err_buf_size, "Setting not editable");
    }
    return false;
  }

  FishCtrl_UpdateThresholds(&th_copy);
  return true;
}

static lv_obj_t *ui_create_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h,
                               lv_color_t bg_color, lv_opa_t bg_opa,
                               lv_color_t border_color,
                               lv_coord_t radius) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_set_style_bg_color(obj, bg_color, 0);
  lv_obj_set_style_bg_opa(obj, bg_opa, 0);
  lv_obj_set_style_border_color(obj, border_color, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  return obj;
}

static lv_obj_t *ui_create_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                 lv_coord_t w, lv_coord_t h) {
  lv_obj_t *panel =
      ui_create_box(parent, x, y, w, h, k_color_panel, 235, k_color_panel_border, 24);
  lv_obj_set_style_shadow_width(panel, 24, 0);
  lv_obj_set_style_shadow_color(panel, lv_color_hex(0x020A12), 0);
  lv_obj_set_style_shadow_opa(panel, 120, 0);
  lv_obj_set_style_shadow_ofs_y(panel, 10, 0);
  return panel;
}

static lv_obj_t *ui_create_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                 lv_coord_t w, const char *text,
                                 const lv_font_t *font, lv_color_t color,
                                 lv_text_align_t align) {
  lv_obj_t *label = lv_label_create(parent);
  if (w > 0) {
    lv_obj_set_width(label, w);
  }
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, align, 0);
  return label;
}

static lv_obj_t *ui_create_pill(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                lv_coord_t w, lv_coord_t h,
                                lv_color_t bg_color, lv_opa_t bg_opa,
                                lv_color_t border_color, const char *text,
                                lv_color_t text_color,
                                lv_obj_t **label_out) {
  lv_obj_t *pill =
      ui_create_box(parent, x, y, w, h, bg_color, bg_opa, border_color, LV_RADIUS_CIRCLE);
  lv_obj_t *label =
      ui_create_label(pill, 0, 5, w, text, &lv_font_montserrat_14, text_color, LV_TEXT_ALIGN_CENTER);
  if (label_out != NULL) {
    *label_out = label;
  }
  return pill;
}

static void ui_hide_toast_timer_cb(lv_timer_t *timer) {
  (void)timer;
  if (s_ui.toast != NULL) {
    lv_obj_add_flag(s_ui.toast, LV_OBJ_FLAG_HIDDEN);
  }
  if (s_ui.toast_timer != NULL) {
    lv_timer_pause(s_ui.toast_timer);
  }
}

static void ui_show_toast(const char *message) {
  if (s_ui.toast == NULL || s_ui.toast_label == NULL || s_ui.toast_timer == NULL) {
    return;
  }

  lv_label_set_text(s_ui.toast_label, message);
  lv_obj_clear_flag(s_ui.toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_ui.toast);
  lv_timer_set_period(s_ui.toast_timer, 1600);
  lv_timer_reset(s_ui.toast_timer);
  lv_timer_resume(s_ui.toast_timer);
}

static void ui_close_setting_editor(void) {
  if (s_ui.editor_backdrop != NULL) {
    lv_obj_del_async(s_ui.editor_backdrop);
  }

  s_ui.editor_backdrop = NULL;
  s_ui.editor_textarea = NULL;
  s_ui.editor_keyboard = NULL;
  s_ui.editing_setting = UI_SETTING_NONE;
}

static void ui_commit_setting_editor(void) {
  const ui_setting_meta_t *meta;
  const char *text;
  char *end_ptr;
  char toast_buf[80];
  float value;

  if (s_ui.editing_setting >= UI_SETTING_COUNT || s_ui.editor_textarea == NULL) {
    ui_close_setting_editor();
    return;
  }

  meta = ui_get_setting_meta(s_ui.editing_setting);
  text = lv_textarea_get_text(s_ui.editor_textarea);
  if (text == NULL) {
    ui_show_toast("Please enter a number");
    return;
  }

  value = strtof(text, &end_ptr);
  if ((end_ptr == text) || (*end_ptr != '\0')) {
    ui_show_toast("Please enter a valid number");
    return;
  }

  if (!ui_apply_setting_value(s_ui.editing_setting, value, toast_buf,
                              sizeof(toast_buf))) {
    ui_show_toast(toast_buf);
    return;
  }

  ui_refresh_settings_values();
  ui_close_setting_editor();

  if (meta != NULL) {
    (void)snprintf(toast_buf, sizeof(toast_buf), "%s updated", meta->title);
    ui_show_toast(toast_buf);
  }
}

/* ── custom numeric keypad (btnmatrix) ── */
static const char *const k_numpad_map[] = {
    "7", "8", "9", LV_SYMBOL_BACKSPACE, "\n",
    "4", "5", "6", "CLR", "\n",
    "1", "2", "3", ".", "\n",
    "+/-", "0", "Cancel", "OK", ""};

static const lv_btnmatrix_ctrl_t k_numpad_ctrl[] = {
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, LV_BTNMATRIX_CTRL_CHECKED | 1, LV_BTNMATRIX_CTRL_CHECKED | 1};

static void ui_numpad_event_cb(lv_event_t *e) {
  lv_obj_t *btnm = lv_event_get_target(e);
  uint32_t id = lv_btnmatrix_get_selected_btn(btnm);
  const char *txt;

  if (id == LV_BTNMATRIX_BTN_NONE) {
    return;
  }

  txt = lv_btnmatrix_get_btn_text(btnm, id);
  if (txt == NULL || s_ui.editor_textarea == NULL) {
    return;
  }

  if (strcmp(txt, "OK") == 0) {
    ui_commit_setting_editor();
  } else if (strcmp(txt, "Cancel") == 0) {
    ui_close_setting_editor();
  } else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
    lv_textarea_del_char(s_ui.editor_textarea);
  } else if (strcmp(txt, "CLR") == 0) {
    lv_textarea_set_text(s_ui.editor_textarea, "");
  } else if (strcmp(txt, "+/-") == 0) {
    const char *cur = lv_textarea_get_text(s_ui.editor_textarea);
    if (cur != NULL && cur[0] == '-') {
      lv_textarea_set_text(s_ui.editor_textarea, cur + 1);
    } else if (cur != NULL) {
      char tmp[24];
      (void)snprintf(tmp, sizeof(tmp), "-%s", cur);
      lv_textarea_set_text(s_ui.editor_textarea, tmp);
    }
    lv_textarea_set_cursor_pos(s_ui.editor_textarea, LV_TEXTAREA_CURSOR_LAST);
  } else {
    /* digit or '.' */
    lv_textarea_add_text(s_ui.editor_textarea, txt);
  }
}

static void ui_open_setting_editor(uint32_t setting_id) {
  const ui_setting_meta_t *meta = ui_get_setting_meta(setting_id);
  lv_obj_t *panel;
  lv_obj_t *value_box;
  lv_obj_t *numpad;
  char value_buf[24];
  char range_buf[72];

  if (meta == NULL || !meta->editable) {
    return;
  }

  if (!ui_setting_edit_supported()) {
    ui_show_toast("Threshold editing unavailable");
    return;
  }

  ui_close_setting_editor();
  s_ui.editing_setting = setting_id;

  /* ── dark semi-transparent backdrop ── */
  s_ui.editor_backdrop = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_ui.editor_backdrop);
  lv_obj_set_pos(s_ui.editor_backdrop, 0, 0);
  lv_obj_set_size(s_ui.editor_backdrop, 800, 480);
  lv_obj_clear_flag(s_ui.editor_backdrop, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(s_ui.editor_backdrop, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_ui.editor_backdrop, LV_OPA_60, 0);
  lv_obj_move_foreground(s_ui.editor_backdrop);

  /* ── popup panel ── */
  panel = ui_create_box(s_ui.editor_backdrop, 172, 30, 456, 420, k_color_panel,
                        245, k_color_panel_border, 24);
  lv_obj_set_style_shadow_width(panel, 30, 0);
  lv_obj_set_style_shadow_color(panel, lv_color_hex(0x020A12), 0);
  lv_obj_set_style_shadow_opa(panel, 120, 0);
  lv_obj_set_style_shadow_ofs_y(panel, 10, 0);

  /* ── title area ── */
  (void)ui_create_label(panel, 24, 14, 180, "NUMERIC INPUT",
                        &lv_font_montserrat_14, k_color_text_faint,
                        LV_TEXT_ALIGN_LEFT);
  (void)ui_create_label(panel, 24, 32, 240, meta->title, &lv_font_montserrat_24,
                        k_color_text_main, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_pill(panel, 328, 16, 104, 28, k_color_chip, 130,
                       lv_color_hex(0x384F62),
                       (meta->unit != NULL && meta->unit[0] != '\0') ? meta->unit
                                                                     : "Value",
                       k_color_text_soft, NULL);

  if ((meta->unit != NULL) && (meta->unit[0] != '\0')) {
    (void)snprintf(range_buf, sizeof(range_buf), "Range %.*f - %.*f %s",
                   meta->decimals, meta->min_value, meta->decimals,
                   meta->max_value, meta->unit);
  } else {
    (void)snprintf(range_buf, sizeof(range_buf), "Range %.*f - %.*f",
                   meta->decimals, meta->min_value, meta->decimals,
                   meta->max_value);
  }
  (void)ui_create_label(panel, 24, 64, 300, range_buf, &lv_font_montserrat_14,
                        k_color_text_soft, LV_TEXT_ALIGN_LEFT);

  /* ── text input box ── */
  value_box = ui_create_box(panel, 24, 90, 408, 54, k_color_chip, 118,
                            lv_color_hex(0x3F6077), 18);
  s_ui.editor_textarea = lv_textarea_create(value_box);
  lv_obj_set_size(s_ui.editor_textarea, 388, 40);
  lv_obj_set_pos(s_ui.editor_textarea, 10, 7);
  lv_textarea_set_one_line(s_ui.editor_textarea, true);
  lv_textarea_set_max_length(s_ui.editor_textarea, 12);
  lv_textarea_set_accepted_chars(s_ui.editor_textarea, "+-.0123456789");
  lv_obj_set_style_bg_opa(s_ui.editor_textarea, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_ui.editor_textarea, 0, 0);
  lv_obj_set_style_outline_width(s_ui.editor_textarea, 0, 0);
  lv_obj_set_style_pad_left(s_ui.editor_textarea, 0, 0);
  lv_obj_set_style_pad_right(s_ui.editor_textarea, 0, 0);
  lv_obj_set_style_pad_top(s_ui.editor_textarea, 6, 0);
  lv_obj_set_style_text_color(s_ui.editor_textarea, k_color_text_main, 0);
  lv_obj_set_style_text_font(s_ui.editor_textarea, &lv_font_montserrat_24, 0);
  ui_format_setting_input_value(setting_id, value_buf, sizeof(value_buf));
  lv_textarea_set_text(s_ui.editor_textarea, value_buf);
  lv_textarea_set_cursor_pos(s_ui.editor_textarea, LV_TEXTAREA_CURSOR_LAST);

  /* ── custom numpad (btnmatrix) ── */
  numpad = lv_btnmatrix_create(panel);
  lv_obj_set_size(numpad, 408, 256);
  lv_obj_set_pos(numpad, 24, 154);
  lv_btnmatrix_set_map(numpad, (const char **)k_numpad_map);
  lv_btnmatrix_set_ctrl_map(numpad, k_numpad_ctrl);

  /* numpad theme – dark background, rounded buttons */
  lv_obj_set_style_bg_color(numpad, k_color_panel, 0);
  lv_obj_set_style_bg_opa(numpad, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(numpad, 0, 0);
  lv_obj_set_style_pad_all(numpad, 6, 0);
  lv_obj_set_style_pad_gap(numpad, 8, 0);

  /* button normal style */
  lv_obj_set_style_bg_color(numpad, k_color_chip, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(numpad, 180, LV_PART_ITEMS);
  lv_obj_set_style_border_color(numpad, lv_color_hex(0x3F6077), LV_PART_ITEMS);
  lv_obj_set_style_border_width(numpad, 1, LV_PART_ITEMS);
  lv_obj_set_style_radius(numpad, 12, LV_PART_ITEMS);
  lv_obj_set_style_text_color(numpad, k_color_text_main, LV_PART_ITEMS);
  lv_obj_set_style_text_font(numpad, &lv_font_montserrat_20, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(numpad, 0, LV_PART_ITEMS);

  /* button pressed style */
  lv_obj_set_style_bg_color(numpad, k_color_aqua, LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(numpad, 80, LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(numpad, k_color_text_main,
                              LV_PART_ITEMS | LV_STATE_PRESSED);

  /* checked buttons (Cancel / OK) highlight */
  lv_obj_set_style_bg_color(numpad, k_color_aqua,
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(numpad, 62, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(numpad, k_color_text_main,
                              LV_PART_ITEMS | LV_STATE_CHECKED);

  lv_obj_add_event_cb(numpad, ui_numpad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  s_ui.editor_keyboard = numpad;

  lv_obj_move_foreground(panel);
}

static void ui_setting_value_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  ui_open_setting_editor((uint32_t)(uintptr_t)lv_event_get_user_data(e));
}

static void ui_make_setting_box_editable(lv_obj_t *box, uint32_t setting_id) {
  const ui_setting_meta_t *meta = ui_get_setting_meta(setting_id);

  if (box == NULL || meta == NULL || !meta->editable || !ui_setting_edit_supported()) {
    return;
  }

  lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(box, ui_setting_value_event_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)setting_id);
  lv_obj_set_style_border_color(box, k_color_aqua, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(box, 145, LV_STATE_PRESSED);
}

static void ui_load_screen(lv_obj_t *screen) {
  if (screen == NULL || lv_scr_act() == screen) {
    return;
  }

  lv_scr_load_anim(screen, UI_SCREEN_LOAD_ANIM, UI_SCREEN_LOAD_ANIM_TIME,
                   UI_SCREEN_LOAD_DELAY, false);
}

static void ui_nav_event_cb(lv_event_t *e) {
  const char *target_name;

  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  target_name = (const char *)lv_event_get_user_data(e);
  if (target_name == NULL) {
    return;
  }

  if (strcmp(target_name, "overview") == 0) {
    ui_load_screen(s_ui.overview_screen);
  } else if (strcmp(target_name, "settings") == 0) {
    ui_load_screen(s_ui.settings_screen);
  }
}

static void ui_hint_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  ui_show_toast((const char *)lv_event_get_user_data(e));
}

static void ui_restore_defaults_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  /* 构造默认阈值，通过 UpdateThresholds 走 mutex + EEPROM 持久化 */
  FishCtrl_Thresholds_t defaults = {
    .temp_upper_limit       = 23.0f,
    .temp_lower_limit       = 20.0f,
    .turbidity_on_thresh    = 50.0f,
    .turbidity_off_thresh   = 20.0f,
    .tds_high_thresh        = 500.0f,
    .ph_low_thresh          = 6.5f,
    .ph_high_thresh         = 8.5f,
    .pressure_low_thresh_pa = 99000.0f,
    .aeration_on_ms         = 300000U,
    .aeration_off_ms        = 1800000U,
    .led_hour_on            = 8U,
    .led_hour_off           = 20U,
  };
  FishCtrl_UpdateThresholds(&defaults);
  ui_refresh_settings_values();
  ui_show_toast("Defaults restored");
}

static void ui_format_clock(char *time_buf, size_t time_buf_size,
                            char *date_buf, size_t date_buf_size) {
  RTC_TimeTypeDef time_now = {0};
  RTC_DateTypeDef date_now = {0};
  static const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed",
                                   "Thu", "Fri", "Sat"};

  if ((HAL_RTC_GetTime(&hrtc, &time_now, RTC_FORMAT_BIN) == HAL_OK) &&
      (HAL_RTC_GetDate(&hrtc, &date_now, RTC_FORMAT_BIN) == HAL_OK) &&
      (time_now.Hours < 24U) && (time_now.Minutes < 60U) &&
      (time_now.Seconds < 60U) && (date_now.Month >= 1U) &&
      (date_now.Month <= 12U) && (date_now.Date >= 1U) &&
      (date_now.Date <= 31U) && (date_now.WeekDay >= 1U) &&
      (date_now.WeekDay <= 7U)) {
    (void)snprintf(time_buf, time_buf_size, "%02u:%02u:%02u",
                   (unsigned)time_now.Hours, (unsigned)time_now.Minutes,
                   (unsigned)time_now.Seconds);
    (void)snprintf(date_buf, date_buf_size, "20%02u-%02u-%02u %s",
                   (unsigned)date_now.Year, (unsigned)date_now.Month,
                   (unsigned)date_now.Date, weekdays[date_now.WeekDay - 1U]);
  } else {
    (void)snprintf(time_buf, time_buf_size, "--:--:--");
    (void)snprintf(date_buf, date_buf_size, "RTC not synced");
  }
}

static void ui_get_runtime(ui_runtime_t *rt) {
  const bool has_live_data =
      (g_Temperature != 0) || (g_BMP280_Pressure > 1000.0f) ||
      (g_Turbidity_NTU > 0.0f) || (g_TDS_Value > 0.0f) || (g_PH_Value > 0.0f);

  memset(rt, 0, sizeof(*rt));
  rt->use_demo_data = !has_live_data;
  rt->wifi_connected = (g_WiFi_Connected != 0U);
  rt->mqtt_status = g_MQTT_Status;

  if (rt->use_demo_data) {
    rt->temperature_c = 25.6f;
    rt->turbidity_ntu = 1.8f;
    rt->tds_ppm = 168.0f;
    rt->ph_value = 7.2f;
    rt->pressure_pa = 100800.0f;
    rt->env_temperature_c = 23.0f;
    rt->water_ok = true;
    rt->alerts_present = false;
    rt->heater_on = true;
    rt->fan_on = false;
    rt->aeration_on = true;
    rt->filter_on = false;
    rt->led_on = true;
    rt->alert_count = 0U;
    (void)snprintf(rt->alert_text, sizeof(rt->alert_text), "All readings in range");
    return;
  }

  rt->temperature_c = (cloud_Temperature != 0.0f) ? cloud_Temperature
                                                 : ((float)g_Temperature / 10.0f);
  rt->turbidity_ntu = g_Turbidity_NTU;
  rt->tds_ppm = g_TDS_Value;
  rt->ph_value = g_PH_Value;
  rt->pressure_pa = g_BMP280_Pressure;
  rt->env_temperature_c =
      (g_BMP280_Temperature != 0.0f) ? g_BMP280_Temperature : rt->temperature_c;
  rt->water_ok = (WaterLevel_Read() != 0U);
  FishCtrl_Thresholds_t th_alarm;
  FishCtrl_CopyThresholds(&th_alarm);
  rt->tds_high = rt->tds_ppm > th_alarm.tds_high_thresh;
  rt->ph_high =
      (rt->ph_value > 0.0f) && (rt->ph_value > th_alarm.ph_high_thresh);
  rt->ph_low =
      (rt->ph_value > 0.0f) && (rt->ph_value < th_alarm.ph_low_thresh);
  rt->water_level_low = !rt->water_ok;
  rt->alerts_present =
      rt->tds_high || rt->ph_high || rt->ph_low || rt->water_level_low;
  rt->alert_count = (uint8_t)(rt->tds_high + rt->ph_high + rt->ph_low +
                              rt->water_level_low);
  rt->heater_on = ui_is_tim8_channel_enabled(TIM_CHANNEL_3);
  rt->fan_on = (g_fan_state != 0U);
  rt->aeration_on = ui_is_tim8_channel_enabled(TIM_CHANNEL_1);
  rt->filter_on = ui_is_tim8_channel_enabled(TIM_CHANNEL_2);
  rt->led_on = aquarium_light_is_on();

  if (rt->tds_high) {
    ui_append_alert_text(rt->alert_text, sizeof(rt->alert_text), "TDS high");
  }
  if (rt->ph_high) {
    ui_append_alert_text(rt->alert_text, sizeof(rt->alert_text), "pH high");
  }
  if (rt->ph_low) {
    ui_append_alert_text(rt->alert_text, sizeof(rt->alert_text), "pH low");
  }
  if (rt->water_level_low) {
    ui_append_alert_text(rt->alert_text, sizeof(rt->alert_text), "Low water");
  }

  if (rt->alert_text[0] == '\0') {
    (void)snprintf(rt->alert_text, sizeof(rt->alert_text), "All readings in range");
  }
}

static void ui_update_device_visual(uint32_t index, bool active,
                                    const char *state_text) {
  static const lv_color_t accent_colors[UI_DEVICE_COUNT] = {
      LV_COLOR_MAKE(0xff, 0xb5, 0x5f), LV_COLOR_MAKE(0xb9, 0xd0, 0xe8),
      LV_COLOR_MAKE(0x70, 0xec, 0xff), LV_COLOR_MAKE(0x8f, 0xf7, 0xca),
      LV_COLOR_MAKE(0xff, 0x8d, 0x72)};

  lv_obj_t *button = s_ui.device_button[index];
  lv_obj_t *state = s_ui.device_state[index];

  if (button == NULL || state == NULL) {
    return;
  }

  if (active) {
    lv_obj_set_style_bg_color(button, accent_colors[index], 0);
    lv_obj_set_style_bg_opa(button, 48, 0);
    lv_obj_set_style_border_color(button, accent_colors[index], 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_shadow_width(button, 10, 0);
    lv_obj_set_style_shadow_color(button, accent_colors[index], 0);
    lv_obj_set_style_shadow_opa(button, 40, 0);
    lv_obj_set_style_shadow_ofs_y(button, 0, 0);
  } else {
    lv_obj_set_style_bg_color(button, k_color_card, 0);
    lv_obj_set_style_bg_opa(button, 130, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x354C5F), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
  }

  lv_label_set_text(state, state_text);
}

static void ui_refresh_settings_values(void) {
  char value_buf[40];
  char note_buf[128];
  FishCtrl_Thresholds_t th_snap;
  FishCtrl_CopyThresholds(&th_snap);
  const FishCtrl_Thresholds_t *th = &th_snap;

  if (s_ui.setting_value[UI_SETTING_TEMP_LOWER_LIMIT] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.1f C",
                   th->temp_lower_limit);
    lv_label_set_text(s_ui.setting_value[UI_SETTING_TEMP_LOWER_LIMIT], value_buf);
  }

  if (s_ui.setting_value[UI_SETTING_TEMP_UPPER_LIMIT] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.1f C",
                   th->temp_upper_limit);
    lv_label_set_text(s_ui.setting_value[UI_SETTING_TEMP_UPPER_LIMIT], value_buf);
  }

  if (s_ui.setting_value[UI_SETTING_TURBIDITY_ON] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.0f NTU",
                   th->turbidity_on_thresh);
    lv_label_set_text(s_ui.setting_value[UI_SETTING_TURBIDITY_ON], value_buf);
  }

  if (s_ui.setting_value[UI_SETTING_TURBIDITY_OFF] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.0f NTU",
                   th->turbidity_off_thresh);
    lv_label_set_text(s_ui.setting_value[UI_SETTING_TURBIDITY_OFF], value_buf);
  }

  if (s_ui.setting_value[UI_SETTING_TDS_HIGH] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.0f ppm",
                   th->tds_high_thresh);
    lv_label_set_text(s_ui.setting_value[UI_SETTING_TDS_HIGH], value_buf);
  }

  if (s_ui.setting_value[UI_SETTING_PH_LOW] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.1f",
                   th->ph_low_thresh);
    lv_label_set_text(s_ui.setting_value[UI_SETTING_PH_LOW], value_buf);
  }

  if (s_ui.setting_value[UI_SETTING_PH_HIGH] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.1f",
                   th->ph_high_thresh);
    lv_label_set_text(s_ui.setting_value[UI_SETTING_PH_HIGH], value_buf);
  }

  if (s_ui.setting_value[UI_SETTING_PRESSURE_LOW] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.0f hPa",
                   th->pressure_low_thresh_pa / 100.0f);
    lv_label_set_text(s_ui.setting_value[UI_SETTING_PRESSURE_LOW], value_buf);
  }

  if (s_ui.setting_value[UI_SETTING_PRESSURE_EFFECT] != NULL) {
    lv_label_set_text(s_ui.setting_value[UI_SETTING_PRESSURE_EFFECT], "Off gap /2");
  }

  if (s_ui.settings_note != NULL) {
    (void)snprintf(note_buf, sizeof(note_buf),
                   "Tap value to edit  |  Aeration %lu min on / %lu min "
                   "off  |  LED %02u:00-%02u:00",
                   (unsigned long)(th->aeration_on_ms / 60000U),
                   (unsigned long)(th->aeration_off_ms / 60000U),
                   (unsigned)th->led_hour_on,
                   (unsigned)th->led_hour_off);
    lv_label_set_text(s_ui.settings_note, note_buf);
  }
}

static void ui_update_alarm_chip(lv_obj_t *value_label, const char *text, bool active,
                                 lv_color_t accent) {
  lv_obj_t *chip;

  if (value_label == NULL) {
    return;
  }

  chip = lv_obj_get_parent(value_label);
  lv_label_set_text(value_label, text);
  lv_obj_set_style_text_color(value_label,
                              active ? k_color_text_main : k_color_text_soft, 0);

  if (chip != NULL) {
    lv_obj_set_style_bg_color(chip, active ? accent : k_color_chip, 0);
    lv_obj_set_style_bg_opa(chip, active ? 55 : 105, 0);
    lv_obj_set_style_border_color(chip, active ? accent : lv_color_hex(0x384F62), 0);
  }
}

static void ui_refresh_timer_cb(lv_timer_t *timer) {
  (void)timer;
  ui_runtime_t rt;
  char time_buf[16];
  char date_buf[32];
  char value_buf[32];

  ui_get_runtime(&rt);
  ui_format_clock(time_buf, sizeof(time_buf), date_buf, sizeof(date_buf));

  for (uint32_t i = 0; i < 2; ++i) {
    if (s_ui.clock_time[i] != NULL) {
      lv_label_set_text(s_ui.clock_time[i], time_buf);
    }
    if (s_ui.clock_date[i] != NULL) {
      lv_label_set_text(s_ui.clock_date[i], date_buf);
    }
  }

  if (s_ui.quality_pill != NULL && s_ui.quality_pill_label != NULL) {
    if (rt.alerts_present) {
      lv_obj_set_style_bg_color(s_ui.quality_pill, k_color_coral, 0);
      lv_obj_set_style_bg_opa(s_ui.quality_pill, 46, 0);
      lv_obj_set_style_border_color(s_ui.quality_pill, k_color_coral, 0);
      lv_label_set_text(s_ui.quality_pill_label, "Need check");
    } else {
      lv_obj_set_style_bg_color(s_ui.quality_pill, k_color_mint, 0);
      lv_obj_set_style_bg_opa(s_ui.quality_pill, 42, 0);
      lv_obj_set_style_border_color(s_ui.quality_pill, k_color_mint, 0);
      lv_label_set_text(s_ui.quality_pill_label, "Water good");
    }
  }

  if (s_ui.sensor_value[UI_SENSOR_TEMP] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.1f C", rt.temperature_c);
    lv_label_set_text(s_ui.sensor_value[UI_SENSOR_TEMP], value_buf);
  }

  if (s_ui.sensor_value[UI_SENSOR_TURBIDITY] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.1f NTU", rt.turbidity_ntu);
    lv_label_set_text(s_ui.sensor_value[UI_SENSOR_TURBIDITY], value_buf);
  }

  if (s_ui.sensor_value[UI_SENSOR_TDS] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.0f ppm", rt.tds_ppm);
    lv_label_set_text(s_ui.sensor_value[UI_SENSOR_TDS], value_buf);
  }

  if (s_ui.sensor_value[UI_SENSOR_PH] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.1f pH", rt.ph_value);
    lv_label_set_text(s_ui.sensor_value[UI_SENSOR_PH], value_buf);
  }

  if (s_ui.sensor_value[UI_SENSOR_LEVEL] != NULL) {
    lv_label_set_text(s_ui.sensor_value[UI_SENSOR_LEVEL],
                      rt.water_ok ? "Water OK" : "Level low");
  }

  if (s_ui.sensor_value[UI_SENSOR_PRESSURE] != NULL) {
    (void)snprintf(value_buf, sizeof(value_buf), "%.0f hPa", rt.pressure_pa / 100.0f);
    lv_label_set_text(s_ui.sensor_value[UI_SENSOR_PRESSURE], value_buf);
  }

  if (s_ui.alarm_pill != NULL && s_ui.alarm_pill_label != NULL) {
    lv_obj_set_style_bg_color(s_ui.alarm_pill,
                              rt.alerts_present ? k_color_coral : k_color_mint, 0);
    lv_obj_set_style_bg_opa(s_ui.alarm_pill, rt.alerts_present ? 52 : 42, 0);
    lv_obj_set_style_border_color(s_ui.alarm_pill,
                                  rt.alerts_present ? k_color_coral : k_color_mint, 0);
    lv_label_set_text(s_ui.alarm_pill_label,
                      rt.alerts_present ? "Alarm" : "Normal");
    lv_obj_set_style_text_color(s_ui.alarm_pill_label,
                                rt.alerts_present ? k_color_text_main : k_color_good, 0);
  }

  if (s_ui.alarm_icon_box != NULL) {
    lv_obj_set_style_bg_color(s_ui.alarm_icon_box,
                              rt.alerts_present ? k_color_coral : k_color_mint, 0);
    lv_obj_set_style_bg_opa(s_ui.alarm_icon_box, rt.alerts_present ? 42 : 36, 0);
    lv_obj_set_style_border_color(s_ui.alarm_icon_box,
                                  rt.alerts_present ? k_color_coral : k_color_mint, 0);
  }

  if (s_ui.alarm_title != NULL) {
    lv_label_set_text(s_ui.alarm_title,
                      rt.alerts_present ? "Alarm active"
                                        : "All channels clear");
  }

  if (s_ui.alarm_headline != NULL) {
    if (rt.alert_count == 0U) {
      lv_label_set_text(s_ui.alarm_headline, "SAFE");
    } else if (rt.alert_count == 1U) {
      lv_label_set_text(s_ui.alarm_headline, "1 ACTIVE");
    } else {
      (void)snprintf(value_buf, sizeof(value_buf), "%u ACTIVE",
                     (unsigned)rt.alert_count);
      lv_label_set_text(s_ui.alarm_headline, value_buf);
    }
    lv_obj_set_style_text_color(s_ui.alarm_headline,
                                rt.alerts_present ? k_color_coral : k_color_text_main, 0);
  }

  if (s_ui.alert_label != NULL) {
    lv_label_set_text(s_ui.alert_label, rt.alert_text);
    lv_obj_set_style_text_color(s_ui.alert_label,
                                rt.alerts_present ? k_color_coral : k_color_text_soft, 0);
  }

  ui_update_alarm_chip(s_ui.alarm_metric_value[0],
                       rt.tds_high ? "High" : "Normal", rt.tds_high, k_color_coral);
  ui_update_alarm_chip(
      s_ui.alarm_metric_value[1],
      rt.ph_high ? "High" : (rt.ph_low ? "Low" : "Normal"), rt.ph_high || rt.ph_low,
      k_color_purple);
  ui_update_alarm_chip(s_ui.alarm_metric_value[2],
                       rt.water_level_low ? "Low" : "Normal", rt.water_level_low,
                       k_color_warm);


  ui_update_device_visual(UI_DEVICE_HEATER, rt.heater_on,
                          rt.heater_on ? "Heating" : "Standby");
  ui_update_device_visual(UI_DEVICE_FAN, rt.fan_on,
                          rt.fan_on ? "Cooling" : "Standby");
  ui_update_device_visual(UI_DEVICE_AERATION, rt.aeration_on,
                          rt.aeration_on ? "Cycling" : "Standby");
  ui_update_device_visual(UI_DEVICE_FILTER, rt.filter_on,
                          rt.filter_on ? "Filtering" : "Manual");
  ui_update_device_visual(UI_DEVICE_LED, rt.led_on,
                          rt.led_on ? "Schedule" : "Standby");

  ui_update_auto_mode_visual();

  ui_refresh_settings_values();
}

static void ui_create_top_tabs(lv_obj_t *parent, bool overview_active) {
  lv_obj_t *tabs =
      ui_create_box(parent, 314, 22, 172, 38, k_color_chip, 110,
                    lv_color_hex(0x344D62), LV_RADIUS_CIRCLE);

  lv_obj_t *overview = ui_create_box(
      tabs, 4, 4, 78, 30,
      overview_active ? k_color_aqua : k_color_chip,
      overview_active ? 70 : 0,
      overview_active ? k_color_aqua : k_color_chip, LV_RADIUS_CIRCLE);
  lv_obj_add_flag(overview, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(overview, ui_nav_event_cb, LV_EVENT_CLICKED, "overview");
  (void)ui_create_label(overview, 0, 7, 78, "Overview", &lv_font_montserrat_14,
                        k_color_text_main, LV_TEXT_ALIGN_CENTER);

  lv_obj_t *settings = ui_create_box(
      tabs, 88, 4, 80, 30,
      overview_active ? k_color_chip : k_color_aqua,
      overview_active ? 0 : 70,
      overview_active ? k_color_chip : k_color_aqua, LV_RADIUS_CIRCLE);
  lv_obj_add_flag(settings, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(settings, ui_nav_event_cb, LV_EVENT_CLICKED, "settings");
  (void)ui_create_label(settings, 0, 7, 80, "Thresholds", &lv_font_montserrat_14,
                        k_color_text_main, LV_TEXT_ALIGN_CENTER);
}

static void ui_create_brand_block(lv_obj_t *parent, bool settings_page) {
  lv_obj_t *mark = ui_create_box(
      parent, 18, 18, 52, 52, settings_page ? k_color_mint : k_color_aqua, 52,
      settings_page ? k_color_mint : k_color_aqua, 16);
  (void)ui_create_label(mark, 0, 14, 52, settings_page ? "CFG" : "AS",
                        &lv_font_montserrat_16, k_color_text_main,
                        LV_TEXT_ALIGN_CENTER);

  (void)ui_create_label(parent, 84, 18, 160,
                        settings_page ? "CONFIG PANEL" : "STM32 H743",
                        &lv_font_montserrat_14, k_color_text_faint,
                        LV_TEXT_ALIGN_LEFT);
  (void)ui_create_label(parent, 84, 34, 260,
                        settings_page ? "Threshold & Linkage" : "AquaSense Aquarium",
                        &lv_font_montserrat_20, k_color_text_main,
                        LV_TEXT_ALIGN_LEFT);
  (void)ui_create_label(
      parent, 84, 56, 260,
      settings_page ? "Tap value to edit threshold"
                    : "Real-time monitoring system",
      &lv_font_montserrat_14, k_color_text_soft, LV_TEXT_ALIGN_LEFT);
}

static lv_obj_t *ui_create_alarm_row(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                     const char *title, lv_obj_t **value_out) {
  lv_obj_t *row =
      ui_create_box(parent, x, y, 208, 22, k_color_chip, 105,
                    lv_color_hex(0x384F62), 11);
  (void)ui_create_label(row, 8, 4, 100, title, &lv_font_montserrat_14,
                        k_color_text_faint, LV_TEXT_ALIGN_LEFT);
  lv_obj_t *value =
      ui_create_label(row, 116, 4, 82, "--", &lv_font_montserrat_14,
                      k_color_text_main, LV_TEXT_ALIGN_RIGHT);
  if (value_out != NULL) {
    *value_out = value;
  }
  return row;
}

static void ui_create_sensor_card(lv_obj_t *parent, uint32_t index,
                                  lv_coord_t x, lv_coord_t y,
                                  lv_color_t accent, const char *icon_text,
                                  const char *label_text) {
  lv_obj_t *card = ui_create_box(parent, x, y, 154, 72, k_color_card, 125,
                                 lv_color_hex(0x354C5F), 20);
  lv_obj_t *icon = ui_create_box(card, 10, 10, 24, 24, accent, 60, accent, 10);
  (void)ui_create_label(icon, 0, 3, 24, icon_text, &lv_font_montserrat_14,
                        k_color_text_main, LV_TEXT_ALIGN_CENTER);
  (void)ui_create_label(card, 42, 11, 100, label_text, &lv_font_montserrat_14,
                        k_color_text_soft, LV_TEXT_ALIGN_LEFT);
  s_ui.sensor_value[index] =
      ui_create_label(card, 10, 38, 132, "--", &lv_font_montserrat_24,
                      k_color_text_main, LV_TEXT_ALIGN_LEFT);
}

static void ui_device_toggle_event_cb(lv_event_t *e) {
  uint32_t index;
  const char *toast_msg = NULL;

  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  /* 自动模式下禁止手动控制 */
  if (g_auto_ctrl_enabled) {
    ui_show_toast("Auto mode, manual disabled");
    return;
  }

  index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

  switch (index) {
  case UI_DEVICE_HEATER:
    if (g_heater_state) {
      Heater_Ctrl(0);
      toast_msg = "Heater OFF";
    } else {
      Heater_Ctrl(1);
      toast_msg = "Heater ON";
    }
    break;
  case UI_DEVICE_FAN:
    if (g_fan_state) {
      Fan_Ctrl(0);
      toast_msg = "Fan OFF";
    } else {
      Fan_Ctrl(1);
      toast_msg = "Fan ON";
    }
    break;
  case UI_DEVICE_AERATION:
    if (g_oxygenpump_state) {
      OxygenPump_Ctrl(0);
      toast_msg = "Aeration OFF";
    } else {
      OxygenPump_Ctrl(1);
      toast_msg = "Aeration ON";
    }
    break;
  case UI_DEVICE_FILTER:
    if (g_submersiblepump_state) {
      SubmersiblePump_Ctrl(0);
      toast_msg = "Water Pump OFF";
    } else {
      SubmersiblePump_Ctrl(1);
      toast_msg = "Water Pump ON";
    }
    break;
  case UI_DEVICE_LED:
    if (aquarium_light_is_on()) {
      aquarium_light_off();
      toast_msg = "LED OFF";
    } else {
      aquarium_light_on();
      toast_msg = "LED ON";
    }
    break;
  default:
    break;
  }

  if (toast_msg != NULL) {
    ui_show_toast(toast_msg);
  }
}

static void ui_auto_mode_toggle_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  g_auto_ctrl_enabled = g_auto_ctrl_enabled ? 0 : 1;
  ui_show_toast(g_auto_ctrl_enabled ? "Auto mode ON" : "Manual mode ON");

  /* 同步上报 device_operating_state 到华为云
   * 云端定义: 0=自动, 1=手动  (与 g_auto_ctrl_enabled 语义相反) */
  MQTT_ReportIntVal("device_operating_state", g_auto_ctrl_enabled ? 0 : 1);
}

static void ui_update_auto_mode_visual(void) {
  if (s_ui.auto_mode_button == NULL || s_ui.auto_mode_label == NULL) {
    return;
  }

  if (g_auto_ctrl_enabled) {
    lv_obj_set_style_bg_color(s_ui.auto_mode_button, k_color_aqua, 0);
    lv_obj_set_style_bg_opa(s_ui.auto_mode_button, 48, 0);
    lv_obj_set_style_border_color(s_ui.auto_mode_button, k_color_aqua, 0);
    lv_obj_set_style_shadow_width(s_ui.auto_mode_button, 10, 0);
    lv_obj_set_style_shadow_color(s_ui.auto_mode_button, k_color_aqua, 0);
    lv_obj_set_style_shadow_opa(s_ui.auto_mode_button, 40, 0);
    lv_obj_set_style_shadow_ofs_y(s_ui.auto_mode_button, 0, 0);
    lv_label_set_text(s_ui.auto_mode_label, "Auto");
  } else {
    lv_obj_set_style_bg_color(s_ui.auto_mode_button, k_color_warm, 0);
    lv_obj_set_style_bg_opa(s_ui.auto_mode_button, 48, 0);
    lv_obj_set_style_border_color(s_ui.auto_mode_button, k_color_warm, 0);
    lv_obj_set_style_shadow_width(s_ui.auto_mode_button, 10, 0);
    lv_obj_set_style_shadow_color(s_ui.auto_mode_button, k_color_warm, 0);
    lv_obj_set_style_shadow_opa(s_ui.auto_mode_button, 40, 0);
    lv_obj_set_style_shadow_ofs_y(s_ui.auto_mode_button, 0, 0);
    lv_label_set_text(s_ui.auto_mode_label, "Manual");
  }
}

static void ui_build_overview_screen(void) {
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_remove_style_all(screen);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, k_color_bg_top, 0);
  lv_obj_set_style_bg_grad_color(screen, k_color_bg_bottom, 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  s_ui.overview_screen = screen;

  (void)ui_create_box(screen, 644, -68, 220, 220, k_color_aqua, 28, k_color_aqua,
                      LV_RADIUS_CIRCLE);
  (void)ui_create_box(screen, -70, 334, 220, 220, lv_color_hex(0x4AA9FF),
                      22, lv_color_hex(0x4AA9FF), LV_RADIUS_CIRCLE);

  ui_create_brand_block(screen, false);
  ui_create_top_tabs(screen, true);
  s_ui.clock_time[0] = ui_create_label(screen, 616, 18, 148, "--:--:--",
                                       &lv_font_montserrat_30, k_color_text_main,
                                       LV_TEXT_ALIGN_RIGHT);
  s_ui.clock_date[0] = ui_create_label(screen, 614, 52, 150, "RTC not synced",
                                       &lv_font_montserrat_14, k_color_text_soft,
                                       LV_TEXT_ALIGN_RIGHT);

  lv_obj_t *sensor_panel = ui_create_panel(screen, 18, 74, 512, 242);
  (void)ui_create_label(sensor_panel, 14, 14, 180, "LIVE SENSORS",
                        &lv_font_montserrat_14, k_color_text_faint, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_label(sensor_panel, 14, 32, 220, "Core Aquarium Metrics",
                        &lv_font_montserrat_16, k_color_text_main, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_pill(sensor_panel, 300, 12, 92, 26, k_color_chip, 130,
                       lv_color_hex(0x384F62), "Refresh 1s",
                       k_color_text_soft, NULL);
  s_ui.quality_pill = ui_create_pill(sensor_panel, 400, 12, 94, 26, k_color_mint,
                                     42, k_color_mint, "Water good", k_color_good,
                                     &s_ui.quality_pill_label);

  ui_create_sensor_card(sensor_panel, UI_SENSOR_TEMP, 14, 66, k_color_warm, "T",
                        "Temperature");
  ui_create_sensor_card(sensor_panel, UI_SENSOR_TURBIDITY, 178, 66, k_color_aqua,
                        "U", "Turbidity");
  ui_create_sensor_card(sensor_panel, UI_SENSOR_TDS, 342, 66, k_color_mint, "D",
                        "TDS");
  ui_create_sensor_card(sensor_panel, UI_SENSOR_PH, 14, 148, k_color_purple, "P",
                        "PH");
  ui_create_sensor_card(sensor_panel, UI_SENSOR_LEVEL, 178, 148, k_color_coral,
                        "L", "Water Level");
  ui_create_sensor_card(sensor_panel, UI_SENSOR_PRESSURE, 342, 148, k_color_steel,
                        "A", "Pressure");

  lv_obj_t *alarm_panel = ui_create_panel(screen, 542, 74, 236, 242);
  (void)ui_create_label(alarm_panel, 14, 12, 120, "ALARM BUS",
                        &lv_font_montserrat_14, k_color_text_faint, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_label(alarm_panel, 14, 28, 120, "Alarm Panel",
                        &lv_font_montserrat_16, k_color_text_main, LV_TEXT_ALIGN_LEFT);
  s_ui.alarm_pill = ui_create_pill(alarm_panel, 146, 10, 76, 26, k_color_mint, 42,
                                   k_color_mint, "Normal", k_color_good,
                                   &s_ui.alarm_pill_label);

  s_ui.alarm_icon_box =
      ui_create_box(alarm_panel, 14, 52, 36, 36, k_color_mint, 36, k_color_mint, 12);
  (void)ui_create_label(s_ui.alarm_icon_box, 0, 9, 36, "AL", &lv_font_montserrat_14,
                        k_color_text_main, LV_TEXT_ALIGN_CENTER);
  s_ui.alarm_title = ui_create_label(alarm_panel, 60, 52, 158, "All channels clear",
                                     &lv_font_montserrat_14, k_color_text_soft,
                                     LV_TEXT_ALIGN_LEFT);
  s_ui.alarm_headline = ui_create_label(alarm_panel, 60, 68, 158, "SAFE",
                                        &lv_font_montserrat_20, k_color_text_main,
                                        LV_TEXT_ALIGN_LEFT);
  s_ui.alert_label = ui_create_label(alarm_panel, 14, 96, 208, "All readings in range",
                                     &lv_font_montserrat_14, k_color_text_soft,
                                     LV_TEXT_ALIGN_LEFT);
  lv_label_set_long_mode(s_ui.alert_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_height(s_ui.alert_label, 24);

  (void)ui_create_alarm_row(alarm_panel, 14, 126, "TDS", &s_ui.alarm_metric_value[0]);
  (void)ui_create_alarm_row(alarm_panel, 14, 152, "pH", &s_ui.alarm_metric_value[1]);
  (void)ui_create_alarm_row(alarm_panel, 14, 178, "Water Level",
                            &s_ui.alarm_metric_value[2]);

  lv_obj_t *device_panel = ui_create_panel(screen, 18, 328, 760, 118);
  static const char *device_names[UI_DEVICE_COUNT] = {"Heater", "Cooling Fan",
                                                      "Aeration", "Water Pump",
                                                      "LED Strip"};
  static const lv_coord_t device_x[UI_DEVICE_COUNT] = {14, 163, 312, 461, 610};

  (void)ui_create_label(device_panel, 14, 12, 140, "ACTUATORS",
                        &lv_font_montserrat_14, k_color_text_faint, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_label(device_panel, 14, 30, 180, "Device Control",
                        &lv_font_montserrat_16, k_color_text_main, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_pill(device_panel, 554, 12, 92, 26, k_color_chip, 130,
                       lv_color_hex(0x384F62), "Tap toggle",
                       k_color_text_soft, NULL);

  /* ── Auto/Manual mode toggle button ── */
  {
    lv_obj_t *auto_btn = ui_create_box(device_panel, 654, 8, 92, 34, k_color_aqua, 48,
                                       k_color_aqua, 16);
    lv_obj_add_flag(auto_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(auto_btn, ui_auto_mode_toggle_event_cb, LV_EVENT_CLICKED, NULL);
    s_ui.auto_mode_label = ui_create_label(auto_btn, 0, 9, 92, "Auto",
                                           &lv_font_montserrat_14,
                                           k_color_text_main, LV_TEXT_ALIGN_CENTER);
    s_ui.auto_mode_button = auto_btn;
  }

  for (uint32_t i = 0; i < UI_DEVICE_COUNT; ++i) {
    lv_obj_t *button = ui_create_box(device_panel, device_x[i], 52, 141, 50, k_color_card,
                                     130, lv_color_hex(0x354C5F), 18);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button, ui_device_toggle_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)i);
    (void)ui_create_label(button, 10, 10, 120, device_names[i], &lv_font_montserrat_14,
                          k_color_text_main, LV_TEXT_ALIGN_LEFT);
    s_ui.device_state[i] = ui_create_label(button, 10, 28, 120, "--",
                                           &lv_font_montserrat_14, k_color_text_soft,
                                           LV_TEXT_ALIGN_LEFT);
    s_ui.device_button[i] = button;
  }
}

static void ui_create_range_boxes(lv_obj_t *parent, const char *left_title,
                                  const char *left_value, const char *right_title,
                                  const char *right_value, uint32_t left_setting_id,
                                  uint32_t right_setting_id, bool single_column,
                                  lv_obj_t **left_value_out,
                                  lv_obj_t **right_value_out) {
  lv_obj_t *left = ui_create_box(parent, 10, 46, single_column ? 210 : 102, 44,
                                 k_color_chip, 105, lv_color_hex(0x384F62), 16);
  (void)ui_create_label(left, 10, 7, 80, left_title, &lv_font_montserrat_14,
                        k_color_text_faint, LV_TEXT_ALIGN_LEFT);
  lv_obj_t *left_value_label =
      ui_create_label(left, 10, 24, 140, left_value, &lv_font_montserrat_14,
                      k_color_text_main, LV_TEXT_ALIGN_LEFT);

  if (left_value_out != NULL) {
    *left_value_out = left_value_label;
  }
  ui_make_setting_box_editable(left, left_setting_id);

  if (!single_column) {
    lv_obj_t *right = ui_create_box(parent, 118, 46, 102, 44, k_color_chip, 105,
                                    lv_color_hex(0x384F62), 16);
    (void)ui_create_label(right, 10, 7, 80, right_title, &lv_font_montserrat_14,
                          k_color_text_faint, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *right_value_label =
        ui_create_label(right, 10, 24, 140, right_value, &lv_font_montserrat_14,
                        k_color_text_main, LV_TEXT_ALIGN_LEFT);
    if (right_value_out != NULL) {
      *right_value_out = right_value_label;
    }
    ui_make_setting_box_editable(right, right_setting_id);
  } else if (right_value_out != NULL) {
    *right_value_out = NULL;
  }
}

static void ui_create_setting_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                   const char *title, const char *tag,
                                   lv_color_t tag_color, const char *left_title,
                                   const char *left_value, const char *right_title,
                                   const char *right_value, uint32_t left_setting_id,
                                   uint32_t right_setting_id, bool single_column,
                                   lv_obj_t **left_value_out,
                                   lv_obj_t **right_value_out) {
  lv_obj_t *card = ui_create_box(parent, x, y, 230, 106, k_color_card, 125,
                                 lv_color_hex(0x354C5F), 20);
  (void)ui_create_label(card, 12, 12, 110, title, &lv_font_montserrat_16,
                        k_color_text_main, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_pill(card, 152, 10, 66, 22, tag_color, 40, tag_color, tag,
                       k_color_text_main, NULL);
  ui_create_range_boxes(card, left_title, left_value, right_title, right_value,
                        left_setting_id, right_setting_id, single_column,
                        left_value_out, right_value_out);
}

static void ui_build_settings_screen(void) {
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_remove_style_all(screen);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, k_color_bg_top, 0);
  lv_obj_set_style_bg_grad_color(screen, k_color_bg_bottom, 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  s_ui.settings_screen = screen;

  (void)ui_create_box(screen, -54, -70, 240, 240, k_color_mint, 20, k_color_mint,
                      LV_RADIUS_CIRCLE);
  (void)ui_create_box(screen, 650, 316, 220, 220, k_color_purple, 20, k_color_purple,
                      LV_RADIUS_CIRCLE);

  ui_create_brand_block(screen, true);
  ui_create_top_tabs(screen, false);
  s_ui.clock_time[1] = ui_create_label(screen, 616, 18, 148, "--:--:--",
                                       &lv_font_montserrat_30, k_color_text_main,
                                       LV_TEXT_ALIGN_RIGHT);
  s_ui.clock_date[1] = ui_create_label(screen, 614, 52, 150, "RTC not synced",
                                       &lv_font_montserrat_14, k_color_text_soft,
                                       LV_TEXT_ALIGN_RIGHT);

  lv_obj_t *settings_board = ui_create_panel(screen, 18, 88, 764, 374);
  (void)ui_create_label(settings_board, 14, 14, 180, "PARAMETER CARDS",
                        &lv_font_montserrat_14, k_color_text_faint, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_label(settings_board, 14, 32, 260, "Threshold Settings",
                        &lv_font_montserrat_16, k_color_text_main, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_pill(settings_board, 596, 12, 82, 26, k_color_aqua, 42,
                       k_color_aqua, "Editable",
                       k_color_text_main, NULL);
  (void)ui_create_pill(settings_board, 686, 12, 64, 26, k_color_mint, 42, k_color_mint,
                       "Default", k_color_good, NULL);

  lv_obj_t *save_btn = ui_create_box(settings_board, 14, 58, 232, 36, k_color_aqua, 62,
                                     k_color_aqua, 16);
  lv_obj_add_flag(save_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(save_btn, ui_hint_event_cb, LV_EVENT_CLICKED,
                      "Settings saved");
  (void)ui_create_label(save_btn, 0, 10, 232, "Save config", &lv_font_montserrat_14,
                        lv_color_hex(0x062131), LV_TEXT_ALIGN_CENTER);

  lv_obj_t *default_btn =
      ui_create_box(settings_board, 258, 58, 196, 36, k_color_warm, 38, k_color_warm, 16);
  lv_obj_add_flag(default_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(default_btn, ui_restore_defaults_event_cb, LV_EVENT_CLICKED, NULL);
  (void)ui_create_label(default_btn, 0, 10, 196, "Restore defaults",
                        &lv_font_montserrat_14, k_color_text_main, LV_TEXT_ALIGN_CENTER);

  lv_obj_t *back_btn =
      ui_create_box(settings_board, 466, 58, 284, 36, k_color_chip, 130,
                    lv_color_hex(0x384F62), 16);
  lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(back_btn, ui_nav_event_cb, LV_EVENT_CLICKED, "overview");
  (void)ui_create_label(back_btn, 0, 10, 284, "Back to overview",
                        &lv_font_montserrat_14, k_color_text_main, LV_TEXT_ALIGN_CENTER);

  ui_create_setting_card(settings_board, 14, 108, "Temperature", "Range",
                         k_color_warm, "Lower", "--", "Upper", "--",
                         UI_SETTING_TEMP_LOWER_LIMIT, UI_SETTING_TEMP_UPPER_LIMIT, false,
                         &s_ui.setting_value[UI_SETTING_TEMP_LOWER_LIMIT],
                         &s_ui.setting_value[UI_SETTING_TEMP_UPPER_LIMIT]);
  ui_create_setting_card(settings_board, 262, 108, "Turbidity", "Pump",
                         k_color_aqua, "Start", "--", "Stop", "--",
                         UI_SETTING_TURBIDITY_ON, UI_SETTING_TURBIDITY_OFF, false,
                         &s_ui.setting_value[UI_SETTING_TURBIDITY_ON],
                         &s_ui.setting_value[UI_SETTING_TURBIDITY_OFF]);
  ui_create_setting_card(settings_board, 510, 108, "TDS", "Alarm",
                         k_color_mint, "High", "--", "", "",
                         UI_SETTING_TDS_HIGH, UI_SETTING_NONE, true,
                         &s_ui.setting_value[UI_SETTING_TDS_HIGH], NULL);
  ui_create_setting_card(settings_board, 14, 226, "PH", "Balance",
                         k_color_purple, "Low", "--", "High", "--",
                         UI_SETTING_PH_LOW, UI_SETTING_PH_HIGH, false,
                         &s_ui.setting_value[UI_SETTING_PH_LOW],
                         &s_ui.setting_value[UI_SETTING_PH_HIGH]);

  lv_obj_t *level_card = ui_create_box(settings_board, 262, 226, 230, 106, k_color_card,
                                       125, lv_color_hex(0x354C5F), 20);
  (void)ui_create_label(level_card, 12, 12, 110, "Water Level", &lv_font_montserrat_16,
                        k_color_text_main, LV_TEXT_ALIGN_LEFT);
  (void)ui_create_pill(level_card, 144, 10, 74, 22, k_color_coral, 40, k_color_coral,
                       "Contact", k_color_text_main, NULL);
  (void)ui_create_pill(level_card, 12, 46, 206, 22, k_color_mint, 34, k_color_mint,
                       "Detected = normal", k_color_good, NULL);
  (void)ui_create_pill(level_card, 12, 74, 206, 22, k_color_chip, 115,
                       lv_color_hex(0x384F62), "Missing = alarm",
                       k_color_text_soft, NULL);

  ui_create_setting_card(settings_board, 510, 226, "Pressure", "Boost",
                         k_color_steel, "Low", "--", "Effect", "--",
                         UI_SETTING_PRESSURE_LOW, UI_SETTING_NONE, false,
                         &s_ui.setting_value[UI_SETTING_PRESSURE_LOW],
                         &s_ui.setting_value[UI_SETTING_PRESSURE_EFFECT]);

  s_ui.settings_note = ui_create_label(settings_board, 14, 344, 736, "--",
                                       &lv_font_montserrat_14, k_color_text_soft,
                                       LV_TEXT_ALIGN_LEFT);
}

void lv_ui_create(void) {
  memset(&s_ui, 0, sizeof(s_ui));
  s_ui.editing_setting = UI_SETTING_NONE;

  ui_build_overview_screen();
  ui_build_settings_screen();


  s_ui.toast = ui_create_box(lv_layer_top(), 215, 430, 370, 34,
                             lv_color_hex(0x0D2436), 220,
                             lv_color_hex(0x3E5B70), 14);
  s_ui.toast_label = ui_create_label(s_ui.toast, 10, 9, 350, "", &lv_font_montserrat_14,
                                     k_color_text_main, LV_TEXT_ALIGN_CENTER);
  lv_obj_add_flag(s_ui.toast, LV_OBJ_FLAG_HIDDEN);

  s_ui.toast_timer = lv_timer_create(ui_hide_toast_timer_cb, 1600, NULL);
  lv_timer_pause(s_ui.toast_timer);

  s_ui.refresh_timer = lv_timer_create(ui_refresh_timer_cb, 1000, NULL);
  ui_refresh_timer_cb(NULL);

  lv_scr_load(s_ui.overview_screen);
}
