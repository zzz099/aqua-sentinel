#ifndef AT_MQTT_OS_H
#define AT_MQTT_OS_H

#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <time.h>
#include "re_printf.h"
#include "mqtt_secrets.h"

/*MQTT用户配置 — 凭据已移至 mqtt_secrets.h */
//#define MQTT_WIFI_SSID            "ST_LEGION 5322"
//#define MQTT_WIFI_PSWD            "lty120712..."

#define MQTT_SERVICE_ID            "data"

/*MQTT固定格式*/
//上报数据
#define MQTT_TOPIC_REPORT        "$oc/devices/"MQTT_USERNAME"/sys/properties/report"
#define MQTT_SUB_TOPIC_REPORT    "AT+MQTTSUB=0,\""MQTT_TOPIC_REPORT"\",1\r\n"
//下发数据
#define MQTT_TOPIC_COMMAND        "$oc/devices/"MQTT_USERNAME"/sys/commands/#"
#define MQTT_SUB_TOPIC_COMMAND    "AT+MQTTSUB=0,\""MQTT_TOPIC_COMMAND"\",1\r\n"
//下发数据反馈
#define MQTT_SUB_REQUEST_F "AT+MQTTSUB=0,\"$oc/devices/"MQTT_USERNAME"/sys/commands/response/request_id=%s\",1\r\n"
#define MQTT_PUB_REQUEST_F "AT+MQTTPUB=0,\"$oc/devices/"MQTT_USERNAME"/sys/commands/response/request_id=%s\",\"\",1,1\r\n"
//上报数据模板
#define MQTT_JSON_REPORT_INT    "{\\\"services\\\":[{\\\"service_id\\\":\\\""MQTT_SERVICE_ID"\\\"\\,\\\"properties\\\":{\\\"%s\\\":%d}}]}"
#define MQTT_CMD_F_PUS_INT        "AT+MQTTPUB=0,\""MQTT_TOPIC_REPORT"\",\""MQTT_JSON_REPORT_INT"\",0,0\r\n"
#define MQTT_JSON_REPORT_DOUBLE "{\\\"services\\\":[{\\\"service_id\\\":\\\""MQTT_SERVICE_ID"\\\"\\,\\\"properties\\\":{\\\"%s\\\":%.3lf}}]}"
#define MQTT_CMD_F_PUS_DOUBLE   "AT+MQTTPUB=0,\""MQTT_TOPIC_REPORT"\",\""MQTT_JSON_REPORT_DOUBLE"\",0,0\r\n"
//自定义Payload上报模板
#define MQTT_JSON_REPORT_CUSTOM "{\\\"services\\\":[{\\\"service_id\\\":\\\""MQTT_SERVICE_ID"\\\"\\,\\\"properties\\\":%s}]}"
#define MQTT_CMD_F_PUS_CUSTOM   "AT+MQTTPUB=0,\""MQTT_TOPIC_REPORT"\",\""MQTT_JSON_REPORT_CUSTOM"\",0,0\r\n"

#define MQTT_SUBRECV_KEYWORD    "MQTTSUBRECV"

/*用户配置*/
#define MQTT_UART                   huart2        //使用的uart外设句柄
#define MQTT_DEFAULT_TIMEOUT        10000       //默认超时时间
/*FreeRTOS配置*/
#define MQTT_QUEUE_LEN              (5)     //队列最多有多少条消息
#define MQTT_QUEUE_SIZE             (512)   //队列每条消息的最大长度
#define MQTT_DELAY                  osDelay

/* 全局状态变量声明 */
extern uint8_t g_WiFi_Connected;
extern int g_MQTT_Status;
extern QueueHandle_t queueMqttMsg;
extern QueueHandle_t queueCloudCmd;


/* 线程安全的调试打印（需要 FreeRTOS mutex 已创建后才可使用） */
void MQTT_SafePrintf(const char *fmt, ...);

HAL_StatusTypeDef MQTT_Init(void);

HAL_StatusTypeDef   MQTT_GetWiFiState(uint32_t timeout);

HAL_StatusTypeDef MQTT_ConnectWiFi(char *ssid, char *pswd, uint32_t timeout);

void MQTT_SendNoRetCmd(char *ATCmd);

HAL_StatusTypeDef MQTT_SendRetCmd(char *at_cmd, char *ret_keyword, uint32_t timeout);

HAL_StatusTypeDef MQTT_ReportIntVal(char *property_name, int val);

HAL_StatusTypeDef MQTT_ReportDoubleVal(char *property_name, double val);

HAL_StatusTypeDef MQTT_ReportCustomJSONPayload(char *payload);

HAL_StatusTypeDef MQTT_HandleRequestID(char *sub_recv_text);

HAL_StatusTypeDef MQTT_GetNTPTimeStr(char *time_str, uint32_t timeout);

HAL_StatusTypeDef MQTT_GetNTPTimeTm(struct tm *p_tm, uint32_t timeout);

void MQTT_HandleUARTInterrupt();

void MQTT_RecoverUART(void);



#endif //AT_MQTT_OS_H
