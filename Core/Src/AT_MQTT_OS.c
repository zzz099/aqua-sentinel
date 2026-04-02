/**
 * @file   AT_MQTT_OS.c
 * @brief  ESP01S AT 指令封装 —— WiFi 连接 / MQTT 会话管理 / NTP 时间同步
 *
 * 架构说明：
 *   - 所有 AT 指令通过 USART2 发送给 ESP01S
 *   - ISR 中通过关键字匹配将云端下发命令路由到 queueCloudCmd，
 *     其他 AT 响应路由到 queueMqttMsg
 *   - 解耦了 FreeRTOS 队列消费者 (task_mqtt.c) 与硬件中断层
 */

#include "AT_MQTT_OS.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include <cmsis_os2.h>
#include "stm32h7xx_hal.h"
#include <stdarg.h>

/* 外部 mutex 用于线程安全打印 (在 freertos.c 中创建) */
extern osMutexId_t mutex_printfHandle;

/* 线程安全的调试打印封装 */
void MQTT_SafePrintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (mutex_printfHandle != NULL) osMutexAcquire(mutex_printfHandle, osWaitForever);
    vprintf(fmt, args);
    if (mutex_printfHandle != NULL) osMutexRelease(mutex_printfHandle);
    va_end(args);
}

/* ---------- AT 指令定义 ---------- */
#define MSG_SUCCESS                     "OK\r\n"
#define MSG_FAILED                      "ERROR\r\n"

#define CMD_ECHO_OFF                    "ATE0\r\n"
#define CMD_SET_STA                     "AT+CWMODE=1\r\n"
#define CMD_GET_CWSTATE                 "AT+CWSTATE?\r\n"
#define CMD_CONNECT_WIFI_F              "AT+CWJAP=\"%s\",\"%s\"\r\n"
#define CMD_SET_MQTTUSERCFG				"AT+MQTTUSERCFG=0,1,\"NULL\",\""MQTT_USERNAME"\",\""MQTT_USERPWSD"\",0,0,\"\"\r\n"
#define CMD_SET_CLIENTID				"AT+MQTTCLIENTID=0,\""MQTT_CLIENTID"\"\r\n"
#define CMD_SET_MQTTCONN				"AT+MQTTCONN=0,\""MQTT_HOST_NAME"\",1883,1\r\n"
#define CMD_SET_TIME_ZONE				"AT+CIPSNTPCFG=1,8\r\n"
#define CMD_GET_TIME					"AT+CIPSNTPTIME?\r\n"

/* ---------- 内部常量 ---------- */
#define TEMP_BUFF_SIZE                  MQTT_QUEUE_SIZE
#define MQTT_REQUEST_ID_LEN             36

/* ---------- 全局状态变量 ---------- */
uint8_t g_WiFi_Connected = 0;   /**< WiFi 连接状态: 0=未连接, 1=已连接 */
int g_MQTT_Status = -1;         /**< MQTT 状态: -1=初始化中, 0=OK, 1=失败 */

/* ---------- 队列 & 接收缓冲 ---------- */
QueueHandle_t queueMqttMsg;
QueueHandle_t queueCloudCmd;
char RecvCh;
char RecvBuff[MQTT_QUEUE_SIZE];
size_t RecvLen = 0;
char TempBuff[TEMP_BUFF_SIZE];

/* ISR 丢帧计数器（调试用） */
volatile uint32_t g_mqtt_drop_count  = 0;
volatile uint32_t g_cloud_drop_count = 0;

