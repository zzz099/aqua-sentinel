/**
 * @file   task_mqtt.c
 * @brief  MQTT 任务——负责与华为云 IoT 平台的上下行通信
 *
 * 功能概述：
 *   - 周期性上报传感器数据及阀值配置
 *   - 解析云端下发的设备控制 / 阀值设置命令
 *   - 心跳监控与断线重连
 */

#include "task_mqtt.h"
#include "app_runtime.h"
#include "AT_MQTT_OS.h"
#include "mqtt_secrets.h"
#include "auto_fish_ctrl.h"
#include "aquarium_light.h"
#include "device_ctrl.h"
#include "waterlevel.h"
#include "cJSON.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- 时间常量 & 缓冲区大小 ---------- */
#define MQTT_SENSOR_UPLOAD_INTERVAL_MS    3000U   /**< 传感器数据上报间隔 */
#define MQTT_THRESHOLD_UPLOAD_INTERVAL_MS 30000U  /**< 阀值配置上报间隔 */
#define MQTT_SENSOR_PAYLOAD_BUF_SIZE      384U
#define MQTT_THRESHOLD_PAYLOAD_BUF_SIZE   320U

/**
 * @brief  构建并上报自动控制阀值到云端
 * @retval HAL_OK=上报成功  HAL_ERROR=格式化/发送失败
 */
static HAL_StatusTypeDef App_ReportThresholdPayload(void)
{
  FishCtrl_Thresholds_t th_snap;
  FishCtrl_CopyThresholds(&th_snap);
  const FishCtrl_Thresholds_t *th = &th_snap;
  static const char *payload_fmt =
      "{\\\"temp_upper_limit\\\":%.1f"
      "\\,\\\"temp_lower_limit\\\":%.1f"
      "\\,\\\"turbidity_on_thresh\\\":%.1f"
      "\\,\\\"turbidity_off_thresh\\\":%.1f"
      "\\,\\\"tds_high_thresh\\\":%.1f"
      "\\,\\\"ph_low_thresh\\\":%.1f"
      "\\,\\\"ph_high_thresh\\\":%.1f"
      "\\,\\\"pressure_low_thresh_pa\\\":%.1f}";
  char payload_buf[MQTT_THRESHOLD_PAYLOAD_BUF_SIZE] = "";
  int payload_len;

  if (th == NULL) {
    return HAL_ERROR;
  }

  payload_len = snprintf(payload_buf, sizeof(payload_buf), payload_fmt,
                         (double)th->temp_upper_limit,
                         (double)th->temp_lower_limit,
                         (double)th->turbidity_on_thresh,
                         (double)th->turbidity_off_thresh,
                         (double)th->tds_high_thresh,
                         (double)th->ph_low_thresh,
                         (double)th->ph_high_thresh,
                         (double)th->pressure_low_thresh_pa);
  if (payload_len < 0 || payload_len >= (int)sizeof(payload_buf)) {
    osPrintf("[MQTT] Threshold payload formatting overflow (%d)\r\n", payload_len);
    return HAL_ERROR;
  }

  return MQTT_ReportCustomJSONPayload(payload_buf);
}

/**
 * @brief  MQTT 主任务入口
 *
 * 整体架构为双层循环：
 *   - 外层重连循环：初始化 WiFi + MQTT 会话
 *   - 内层主循环：周期上报 + 心跳 + 命令解析
 */
