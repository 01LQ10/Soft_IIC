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
 * @file      driver_soft_iic.c
 * @brief     driver software iic source file
 * @version   1.3.0
 * @author    LQ
 * @date      2026-11-04
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author  <th>Description
 * <tr><td>2026/11/04  <td>1.3.0    <td>LQ      <td>use handle slots directly, drop interface include
 * <tr><td>2026/11/04  <td>1.2.0    <td>LQ      <td>remove inline branch guard, fix init order
 * <tr><td>2026/11/04  <td>1.1.0    <td>LQ      <td>format the comments
 * <tr><td>2026/11/04  <td>1.0.0    <td>LQ      <td>first upload
 * </table>
 */

#include "driver_soft_iic.h"

/* port layer
 * every hardware access goes through the handle function pointer slots
 * (LibDriver injection style); the pin init / deinit hooks are slots too,
 * they are bound by the caller with the DRIVER_SOFT_IIC_LINK_* macros; this file does
 * not include any interface header, multi instance (one handle per bus) and
 * host mock testing with fake pins are both possible; no HAL / register /
 * pin symbol appears in this file (src/).
 */

/**
 * @defgroup soft_iic_driver soft iic driver function
 * @brief    soft iic driver modules
 * @{
 */

/**
 * @brief     wait one half bit period
 * @param[in] *handle pointer to a soft_iic_handle_t structure
 * @return    none
 * @note      none
 */
static void a_soft_iic_half_delay(soft_iic_handle_t *handle)
{
    handle->delay_us(handle->half_delay_us);
}

/**
 * @brief     wait until the scl line is released by the slave
 * @param[in] *handle pointer to a soft_iic_handle_t structure
 * @return    status code
 *            - 0 success
 *            - 4 clock stretching timeout
 * @note      the slave may hold scl low while it is busy
 */
static uint8_t a_soft_iic_wait_scl_high(soft_iic_handle_t *handle)
{
    uint16_t waited = 0U;

    while (handle->scl_read() == 0U)
    {
        if (waited >= handle->stretch_timeout_us)
        {
            return 4; /* clock stretching timeout */
        }
        handle->delay_us(1U);
        waited++;
    }

    return 0;
}

/**
 * @brief     release both lines and check the bus is idle
 * @param[in] *handle pointer to a soft_iic_handle_t structure
 * @return    status code
 *            - 0 success
 *            - 1 a line is held low
 * @note      none
 */
static uint8_t a_soft_iic_release_bus(soft_iic_handle_t *handle)
{
    handle->sda_write(1);
    handle->scl_write(1);
    a_soft_iic_half_delay(handle);

    if ((handle->scl_read() == 0U) || (handle->sda_read() == 0U))
    {
        return 1; /* bus is not idle */
    }

    return 0;
}

/**
 * @brief     clock a stuck bus free and send a stop condition
 * @param[in] *handle pointer to a soft_iic_handle_t structure
 * @return    status code
 *            - 0 success
 *            - 1 recovery failed
 * @note      at most 9 clocks, then a stop condition, then an idle check
 */
static uint8_t a_soft_iic_bus_recover(soft_iic_handle_t *handle)
{
    uint8_t i;

    handle->sda_write(1); /* release sda, the slave decides */

    for (i = 0U; i < 9U; i++)
    {
        handle->scl_write(0);
        a_soft_iic_half_delay(handle);
        handle->scl_write(1);
        a_soft_iic_half_delay(handle);
    }

    /* finish with a stop condition so the slave state machine is reset */
    handle->scl_write(0);
    a_soft_iic_half_delay(handle);
    handle->sda_write(0);
    a_soft_iic_half_delay(handle);
    handle->scl_write(1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        return 1; /* return error */
    }
    a_soft_iic_half_delay(handle);
    handle->sda_write(1); /* stop condition */
    a_soft_iic_half_delay(handle);

    if ((handle->scl_read() == 0U) || (handle->sda_read() == 0U))
    {
        return 1; /* still stuck */
    }

    return 0;
}