const char MONTH_LIST[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

#if MQTT_QUEUE_SIZE < 200
#error "MQTT_QUEUE_SIZE 应当大于200"
#endif

/**
 * @brief 恢复UART接收：清除所有错误标志、清空缓冲区、重启IT接收
 */
void MQTT_RecoverUART(void)
{
    __HAL_UART_CLEAR_OREFLAG(&MQTT_UART);
    __HAL_UART_CLEAR_NEFLAG(&MQTT_UART);
    __HAL_UART_CLEAR_FEFLAG(&MQTT_UART);
    volatile uint32_t dummy;
    dummy = MQTT_UART.Instance->ISR;
    dummy = MQTT_UART.Instance->RDR;
    (void)dummy;
    memset(RecvBuff, 0, MQTT_QUEUE_SIZE);
    RecvLen = 0;
    if (queueMqttMsg != NULL) xQueueReset(queueMqttMsg);
    HAL_UART_AbortReceive(&MQTT_UART);
    HAL_UART_Receive_IT(&MQTT_UART, (uint8_t*)&RecvCh, 1);
}

/**
 * @brief 对ESP01执行软件复位并等待其就绪
 * @retval HAL_OK=AT同步成功  HAL_ERROR=始终无响应
 */
static HAL_StatusTypeDef MQTT_SoftResetAndSync(void)
{
    // 先尝试直接AT同步，如果ESP已经在线就不需要RST
    MQTT_RecoverUART();
    MQTT_DELAY(200);
    for (int i = 0; i < 3; i++) {
        if (MQTT_SendRetCmd("AT\r\n", MSG_SUCCESS, 1000) == HAL_OK) {
            MQTT_SafePrintf("[MQTT] AT Sync OK (no RST needed, attempt %d)\r\n", i + 1);
            return HAL_OK;
        }
        MQTT_RecoverUART();
        MQTT_DELAY(200);
    }

    // 直接同步失败，执行RST
    MQTT_SafePrintf("[MQTT] Direct AT failed, doing RST...\r\n");
    MQTT_RecoverUART();
    MQTT_SendNoRetCmd("AT+RST\r\n");
    MQTT_DELAY(8000);
    MQTT_SafePrintf("[MQTT] AT+RST done, recovering UART...\r\n");
    MQTT_RecoverUART();

    for (int i = 0; i < 10; i++) {
        if (MQTT_SendRetCmd("AT\r\n", MSG_SUCCESS, 1000) == HAL_OK) {
            MQTT_SafePrintf("[MQTT] AT Sync OK (attempt %d)\r\n", i + 1);
            return HAL_OK;
        }
        MQTT_SafePrintf("[MQTT] AT Sync retry %d/10\r\n", i + 1);
        MQTT_RecoverUART();
        MQTT_DELAY(300);
    }
    MQTT_SafePrintf("[MQTT] AT Sync Failed after 10 retries\r\n");
    return HAL_ERROR;
}

/**
 * @brief MQTT初始化
 * @retval 成功返回HAL_OK
 */
HAL_StatusTypeDef MQTT_Init(void)
{
    HAL_StatusTypeDef status;
    TickType_t beg_tick = 0;
    
    MQTT_SafePrintf("[MQTT] Init Start\r\n");

    if (queueMqttMsg == NULL)
    {
        queueMqttMsg = xQueueCreate(MQTT_QUEUE_LEN, MQTT_QUEUE_SIZE);
        if (queueMqttMsg == NULL)
        {
            MQTT_SafePrintf("[MQTT] Queue Create Failed\r\n");
            return HAL_ERROR;
        }
    }

    if (queueCloudCmd == NULL)
    {
        queueCloudCmd = xQueueCreate(MQTT_QUEUE_LEN, MQTT_QUEUE_SIZE);
        if (queueCloudCmd == NULL)
        {
            MQTT_SafePrintf("[MQTT] Cloud Queue Create Failed\r\n");
            return HAL_ERROR;
        }
    }
    MQTT_SafePrintf("[MQTT] Queue Created\r\n");

    // ====== 阶段1：软件复位ESP01并建立AT同步 ======
    status = MQTT_SoftResetAndSync();
    if (status != HAL_OK) {
        MQTT_SafePrintf("[MQTT] RST+Sync Failed\r\n");
        return status;
    }

    status = MQTT_SendRetCmd(CMD_ECHO_OFF, MSG_SUCCESS, 2000);
    if (status != HAL_OK) {
        MQTT_SafePrintf("[MQTT] Echo Off Failed\r\n");
        return status;
    }
    MQTT_SafePrintf("[MQTT] Echo Off OK\r\n");

    // ====== 阶段2：连接WiFi（带多次重试） ======
    beg_tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - beg_tick < 10000)
    {
        status = MQTT_GetWiFiState(500);
        if (status == HAL_OK) break;
        MQTT_DELAY(500);
    }
    MQTT_SafePrintf("[MQTT] Cached WiFi State: %d\r\n", status);

    if (status != HAL_OK)
    {
        uint8_t wifi_ok = 0;
        for (int wifi_retry = 0; wifi_retry < 3; wifi_retry++) {
            MQTT_SafePrintf("[MQTT] WiFi Connect attempt %d/3...\r\n", wifi_retry + 1);
            status = MQTT_ConnectWiFi(MQTT_WIFI_SSID, MQTT_WIFI_PSWD, 30000);
            if (status == HAL_OK) {
                wifi_ok = 1;
                break;
            }
            MQTT_SafePrintf("[MQTT] WiFi attempt %d failed (status=%d)\r\n", wifi_retry + 1, status);
            MQTT_SendRetCmd("AT+CWQAP\r\n", MSG_SUCCESS, 3000);
            MQTT_DELAY(2000);
        }
        if (!wifi_ok) {
            MQTT_SafePrintf("[MQTT] WiFi Connect Failed after 3 attempts\r\n");
            return HAL_ERROR;
        }
    }
    MQTT_SafePrintf("[MQTT] WiFi Connected!\r\n");
    g_WiFi_Connected = 1;

    // ====== 阶段3：配置并连接 MQTT 服务器 ======
    MQTT_SendRetCmd("AT+MQTTCLEAN=0\r\n", MSG_SUCCESS, 2000); /* 清理残留会话 */
    MQTT_DELAY(500);

    status = MQTT_SendRetCmd(CMD_SET_MQTTUSERCFG, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);
    if (status != HAL_OK) { MQTT_SafePrintf("[MQTT] USERCFG Failed\r\n"); return status; }
    MQTT_SafePrintf("[MQTT] USERCFG OK\r\n");

    status = MQTT_SendRetCmd(CMD_SET_CLIENTID, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);
    if (status != HAL_OK) { MQTT_SafePrintf("[MQTT] CLIENTID Failed\r\n"); return status; }
    MQTT_SafePrintf("[MQTT] CLIENTID OK\r\n");

    status = MQTT_SendRetCmd(CMD_SET_MQTTCONN, MSG_SUCCESS, 30000);
    if (status != HAL_OK) { MQTT_SafePrintf("[MQTT] MQTTCONN Failed\r\n"); return status; }
    MQTT_SafePrintf("[MQTT] MQTTCONN OK\r\n");

    status = MQTT_SendRetCmd(MQTT_SUB_TOPIC_REPORT, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);
    if (status != HAL_OK) { MQTT_SafePrintf("[MQTT] SUB REPORT Failed\r\n"); return status; }

    status = MQTT_SendRetCmd(MQTT_SUB_TOPIC_COMMAND, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);
    if (status != HAL_OK) { MQTT_SafePrintf("[MQTT] SUB COMMAND Failed\r\n"); return status; }

    status = MQTT_SendRetCmd(CMD_SET_TIME_ZONE, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);
    if (status != HAL_OK) { MQTT_SafePrintf("[MQTT] TIMEZONE Failed\r\n"); return status; }

    return HAL_OK;
}

