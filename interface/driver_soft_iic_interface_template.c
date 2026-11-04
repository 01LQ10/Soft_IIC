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
 * @file      driver_soft_iic_interface_template.c
 * @brief     driver software iic interface template source file
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
 *
 * Porting template: replace the stubs with the target platform
 * implementation, the driver body stays untouched. This file is not
 * compiled, it is a porting reference only.
 */

#include "driver_soft_iic_interface.h"

/**
 * @addtogroup soft_iic_interface_driver
 * @{
 */

uint8_t soft_iic_interface_init(void)
{
    /* configure scl / sda as open-drain outputs and release both lines */

    return 0;
}

uint8_t soft_iic_interface_deinit(void)
{
    /* release both lines and restore the pin mode */

    return 0;
}

void soft_iic_interface_scl_write(uint8_t level)
{
    (void)level; /* drive scl low when level is 0, release it when level is 1 */
}

void soft_iic_interface_sda_write(uint8_t level)
{
    (void)level; /* drive sda low when level is 0, release it when level is 1 */
}

uint8_t soft_iic_interface_scl_read(void)
{
    /* read the real scl line level */

    return 1;
}

uint8_t soft_iic_interface_sda_read(void)
{
    /* read the real sda line level */

    return 1;
}

void soft_iic_interface_delay_us(uint32_t us)
{
    (void)us; /* must be accurate and independent of the optimization level, see the README delay_us example */
}

void soft_iic_interface_debug_print(const char *const fmt, ...)
{
    (void)fmt; /* forward to the platform log output */
}

uint8_t soft_iic_interface_link(soft_iic_handle_t *handle)
{
    if (handle == NULL)
    {
        return 2; /* invalid parameter */
    }

    DRIVER_SOFT_IIC_LINK_INIT(handle, soft_iic_handle_t); /* initialize handle */
    DRIVER_SOFT_IIC_LINK_SCL_WRITE(handle, soft_iic_interface_scl_write);
    DRIVER_SOFT_IIC_LINK_SDA_WRITE(handle, soft_iic_interface_sda_write);
    DRIVER_SOFT_IIC_LINK_SCL_READ(handle, soft_iic_interface_scl_read);
    DRIVER_SOFT_IIC_LINK_SDA_READ(handle, soft_iic_interface_sda_read);
    DRIVER_SOFT_IIC_LINK_DELAY_US(handle, soft_iic_interface_delay_us);
    DRIVER_SOFT_IIC_LINK_DEBUG_PRINT(handle, soft_iic_interface_debug_print);

    return 0;
}

/**
 * @}
 */
