#include "gt911_port.h"
#include "gt911_reg.h"
#include "re_printf.h"
#include "AT_MQTT_OS.h"
#include "stm32h7xx_hal.h"

// 实例化 GT911 驱动对象
GT911_Object_t gt911_obj;

// 10.1 寸电容触摸屏专属配置表
unsigned char gt911_config_table_10_1_tp[] = {
    0x5d, 0x00, 0x04, 0x58, 0x02, 0x05, 0x0d, 0x20, 0x01, 0x0a,
    0x28, 0x0f, 0x5a, 0x37, 0x03, 0x05, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06, 0x18, 0x1a, 0x1e, 0x14, 0x8c, 0x2e, 0x0e,
    0x28, 0x2a, 0x0c, 0x08, 0x00, 0x00, 0x00, 0x41, 0x02, 0x1d,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1e, 0x82, 0x94, 0xc5, 0x02, 0x07, 0x00, 0x00, 0x04,
    0x96, 0x2c, 0x00, 0x89, 0x30, 0x00, 0x7e, 0x35, 0x00, 0x74,
    0x3b, 0x00, 0x6b, 0x42, 0x00, 0x6b, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10,
    0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0f, 0x10,
    0x12, 0x13, 0x14, 0x16, 0x18, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x24, 0x26, 0x28, 0x29, 0x2a, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x73, 0x01
};

/* ================= 极简版 GPIO 模拟 I2C 实现 ================= */
// H7 主频太高，需要加短暂的微秒级延时让 I2C 信号稳定
static void I2C_Delay(void) {
    for (volatile uint32_t i = 0; i < 200; i++) { __NOP(); }
}

// 宏定义：操作 SCL 和 SDA 引脚 (请根据你在 CubeMX 里的实际引脚名修改)
#define SCL_HIGH()  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_6, GPIO_PIN_SET)
#define SCL_LOW()   HAL_GPIO_WritePin(GPIOH, GPIO_PIN_6, GPIO_PIN_RESET)
#define SDA_HIGH()  HAL_GPIO_WritePin(GPIOI, GPIO_PIN_3, GPIO_PIN_SET)
#define SDA_LOW()   HAL_GPIO_WritePin(GPIOI, GPIO_PIN_3, GPIO_PIN_RESET)
#define SDA_READ()  HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_3)

static void I2C_Start(void) {
    SDA_HIGH(); SCL_HIGH(); I2C_Delay();
    SDA_LOW(); I2C_Delay();
    SCL_LOW(); I2C_Delay();
}

static void I2C_Stop(void) {
    SDA_LOW(); SCL_HIGH(); I2C_Delay();
    SDA_HIGH(); I2C_Delay();
}

static uint8_t I2C_WaitAck(void) {
    uint8_t ack = 0;
    SDA_HIGH(); I2C_Delay();
    SCL_HIGH(); I2C_Delay();
    if (SDA_READ() == GPIO_PIN_SET) ack = 1; // NACK
    SCL_LOW(); I2C_Delay();
    return ack;
}

static void I2C_SendByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) SDA_HIGH();
        else SDA_LOW();
        I2C_Delay();
        SCL_HIGH(); I2C_Delay();
        SCL_LOW();
        byte <<= 1;
    }
}

static uint8_t I2C_ReadByte(uint8_t ack) {
    uint8_t byte = 0;
    SDA_HIGH(); // 释放总线准备读取
    for (uint8_t i = 0; i < 8; i++) {
        byte <<= 1;
        SCL_HIGH(); I2C_Delay();
        if (SDA_READ() == GPIO_PIN_SET) byte |= 0x01;
        SCL_LOW(); I2C_Delay();
    }
    // 发送应答
    if (ack) SDA_LOW(); // ACK
    else SDA_HIGH();    // NACK
    I2C_Delay();
    SCL_HIGH(); I2C_Delay();
    SCL_LOW(); I2C_Delay();
    return byte;
}