/**
 * @brief 获取Wifi状态（需要在调用初始化之后才可用）
 * @param timeout 超时时间，超出返回 HAL_TIMEOUT
 * @retval WiFi连接正常返回 HAL_OK
 * */
HAL_StatusTypeDef MQTT_GetWiFiState(uint32_t timeout)
{
    HAL_StatusTypeDef status;
    char* keyword_pos;

    status = MQTT_SendRetCmd(CMD_GET_CWSTATE, "+CWSTATE", timeout);
    if (status != HAL_OK) return status;

    keyword_pos = strstr(TempBuff, "+CWSTATE");
    if (*(keyword_pos + 9) == '2')
    {
        status = HAL_OK;
        return status;
    }

    status = HAL_ERROR;
    return status;
}

/**
 * @brief 连接Wifi
 * @param ssid WIFI的名称
 * @param pswd WIFI的密码
 * @param timeout 超时时间, 超过返回 HAL_TIMEOUT, 建议10s左右
 * @retval 成功连接到指定WiFi后返回 HAL_OK
 * */
HAL_StatusTypeDef MQTT_ConnectWiFi(char* ssid, char* pswd, uint32_t timeout)
{
    HAL_StatusTypeDef status;

    status = MQTT_SendRetCmd(CMD_SET_STA, MSG_SUCCESS, 4000);
    if (status != HAL_OK) return status;

    memset(TempBuff, 0, TEMP_BUFF_SIZE);
    int wifi_ret = snprintf(TempBuff, TEMP_BUFF_SIZE, CMD_CONNECT_WIFI_F, ssid, pswd);
    if (wifi_ret < 0 || wifi_ret >= (int)TEMP_BUFF_SIZE) return HAL_ERROR;

    status = MQTT_SendRetCmd(TempBuff, "WIFI GOT IP", timeout);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MQTT_GetWiFiState(500);
    return status;
}

