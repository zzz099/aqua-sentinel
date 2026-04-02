#include "ds18b20.h"
#include "tim.h"

// 声明外部的定时器句柄，确保 main.c 或 tim.c 中已经定义了 htim7
extern TIM_HandleTypeDef htim6;

// 基于 TIM6 的微秒延时函数 (带超时保护)
void delay_us(uint16_t us)
{
    // 设置计数器为0
    __HAL_TIM_SET_COUNTER(&htim6, 0);
    // 等待计数器达到指定值
    // 增加超时保护，防止TIM6挂掉导致死循环 (最大超时约1秒，如果SysTick也没了，那就真的没办法了)
    uint32_t start = HAL_GetTick();
    while (__HAL_TIM_GET_COUNTER(&htim6) < us)
    {
        if((HAL_GetTick() - start) > 1000) break; 
    }
}

// 模式切换函数：HAL库版本
// mode: 1=Output (PP), 0=Input (Floating)
void DS18B20_Mode(uint8_t mode)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if(mode == DS18B20_MODE_OUT)
    {
        GPIO_InitStruct.Pin = DS18B20_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
    }
    else
    {
        GPIO_InitStruct.Pin = DS18B20_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;     // 输入模式
        GPIO_InitStruct.Pull = GPIO_NOPULL;         // 浮空输入 (F1系列无专门Floating宏，NOPULL即浮空)
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
    }
}

// 复位DS18B20
void DS18B20_Rst(void)
{
    DS18B20_Mode(DS18B20_MODE_OUT); // SET OUTPUT
    DS18B20_Low;                    // 拉低DQ
    delay_us(750);                  // 拉低750us
    DS18B20_High;                   // DQ=1
    delay_us(15);                   // 15US
}

// 等待DS18B20的回应
// 返回1:未检测到DS18B20的存在
// 返回0:存在
uint8_t DS18B20_Check(void)
{
    uint8_t retry = 0;
    DS18B20_Mode(DS18B20_MODE_IN); // SET INPUT

    // 等待拉低
    while (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN) && retry < 200)
    {
        retry++;
        delay_us(1);
    }
    if(retry >= 200) return 1;
    else retry = 0;

    // 等待拉高
    while (!HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN) && retry < 240)
    {
        retry++;
        delay_us(1);
    }
    if(retry >= 240) return 1;
    return 0;
}

// 从DS18B20读取一个位
// 返回值：1/0
uint8_t DS18B20_Read_Bit(void)
{
    uint8_t data;
    DS18B20_Mode(DS18B20_MODE_OUT); // SET OUTPUT
    DS18B20_Low;
    delay_us(2);
    DS18B20_High;
    DS18B20_Mode(DS18B20_MODE_IN);  // SET INPUT
    delay_us(12);

    if(HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN)) data = 1;
    else data = 0;

    delay_us(50);
    return data;
}

// 从DS18B20读取一个字节
// 返回值：读到的数据
uint8_t DS18B20_Read_Byte(void)
{
    uint8_t i, j, dat;
    dat = 0;

    __disable_irq();// 禁止中断，确保读取过程中不被打断，避免时序问题导致读取错误
    for (i = 1; i <= 8; i++)
    {
        j = DS18B20_Read_Bit();
        dat = (j << 7) | (dat >> 1);
    }
    __enable_irq(); // 重新使能中断
    return dat;
}

// 写一个字节到DS18B20
void DS18B20_Write_Byte(uint8_t dat)
{
    uint8_t j;
    uint8_t testb;
    DS18B20_Mode(DS18B20_MODE_OUT); // SET OUTPUT
    for (j = 1; j <= 8; j++)
    {
        testb = dat & 0x01;
        dat = dat >> 1;
        if (testb)
        {
            DS18B20_Low;  // Write 1
            delay_us(2);
            DS18B20_High;
            delay_us(60);
        }
        else
        {
            DS18B20_Low;  // Write 0
            delay_us(60);
            DS18B20_High;
            delay_us(2);
        }
    }
}

// 开始温度转换
void DS18B20_Start(void)
{
    DS18B20_Rst();
    DS18B20_Check();
    DS18B20_Write_Byte(0xcc); // skip rom
    DS18B20_Write_Byte(0x44); // convert
}

// 初始化DS18B20
// 返回1:不存在
// 返回0:存在
uint8_t DS18B20_Init(void)
{
    // 开启GPIO时钟
    DS18B20_CLK_ENABLE();
    HAL_TIM_Base_Start(&htim6); // 启动定时器

    // 初始状态配置为输出高电平
    DS18B20_Mode(DS18B20_MODE_OUT);
    DS18B20_High;

    DS18B20_Rst();
    return DS18B20_Check();
}

// 从ds18b20得到温度值
// 精度：0.1C
// 返回值：温度值 （-550~1250）
short DS18B20_Get_Temp(void)
{
    uint8_t temp;
    uint8_t TL, TH;
    short tem;

    //DS18B20_Start();          // ds1820 start convert
    DS18B20_Rst();
    DS18B20_Check();
    DS18B20_Write_Byte(0xcc); // skip rom
    DS18B20_Write_Byte(0xbe); // read scratchpad
    TL = DS18B20_Read_Byte(); // LSB
    TH = DS18B20_Read_Byte(); // MSB

    if(TH > 7)
    {
        TH = ~TH;
        TL = ~TL;
        temp = 0; // 温度为负
    }
    else temp = 1; // 温度为正

    tem = TH;
    tem <<= 8;
    tem += TL;

    // 原始代码是 tem * 0.625，但这里直接转为 float 再转回 short 可能会损失
    // 实际上 DS18B20 分辨率是 0.0625 度。
    // 原作者代码逻辑：(tem * 0.0625) * 10 = tem * 0.625
    // 目的是保留一位小数并作为整数返回 (例如 25.5度 返回 255)

    tem = (short)((float)tem * 0.625);

    if(temp) return tem;
    else return -tem;
}