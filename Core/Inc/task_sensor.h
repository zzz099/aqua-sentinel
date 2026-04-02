/**
 * @file   task_sensor.h
 * @brief  传感器采集任务入口声明
 */

#ifndef __TASK_SENSOR_H__
#define __TASK_SENSOR_H__

#ifdef __cplusplus
extern "C" {
#endif

void TaskReadSensorData(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_SENSOR_H__ */