/**
 * @brief 发送不带返回值的MQTT命令
 * @param at_cmd AT命令，应当以“\r\n”结尾
 */
void MQTT_SendNoRetCmd(char* at_cmd)
{
    HAL_UART_Transmit(&MQTT_UART, (uint8_t*)at_cmd, strlen(at_cmd), MQTT_DEFAULT_TIMEOUT);
}

/**
 * @brief 发送带返回值的MQTT命令
 * @param at_cmd AT命令，应当以“\\r\\n”结尾
 * @param ret_keyword 期待的返回值，当检测到该关键词之后返回 HAL_OK
 * @param timeout 超时时间（ms），在超过时间之后返回 HAL_ERROR
 * @retval 成功返回HAL_OK
 * @note 如果返回的数据中包含ERROR，则立即返回 HAL_ERROR
 * @note 发送命令之后返回的数据会留在TempBuff中
 */
HAL_StatusTypeDef MQTT_SendRetCmd(char* at_cmd, char* ret_keyword, uint32_t timeout)
{
    TickType_t beg_tick;
    HAL_StatusTypeDef status = HAL_ERROR;
    if (queueMqttMsg == NULL) return HAL_ERROR;

    beg_tick = xTaskGetTickCount();
    /* 清空队列后再接收新数据 */
    if (xQueueReset(queueMqttMsg) != pdPASS)
    {
        return HAL_ERROR;
    }
    MQTT_SendNoRetCmd(at_cmd);

    /* 多条数据情况下持续接收直到找到目标关键字 */
    while (xTaskGetTickCount() - beg_tick <= timeout)
    {
        if (xQueueReceive(queueMqttMsg, TempBuff, timeout) != pdTRUE)
        {
            status = HAL_TIMEOUT;
            return status;
        }
        if (strstr(TempBuff, ret_keyword))
        {
            status = HAL_OK;
            return status;
        }
        if (strstr(TempBuff, MSG_FAILED))
        {
            status = HAL_ERROR;
            return status;
        }
    }
    return status;
}

/**
 * @brief 上报整型数据
 * @param property_name 属性名
 * @param val 值
 * @retval 成功返回HAL_OK
 * @note 需要订阅Report的Topic
 */
HAL_StatusTypeDef MQTT_ReportIntVal(char* property_name, int val)
{
    memset(TempBuff, 0, TEMP_BUFF_SIZE);
    int ret = snprintf(TempBuff, TEMP_BUFF_SIZE, MQTT_CMD_F_PUS_INT, property_name, val);
    if (ret < 0 || ret >= (int)TEMP_BUFF_SIZE) return HAL_ERROR;
    return MQTT_SendRetCmd(TempBuff, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);
}

/**
 * @brief 上报浮点数据
 * @param property_name 属性名
 * @param val 值
 * @note 需要订阅Report的Topic
 * @note 需要编译器支持浮点打印 target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)
 * @note 浮点数格式化默认保留三位小数
 * @retval 成功返回HAL_OK
 */
HAL_StatusTypeDef MQTT_ReportDoubleVal(char* property_name, double val)
{
    /*
     * target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)
     * */
    memset(TempBuff, 0, TEMP_BUFF_SIZE);
    int ret = snprintf(TempBuff, TEMP_BUFF_SIZE, MQTT_CMD_F_PUS_DOUBLE, property_name, val);
    if (ret < 0 || ret >= (int)TEMP_BUFF_SIZE) return HAL_ERROR;
    return MQTT_SendRetCmd(TempBuff, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);
}

/**
 * @brief 上报自定义JSON Payload
 * @param payload 已格式化的properties JSON字符串，需使用转义格式，长度不应超过256字符
 * @retval 成功返回HAL_OK
 * @note 使用示例:
 *   const char *json_payload = "{\\\"Temp\\\":%d\\,\\\"PH\\\":%d}";
 *   char payload_buffer[100] = "";
 *   sprintf(payload_buffer, json_payload, 10, 10);
 *   MQTT_ReportCustomJSONPayload(payload_buffer);
 */
