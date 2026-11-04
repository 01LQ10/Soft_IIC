/**
 * @file      driver_soft_iic_scan_test.c
 * @brief     driver software iic scan test source file
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

#include "driver_soft_iic_scan_test.h"
#include "driver_soft_iic_basic.h"

#ifndef SOFT_IIC_PORT_HEADER
#error "inline 分支需要 -DSOFT_IIC_PORT_HEADER=<平台端口头>"
#endif
#include SOFT_IIC_PORT_HEADER

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

    SOFT_IIC_PORT_LOG("soft iic: scan test start.\n");

    res = soft_iic_basic_init();
    if (res != 0U)
    {
        SOFT_IIC_PORT_LOG("soft iic: scan test init failed.\n");

        return 1;
    }

    res = soft_iic_basic_scan(addr_list, &count);
    if (res != 0U)
    {
        SOFT_IIC_PORT_LOG("soft iic: scan test scan failed.\n");
        (void)soft_iic_basic_deinit();

        return 1;
    }

    SOFT_IIC_PORT_LOG("soft iic: %u device(s) found.\n", (unsigned int)count);
    for (i = 0U; i < count; i++)
    {
        SOFT_IIC_PORT_LOG("soft iic: address 0x%02X acknowledged.\n",
                                       (unsigned int)addr_list[i]);
    }

    res = soft_iic_basic_deinit();
    if (res != 0U)
    {
        SOFT_IIC_PORT_LOG("soft iic: scan test deinit failed.\n");

        return 1;
    }
    SOFT_IIC_PORT_LOG("soft iic: scan test stop.\n");

    return 0;
}

/** @} */