/* ================= 桥接官方驱动的读写接口 ================= */
// 包装写函数
int32_t My_I2C_WriteReg(uint16_t DevAddr, uint16_t RegAddr, uint8_t *pData, uint16_t Length) {
    I2C_Start();
    I2C_SendByte(DevAddr);
    if (I2C_WaitAck()) { I2C_Stop(); return -1; }
    I2C_SendByte((uint8_t)(RegAddr >> 8)); // 寄存器高八位
    I2C_WaitAck();
    I2C_SendByte((uint8_t)(RegAddr & 0xFF)); // 寄存器低八位
    I2C_WaitAck();
    for (uint16_t i = 0; i < Length; i++) {
        I2C_SendByte(pData[i]);
        I2C_WaitAck();
    }
    I2C_Stop();
    return 0;
}

// 包装读函数
int32_t My_I2C_ReadReg(uint16_t DevAddr, uint16_t RegAddr, uint8_t *pData, uint16_t Length) {
    I2C_Start();
    I2C_SendByte(DevAddr);
    if (I2C_WaitAck()) { I2C_Stop(); return -1; }
    I2C_SendByte((uint8_t)(RegAddr >> 8));
    I2C_WaitAck();
    I2C_SendByte((uint8_t)(RegAddr & 0xFF));
    I2C_WaitAck();

    I2C_Start();
    I2C_SendByte(DevAddr | 0x01); // 读指令
    if (I2C_WaitAck()) { I2C_Stop(); return -1; }
    for (uint16_t i = 0; i < Length; i++) {
        pData[i] = I2C_ReadByte(i == (Length - 1) ? 0 : 1); // 最后一个字节给 NACK
    }
    I2C_Stop();
    return 0;
}

// 包装 HAL_GetTick，强制转换返回值为有符号整型以匹配官方驱动接口
static int32_t My_GetTick(void) {
    return (int32_t)HAL_GetTick();
}

// 空的 Init/DeInit 回调，GT911 驱动要求非 NULL
static int32_t My_IO_Init(void) { return 0; }
static int32_t My_IO_DeInit(void) { return 0; }

/* ================= 硬件复位与终极初始化序列 ================= */
// 根据 GT911 数据手册编写的地址选定复位时序
static void GT911_Hardware_Reset(void) {
    //PI8 - T_CS - RST (复位引脚)
    //PH7 - T_INT - INT (中断引脚)
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_Delay(10);
    // 拉低 INT，告诉 GT911 将它的 I2C 地址设置为 0x28 (写) / 0x29 (读)
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_Delay(5);
    // 释放 INT，变回高电平
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(50); // 等待 GT911 内部系统就绪
}

// 全局标志：触摸屏是否初始化成功
uint8_t g_touch_initialized = 0;

void Touch_Init(void) {
    g_touch_initialized = 0;
    GT911_Hardware_Reset(); // 1. 硬件强行复位并固定地址

    // 2. 将我们的模拟 I2C 函数挂载到官方驱动里
    GT911_IO_t gt911_io = {
        .Init = My_IO_Init, .DeInit = My_IO_DeInit,
        .Address = 0x28, // 我们刚才通过硬件固定的 0x28 地址
        .WriteReg = My_I2C_WriteReg,
        .ReadReg = My_I2C_ReadReg,
        .GetTick = My_GetTick
    };
    GT911_RegisterBusIO(&gt911_obj, &gt911_io);

    // 3. 调用官方驱动初始化 (读取芯片 ID 等)
    if (GT911_Init(&gt911_obj) != GT911_OK) {
        MQTT_SafePrintf("GT911 Init Failed! (LCD not connected?)\r\n");
        return;
    }
    MQTT_SafePrintf("GT911 Init Success!\r\n");

    // 4. 将 10.1 寸的专属配置参数下发到触摸芯片的 0x8047 寄存器
    My_I2C_WriteReg(0x28, 0x8047, gt911_config_table_10_1_tp, sizeof(gt911_config_table_10_1_tp));
    g_touch_initialized = 1;
}