HAL_StatusTypeDef MQTT_ReportCustomJSONPayload(char* payload)
{
    memset(TempBuff, 0, TEMP_BUFF_SIZE);
    int ret = snprintf(TempBuff, TEMP_BUFF_SIZE, MQTT_CMD_F_PUS_CUSTOM, payload);
    if (ret < 0 || ret >= (int)TEMP_BUFF_SIZE) return HAL_ERROR;
    return MQTT_SendRetCmd(TempBuff, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);
}

/**
 * @brief 处理下发命令中的request_id，该函数将下发内容中断 request_id提取出来并完成topic的订阅和数据推送，函数应当在收到下发指令后20S内调用
 * @param sub_recv_text 接收到的下发命令的完整字段
 * @retval 成功返回HAL_OK
 * */
HAL_StatusTypeDef MQTT_HandleRequestID(char* sub_recv_text)
{
    HAL_StatusTypeDef status = HAL_OK;
    const char* request_id_keyword =  "request_id=";

    /* 使用栈分配，消除频繁 malloc/free 导致的堆碎片化风险 */
    char request_id[MQTT_REQUEST_ID_LEN + 1];
    memset(request_id, 0, sizeof(request_id));

    char *request_id_pos = strstr(sub_recv_text, request_id_keyword);
    if (request_id_pos)
    {
    /* 校验源串剩余长度足够 */
        char *id_start = request_id_pos + strlen(request_id_keyword);
        size_t remaining = strlen(id_start);
        size_t copy_len = (remaining < MQTT_REQUEST_ID_LEN) ? remaining : MQTT_REQUEST_ID_LEN;
        memcpy(request_id, id_start, copy_len);
        request_id[copy_len] = '\0';

        memset(TempBuff, 0, TEMP_BUFF_SIZE);
        int sub_ret = snprintf(TempBuff, TEMP_BUFF_SIZE, MQTT_SUB_REQUEST_F, request_id);
        if (sub_ret < 0 || sub_ret >= (int)TEMP_BUFF_SIZE) return HAL_ERROR;
        MQTT_SendRetCmd(TempBuff, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);

        memset(TempBuff, 0, TEMP_BUFF_SIZE);
        int pub_ret = snprintf(TempBuff, TEMP_BUFF_SIZE, MQTT_PUB_REQUEST_F, request_id);
        if (pub_ret < 0 || pub_ret >= (int)TEMP_BUFF_SIZE) return HAL_ERROR;
        MQTT_SendRetCmd(TempBuff, MSG_SUCCESS, MQTT_DEFAULT_TIMEOUT);

        status = HAL_OK;
    }
    else
    {
        status = HAL_ERROR;
    }

    return status;
}

/**
 * @brief 从MQTT服务器获取时间日期
 * @param time_str 获取到的时间日期字符串，大小应 >= 25 Byte，格式为 Sat Jan 10 15:58:27 2026
 * @param timeout 超时时间，超出返回 HAL_TIMEOUT
 * @retval 成功返回 HAL_OK
 * @note 获取时间需要配置MQTT服务器，且连接正常，如果正常连接到服务器之后仍然获取失败（例如返回1970年），请等待一段时间之后再获取
 * */
HAL_StatusTypeDef MQTT_GetNTPTimeStr(char* time_str, uint32_t timeout)
{
    /*
     * +CIPSNTPTIME:Sat Jan 10 15:58:27 2026
     * Length:24
     * */
    const size_t time_str_len = 24;
    HAL_StatusTypeDef status;

    status = MQTT_SendRetCmd(CMD_GET_TIME, "CIPSNTPTIME", timeout);
    if (status != HAL_OK)
        return status;

    char* keywork_pos = strstr(TempBuff, "CIPSNTPTIME");
    if (keywork_pos)
    {
//        memcpy(time_str, keywork_pos + strlen("CIPSNTPTIME") + 1, (time_str_len) * sizeof (char));
        strncpy(time_str, keywork_pos + strlen("CIPSNTPTIME") + 1, time_str_len);
        time_str[time_str_len] = 0; //手动添加结束符
        status = HAL_OK;
    }
    else
    {
        status = HAL_ERROR;
    }
    return status;
}

