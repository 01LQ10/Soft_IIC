/**
 * @file      driver_soft_iic_scan_test.h
 * @brief     driver software iic scan test header file
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

#ifndef DRIVER_SOFT_IIC_SCAN_TEST_H
#define DRIVER_SOFT_IIC_SCAN_TEST_H

#include "driver_soft_iic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup soft_iic_test_driver soft iic test driver function
 * @brief    soft iic test driver modules
 * @{
 */

/**
 * @brief     scan the bus and print every acknowledged address
 * @return    status code
 *            - 0 success
 *            - 1 test failed
 * @note      needs a real bus, only build it with -DSOFT_IIC_BUILD_TEST=ON
 */
uint8_t soft_iic_scan_test(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
