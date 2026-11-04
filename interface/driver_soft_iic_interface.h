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
 * @file      driver_soft_iic_interface.h
 * @brief     driver software iic interface header file
 * @version   1.2.0
 * @author    LQ
 * @date      2026-11-04
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author  <th>Description
 * <tr><td>2026/11/04  <td>1.2.0    <td>LQ      <td>rename to pin_init / pin_deinit, drop interface_link
 * <tr><td>2026/11/04  <td>1.1.0    <td>LQ      <td>format the comments
 * <tr><td>2026/11/04  <td>1.0.0    <td>LQ      <td>first upload
 * </table>
 *
 * This file contains declarations only, no platform header is included:
 * the platform implementation lives in the project (for this repository it
 * is User/Src/driver_soft_iic_interface.c, PY32F030 + HAL). To port to a
 * new platform, replace that one implementation file only, the driver body
 * (src/) stays untouched.
 *
 * Layer rule: this header is included by the platform implementation file
 * and by src/driver_soft_iic.c only. The example and test layers must not
 * include it, they bind every slot with the DRIVER_SOFT_IIC_LINK_* macros.
 */

#ifndef DRIVER_SOFT_IIC_INTERFACE_H
#define DRIVER_SOFT_IIC_INTERFACE_H

#include "driver_soft_iic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup soft_iic_interface_driver soft iic interface driver function
 * @brief    soft iic interface driver modules
 * @{
 */

/**
 * @brief     initialize the pin layer (open-drain outputs, bus released)
 * @return    status code
 *            - 0 success
 *            - 1 initialization failed
 * @note      must release any hardware iic peripheral using the same pins first
 */
uint8_t soft_iic_interface_pin_init(void);

/**
 * @brief     close the pin layer
 * @return    status code
 *            - 0 success
 *            - 1 deinitialization failed
 * @note      releases both lines so a hardware iic peripheral can take over
 */
uint8_t soft_iic_interface_pin_deinit(void);

/**
 * @brief     drive the scl line
 * @param[in] level 0 pulls low, 1 releases the line
 * @return    none
 * @note      none
 */
void soft_iic_interface_scl_write(uint8_t level);

/**
 * @brief     drive the sda line
 * @param[in] level 0 pulls low, 1 releases the line
 * @return    none
 * @note      none
 */
void soft_iic_interface_sda_write(uint8_t level);

/**
 * @brief     read the real scl line level
 * @return    line level (0 or 1)
 * @note      used by the clock stretching detection
 */
uint8_t soft_iic_interface_scl_read(void);

/**
 * @brief     read the real sda line level
 * @return    line level (0 or 1)
 * @note      used by the acknowledge detection
 */
uint8_t soft_iic_interface_sda_read(void);

/**
 * @brief     blocking microsecond delay
 * @param[in] us delay time in us
 * @return    none
 * @note      must not depend on the optimization level
 */
void soft_iic_interface_delay_us(uint32_t us);

/**
 * @brief     formatted debug output
 * @param[in] *fmt format string
 * @return    none
 * @note      none
 */
void soft_iic_interface_debug_print(const char *const fmt, ...);

/* the caller binds every slot with the DRIVER_SOFT_IIC_LINK_* macros
 * declared in driver_soft_iic.h; the pin layer functions above are bound to
 * the pin_init / pin_deinit slots, the example and test layers still need
 * this header to reference those function names */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
