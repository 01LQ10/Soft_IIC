/**
 * Copyright (c) 2026 LQ
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @file      driver_soft_iic_basic.c
 * @brief     driver software iic basic example source file
 * @version   1.1.0
 * @author    LQ
 * @date      2026-11-04
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author  <th>Description
 * <tr><td>2026/11/04  <td>1.1.0    <td>LQ      <td>format the comments
 * <tr><td>2026/11/04  <td>1.0.0    <td>LQ      <td>first upload
 * </table>
 */

#include "driver_soft_iic_basic.h"

/* This branch (interface) is the slot mode version: the log output goes
 * through the pin layer function soft_iic_interface_debug_print()
 * (printf -> USART1). The inline mode version lives in the inline branch. */
#include "driver_soft_iic_interface.h"

#define SOFT_IIC_BASIC_LOG(...) soft_iic_interface_debug_print(__VA_ARGS__)

static soft_iic_handle_t gs_handle; /**< soft iic handle */

/**
 * @addtogroup soft_iic_example_driver
 * @{
 */

uint8_t soft_iic_basic_init(void)
{
    /* link functions */
    if (soft_iic_interface_link(&gs_handle) != 0U)
    {
        return 1; /* link failed */
    }

    /* the pin layer is handled by SOFT_IIC_PORT_PIN_INIT() inside soft_iic_init() */
    if (soft_iic_init(&gs_handle) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic init failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_deinit(void)
{
    /* the pin layer is released by SOFT_IIC_PORT_PIN_DEINIT() inside soft_iic_deinit() */
    if (soft_iic_deinit(&gs_handle) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic deinit failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_scan(uint8_t *addr_list, uint8_t *count)
{
    if (soft_iic_scan(&gs_handle, addr_list, count) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic scan failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_probe(uint8_t addr)
{
    uint8_t res;

    res = soft_iic_is_device_ready(&gs_handle, addr, SOFT_IIC_DEFAULT_TRIALS);
    if (res != 0U)
    {
        return 1; /* no device found */
    }

    return 0;
}

uint8_t soft_iic_basic_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (soft_iic_read_reg(&gs_handle, addr, reg, buf, len) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic read register failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_write_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (soft_iic_write_reg(&gs_handle, addr, reg, buf, len) != 0U)
    {
        SOFT_IIC_BASIC_LOG("soft iic: basic write register failed.\n");

        return 1; /* return error */
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
            return 1; /* return error */
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
