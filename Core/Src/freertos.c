/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "device_ctrl.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* C 标准库 */
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* FreeRTOS */
#include "queue.h"

/* BSP & 外设驱动 */
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "rtc.h"
#include "lcd_pwm.h"
#include "lcd_rgb.h"
#include "lcd_test.h"
#include "touch_800x480.h"
#include "re_printf.h"
#include "AT24C256.h"

/* 传感器 */
#include "ds18b20.h"
#include "bmp280function.h"
#include "turbidity.h"
#include "tds.h"
#include "ph_sensor.h"
#include "waterlevel.h"

/* MQTT & 云端 */
#include "AT_MQTT_OS.h"
#include "cJSON.h"

/* 应用层 */
#include "app_runtime.h"
#include "task_mqtt.h"
#include "task_sensor.h"
#include "auto_fish_ctrl.h"
#include "aquarium_light.h"
#include "device_ctrl.h"

/* LVGL 图形库 */
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_ui.h"
#include "benchmark/lv_demo_benchmark.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/** @brief  仅 LCD 调试模式：置 1 则跳过 MQTT 任务创建 */
#define APP_LCD_DEBUG_ONLY  0
#define APP_LCD_FONT_USE_MMAP 0

/**
 * LVGL 任务延时策略
 * ─────────────────
 * 根据 lv_timer_handler() 返回的「下次建议唤醒时间」动态睡眠，
 * 并限制上下界以兼顾触摸响应和 CPU 占用率。
 */
#define LVGL_TASK_MIN_DELAY_MS  1U
#define LVGL_TASK_MAX_DELAY_MS                                             \
  ((LV_INDEV_DEF_READ_PERIOD < LV_DISP_DEF_REFR_PERIOD)                    \
       ? (uint32_t)LV_INDEV_DEF_READ_PERIOD                                \
       : (uint32_t)LV_DISP_DEF_REFR_PERIOD)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/*
 * ADC DMA 缓冲区
 * ─────────────────────────────────────────────────────────────────────────
 * DMA1 工作在 D2 域，缓冲区必须放入 D2 SRAM (0x30000000) 才能正常搬运数据。
 * turbidity.c / tds.c / ph_sensor.c 通过 extern 访问此缓冲区。
 */
uint16_t adc_dma_buffer[ADC_TOTAL_BUF_SIZE]
    __attribute__((section(".RAM_D2"), aligned(32)));

/*
 * 全局传感器变量 —— 用于任务间通信
 * ─────────────────────────────────────────────────────────────────────────
 * Cortex-M7 对 float/short 的单次读写是原子的，因此多任务直接访问全局变量
 * 足以满足本系统的实时性要求（非严格精确场景）。
 */
float g_Turbidity_NTU      = 0.0f;   /**< 浊度值 (NTU) */
float g_Turbidity_Voltage  = 0.0f;   /**< 浊度传感器电压 (V) */
float g_TDS_Value          = 0.0f;   /**< TDS 值 (ppm) */
float g_PH_Value           = 0.0f;   /**< PH 值 */
short g_Temperature        = 0;      /**< DS18B20 水温 (×10, 单位 0.1°C) */
float cloud_Temperature    = 0.0f;   /**< 上报云端的水温 (°C) */

/* BMP280 气压/环境温度 */
float g_BMP280_Temperature = 0.0f;   /**< BMP280 环境温度 (°C) */
float g_BMP280_Pressure    = 0.0f;   /**< BMP280 气压 (Pa) */
BMP280ObjectType g_bmp280;           /**< BMP280 驱动对象实体 */