void Start_MQTT(void *argument)
{
  UNUSED(argument);
  HAL_StatusTypeDef status = HAL_OK;
  char subrecv_text[MQTT_QUEUE_SIZE] = "";
  uint32_t last_upload_time = 0;
  uint32_t last_threshold_upload_time = 0;

  uint32_t last_heartbeat_time = 0;

  cJSON_Hooks hooks;
  hooks.malloc_fn = pvPortMalloc;
  hooks.free_fn = vPortFree;
  cJSON_InitHooks(&hooks);

  osPrintf("[MQTT] Waiting for ESP01 to boot...\r\n");
  osDelay(3000);

  uint8_t state = 0;
  char *json_text = NULL, *command_name = NULL;
  cJSON *root = NULL, *paras = NULL, *j_cmd = NULL;

  /* === 外层重连循环：初始化 WiFi + MQTT，心跳失败时回到此处重新初始化 === */
  for (;;) {
    g_MQTT_Status = -1;
    g_WiFi_Connected = 0;

    status = MQTT_Init();
    if (status != HAL_OK) {
      g_MQTT_Status = 1;
      osPrintf("MQTT Init Failed, retrying in 8 seconds...\r\n");
      osDelay(8000);
      continue;
    }

    g_MQTT_Status = 0;
    osPrintf("MQTT Init Success!\r\n");

  osPrintf("MQTT Init Status:%d\r\n", status);

  /* 启动后清空缓存的旧命令，避免重连后误执行 */
  if (queueCloudCmd != NULL) {
    char discard_buf[MQTT_QUEUE_SIZE];
    while (xQueueReceive(queueCloudCmd, discard_buf, 0) == pdTRUE) {
      osPrintf("[MQTT] Discarded cached cloud command on startup\r\n");
    }
  }

  if (App_ReportThresholdPayload() == HAL_OK) {
    last_threshold_upload_time = HAL_GetTick();
    osPrintf("[MQTT] Threshold properties uploaded.\r\n");
  } else {
    osPrintf("[MQTT] Initial threshold upload failed, will retry later.\r\n");
  }


  /* ---- 内层主循环：周期上报 + 心跳检测 + 云端命令解析 ---- */
  last_heartbeat_time = HAL_GetTick();
  for (;;) {
    /* -- 心跳检测 (30s 周期)：AT 同步 + WiFi 状态检查 -- */
    if (HAL_GetTick() - last_heartbeat_time > 30000) {
      last_heartbeat_time = HAL_GetTick();
      if (MQTT_SendRetCmd("AT\r\n", "OK\r\n", 2000) != HAL_OK) {
        osPrintf("[MQTT] Heartbeat FAILED! ESP01 not responding\r\n");
        g_MQTT_Status = -1;
        g_WiFi_Connected = 0;
        status = HAL_ERROR;

        MQTT_RecoverUART();
        osDelay(3000);
        break;
      }

      if (MQTT_GetWiFiState(2000) != HAL_OK) {
        osPrintf("[MQTT] WiFi lost! Full MQTT reinit required.\r\n");
        g_WiFi_Connected = 0;
        g_MQTT_Status = -1;
        MQTT_RecoverUART();
        osDelay(3000);
        break;  /* 跳回外层重连循环，重建 WiFi + MQTT 会话 */
      }
    }

    /* -- 周期上报传感器数据 -- */
    if (status == HAL_OK &&
        (HAL_GetTick() - last_upload_time >= MQTT_SENSOR_UPLOAD_INTERVAL_MS)) {
      int wl = WaterLevel_Read() ? 1 : 0;

      static const char *payload_fmt =
          "{\\\"temp\\\":%.1f"
          "\\,\\\"humi\\\":%.1f"
          "\\,\\\"turbidity\\\":%.2f"
          "\\,\\\"dity_voltage\\\":%.3f"
          "\\,\\\"tds\\\":%.1f"
          "\\,\\\"PH\\\":%.2f"
          "\\,\\\"Atmospheric_pressure\\\":%.2f"
          "\\,\\\"water_level\\\":%d"
          "\\,\\\"led_state\\\":%d"
          "\\,\\\"heater_state\\\":%d"
          "\\,\\\"fan_state\\\":%d"
          "\\,\\\"oxygenpump_state\\\":%d"
          "\\,\\\"submersiblepump_state\\\":%d}";

      char payload_buf[MQTT_SENSOR_PAYLOAD_BUF_SIZE] = "";
      int payload_len = snprintf(payload_buf, sizeof(payload_buf), payload_fmt,
                                 (double)cloud_Temperature,
                                 (double)g_BMP280_Temperature,
                                 (double)g_Turbidity_NTU,
                                 (double)g_Turbidity_Voltage,
                                 (double)g_TDS_Value,
                                 (double)g_PH_Value,
                                 (double)g_BMP280_Pressure,
                                 wl,
                                 (int)g_led_state,
                                 (int)g_heater_state,
                                 (int)g_fan_state,
                                 (int)g_oxygenpump_state,
                                 (int)g_submersiblepump_state);

      if (payload_len < 0 || payload_len >= (int)sizeof(payload_buf)) {
        osPrintf("[MQTT] Sensor payload formatting overflow (%d)\r\n", payload_len);
      } else {
        MQTT_ReportCustomJSONPayload(payload_buf);
        last_upload_time = HAL_GetTick();
      }
    }

    /* -- 周期上报阀值配置 -- */
    if (status == HAL_OK &&
        (HAL_GetTick() - last_threshold_upload_time >=
         MQTT_THRESHOLD_UPLOAD_INTERVAL_MS)) {
      if (App_ReportThresholdPayload() != HAL_OK) {
        osPrintf("[MQTT] Threshold property upload failed.\r\n");
      }
      last_threshold_upload_time = HAL_GetTick();
    }

    /* -- 处理云端下发命令 (Queue 接收, 100ms 超时) -- */
    if (xQueueReceive(queueCloudCmd, subrecv_text, 100) == pdTRUE) {
      if (strstr(subrecv_text, MQTT_SUBRECV_KEYWORD))
      {
        MQTT_HandleRequestID(subrecv_text);

        char *json_start = strstr(subrecv_text, ",{");
        if (!json_start) {
          osPrintf("[MQTT] Malformed frame: missing ',{'\r\n");
          continue;
        }
        json_text = json_start + 1;

        char *json_end = strstr(json_text, "\r\n");
        if (!json_end) {
          osPrintf("[MQTT] Malformed frame: missing '\\r\\n' terminator\r\n");
          continue;
        }
        *json_end = 0;

        root = cJSON_Parse(json_text);
        if (!root) {
          osPrintf("Failed to Parse JSON text:%s\r\n", json_text);
          goto mqtt_msg_cleanup;
        }
        paras = cJSON_GetObjectItem(root, "paras");
        if (!cJSON_IsObject(paras))
          goto mqtt_msg_cleanup;

        cJSON *j_param = cJSON_GetObjectItem(paras, "state");
        if (!cJSON_IsNumber(j_param))
          goto mqtt_msg_cleanup;
        state = j_param->valueint;

        j_cmd = cJSON_GetObjectItem(root, "command_name");
        if (!cJSON_IsString(j_cmd))
          goto mqtt_msg_cleanup;
        command_name = j_cmd->valuestring;

        osPrintf("command:%s state:%d\r\n", command_name, state);

        /* ── 设备控制命令：自动模式下拒绝执行 ── */
        if (!strcmp(command_name, "led_ctrl") ||
            !strcmp(command_name, "fan_ctrl") ||
            !strcmp(command_name, "res_ctrl") ||
            !strcmp(command_name, "aeration_pump_ctrl") ||
            !strcmp(command_name, "submersible_pump_ctrl")) {
          if (g_auto_ctrl_enabled) {
            osPrintf("[MQTT] Rejected cloud cmd '%s': auto mode active\r\n", command_name);
            goto mqtt_msg_cleanup;
          }
        }

        if (!strcmp(command_name, "led_ctrl")) {
          if (state == 0) {
            aquarium_light_off();
            osPrintf("Aquarium Light OFF\r\n");
          } else if (state == 1) {
            aquarium_light_on();
            osPrintf("Aquarium Light ON\r\n");
          } else {
            osPrintf("Invalid state value: %d (only 0 or 1 allowed)\r\n",
                     state);
          }
        } else if (!strcmp(command_name, "submersible_pump_ctrl")) {
          if (state == 0) {
            SubmersiblePump_Ctrl(0);
            osPrintf("Submersible Pump OFF\r\n");
          } else if (state == 1) {
            SubmersiblePump_Ctrl(1);
            osPrintf("Submersible Pump ON\r\n");
          } else {
            osPrintf("Invalid state value: %d (only 0 or 1 allowed)\r\n",
                     state);
          }
        } else if (!strcmp(command_name, "aeration_pump_ctrl")) {
          if (state == 0) {
            OxygenPump_Ctrl(0);
            osPrintf("Aeration Pump OFF\r\n");
          } else if (state == 1) {
            OxygenPump_Ctrl(1);
            osPrintf("Aeration Pump ON\r\n");
          } else {
            osPrintf("Invalid state value: %d (only 0 or 1 allowed)\r\n",
                     state);
          }
        } else if (!strcmp(command_name, "res_ctrl")) {
          if (state == 0) {
            Heater_Ctrl(0);
            osPrintf("Cement Resistor OFF\r\n");
          } else if (state == 1) {
            Heater_Ctrl(1);
            osPrintf("Cement Resistor ON\r\n");
          } else {
            osPrintf("Invalid state value: %d (only 0 or 1 allowed)\r\n",
                     state);
          }
        } else if (!strcmp(command_name, "fan_ctrl")) {
          if (state == 0) {
            Fan_Ctrl(0);
            osPrintf("Cooling Fan OFF\r\n");
          } else if (state == 1) {
            Fan_Ctrl(1);
            osPrintf("Cooling Fan ON\r\n");
          } else {
            osPrintf("Invalid state value: %d (only 0 or 1 allowed)\r\n",
                     state);
          }
        }

mqtt_msg_cleanup:
        cJSON_Delete(root);
        root = NULL;
      }
    }
    osDelay(1);
  } /* end: 内层主循环 */
  } /* end: 外层重连循环 */
}
