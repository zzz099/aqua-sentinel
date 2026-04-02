/**
 * @file fish_ctrl.c
 * 鱼缸数字闭环控制算法实现
 *
 * 控制策略:
 *   温度  → 数字 PID → 水泥电阻(加热, PWM) / 风扇(制冷, PWM)
 *   浊度  → 迟滞阈值 + 定时 → 潜水泵(过滤)
 *   TDS   → 阈值 → 告警 (无执行设备)
 *   PH    → 阈值 → 告警 (无执行设备)
 *   水位  → 二值 → 告警
 *   气压  → 定时 + 气压修正 → 增氧泵
 *   RTC   → 时段策略 → 水族灯
 */

#include "fish_ctrl.h"

#include "aeration_pump.h"
#include "aquarium_light.h"
#include "cement_resistor.h"
#include "cooling_fan.h"
#include "rtc.h"
#include "submersible_pump.h"
#include "tb6612fng.h"
#include "waterlevel.h"

#include <stdio.h>

/* ===================== 内部变量 ===================== */

static PID_Controller_t  s_temp_pid;
static FishCtrl_Params_t s_params;
static FishCtrl_Alerts_t s_alerts;
static FishCtrl_Output_t s_output;
static char             s_alert_summary[96];

/* 过滤泵状态机 */
static bool     s_filter_running;
static uint32_t s_filter_start_ms;     /* 本轮开启时间戳 */
static uint32_t s_filter_last_off_ms;  /* 上轮关闭时间戳 */

/* 增氧泵定时状态机 */
static bool     s_aeration_running;
static uint32_t s_aeration_toggle_ms;   /* 上次切换时间戳 */
static bool     s_aeration_low_pressure; /* 低气压增强标记 */

/* 温度 PID 采样时间 */
static uint32_t s_temp_last_update_ms;

/* ===================== PID 辅助 ===================== */

static void PID_Init(PID_Controller_t *pid, float kp, float ki, float kd,
                     float setpoint, float out_min, float out_max, float i_max)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->setpoint = setpoint;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->out_min = out_min;
    pid->out_max = out_max;
    pid->integral_max = i_max;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/**
 * @brief  数字位置式 PID 计算
 * @param  pid      控制器
 * @param  measured 当前测量值
 * @param  dt_sec   本次采样周期 (秒)
 * @return          控制输出 [out_min, out_max]
 *                  正值 → 需要加热; 负值 → 需要制冷
 */
static float PID_Compute(PID_Controller_t *pid, float measured, float dt_sec)
{
    float error;
    float derivative;
    float trial_integral;
    float output;

    if (dt_sec <= 0.0f) {
        dt_sec = 1.0f;
    }

    error = pid->setpoint - measured;
    derivative = (error - pid->prev_error) / dt_sec;
    trial_integral = pid->integral + error * dt_sec;
    trial_integral = clampf(trial_integral, -pid->integral_max, pid->integral_max);

    output = pid->kp * error + pid->ki * trial_integral + pid->kd * derivative;

    /*
     * 简单抗积分饱和:
     * 只有在输出未继续向饱和方向推进时，才接受新的积分值。
     */
    if (((output < pid->out_max) || (error < 0.0f)) &&
        ((output > pid->out_min) || (error > 0.0f))) {
        pid->integral = trial_integral;
    }

    output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
    output = clampf(output, pid->out_min, pid->out_max);
    pid->prev_error = error;

    return output;
}

/* ===================== 告警辅助 ===================== */

static void AlertSummary_Reset(void)
{
    (void)snprintf(s_alert_summary, sizeof(s_alert_summary), "All readings in range");
}

static void AlertSummary_Append(const char *text)
{
    int written;
    size_t used = 0U;

    while ((used < sizeof(s_alert_summary)) && (s_alert_summary[used] != '\0')) {
        ++used;
    }

    if (used >= sizeof(s_alert_summary) - 1U) {
        return;
    }

    if (used == 0U) {
        written = snprintf(s_alert_summary, sizeof(s_alert_summary), "%s", text);
    } else {
        written = snprintf(&s_alert_summary[used], sizeof(s_alert_summary) - used,
                           " | %s", text);
    }

    (void)written;
}

static void AlertSummary_Update(void)
{
    s_alert_summary[0] = '\0';

    if (s_alerts.tds_high) {
        AlertSummary_Append("TDS high");
    }
    if (s_alerts.ph_high) {
        AlertSummary_Append("pH high");
    }
    if (s_alerts.ph_low) {
        AlertSummary_Append("pH low");
    }
    if (s_alerts.water_level_low) {
        AlertSummary_Append("Low water");
    }

    if (s_alert_summary[0] == '\0') {
        AlertSummary_Reset();
    }
}

/* =================== 温度控制 =================== */