/* USER CODE END Variables */
/* Definitions for MQTT */
osThreadId_t MQTTHandle;
const osThreadAttr_t MQTT_attributes = {
  .name = "MQTT",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ReadSensorData */
osThreadId_t ReadSensorDataHandle;
const osThreadAttr_t ReadSensorData_attributes = {
  .name = "ReadSensorData",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for UiManager */
osThreadId_t UiManagerHandle;
const osThreadAttr_t UiManager_attributes = {
  .name = "UiManager",
  .stack_size = 1024 * 8,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for LvglHandler — dedicated LVGL refresh task */
osThreadId_t LvglHandlerHandle;
const osThreadAttr_t LvglHandler_attributes = {
  .name = "LvglHandler",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for AutoFishCtrl — 自动鱼缸控制任务 */
osThreadId_t AutoFishCtrlHandle;
const osThreadAttr_t AutoFishCtrl_attributes = {
  .name = "AutoFishCtrl",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for mutex_printf */
osMutexId_t mutex_printfHandle;
const osMutexAttr_t mutex_printf_attributes = {
  .name = "mutex_printf"
};
/* Definitions for mutex_lvgl */
osMutexId_t mutex_lvglHandle;
const osMutexAttr_t mutex_lvgl_attributes = {
  .name = "mutex_lvgl",
  .attr_bits = osMutexRecursive | osMutexPrioInherit,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* ---------- LVGL 互斥锁封装 (递归锁 + 优先级继承) ---------- */

/**
 * @brief  尝试在指定超时内获取 LVGL 互斥锁
 * @param  timeout_ms  超时时间 (ms), 传 osWaitForever 则阻塞等待
 * @retval true=获取成功  false=超时或锁尚未创建
 */
bool app_lvgl_lock_timeout(uint32_t timeout_ms) {
  if (mutex_lvglHandle == NULL) {
    return false;
  }
  return osMutexAcquire(mutex_lvglHandle, timeout_ms) == osOK;
}

/** @brief  阻塞获取 LVGL 互斥锁 */
void app_lvgl_lock(void) { (void)app_lvgl_lock_timeout(osWaitForever); }

/** @brief  释放 LVGL 互斥锁 */
void app_lvgl_unlock(void) {
  if (mutex_lvglHandle != NULL) {
    (void)osMutexRelease(mutex_lvglHandle);
  }
}

/**
 * @brief  根据 lv_timer_handler() 返回值计算本轮延时
 * @param  handler_delay_ms  lv_timer_handler() 建议的下次唤醒间隔
 * @return 钳位到 [MIN, MAX] 范围后的延时值 (ms)
 */
static uint32_t app_lvgl_get_task_delay(uint32_t handler_delay_ms) {
  if (handler_delay_ms == LV_NO_TIMER_READY ||
      handler_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
    return LVGL_TASK_MAX_DELAY_MS;
  }
  if (handler_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
    return LVGL_TASK_MIN_DELAY_MS;
  }
  return handler_delay_ms;
}

/* USER CODE END FunctionPrototypes */

void TaskUiManager(void *argument);
void TaskLvglHandler(void *argument);
void TaskAutoFishCtrl(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS 初始化
 *         创建所有互斥锁、消息队列、任务线程。
 */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  FishCtrl_InitDefaultThresholds();
  /* USER CODE END Init */

  /* ---- 互斥锁 ---- */
  mutex_printfHandle = osMutexNew(&mutex_printf_attributes);
  mutex_lvglHandle   = osMutexNew(&mutex_lvgl_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  configASSERT(mutex_printfHandle != NULL);
  configASSERT(mutex_lvglHandle != NULL);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* ---- 任务创建 ---- */
  #if !APP_LCD_DEBUG_ONLY
  MQTTHandle = osThreadNew(Start_MQTT, NULL, &MQTT_attributes);
  #endif

  ReadSensorDataHandle  = osThreadNew(TaskReadSensorData, NULL, &ReadSensorData_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  UiManagerHandle   = osThreadNew(TaskUiManager,    NULL, &UiManager_attributes);
  LvglHandlerHandle = osThreadNew(TaskLvglHandler,  NULL, &LvglHandler_attributes);
  AutoFishCtrlHandle = osThreadNew(TaskAutoFishCtrl, NULL, &AutoFishCtrl_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE END Header_Start_MQTT */

/* USER CODE BEGIN Header_TaskReadSensorData */

/**
 * @brief  UI 管理任务
 *         负责初始化 LTDC、背光、LVGL 及水族箱 UI 组件；
 *         初始化完成后自删以释放 8 KB 栈空间。
 */
void TaskUiManager(void *argument)
{
  /* USER CODE BEGIN TaskUiManager */
  UNUSED(argument);

  #if APP_LCD_DEBUG_ONLY
  osPrintf("[APP] LCD debug mode enabled, MQTT task skipped.\r\n");
  #endif

  /* ——— 硬件初始化 (LTDC + 背光) ——— */
  MX_LTDC_Init();
  LCD_PWMinit(80);

  /* ——— LVGL 库 + 显示/输入驱动 ——— */
  app_lvgl_lock();
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  app_lvgl_unlock();
  osPrintf("LVGL display driver registered\r\n");

  /* ——— 创建水族箱 UI 界面 ——— */
  app_lvgl_lock();
  lv_ui_create();
  app_lvgl_unlock();
  osPrintf("LVGL aquarium UI created\r\n");

  /* 初始化完成，UI 刷新由 TaskLvglHandler 负责，本任务自删 */
  osThreadExit();
  /* USER CODE END TaskUiManager */
}

/**
 * @brief  Dedicated LVGL refresh task.
 *         Periodically calls lv_timer_handler() to drive rendering, animations
 *         and input processing. No other LVGL API should be called here.
 *
 *         LVGL tick is provided by LV_TICK_CUSTOM = HAL_GetTick(), so no
 *         lv_tick_inc() is needed.
 */
void TaskLvglHandler(void *argument)
{
  UNUSED(argument);
  uint32_t lvgl_delay_ms;

  /* Wait until LVGL is initialised by TaskUiManager */
  while (lv_disp_get_default() == NULL) {
    osDelay(5);
  }

  for (;;) {
    app_lvgl_lock();
    lvgl_delay_ms = app_lvgl_get_task_delay(lv_timer_handler());
    app_lvgl_unlock();

    osDelay(lvgl_delay_ms);
  }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief  自动鱼缸控制任务
 *         根据传感器数据和阈值自动控制执行设备，并更新设备运行状态及屏幕报警标志。
 *         增氧泵：定时开关 + 低气压加速策略
 *         加热器/风扇：按水温上下限联动控制
 *         潜水泵：浊度控制
 *         LED：RTC 定时控制
 *         TDS/PH：仅报警，不控制设备
 */
void TaskAutoFishCtrl(void *argument)
{
  UNUSED(argument);

  osPrintf("[AutoCtrl] Thresholds ready.\r\n");

  FishCtrl_Thresholds_t th_local;
  const FishCtrl_Thresholds_t *th = &th_local;

  /* 增氧泵定时状态机 */
  uint8_t  aeration_running  = 0;
  uint32_t aeration_last_toggle = HAL_GetTick();
  uint8_t  prev_auto_enabled = g_auto_ctrl_enabled; // 用于检测自动→手动→自动切换

  /* 等待传感器任务先采集一轮数据 (约3秒) */
  osDelay(5000);

  /* 上电默认先开启增氧泵 */
  OxygenPump_Ctrl(1);
  aeration_running = 1;
  osPrintf("[AutoCtrl] Aeration pump ON (startup).\r\n");

  for (;;) {
    /* 每次循环刷新阈值快照，确保线程安全 */
    FishCtrl_CopyThresholds(&th_local);

    /* 检测自动控制从关→开的切换，重置增氧泵状态机 */
    if (g_auto_ctrl_enabled && !prev_auto_enabled) {
      aeration_running = 1;
      aeration_last_toggle = HAL_GetTick();
      OxygenPump_Ctrl(1);
      osPrintf("[AutoCtrl] Auto re-enabled, aeration state machine reset.\r\n");
    }
    prev_auto_enabled = g_auto_ctrl_enabled;

    /* ── 仅当自动控制开关打开时，才执行设备联动控制 ── */
    if (g_auto_ctrl_enabled) {

    /* ——————— 1. 水温联动控制 (风扇/加热互斥) ——————— */
    {
      float water_temp = (cloud_Temperature != 0.0f)
                             ? cloud_Temperature
                             : ((float)g_Temperature / 10.0f);
      float temp_lower_limit = th->temp_lower_limit;
      float temp_upper_limit = th->temp_upper_limit;

      if (temp_lower_limit > temp_upper_limit) {
        float temp_limit = temp_lower_limit;
        temp_lower_limit = temp_upper_limit;
        temp_upper_limit = temp_limit;
      }

      if (water_temp < temp_lower_limit) {
        if (g_fan_state == 1) {
          Fan_Ctrl(0);
          osPrintf("[AutoCtrl] Fan OFF (%.1f < %.1f)\r\n",
                   (double)water_temp,
                   (double)temp_lower_limit);
        }

        if (g_heater_state == 0) {
          Heater_Ctrl(1);
          osPrintf("[AutoCtrl] Heater ON (%.1f < %.1f)\r\n",
                   (double)water_temp,
                   (double)temp_lower_limit);
        }
      } else if (water_temp > temp_upper_limit) {
        if (g_heater_state == 1) {
          Heater_Ctrl(0);
          osPrintf("[AutoCtrl] Heater OFF (%.1f > %.1f)\r\n",
                   (double)water_temp,
                   (double)temp_upper_limit);
        }

        if (g_fan_state == 0) {
          Fan_Ctrl(1);
          osPrintf("[AutoCtrl] Fan ON (%.1f > %.1f)\r\n",
                   (double)water_temp,
                   (double)temp_upper_limit);
        }
      } else {
        if (g_heater_state == 1) {
          Heater_Ctrl(0);
          osPrintf("[AutoCtrl] Heater OFF (%.1f in %.1f-%.1f)\r\n",
                   (double)water_temp,
                   (double)temp_lower_limit,
                   (double)temp_upper_limit);
        }

        if (g_fan_state == 1) {
          Fan_Ctrl(0);
          osPrintf("[AutoCtrl] Fan OFF (%.1f in %.1f-%.1f)\r\n",
                   (double)water_temp,
                   (double)temp_lower_limit,
                   (double)temp_upper_limit);
        }
      }
    }

    /* ——————— 2. 潜水泵/过滤 控制 (浊度) ——————— */
    {
      float turb = g_Turbidity_NTU;

      if (turb > th->turbidity_on_thresh) {
        if (g_submersiblepump_state == 0) {
          SubmersiblePump_Ctrl(1);
          osPrintf("[AutoCtrl] Filter Pump ON (turb %.1f > %.1f)\r\n",
                   (double)turb, (double)th->turbidity_on_thresh);
        }
      } else if (turb < th->turbidity_off_thresh) {
        if (g_submersiblepump_state == 1) {
          SubmersiblePump_Ctrl(0);
          osPrintf("[AutoCtrl] Filter Pump OFF (turb %.1f < %.1f)\r\n",
                   (double)turb, (double)th->turbidity_off_thresh);
        }
      }
    }

    /* ——————— 3. 增氧泵控制 (定时 + 低气压加速) ——————— */
    {
      uint32_t now = HAL_GetTick();
      uint32_t elapsed = now - aeration_last_toggle;

      /* 计算当前关闭间隔：气压低时减半 */
      uint32_t current_off_ms = th->aeration_off_ms;
      if (g_BMP280_Pressure > 1000.0f &&
          g_BMP280_Pressure < th->pressure_low_thresh_pa) {
        current_off_ms = th->aeration_off_ms / 2;
      }

      if (aeration_running) {
        /* 正在运行，检查是否到了关闭时间 */
        if (elapsed >= th->aeration_on_ms) {
          OxygenPump_Ctrl(0);
          aeration_running = 0;
          aeration_last_toggle = now;
          osPrintf("[AutoCtrl] Aeration OFF (ran %lu ms)\r\n",
                   (unsigned long)elapsed);
        }
      } else {
        /* 已关闭，检查是否到了开启时间 */
        if (elapsed >= current_off_ms) {
          OxygenPump_Ctrl(1);
          aeration_running = 1;
          aeration_last_toggle = now;
          osPrintf("[AutoCtrl] Aeration ON (off %lu ms, thresh %lu ms)\r\n",
                   (unsigned long)elapsed, (unsigned long)current_off_ms);
        }
      }
    }

    /* ——————— 4. LED 照明控制 (RTC 定时) ——————— */
    {
      RTC_TimeTypeDef time_now = {0};
      RTC_DateTypeDef date_now = {0};

      if (HAL_RTC_GetTime(&hrtc, &time_now, RTC_FORMAT_BIN) == HAL_OK &&
          HAL_RTC_GetDate(&hrtc, &date_now, RTC_FORMAT_BIN) == HAL_OK) {
        uint8_t hour = time_now.Hours;
        uint8_t should_be_on;
        if (th->led_hour_on < th->led_hour_off) {
          /* 不跨午夜，如 08:00~20:00 */
          should_be_on = (hour >= th->led_hour_on && hour < th->led_hour_off) ? 1 : 0;
        } else if (th->led_hour_on > th->led_hour_off) {
          /* 跨午夜，如 20:00~08:00 */
          should_be_on = (hour >= th->led_hour_on || hour < th->led_hour_off) ? 1 : 0;
        } else {
          /* on == off 视为全天关灯 */
          should_be_on = 0;
        }

        if (should_be_on && g_led_state == 0) {
          aquarium_light_on();
          osPrintf("[AutoCtrl] LED ON (hour=%u)\r\n", hour);
        } else if (!should_be_on && g_led_state == 1) {
          aquarium_light_off();
          osPrintf("[AutoCtrl] LED OFF (hour=%u)\r\n", hour);
        }
      }
    }

    } /* end if (g_auto_ctrl_enabled) */

    /* ——————— 5. TDS / PH 报警 (仅设置标志，不控制设备) ——————— */
    {
      /* TDS 报警 */
      g_alarm_tds_high = (g_TDS_Value > th->tds_high_thresh) ? 1 : 0;

      /* PH 报警 (仅当 PH 有有效读数时判断) */
      if (g_PH_Value > 0.0f) {
        g_alarm_ph_high = (g_PH_Value > th->ph_high_thresh) ? 1 : 0;
        g_alarm_ph_low  = (g_PH_Value < th->ph_low_thresh)  ? 1 : 0;
      } else {
        g_alarm_ph_high = 0;
        g_alarm_ph_low  = 0;
      }
    }

    /* 控制周期: 每3秒执行一次 */
    osDelay(3000);
  }
}

/* USER CODE END Application */

