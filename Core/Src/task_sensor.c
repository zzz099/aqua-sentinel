/**
 * @file   task_sensor.c
 * @brief  传感器采集任务
 *
 * 功能概述：
 *   - 初始化 DS18B20 / BMP280 / ADC-DMA / 水位 / PH 传感器
 *   - 从 EEPROM 恢复上次保存的数据及阀值
 *   - 周期采集并更新全局传感器变量
 *   - 周期将数据持久化到 EEPROM
 */

#include "task_sensor.h"
#include "app_runtime.h"
#include "adc_config.h"
#include "adc.h"
#include "AT24C256.h"
#include "auto_fish_ctrl.h"
#include "ds18b20.h"
#include "i2c.h"
#include "ph_sensor.h"
#include "tds.h"
#include "turbidity.h"
#include "waterlevel.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/* ============ EEPROM 存储地址分配 ============ */
#define SAVE_INTERVAL_MS    60000   /**< 定时存储间隔 (ms) */
#define EEPROM_ADDR_TEMP    0x00    /**< 水温 */
#define EEPROM_ADDR_TURB    0x10    /**< 浊度 (NTU) */
#define EEPROM_ADDR_VOLT    0x20    /**< 浊度电压 (V) */
#define EEPROM_ADDR_TDS     0x30    /**< TDS (ppm) */
#define EEPROM_ADDR_PRESS   0x40    /**< 气压 (Pa) */
#define EEPROM_ADDR_PH      0x50    /**< PH 值 */

/* ============ BMP280 I2C 回调 (HAL 封装) ============ */
#define BMP280_I2C_HANDLE   hi2c2
#define BMP280_I2C_ADDR     0xEC    /**< BMP280 7-bit 地址 0x76 左移一位 */

static void BMP280_ReadData(BMP280ObjectType *bmp, uint8_t regAddress,
                            uint8_t *rData, uint16_t rSize) {
  HAL_I2C_Mem_Read(&BMP280_I2C_HANDLE, bmp->bmpAddress, regAddress,
                   I2C_MEMADD_SIZE_8BIT, rData, rSize, 100);
}

static void BMP280_WriteData(BMP280ObjectType *bmp, uint8_t regAddress,
                             uint8_t command) {
  HAL_I2C_Mem_Write(&BMP280_I2C_HANDLE, bmp->bmpAddress, regAddress,
                    I2C_MEMADD_SIZE_8BIT, &command, 1, 100);
}

static void BMP280_DelayMs(volatile uint32_t nTime) {
  osDelay(nTime);
}

/**
 * @brief  传感器采集任务入口
 *
 * 执行流程：
 *   1. 初始化各传感器及 ADC-DMA
 *   2. 从 EEPROM 恢复历史数据与阀值
 *   3. 进入主循环：读水温 → ADC 读浊度/TDS/PH → 读气压 → 定时存 EEPROM
 */
