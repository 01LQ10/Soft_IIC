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
 * @version   1.3.0
 * @author    LQ
 * @date      2026-11-04
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author  <th>Description
 * <tr><td>2026/11/04  <td>1.3.0    <td>LQ      <td>use the basic layer default macros
 * <tr><td>2026/11/04  <td>1.2.0    <td>LQ      <td>call soft_iic_link, drop interface include and log macro
 * <tr><td>2026/11/04  <td>1.1.0    <td>LQ      <td>format the comments
 * <tr><td>2026/11/04  <td>1.0.0    <td>LQ      <td>first upload
 * </table>
 */

#include "driver_soft_iic_basic.h"
#include "driver_soft_iic_interface.h"

/* the example layer calls the src/ api only: every slot is bound with the
 * DRIVER_SOFT_IIC_LINK_* macros, the log goes through the bound debug_print slot */

static soft_iic_handle_t gs_handle; /**< soft iic handle */

/**
 * @addtogroup soft_iic_example_driver
 * @{
 */

uint8_t soft_iic_basic_init(void)
{
    /* link functions */
    DRIVER_SOFT_IIC_LINK_INIT(&gs_handle, soft_iic_handle_t); /* initialize handle */
    DRIVER_SOFT_IIC_LINK_PIN_INIT(&gs_handle, soft_iic_interface_pin_init);
    DRIVER_SOFT_IIC_LINK_PIN_DEINIT(&gs_handle, soft_iic_interface_pin_deinit);
    DRIVER_SOFT_IIC_LINK_SCL_WRITE(&gs_handle, soft_iic_interface_scl_write);
    DRIVER_SOFT_IIC_LINK_SDA_WRITE(&gs_handle, soft_iic_interface_sda_write);
    DRIVER_SOFT_IIC_LINK_SCL_READ(&gs_handle, soft_iic_interface_scl_read);
    DRIVER_SOFT_IIC_LINK_SDA_READ(&gs_handle, soft_iic_interface_sda_read);
    DRIVER_SOFT_IIC_LINK_DELAY_US(&gs_handle, soft_iic_interface_delay_us);
    DRIVER_SOFT_IIC_LINK_DEBUG_PRINT(&gs_handle, soft_iic_interface_debug_print);

    /* the pin layer slot is called inside soft_iic_init() */
    if (soft_iic_init(&gs_handle) != 0U)
    {
        gs_handle.debug_print("soft iic: basic init failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_deinit(void)
{
    /* the pin layer slot is released inside soft_iic_deinit() */
    if (soft_iic_deinit(&gs_handle) != 0U)
    {
        gs_handle.debug_print("soft iic: basic deinit failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_scan(uint8_t *addr_list, uint8_t *count)
{
    if (soft_iic_scan(&gs_handle, addr_list, count) != 0U)
    {
        gs_handle.debug_print("soft iic: basic scan failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_probe(uint8_t addr)
{
    uint8_t res;

    res = soft_iic_is_device_ready(&gs_handle, addr, SOFT_IIC_BASIC_DEFAULT_TRIALS);
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
        gs_handle.debug_print("soft iic: basic read register failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_write_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (soft_iic_write_reg(&gs_handle, addr, reg, buf, len) != 0U)
    {
        gs_handle.debug_print("soft iic: basic write register failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_basic_dump(uint8_t addr, uint8_t start_reg, uint8_t count)
{
    uint8_t buf[SOFT_IIC_BASIC_DEFAULT_DUMP_WIDTH];
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
        gs_handle.debug_print("soft iic: 0x%02X:", (unsigned int)(start_reg + done));
        for (i = 0U; i < n; i++)
        {
            gs_handle.debug_print(" %02X", (unsigned int)buf[i]);
        }
        gs_handle.debug_print("\n");
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
