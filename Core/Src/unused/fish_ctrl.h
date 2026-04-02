#ifndef __FISH_CTRL_H__
#define __FISH_CTRL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* =====================================================================
 *  fish_ctrl — 鱼缸闭环控制模块
 *
 *  控制策略:
 *    温度  → 数字PID   → 水泥电阻(加热) / 风扇(制冷)
 *    浊度  → 阈值比较   → 潜水泵(过滤)
 *    TDS   → 阈值比较   → 屏幕告警 (无执行设备)
 *    PH    → 阈值比较   → 屏幕告警 (无执行设备)
 *    水位  → 二值检测   → 屏幕告警
 *    气压  → 结合定时   → 增氧泵
 *    RTC   → 时段控制   → 水族灯
 * ===================================================================== */

/* ---------- PID 控制器结构体 ---------- */
typedef struct {
    float kp;           /* 比例系数 */
    float ki;           /* 积分系数 */
    float kd;           /* 微分系数 */
    float setpoint;     /* 目标值 */
    float integral;     /* 积分累计 */
    float prev_error;   /* 上次误差 */
    float out_min;      /* 输出下限 */
    float out_max;      /* 输出上限 */
    float integral_max; /* 积分限幅 (防积分饱和) */
} PID_Controller_t;

/* ---------- 告警标志 (供 UI 查询) ---------- */
typedef struct {
    bool tds_high;          /* TDS 超标 */
    bool ph_high;           /* PH 偏高 */
    bool ph_low;            /* PH 偏低 */
    bool water_level_low;   /* 水位过低 */
} FishCtrl_Alerts_t;

/* ---------- 控制参数: 用户可调阈值 ---------- */
typedef struct {
    /* 温度 PID */
    float temp_setpoint;        /* 目标温度 ℃, 默认 26.0 */
    float temp_deadband;        /* 死区 ℃, 默认 0.5 (±0.5℃内不动作) */

    /* 浊度 */
    float turbidity_on_thresh;  /* 浊度开启过滤阈值 NTU, 默认 50.0 */
    float turbidity_off_thresh; /* 浊度关闭过滤阈值 NTU, 默认 20.0 (回差) */
    uint32_t filter_min_run_ms; /* 过滤泵最少运行时间 ms, 默认 60000 */
    uint32_t filter_interval_ms;/* 定时过滤间隔 ms, 默认 3600000 (1小时) */
    uint32_t filter_duration_ms;/* 定时过滤持续时间 ms, 默认 300000 (5分钟) */

    /* TDS */
    float tds_high_thresh;      /* TDS 告警阈值 ppm, 默认 500 */

    /* PH */
    float ph_high_thresh;       /* PH 偏高阈值, 默认 8.5 */
    float ph_low_thresh;        /* PH 偏低阈值, 默认 6.5 */

    /* 增氧泵 (定时 + 气压联合策略) */
    uint32_t aeration_on_ms;    /* 增氧开启持续 ms, 默认 300000 (5分钟) */
    uint32_t aeration_off_ms;   /* 增氧关闭间隔 ms, 默认 1800000 (30分钟) */
    float pressure_low_thresh;  /* 低气压阈值 Pa, 默认 99000 (低压时增加充氧) */

    /* LED 照明 (预留) */
    uint8_t  led_hour_on;       /* 开灯时间 (小时), 默认 8 */
    uint8_t  led_hour_off;      /* 关灯时间 (小时), 默认 20 */
} FishCtrl_Params_t;

/* ---------- 传感器数据输入结构体 ---------- */
typedef struct {
    float temperature;      /* DS18B20 温度 ℃ (已除10) */
    float turbidity_ntu;    /* 浊度 NTU */
    float tds_ppm;          /* TDS ppm */
    float ph;               /* PH 值 */
    uint8_t water_level;    /* 水位: 1=有水, 0=低水位 */
    float pressure_pa;      /* 大气压 Pa */
} FishCtrl_SensorData_t;

/* ---------- 控制输出结构体 (供调试/UI显示) ---------- */
typedef struct {
    float    temp_pid_output;   /* 温度 PID 原始输出 (-100~100) */
    uint8_t  heater_pwm;        /* 水泥电阻 PWM 0~100 */
    uint8_t  fan_pwm;           /* 风扇 PWM 0~100 (预留, 当前只 on/off) */
    bool     heater_on;         /* 加热器状态 */
    bool     fan_on;            /* 风扇状态 */
    bool     filter_pump_on;    /* 过滤泵状态 */
    bool     aeration_pump_on;  /* 增氧泵状态 */
    bool     led_on;            /* LED状态 (预留) */
} FishCtrl_Output_t;

/* ==================== API ==================== */

/**
 * @brief  初始化控制模块 (使用默认参数)
 */
void FishCtrl_Init(void);

/**
 * @brief  将参数恢复为系统默认值
 */
void FishCtrl_LoadDefaults(void);

/**
 * @brief  获取参数指针 (用于修改参数)
 */
FishCtrl_Params_t *FishCtrl_GetParams(void);

/**
 * @brief  获取告警标志 (只读)
 */
const FishCtrl_Alerts_t *FishCtrl_GetAlerts(void);

/**
 * @brief  获取控制输出 (只读, 供 UI 显示)
 */
const FishCtrl_Output_t *FishCtrl_GetOutput(void);

/**
 * @brief  获取当前告警摘要文本
 */
const char *FishCtrl_GetAlertSummary(void);

/**
 * @brief  控制周期执行函数 (在 FreeRTOS 任务中周期调用)
 * @param  sensor  当前传感器数据
 * @param  now_ms  当前系统时间 (HAL_GetTick)
 */
void FishCtrl_Update(const FishCtrl_SensorData_t *sensor, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* __FISH_CTRL_H__ */