void TaskReadSensorData(void *argument)
{
  UNUSED(argument);
  uint8_t tempsensor_status = DS18B20_Init();
  uint32_t last_eeprom_write_tick = 0;
  /* ---- 启动 ADC校准 + DMA 连续转换 ---- */
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buffer, ADC_TOTAL_BUF_SIZE);
  osPrintf("Sensor Init Done\r\n");

  /* ---- BMP280 初始化 ---- */

  BMP280Initialization(&g_bmp280,
                       BMP280_I2C_ADDR,
                       BMP280_I2C,
                       BMP280_T_SB_500,
                       BMP280_IIR_FILTER_COEFF_X16,
                       BMP280_SPI3W_DISABLE,
                       BMP280_TEMP_SAMPLE_X2,
                       BMP280_PRES_SAMPLE_X16,
                       BMP280_POWER_NORMAL_MODE,
                       BMP280_ReadData,
                       BMP280_WriteData,
                       BMP280_DelayMs,
                       NULL);
  if (g_bmp280.chipID == 0x58) {
    osPrintf("BMP280 Connected (ID=0x%02X).\r\n", g_bmp280.chipID);
  } else {
    osPrintf("BMP280 Not Detected! (ID=0x%02X)\r\n", g_bmp280.chipID);
  }

  WaterLevel_Init();
  osPrintf("WaterLevel sensor (GPIO PA1) initialized.\r\n");

  PH_Init();
  osPrintf("PH sensor (ADC1_INP4) initialized.\r\n");

  /* ---- EEPROM 数据恢复 ---- */
  if (HAL_I2C_IsDeviceReady(&AT_I2C_H, AT_WRITEADDR, 2, 100) == HAL_OK) {
    osPrintf("EEPROM (AT24C256) Connected.\r\n");
    osPrintf("Read EEPROM data...\r\n");
    double last_temp = AT24C_ReadDouble(EEPROM_ADDR_TEMP);
    double last_turb = AT24C_ReadDouble(EEPROM_ADDR_TURB);
    double last_volt = AT24C_ReadDouble(EEPROM_ADDR_VOLT);
    double last_tds = AT24C_ReadDouble(EEPROM_ADDR_TDS);
    double last_ph = AT24C_ReadDouble(EEPROM_ADDR_PH);

    if (isnan(last_temp))
      last_temp = 0.0;
    if (isnan(last_turb))
      last_turb = 0.0;
    if (isnan(last_volt))
      last_volt = 0.0;
    if (isnan(last_tds))
      last_tds = 0.0;
    if (isnan(last_ph))
      last_ph = 0.0;

    osPrintf("=== [EEPROM Load] Last Saved Data ===\r\n");
    osPrintf("Temp: %.2f, Turb: %.2f, Volt: %.3f, TDS: %.0f, PH: %.2f\r\n",
             last_temp, last_turb, last_volt, last_tds, last_ph);
    osPrintf("=====================================\r\n");

    FishCtrl_LoadFromEEPROM();
    osPrintf("[EEPROM] Thresholds restored from EEPROM.\r\n");
  } else {
    osPrintf("EEPROM (AT24C256) Not Detected!\r\n");
  }

  osPrintf("[Sensor] All sensors initialized, entering main loop.\r\n");

  /* ========== 主采集循环 ========== */
  for (;;) {
    /* -- 1. DS18B20 水温 -- */
    if (tempsensor_status != 0) {
      osPrintf("Sensor Error, Retrying...\r\n");
      tempsensor_status = DS18B20_Init();
      osDelay(10);
    } else {
      DS18B20_Start();
      osDelay(800);
      short temp_raw = DS18B20_Get_Temp();
      if (temp_raw < 950 && temp_raw > -200) {
        g_Temperature = temp_raw;
        cloud_Temperature = g_Temperature / 10.0f;
      }
    }

    /* -- 2. 失效 D-Cache 以确保读到最新 DMA 数据 -- */
    SCB_InvalidateDCache_by_Addr((uint32_t *)adc_dma_buffer,
                                 ADC_TOTAL_BUF_SIZE * sizeof(uint16_t));

    /* -- 3. 浊度 -- */
    float voltage = Turbidity_Get_Voltage_DMA();
    float ntu     = Turbidity_Get_NTU_DMA();
    g_Turbidity_NTU = ntu;
    g_Turbidity_Voltage = voltage;

    /* -- 4. TDS (带水温补偿) -- */
    float tds = TDS_Get_Value_DMA((float)cloud_Temperature);
    g_TDS_Value = tds;

    /* -- 5. PH -- */
    float ph = PH_ReadValue();
    g_PH_Value = ph;

    /* -- 6. BMP280 气压/环境温度 -- */
    if (g_bmp280.chipID == 0x58) {
      GetBMP280Measure(&g_bmp280);
      g_BMP280_Temperature = g_bmp280.temperature;
      g_BMP280_Pressure = g_bmp280.pressure;
    }

    /* -- 7. 调试日志 (osPrintf 线程安全) -- */
    int ntu_int = (int)(ntu);
    int ntu_dec = (int)((ntu - ntu_int) * 100);
    int vol_int = (int)(voltage);
    int vol_dec = (int)((voltage - vol_int) * 1000);
    int tds_int = (int)(tds);

    osPrintf(
        "Temp: %d.%d C | Turbidity: %d.%02d NTU (%d.%03d V) | TDS: %d ppm\r\n",
        g_Temperature / 10, abs(g_Temperature % 10), ntu_int, abs(ntu_dec),
        vol_int, abs(vol_dec), tds_int);

    /* -- 8. 定时存储到 EEPROM -- */
    if ((HAL_GetTick() - last_eeprom_write_tick) > SAVE_INTERVAL_MS) {
      osPrintf(">> Saving data to AT24C256...\r\n");

      AT24C_WriteDouble(EEPROM_ADDR_TEMP, (double)cloud_Temperature);
      AT24C_WriteDouble(EEPROM_ADDR_TURB, (double)g_Turbidity_NTU);
      AT24C_WriteDouble(EEPROM_ADDR_VOLT, (double)g_Turbidity_Voltage);
      AT24C_WriteDouble(EEPROM_ADDR_TDS, (double)g_TDS_Value);
      AT24C_WriteDouble(EEPROM_ADDR_PH, (double)g_PH_Value);
      AT24C_WriteDouble(EEPROM_ADDR_PRESS, (double)g_BMP280_Pressure);

      last_eeprom_write_tick = HAL_GetTick();
      osPrintf(">> Save Complete.\r\n");
    }
    osDelay(1);
  }
}