static void Ctrl_Temperature(float temp_celsius, uint32_t now_ms)
{
    float error = s_params.temp_setpoint - temp_celsius;
    float dt_sec;
    float pid_out;

    if (s_temp_last_update_ms == 0U) {
        dt_sec = 1.0f;
    } else {
        dt_sec = (float)(now_ms - s_temp_last_update_ms) / 1000.0f;
        dt_sec = clampf(dt_sec, 0.2f, 5.0f);
    }
    s_temp_last_update_ms = now_ms;

    /* 死区: 误差在 +/- deadband 内时关闭执行器并缓释积分 */
    if ((error > -s_params.temp_deadband) && (error < s_params.temp_deadband)) {
        s_temp_pid.integral *= 0.85f;
        s_temp_pid.prev_error = error;
        s_output.temp_pid_output = 0.0f;
        s_output.heater_pwm = 0;
        s_output.fan_pwm = 0;
        s_output.heater_on = false;
        s_output.fan_on = false;
        off_cement_resistor();
        off_cooling_fan();
        return;
    }

    pid_out = PID_Compute(&s_temp_pid, temp_celsius, dt_sec);
    s_output.temp_pid_output = pid_out;

    if (pid_out > 0.0f) {
        /* 需要加热: 启动水泥电阻, 关闭风扇 */
        s_output.heater_pwm = 100;
        s_output.heater_on = true;
        s_output.fan_pwm = 0;
        s_output.fan_on = false;

        start_cement_resistor();
        off_cooling_fan();
    } else {
        /* 需要制冷: 启动风扇, 关闭水泥电阻 */
        s_output.fan_pwm = 100;
        s_output.fan_on = true;
        s_output.heater_pwm = 0;
        s_output.heater_on = false;

        start_cooling_fan();
        off_cement_resistor();
    }
}

/* =================== 浊度 → 过滤泵控制 =================== */

static void Ctrl_Turbidity(float ntu, uint32_t now_ms)
{
    if (!s_filter_running) {
        bool trigger = false;

        if (ntu > s_params.turbidity_on_thresh) {
            trigger = true;
        }

        if ((now_ms - s_filter_last_off_ms) >= s_params.filter_interval_ms) {
            trigger = true;
        }

        if (trigger) {
            start_submersible_pump();
            s_filter_running = true;
            s_filter_start_ms = now_ms;
        }
    } else {
        uint32_t elapsed = now_ms - s_filter_start_ms;

        if (elapsed < s_params.filter_min_run_ms) {
            s_output.filter_pump_on = s_filter_running;
            return;
        }

        if ((elapsed >= s_params.filter_duration_ms) &&
            (ntu < s_params.turbidity_off_thresh)) {
            off_submersible_pump();
            s_filter_running = false;
            s_filter_last_off_ms = now_ms;
        } else if (ntu < s_params.turbidity_off_thresh) {
            off_submersible_pump();
            s_filter_running = false;
            s_filter_last_off_ms = now_ms;
        }
    }

    s_output.filter_pump_on = s_filter_running;
}

/* =================== TDS / PH / 水位 告警 =================== */

static void Ctrl_Alerts(float tds, float ph, uint8_t water_level)
{
    s_alerts.tds_high = (tds > s_params.tds_high_thresh);
    s_alerts.ph_high = (ph > s_params.ph_high_thresh);
    s_alerts.ph_low = (ph < s_params.ph_low_thresh);
    s_alerts.water_level_low = (water_level == 0U);
    AlertSummary_Update();
}

/* =================== 增氧泵控制 (定时 + 气压联合) =================== */

static void Ctrl_Aeration(float pressure_pa, uint32_t now_ms)
{
    uint32_t off_duration = s_params.aeration_off_ms;
    uint32_t elapsed = now_ms - s_aeration_toggle_ms;

    s_aeration_low_pressure =
        (pressure_pa > 100.0f) && (pressure_pa < s_params.pressure_low_thresh);

    if (s_aeration_low_pressure) {
        off_duration /= 2U;
        if (off_duration < 60000U) {
            off_duration = 60000U;
        }
    }

    if (s_aeration_running) {
        if (elapsed >= s_params.aeration_on_ms) {
            off_aeration_pump();
            s_aeration_running = false;
            s_aeration_toggle_ms = now_ms;
        }
    } else {
        if (elapsed >= off_duration) {
            start_aeration_pump();
            s_aeration_running = true;
            s_aeration_toggle_ms = now_ms;
        }
    }

    s_output.aeration_pump_on = s_aeration_running;
}

/* =================== LED 照明时段控制 =================== */

static bool Ctrl_IsHourInRange(uint8_t current_hour, uint8_t hour_on,
                               uint8_t hour_off)
{
    if (hour_on == hour_off) {
        return true;
    }

    if (hour_on < hour_off) {
        return (current_hour >= hour_on) && (current_hour < hour_off);
    }

    return (current_hour >= hour_on) || (current_hour < hour_off);
}

