//
// Created by AQin on 2022/11/29.
//
#include "AT24C256.h"

#if AT24CXX_TYPE == 128 || AT24CXX_TYPE == 256
#define AT_PAGESIZE 8
#elif AT24CXX_TYPE == 512 || AT24CXX_TYPE == 1024 || AT24CXX_TYPE == 2048
#define AT_PAGESIZE 16
#elif AT24CXX_TYPE == 4096 || AT24CXX_TYPE == 8192
#define AT_PAGESIZE 32
#elif AT24CXX_TYPE == 16384 || AT24CXX_TYPE == 32768
#define AT_PAGESIZE 64
#endif

uint8_t page_white[AT_PAGESIZE]; //Blank Page
extern I2C_HandleTypeDef AT_I2C_H;

static uint8_t Write(uint16_t add, uint8_t* src_val, uint16_t len)
{
    return HAL_I2C_Mem_Write(&AT_I2C_H,
                             AT_WRITEADDR,
                             add,
                             AT24CXX_TYPE <= AT24C16 ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT,
                             src_val,
                             len,
                             AT_TIMEOUT);
}

static uint8_t Read(uint16_t add, uint8_t* dst_val, uint16_t len)
{
    return HAL_I2C_Mem_Read(&AT_I2C_H,
                            AT_READADDR,
                            add,
                            AT24CXX_TYPE <= AT24C16 ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT,
                            dst_val,
                            len,
                            AT_TIMEOUT);
}

void AT24C_ClearAll()
{
    uint16_t pages = AT24CXX_TYPE / AT_PAGESIZE;
    while (pages--)
    {
        Write(pages, page_white, AT_PAGESIZE);
    }
#ifdef ENABLE_DELAY
    HAL_Delay(AT_REST_TIME);
#endif
}

uint8_t AT24C_WriteByte(uint16_t add, uint8_t src_val)
{
    uint8_t state;
    state = Write(add, &src_val, 1);
#ifdef ENABLE_DELAY
    HAL_Delay(AT_REST_TIME);
#endif
    return state == HAL_OK;
}

uint8_t AT24C_ReadByte(uint16_t add, uint8_t* dst_val)
{
    uint8_t state;
    state = Read(add, dst_val, 1);
#ifdef ENABLE_DELAY
    HAL_Delay(AT_REST_TIME);
#endif
    return state == HAL_OK;
}

HAL_StatusTypeDef AT24C_WriteArray(uint16_t add, uint8_t* src_array, uint16_t len)
{
    uint8_t* p_src = src_array;
    uint8_t status;
    /* 当前地址到页边界的剩余字节数 */
    uint16_t first_len = AT_PAGESIZE - (add % AT_PAGESIZE);
    if (first_len > len)
        first_len = len;

    /* 写第一段（对齐到页边界） */
    status = Write(add, p_src, first_len);
    if (status != HAL_OK) return (HAL_StatusTypeDef)status;
#ifdef ENABLE_DELAY
    HAL_Delay(AT_REST_TIME);
#endif
    add   += first_len;
    p_src += first_len;
    len   -= first_len;

    /* 按整页写入 */
    while (len >= AT_PAGESIZE)
    {
        status = Write(add, p_src, AT_PAGESIZE);
        if (status != HAL_OK) return (HAL_StatusTypeDef)status;
#ifdef ENABLE_DELAY
        HAL_Delay(AT_REST_TIME);
#endif
        add   += AT_PAGESIZE;
        p_src += AT_PAGESIZE;
        len   -= AT_PAGESIZE;
    }

    /* 写剩余不足一页的部分 */
    if (len != 0)
    {
        status = Write(add, p_src, len);
        if (status != HAL_OK) return (HAL_StatusTypeDef)status;
#ifdef ENABLE_DELAY
        HAL_Delay(AT_REST_TIME);
#endif
    }

    return HAL_OK;
}

HAL_StatusTypeDef AT24C_ReadArray(uint16_t add, uint8_t* dst_array, uint16_t len)
{
    uint8_t status = Read(add, dst_array, len);
    return (HAL_StatusTypeDef)status;
}

void AT24C_WriteBigInt(uint16_t add, big_int val, uint8_t bits)
{
    uint8_t u8_list[8] = { 0 };
    uint8_t temp = 0;
    big_int v = val;
    for (int8_t i = bits / 8 - 1; i >= 0; i--)
    {
        temp = v % 256;
        v >>= 8;
        u8_list[i] = temp;
    }
    AT24C_WriteArray(add, u8_list, bits / 8);
}

big_int AT24C_ReadBigInt(uint16_t add, uint8_t bits)
{
    big_int val = 0;
    uint8_t u8_list[8] = { 0 };
    AT24C_ReadArray(add, u8_list, bits / 8);

    for (uint8_t i = 0; i < bits / 8; i++)
    {
        val <<= 8;
        val |= u8_list[i];
    }
    return val;
}

void AT24C_WriteDouble(uint16_t add, double val)
{
    union {
        double d;
        uint64_t i;
    } u;
    u.d = val;
    uint64_t val_int = u.i;

    uint8_t u8_list[8] = { 0 };
    uint8_t temp = 0;

    for (int8_t i = 64 / 8 - 1; i >= 0; i--)
    {
        temp = val_int % 256;
        val_int >>= 8;
        u8_list[i] = temp;
    }
    AT24C_WriteArray(add, u8_list, 64 / 8);
}

double AT24C_ReadDouble(uint16_t add)
{
    uint64_t val = 0;
    uint8_t u8_list[8] = { 0 };
    AT24C_ReadArray(add, u8_list, 64 / 8);

    for (uint8_t i = 0; i < 64 / 8; i++)
    {
        val <<= 8;
        val |= u8_list[i];
    }
    
    union {
        uint64_t i;
        double d;
    } u;
    u.i = val;
    return u.d;
}
