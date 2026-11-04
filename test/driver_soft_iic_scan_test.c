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
 * @file      driver_soft_iic_scan_test.c
 * @brief     driver software iic scan test source file
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

#include "driver_soft_iic_scan_test.h"
#include "driver_soft_iic_basic.h"
#include "driver_soft_iic_interface.h"

/**
 * @addtogroup soft_iic_test_driver soft iic test driver function
 * @brief    soft iic test driver modules
 * @{
 */

uint8_t soft_iic_scan_test(void)
{
    uint8_t addr_list[SOFT_IIC_BASIC_MAX_DEVICES];
    uint8_t count = (uint8_t)sizeof(addr_list);
    uint8_t i;
    uint8_t res;

    soft_iic_interface_debug_print("soft iic: scan test start.\n");

    /* init the basic example */
    res = soft_iic_basic_init();
    if (res != 0U)
    {
        soft_iic_interface_debug_print("soft iic: scan test init failed.\n");

        return 1; /* return error */
    }

    /* scan the bus */
    res = soft_iic_basic_scan(addr_list, &count);
    if (res != 0U)
    {
        soft_iic_interface_debug_print("soft iic: scan test scan failed.\n");
        (void)soft_iic_basic_deinit();

        return 1; /* return error */
    }

    soft_iic_interface_debug_print("soft iic: %u device(s) found.\n", (unsigned int)count);
    for (i = 0U; i < count; i++)
    {
        soft_iic_interface_debug_print("soft iic: address 0x%02X acknowledged.\n",
                                       (unsigned int)addr_list[i]);
    }

    /* deinit the basic example */
    res = soft_iic_basic_deinit();
    if (res != 0U)
    {
        soft_iic_interface_debug_print("soft iic: scan test deinit failed.\n");

        return 1; /* return error */
    }
    soft_iic_interface_debug_print("soft iic: scan test stop.\n");

    return 0;
}

/** @} */