/**
 * @brief 获取struct tm格式的时间
 * @param timeout 超时时间，超出返回 HAL_TIMEOUT
 * @retval 成功返回 HAL_OK
 * @note 获取时间需要配置MQTT服务器，且连接正常，如果正常连接到服务器之后仍然获取失败（例如返回1970年），请等待一段时间之后再获取
 */
HAL_StatusTypeDef MQTT_GetNTPTimeTm(struct tm *p_tm, uint32_t timeout)
{
    HAL_StatusTypeDef status;
    char time_str[25] = "";
    char month_str[4] = "";
    char *number_begin = NULL;
    char *convert_end = NULL;
    uint16_t temp_year = 0;
    uint8_t i = 0;

    status = MQTT_GetNTPTimeStr(time_str, timeout);
    if (status != HAL_OK) return status;

    //time_str: Sat Jan 10 15:58:27 2026
    strncpy(month_str, time_str + 4, 3);
    month_str[3] = '\0';

    for (i = 0; i < 12; i++)
    {
        if (!strcmp(month_str, MONTH_LIST[i]))
        {
            p_tm->tm_mon = i;
            break;
        }
    }

    number_begin = time_str + 8;
    //printf("<%s>", number_begin); < 1 08:00:01 1970>

    //number_begin:< 1 08:00:01 1970>
    p_tm->tm_mday = strtol(number_begin, &convert_end, 10);
    //convert_end:< 15:58:27 2026>
    p_tm->tm_hour = strtol(convert_end, &convert_end, 10);
    convert_end += 1;   //跳过 “:”
    p_tm->tm_min = strtol(convert_end, &convert_end, 10);
    convert_end += 1;   //跳过 “:”
    p_tm->tm_sec = strtol(convert_end, &convert_end, 10);
    temp_year = strtol(convert_end, &convert_end, 10);
    p_tm->tm_year = temp_year - 1900;

    status = HAL_OK;
    return status;
}

/**
 * @brief 处理串口中断事件，应在串口中断的中断服务函数中调用
 */
void MQTT_HandleUARTInterrupt(void)
{
    static const char subrecv_kw[] = MQTT_SUBRECV_KEYWORD;
    static const size_t subrecv_kw_len = sizeof(subrecv_kw) - 1;
    static size_t kw_match_pos = 0;   /* 当前已匹配的关键字前缀长度 */
    static uint8_t kw_found = 0;      /* 当前行中是否已检测到关键字 */

    if (RecvLen >= MQTT_QUEUE_SIZE)
    {
        memset(RecvBuff, 0, MQTT_QUEUE_SIZE);
        RecvLen = 0;
        kw_match_pos = 0;
        kw_found = 0;
    }
    else
    {
        RecvBuff[RecvLen++] = RecvCh;

        /* 逐字符增量匹配关键字，避免在 '\n' 时做全缓冲区 strstr */
        if (!kw_found && kw_match_pos < subrecv_kw_len)
        {
            if (RecvCh == subrecv_kw[kw_match_pos])
            {
                kw_match_pos++;
                if (kw_match_pos == subrecv_kw_len)
                    kw_found = 1;
            }
            else
            {
                /* 简单复位：不处理部分重叠，MQTTSUBRECV 无重叠前缀 */
                kw_match_pos = (RecvCh == subrecv_kw[0]) ? 1 : 0;
            }
        }

        if (RecvCh == '\n')
        {
            BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
            if (kw_found)
            {
                if (xQueueSendFromISR(queueCloudCmd, RecvBuff, &pxHigherPriorityTaskWoken) != pdTRUE)
                {
                    g_cloud_drop_count++;
                }
            }
            else
            {
                if (xQueueSendFromISR(queueMqttMsg, RecvBuff, &pxHigherPriorityTaskWoken) != pdTRUE)
                {
                    g_mqtt_drop_count++;
                }
            }
            RecvLen = 0;
            memset(RecvBuff, 0, MQTT_QUEUE_SIZE);
            kw_match_pos = 0;
            kw_found = 0;
            portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        }
    }
    HAL_UART_Receive_IT(&MQTT_UART, (uint8_t*)&RecvCh, 1);
}