/**
 * @brief      drive one bit out and clock it
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  bit bit value (0 or 1)
 * @return     status code
 *             - 0 success
 *             - 4 scl is held low
 * @note       ends with scl high and sda released by the caller
 */
static uint8_t a_soft_iic_write_bit(soft_iic_handle_t *handle, uint8_t bit)
{
    handle->scl_write(0);
    a_soft_iic_half_delay(handle);
    handle->sda_write((bit != 0U) ? 1U : 0U);
    a_soft_iic_half_delay(handle); /* tSU;DAT */
    handle->scl_write(1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        return 4; /* return error */
    }
    a_soft_iic_half_delay(handle); /* tHIGH */

    return 0;
}

/**
 * @brief      release sda and clock one bit in
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[out] *bit pointer to the sampled bit
 * @return     status code
 *             - 0 success
 *             - 4 scl is held low
 * @note       the bit is sampled while scl is high; sda is left released
 */
static uint8_t a_soft_iic_read_bit(soft_iic_handle_t *handle, uint8_t *bit)
{
    handle->scl_write(0);
    a_soft_iic_half_delay(handle);
    handle->sda_write(1); /* release sda to the slave */
    a_soft_iic_half_delay(handle); /* tSU;DAT */
    handle->scl_write(1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        return 4; /* return error */
    }
    *bit = handle->sda_read();
    a_soft_iic_half_delay(handle); /* tHIGH */

    return 0;
}

/**
 * @brief      abort the current transaction
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @return     none
 * @note       a stop condition is always sent so the bus is not left busy
 */
static void a_soft_iic_abort(soft_iic_handle_t *handle)
{
    (void)soft_iic_stop(handle);
}

/**
 * @addtogroup soft_iic_driver
 * @{
 */

uint8_t soft_iic_init(soft_iic_handle_t *handle)
{
    if (handle == NULL)
    {
        return 2; /* invalid handle */
    }
    if (handle->debug_print == NULL)
    {
        return 3; /* the log slot must be valid before a slot error is reported */
    }
    if ((handle->pin_init == NULL) || (handle->pin_deinit == NULL) ||
        (handle->scl_write == NULL) || (handle->sda_write == NULL) ||
        (handle->scl_read == NULL) || (handle->sda_read == NULL) ||
        (handle->delay_us == NULL))
    {
        handle->debug_print("soft iic: pin slot is null.\n"); /* pin slot is null */
        return 3; /* slot is null */
    }
    if (handle->half_delay_us == 0U)
    {
        handle->half_delay_us = SOFT_IIC_DEFAULT_HALF_DELAY_US;
    }
    if (handle->stretch_timeout_us == 0U)
    {
        handle->stretch_timeout_us = SOFT_IIC_DEFAULT_STRETCH_TIMEOUT_US;
    }

    /* the pin layer must come up before any bus operation */
    if (handle->pin_init() != 0)
    {
        handle->debug_print("soft iic: pin init failed.\n"); /* pin init failed */
        return 1; /* return error */
    }

    if (a_soft_iic_release_bus(handle) != 0U)
    {
        if (a_soft_iic_bus_recover(handle) != 0U)
        {
            handle->debug_print("soft iic: bus is stuck.\n"); /* bus is stuck */
            (void)handle->pin_deinit(); /* rollback pin layer */

            return 1; /* return error */
        }
    }

    handle->inited = 1U; /* flag finish */

    return 0;
}

