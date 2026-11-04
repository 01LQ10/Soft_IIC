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
 * @version   2.0.0
 * @author    LQ
 * @date      2026-11-04
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author  <th>Description
 * <tr><td>2026/11/04  <td>2.0.0    <td>LQ      <td>self contained test, drop the example layer dependency
 * <tr><td>2026/11/04  <td>1.2.0    <td>LQ      <td>call the src api only, drop the interface include
 * <tr><td>2026/11/04  <td>1.1.0    <td>LQ      <td>format the comments
 * <tr><td>2026/11/04  <td>1.0.0    <td>LQ      <td>first upload
 * </table>
 */

#include "driver_soft_iic_scan_test.h"
#include "driver_soft_iic_interface.h"

#define SOFT_IIC_SCAN_TEST_MAX_DEVICES (16U) /**< scan list capacity */
#define SOFT_IIC_SCAN_TEST_DELAY_MS (100U)   /**< delay between scan rounds, in ms */

/**
 * @addtogroup soft_iic_test_driver soft iic test driver function
 * @brief    soft iic test driver modules
 * @{
 */

static soft_iic_handle_t gs_handle; /**< soft iic handle */

/**
 * @brief     scan the bus and print every acknowledged address
 * @param[in] times scan repeat times
 * @return    status code
 *            - 0 success
 *            - 1 test failed
 * @note      none
 */
uint8_t soft_iic_scan_test(uint32_t times)
{
    uint8_t addr_list[SOFT_IIC_SCAN_TEST_MAX_DEVICES];
    uint8_t count = (uint8_t)sizeof(addr_list);
    soft_iic_info_t info;
    uint32_t round;
    uint8_t i;
    uint8_t res;

    /* link functions, the test binds the interface layer directly */
    DRIVER_SOFT_IIC_LINK_INIT(&gs_handle, soft_iic_handle_t);
    DRIVER_SOFT_IIC_LINK_PIN_INIT(&gs_handle, soft_iic_interface_pin_init);
    DRIVER_SOFT_IIC_LINK_PIN_DEINIT(&gs_handle, soft_iic_interface_pin_deinit);
    DRIVER_SOFT_IIC_LINK_SCL_WRITE(&gs_handle, soft_iic_interface_scl_write);
    DRIVER_SOFT_IIC_LINK_SDA_WRITE(&gs_handle, soft_iic_interface_sda_write);
    DRIVER_SOFT_IIC_LINK_SCL_READ(&gs_handle, soft_iic_interface_scl_read);
    DRIVER_SOFT_IIC_LINK_SDA_READ(&gs_handle, soft_iic_interface_sda_read);
    DRIVER_SOFT_IIC_LINK_DELAY_US(&gs_handle, soft_iic_interface_delay_us);
    DRIVER_SOFT_IIC_LINK_DEBUG_PRINT(&gs_handle, soft_iic_interface_debug_print);

    /* info self check first */
    res = soft_iic_info(&info);
    if (res != 0U)
    {
        soft_iic_interface_debug_print("soft iic: info failed.\n");

        return 1;
    }
    soft_iic_interface_debug_print("soft iic: chip name is %s.\n", info.chip_name);
    soft_iic_interface_debug_print("soft iic: manufacturer is %s.\n", info.manufacturer_name);
    soft_iic_interface_debug_print("soft iic: interface is %s.\n", info.interface);

    /* init the bus */
    res = soft_iic_init(&gs_handle);
    if (res != 0U)
    {
        soft_iic_interface_debug_print("soft iic: init failed.\n");

        return 1;
    }

    /* scan loop */
    for (round = 0U; round < times; round++)
    {
        count = (uint8_t)sizeof(addr_list);
        res = soft_iic_scan(&gs_handle, addr_list, &count);
        if (res != 0U)
        {
            soft_iic_interface_debug_print("soft iic: scan failed.\n");
            (void)soft_iic_deinit(&gs_handle);

            return 1;
        }

        soft_iic_interface_debug_print("soft iic: round %u found %u device(s).\n",
                                       (unsigned int)round, (unsigned int)count);
        for (i = 0U; i < count; i++)
        {
            soft_iic_interface_debug_print("soft iic: address 0x%02X acknowledged.\n",
                                           (unsigned int)addr_list[i]);
        }

        gs_handle.delay_us((uint32_t)SOFT_IIC_SCAN_TEST_DELAY_MS * 1000U); /* round delay */
    }

    /* deinit the bus */
    res = soft_iic_deinit(&gs_handle);
    if (res != 0U)
    {
        soft_iic_interface_debug_print("soft iic: deinit failed.\n");

        return 1;
    }

    return 0;
}

/**
 * @}
 */
