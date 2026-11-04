/**
 * @file      driver_soft_iic_basic.c
 * @brief     driver software iic basic example source file
 * @version   1.0.0
 * @author    LQ
 * @date      2026-11-04
 * @license   MIT
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author  <th>Description
 * <tr><td>2026/11/04  <td>1.0.0    <td>LQ      <td>first upload
 * </table>
 */

#include "driver_soft_iic_basic.h"

/* 本分支（inline）是内联模式专用版本：日志出口走端口头的
 * SOFT_IIC_PORT_LOG()（static inline，转发到平台 printf -> USART1）。
 * 槽位模式（soft_iic_interface_debug_print、handle 函数指针槽位）在 interface 分支。
 */
#ifndef SOFT_IIC_PORT_HEADER
#error "inline 分支需要 -DSOFT_IIC_PORT_HEADER=<平台端口头>"
#endif

#include SOFT_IIC_PORT_HEADER

#define SOFT_IIC_BASIC_LOG(...) SOFT_IIC_PORT_LOG(__VA_ARGS__)

static soft_iic_handle_t gs_handle; /**< soft iic handle */

/**
 * @addtogroup soft_iic_example_driver
 * @{
 */

uint8_t soft_iic_basic_init(void)
{
    /* 引脚层由 soft_iic_init() 内部的 SOFT_IIC_PORT_PIN_INIT() 负责，
     * 这里只管启动总线 */
    if (soft_iic_init(&gs_handle) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic init failed.\n");

        return 1;
    }

    return 0;
}

uint8_t soft_iic_basic_deinit(void)
{
    /* 引脚层由 soft_iic_deinit() 内部的 SOFT_IIC_PORT_PIN_DEINIT() 释放 */
    if (soft_iic_deinit(&gs_handle) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic deinit failed.\n");

        return 1;
    }

    return 0;
}

uint8_t soft_iic_basic_scan(uint8_t *addr_list, uint8_t *count)
{
    if (soft_iic_scan(&gs_handle, addr_list, count) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic scan failed.\n");

        return 1;
    }

    return 0;
}

uint8_t soft_iic_basic_probe(uint8_t addr)
{
    uint8_t res;

    res = soft_iic_is_device_ready(&gs_handle, addr, SOFT_IIC_DEFAULT_TRIALS);
    if (res != 0U)
    {
        return 1;
    }

    return 0;
}

uint8_t soft_iic_basic_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (soft_iic_read_reg(&gs_handle, addr, reg, buf, len) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic read register failed.\n");

        return 1;
    }

    return 0;
}

uint8_t soft_iic_basic_write_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (soft_iic_write_reg(&gs_handle, addr, reg, buf, len) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic write register failed.\n");

        return 1;
    }

    return 0;
}

uint8_t soft_iic_basic_dump(uint8_t addr, uint8_t start_reg, uint8_t count)
{
    uint8_t buf[16];
    uint8_t done = 0U;
    uint8_t i;
    uint8_t n;

    if (count == 0U)
    {
        return 2; /* invalid parameter */
    }

    while (done < count)
    {
        n = (uint8_t)(count - done);
        if (n > sizeof(buf))
        {
            n = (uint8_t)sizeof(buf);
        }
        if (soft_iic_basic_read_reg(addr, (uint8_t)(start_reg + done), buf, n) != 0U)
        {
            return 1;
        }
        SOFT_IIC_BASIC_LOG("soft iic: 0x%02X:", (unsigned int)(start_reg + done));
        for (i = 0U; i < n; i++)
        {
            SOFT_IIC_BASIC_LOG(" %02X", (unsigned int)buf[i]);
        }
        SOFT_IIC_BASIC_LOG("\n");
        done = (uint8_t)(done + n);
    }

    return 0;
}

uint8_t soft_iic_basic_get_handle(soft_iic_handle_t **handle)
{
    if (handle == NULL)
    {
        return 2; /* invalid parameter */
    }
    *handle = &gs_handle;

    return 0;
}

/** @} */