static bool Ctrl_ReadRtcHour(uint8_t *hour_out)
{
    RTC_TimeTypeDef time_now = {0};
    RTC_DateTypeDef date_now = {0};

    if ((HAL_RTC_GetTime(&hrtc, &time_now, RTC_FORMAT_BIN) != HAL_OK) ||
        (HAL_RTC_GetDate(&hrtc, &date_now, RTC_FORMAT_BIN) != HAL_OK)) {
        return false;
    }

    if (time_now.Hours >= 24U) {
        return false;
    }

    *hour_out = (uint8_t)time_now.Hours;
    return true;
}

static void Ctrl_Lighting(void)
{
    uint8_t current_hour = 0U;
    bool light_on = false;

    if (Ctrl_ReadRtcHour(&current_hour)) {
        light_on = Ctrl_IsHourInRange(current_hour, s_params.led_hour_on,
                                      s_params.led_hour_off);
    }

    if (light_on) {
        aquarium_light_on();
    } else {
        aquarium_light_off();
    }

    s_output.led_on = light_on;
}

/* ===================== 公共 API ===================== */

void FishCtrl_LoadDefaults(void)
{
    s_params.temp_setpoint = 26.0f;
    s_params.temp_deadband = 0.5f;

    s_params.turbidity_on_thresh = 50.0f;
    s_params.turbidity_off_thresh = 20.0f;
    s_params.filter_min_run_ms = 60000U;      /* 1 分钟 */
    s_params.filter_interval_ms = 3600000U;   /* 1 小时 */
    s_params.filter_duration_ms = 300000U;    /* 5 分钟 */

    s_params.tds_high_thresh = 500.0f;
    s_params.ph_high_thresh = 8.5f;
    s_params.ph_low_thresh = 6.5f;

    s_params.aeration_on_ms = 300000U;        /* 5 分钟 */
    s_params.aeration_off_ms = 1800000U;      /* 30 分钟 */
    s_params.pressure_low_thresh = 99000.0f;  /* 99 kPa */

    s_params.led_hour_on = 8U;
    s_params.led_hour_off = 20U;

    s_temp_pid.setpoint = s_params.temp_setpoint;
    s_temp_pid.integral = 0.0f;
    s_temp_pid.prev_error = 0.0f;
}

void FishCtrl_Init(void)
{
    FishCtrl_LoadDefaults();

#if 0  /* ===== 调试阶段: 禁用 PID 初始化及执行器状态重置 ===== */
    /*
     * 温度 PID 默认参数:
     * Kp: 温差 1.0C 时给出较明显的驱动强度
     * Ki: 慢速消除稳态误差
     * Kd: 温度变化缓慢, 适度抑制超调
     */
    PID_Init(&s_temp_pid, 20.0f, 0.40f, 6.0f,
             s_params.temp_setpoint, -100.0f, 100.0f, 60.0f);

    s_filter_running = false;
    s_filter_start_ms = 0U;
    s_filter_last_off_ms = 0U;

    s_aeration_running = false;
    s_aeration_toggle_ms = 0U;
    s_aeration_low_pressure = false;

    s_temp_last_update_ms = 0U;

    s_output.temp_pid_output = 0.0f;
    s_output.heater_pwm = 0U;
    s_output.fan_pwm = 0U;
    s_output.heater_on = false;
    s_output.fan_on = false;
    s_output.filter_pump_on = false;
    s_output.aeration_pump_on = false;
    s_output.led_on = false;
#endif

    s_alerts.tds_high = false;
    s_alerts.ph_high = false;
    s_alerts.ph_low = false;
    s_alerts.water_level_low = false;
    AlertSummary_Reset();
}

FishCtrl_Params_t *FishCtrl_GetParams(void)
{
    return &s_params;
}

const FishCtrl_Alerts_t *FishCtrl_GetAlerts(void)
{
    return &s_alerts;
}

const FishCtrl_Output_t *FishCtrl_GetOutput(void)
{
    return &s_output;
}

const char *FishCtrl_GetAlertSummary(void)
{
    return s_alert_summary;
}

void FishCtrl_Update(const FishCtrl_SensorData_t *sensor, uint32_t now_ms)
{
#if 0  /* ===== 调试阶段: 禁用 PID 及所有自动控制, 改为手动操作执行设备 ===== */
    s_temp_pid.setpoint = s_params.temp_setpoint;

    /* 1. 温度 PID 闭环 */
    Ctrl_Temperature(sensor->temperature, now_ms);

    /* 2. 浊度 → 过滤泵 */
    Ctrl_Turbidity(sensor->turbidity_ntu, now_ms);

    /* 3. TDS / PH / 水位 告警 */
    Ctrl_Alerts(sensor->tds_ppm, sensor->ph, sensor->water_level);

    /* 4. 增氧泵 (定时 + 气压) */
    Ctrl_Aeration(sensor->pressure_pa, now_ms);

    /* 5. LED 照明 */
    Ctrl_Lighting();
#else
    /* 调试模式: 仅更新告警信息, 不驱动任何执行设备 */
    Ctrl_Alerts(sensor->tds_ppm, sensor->ph, sensor->water_level);
#endif
}