uint8_t soft_iic_deinit(soft_iic_handle_t *handle)
{
    if (handle == NULL)
    {
        return 2; /* invalid handle */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    handle->sda_write(1); /* release sda */
    handle->scl_write(1); /* release scl */

    (void)handle->pin_deinit(); /* deinit the pin layer */

    handle->inited = 0U; /* flag close */

    return 0;
}

uint8_t soft_iic_set_half_delay(soft_iic_handle_t *handle, uint16_t us)
{
    if ((handle == NULL) || (us == 0U))
    {
        return 2; /* invalid parameter */
    }

    handle->half_delay_us = us; /* set half delay */

    return 0;
}

uint8_t soft_iic_start(soft_iic_handle_t *handle)
{
    if (handle == NULL)
    {
        return 2; /* invalid handle */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    handle->sda_write(1);
    handle->scl_write(1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        handle->debug_print("soft iic: start failed, scl is held low.\n");
        return 4; /* return error */
    }
    if (handle->sda_read() == 0U)
    {
        handle->debug_print("soft iic: start failed, bus is busy.\n");
        return 1; /* sda is held low by a slave, bus is busy */
    }
    a_soft_iic_half_delay(handle); /* tSU;STA */
    handle->sda_write(0);
    a_soft_iic_half_delay(handle); /* tHD;STA */
    handle->scl_write(0);
    a_soft_iic_half_delay(handle);

    return 0;
}

uint8_t soft_iic_restart(soft_iic_handle_t *handle)
{
    if (handle == NULL)
    {
        return 2; /* invalid handle */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    /* scl low first, then release sda: no stop condition can be generated */
    handle->scl_write(0);
    a_soft_iic_half_delay(handle);
    handle->sda_write(1);
    a_soft_iic_half_delay(handle);
    handle->scl_write(1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        handle->debug_print("soft iic: restart failed, scl is held low.\n");
        return 4; /* return error */
    }
    a_soft_iic_half_delay(handle); /* tSU;STA */
    handle->sda_write(0);
    a_soft_iic_half_delay(handle); /* tHD;STA */
    handle->scl_write(0);
    a_soft_iic_half_delay(handle);

    return 0;
}

uint8_t soft_iic_stop(soft_iic_handle_t *handle)
{
    uint8_t res;

    if (handle == NULL)
    {
        return 2; /* invalid handle */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    handle->scl_write(0);
    a_soft_iic_half_delay(handle);
    handle->sda_write(0);
    a_soft_iic_half_delay(handle);
    handle->scl_write(1);
    res = a_soft_iic_wait_scl_high(handle);
    a_soft_iic_half_delay(handle); /* tSU;STO */
    handle->sda_write(1); /* stop condition */
    a_soft_iic_half_delay(handle); /* tBUF */
    if (res != 0U)
    {
        handle->debug_print("soft iic: stop failed, scl is held low.\n");

        return 4; /* return error */
    }

    return 0;
}

uint8_t soft_iic_write_byte(soft_iic_handle_t *handle, uint8_t data)
{
    uint8_t i;
    uint8_t res;
    uint8_t ack;

    if (handle == NULL)
    {
        return 2; /* invalid handle */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    for (i = 0U; i < 8U; i++)
    {
        res = a_soft_iic_write_bit(handle, (uint8_t)((data & 0x80U) != 0U));
        if (res != 0U)
        {
            handle->debug_print("soft iic: write byte failed.\n");

            return 4; /* return error */
        }
        data = (uint8_t)(data << 1);
    }

    /* 9th clock: release sda and sample the acknowledge bit */
    res = a_soft_iic_read_bit(handle, &ack);
    if (res != 0U)
    {
        handle->debug_print("soft iic: write byte failed.\n");

        return 4;
    }
    if (ack != 0U)
    {
        return SOFT_IIC_NACK; /* nack */
    }

    return SOFT_IIC_ACK;
}

uint8_t soft_iic_read_byte(soft_iic_handle_t *handle, uint8_t ack, uint8_t *data)
{
    uint8_t i;
    uint8_t bit;
    uint8_t value = 0U;
    uint8_t res;

    if ((handle == NULL) || (data == NULL))
    {
        return 2; /* invalid parameter */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    for (i = 0U; i < 8U; i++)
    {
        res = a_soft_iic_read_bit(handle, &bit);
        if (res != 0U)
        {
            handle->debug_print("soft iic: read byte failed.\n");

            return 4; /* return error */
        }
        value = (uint8_t)((value << 1) | bit);
    }

    /* 9th clock: the master drives ack (more bytes) or nack (last byte) */
    handle->scl_write(0);
    a_soft_iic_half_delay(handle);
    handle->sda_write((ack != 0U) ? 0U : 1U);
    a_soft_iic_half_delay(handle); /* tSU;DAT */
    handle->scl_write(1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        handle->debug_print("soft iic: read byte failed.\n");

        return 4;
    }
    a_soft_iic_half_delay(handle); /* tHIGH */

    *data = value;

    return 0;
}

uint8_t soft_iic_write(soft_iic_handle_t *handle, uint8_t addr, uint8_t *buf, uint16_t len)
{
    uint8_t res;
    uint16_t i;

    if ((handle == NULL) || (buf == NULL) || (len == 0U) || (addr > SOFT_IIC_MAX_ADDRESS))
    {
        return 2; /* invalid parameter */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    res = soft_iic_start(handle);
    if (res == 0U)
    {
        res = soft_iic_write_byte(handle, (uint8_t)(addr << 1));
    }
    if (res == SOFT_IIC_ACK)
    {
        for (i = 0U; i < len; i++)
        {
            res = soft_iic_write_byte(handle, buf[i]);
            if (res != SOFT_IIC_ACK)
            {
                break;
            }
        }
    }
    if (res != SOFT_IIC_ACK)
    {
        a_soft_iic_abort(handle);
        handle->debug_print("soft iic: write failed.\n");

        return 1; /* return error */
    }
    if (soft_iic_stop(handle) != 0U)
    {
        handle->debug_print("soft iic: write failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_read(soft_iic_handle_t *handle, uint8_t addr, uint8_t *buf, uint16_t len)
{
    uint8_t res;
    uint16_t i;

    if ((handle == NULL) || (buf == NULL) || (len == 0U) || (addr > SOFT_IIC_MAX_ADDRESS))
    {
        return 2; /* invalid parameter */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    res = soft_iic_start(handle);
    if (res == 0U)
    {
        res = soft_iic_write_byte(handle, (uint8_t)((addr << 1) | 0x01U));
    }
    if (res == SOFT_IIC_ACK)
    {
        for (i = 0U; i < len; i++)
        {
            res = soft_iic_read_byte(handle, (uint8_t)((i + 1U) < len), &buf[i]);
            if (res != 0U)
            {
                break;
            }
        }
    }
    else
    {
        res = 1; /* address nack */
    }
    if (res != 0U)
    {
        a_soft_iic_abort(handle);
        handle->debug_print("soft iic: read failed.\n");

        return 1; /* return error */
    }
    if (soft_iic_stop(handle) != 0U)
    {
        handle->debug_print("soft iic: read failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_write_reg(soft_iic_handle_t *handle, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t res;
    uint16_t i;

    if ((handle == NULL) || ((buf == NULL) && (len != 0U)) || (addr > SOFT_IIC_MAX_ADDRESS))
    {
        return 2; /* invalid parameter */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    res = soft_iic_start(handle);
    if (res == 0U)
    {
        res = soft_iic_write_byte(handle, (uint8_t)(addr << 1));
    }
    if (res == SOFT_IIC_ACK)
    {
        res = soft_iic_write_byte(handle, reg);
    }
    if (res == SOFT_IIC_ACK)
    {
        for (i = 0U; i < len; i++)
        {
            res = soft_iic_write_byte(handle, buf[i]);
            if (res != SOFT_IIC_ACK)
            {
                break;
            }
        }
    }
    if (res != SOFT_IIC_ACK)
    {
        a_soft_iic_abort(handle);
        handle->debug_print("soft iic: write register failed.\n");

        return 1; /* return error */
    }
    if (soft_iic_stop(handle) != 0U)
    {
        handle->debug_print("soft iic: write register failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_read_reg(soft_iic_handle_t *handle, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t res;
    uint16_t i;

    if ((handle == NULL) || (buf == NULL) || (len == 0U) || (addr > SOFT_IIC_MAX_ADDRESS))
    {
        return 2; /* invalid parameter */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    res = soft_iic_start(handle);
    if (res == 0U)
    {
        res = soft_iic_write_byte(handle, (uint8_t)(addr << 1));
    }
    if (res == SOFT_IIC_ACK)
    {
        res = soft_iic_write_byte(handle, reg);
    }
    if (res == SOFT_IIC_ACK)
    {
        res = soft_iic_restart(handle); /* keep the auto-increment state */
    }
    if (res == 0U)
    {
        res = soft_iic_write_byte(handle, (uint8_t)((addr << 1) | 0x01U));
    }
    if (res == SOFT_IIC_ACK)
    {
        for (i = 0U; i < len; i++)
        {
            res = soft_iic_read_byte(handle, (uint8_t)((i + 1U) < len), &buf[i]);
            if (res != 0U)
            {
                break;
            }
        }
    }
    else
    {
        res = 1; /* address or sub-address nack */
    }
    if (res != 0U)
    {
        a_soft_iic_abort(handle);
        handle->debug_print("soft iic: read register failed.\n");

        return 1; /* return error */
    }
    if (soft_iic_stop(handle) != 0U)
    {
        handle->debug_print("soft iic: read register failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_is_device_ready(soft_iic_handle_t *handle, uint8_t addr, uint8_t trials)
{
    uint8_t i;
    uint8_t res;

    if ((handle == NULL) || (addr > SOFT_IIC_MAX_ADDRESS))
    {
        return 2; /* invalid parameter */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }
    if (trials == 0U)
    {
        trials = 1U;
    }

    for (i = 0U; i < trials; i++)
    {
        res = soft_iic_start(handle);
        if (res == 0U)
        {
            res = soft_iic_write_byte(handle, (uint8_t)(addr << 1));
            (void)soft_iic_stop(handle);
            if (res == SOFT_IIC_ACK)
            {
                return 0;
            }
        }
    }

    return 1; /* no device found */
}

uint8_t soft_iic_bus_recover(soft_iic_handle_t *handle)
{
    if (handle == NULL)
    {
        return 2; /* invalid handle */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    if (a_soft_iic_bus_recover(handle) != 0U)
    {
        handle->debug_print("soft iic: bus recover failed.\n");

        return 1; /* return error */
    }

    return 0;
}

uint8_t soft_iic_scan(soft_iic_handle_t *handle, uint8_t *addr_list, uint8_t *count)
{
    uint8_t capacity;
    uint8_t found = 0U;
    uint8_t addr;

    if ((handle == NULL) || (addr_list == NULL) || (count == NULL))
    {
        return 2; /* invalid parameter */
    }
    if (handle->inited == 0U)
    {
        return 3; /* not inited */
    }

    capacity = *count;
    *count = 0U;
    for (addr = SOFT_IIC_SCAN_ADDRESS_START; addr <= SOFT_IIC_SCAN_ADDRESS_END; addr++)
    {
        if (soft_iic_is_device_ready(handle, addr, 1U) == 0U)
        {
            if (found < capacity)
            {
                addr_list[found] = addr;
            }
            found++;
        }
    }
    *count = (found > capacity) ? capacity : found;

    return 0;
}

uint8_t soft_iic_info(soft_iic_info_t *info)
{
    if (info == NULL)
    {
        return 2; /* invalid parameter */
    }

    memset(info, 0, sizeof(soft_iic_info_t));
    (void)strncpy(info->chip_name, "soft iic", sizeof(info->chip_name) - 1U); /* set chip name */
    (void)strncpy(info->manufacturer_name, "LQ", sizeof(info->manufacturer_name) - 1U);
    (void)strncpy(info->interface, "GPIO", sizeof(info->interface) - 1U);

    return 0;
}

/**
 * @}
 */