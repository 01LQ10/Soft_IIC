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
 * @file      driver_soft_iic.h
 * @brief     driver software iic header file
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
 * Platform independent software (bit banged) iic master driver:
 *   - 7 bit address master mode only, with start / repeated start / stop,
 *     acknowledge detection, slave clock stretching wait with timeout and a
 *     9 clock bus recovery;
 *   - every pin access goes through the handle function pointer slots
 *     (LibDriver injection style), no HAL / platform header appears in src/;
 *   - this branch (interface) is the slot mode version
 *     (SOFT_IIC_PORT_INLINE=0); the port layer macros at the top of
 *     driver_soft_iic.c map to the handle slots, so multiple instances are
 *     possible (one handle per bus) and host mock testing is easy;
 *     the inline mode (static inline port layer) lives in the inline branch.
 *
 * Return code convention (LibDriver style):
 *   - 0 success (slave acknowledged, for soft_iic_write_byte)
 *   - 1 bus operation failed (nack / bus busy / recovery failed)
 *   - 2 invalid parameter (handle or buffer is null, bad length)
 *   - 3 not inited / pin slot is null
 *   - 4 timeout (clock stretching timeout or the bus is held low)
 *
 * Timing note: iic only constrains the minimum pulse widths, so the bit period
 * can only be stretched, never violated. Every half bit period (including the
 * start / stop setup and hold times) is derived from handle->half_delay_us;
 * the default is 4 us, giving an effective bus rate of about 100 kHz at
 * 48 MHz.
 */

#ifndef DRIVER_SOFT_IIC_H
#define DRIVER_SOFT_IIC_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup soft_iic_driver soft iic driver function
 * @brief    soft iic driver modules
 * @{
 */

#define SOFT_IIC_DEFAULT_HALF_DELAY_US (4U)           /**< default half bit period in us, about 100 kHz in practice */
#define SOFT_IIC_DEFAULT_STRETCH_TIMEOUT_US (10000U)  /**< default clock stretching timeout in us */
#define SOFT_IIC_DEFAULT_TRIALS (3U)                  /**< default device ready probe trials */
#define SOFT_IIC_MAX_ADDRESS (0x7FU)                  /**< 7 bit slave address upper limit */
#define SOFT_IIC_SCAN_ADDRESS_START (0x08U)           /**< scan start address, after the reserved range */
#define SOFT_IIC_SCAN_ADDRESS_END (0x77U)             /**< scan end address, before the reserved range */

/**
 * @brief soft iic byte acknowledge enumeration definition
 */
typedef enum
{
    SOFT_IIC_ACK = 0x00,     /**< slave acknowledged the byte */
    SOFT_IIC_NACK = 0x01,    /**< slave did not acknowledge the byte */
    SOFT_IIC_TIMEOUT = 0x04  /**< scl held low longer than the stretch timeout */
} soft_iic_ack_t;

/**
 * @brief soft iic handle structure
 */
typedef struct soft_iic_handle_s
{
    void (*scl_write)(uint8_t level); /**< point to a scl_write function address */
    void (*sda_write)(uint8_t level); /**< point to a sda_write function address */
    uint8_t (*scl_read)(void); /**< point to a scl_read function address */
    uint8_t (*sda_read)(void); /**< point to a sda_read function address */
    void (*delay_us)(uint32_t us); /**< point to a delay_us function address */
    void (*debug_print)(const char *const fmt, ...); /**< point to a debug_print function address */
    uint16_t half_delay_us; /**< half bit period in us */
    uint16_t stretch_timeout_us; /**< clock stretching timeout in us */
    uint8_t inited; /**< inited flag */
} soft_iic_handle_t;

/**
 * @brief soft iic information structure
 */
typedef struct soft_iic_info_s
{
    char chip_name[32]; /**< chip name */
    char manufacturer_name[32]; /**< manufacturer name */
    char interface[8]; /**< bus interface name */
} soft_iic_info_t;

/**
 * @brief initialize the handle (clear every field)
 */
#define DRIVER_SOFT_IIC_LINK_INIT(handle, structure) memset((handle), 0, sizeof(structure))

/**
 * @brief link the pin and platform slots
 */
#define DRIVER_SOFT_IIC_LINK_SCL_WRITE(handle, f) ((handle)->scl_write = (f))
#define DRIVER_SOFT_IIC_LINK_SDA_WRITE(handle, f) ((handle)->sda_write = (f))
#define DRIVER_SOFT_IIC_LINK_SCL_READ(handle, f) ((handle)->scl_read = (f))
#define DRIVER_SOFT_IIC_LINK_SDA_READ(handle, f) ((handle)->sda_read = (f))
#define DRIVER_SOFT_IIC_LINK_DELAY_US(handle, f) ((handle)->delay_us = (f))
#define DRIVER_SOFT_IIC_LINK_DEBUG_PRINT(handle, f) ((handle)->debug_print = (f))

/**
 * @brief      initialize the software iic bus
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @return     status code
 *             - 0 success
 *             - 1 bus is stuck (recovery failed)
 *             - 2 handle is null
 *             - 3 pin slot is null
 * @note       releases both lines, checks the bus is idle, falls back to the
 *             9-clock recovery sequence when a slave holds SDA low
 */
uint8_t soft_iic_init(soft_iic_handle_t *handle);

/**
 * @brief      close the software iic bus
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is null
 *             - 3 handle is not inited
 * @note       releases both lines (open-drain high) but keeps the pin slots valid
 */
uint8_t soft_iic_deinit(soft_iic_handle_t *handle);

/**
 * @brief      set the half bit period
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  us half bit period in us
 * @return     status code
 *             - 0 success
 *             - 2 handle is null or us is 0
 * @note       the real bit period is us + the pin callback overhead (about 1.2 us)
 */
uint8_t soft_iic_set_half_delay(soft_iic_handle_t *handle, uint16_t us);

/**
 * @brief      send a start condition
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @return     status code
 *             - 0 success
 *             - 1 bus is busy
 *             - 2 handle is null
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       requires an idle bus (scl and sda released high)
 */
uint8_t soft_iic_start(soft_iic_handle_t *handle);

/**
 * @brief      send a repeated start condition
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is null
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       scl is pulled low before sda is released so that no stop
 *             condition can be generated by accident
 */
uint8_t soft_iic_restart(soft_iic_handle_t *handle);

/**
 * @brief      send a stop condition
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is null
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       ends with the bus free (both lines released)
 */
uint8_t soft_iic_stop(soft_iic_handle_t *handle);

/**
 * @brief      write one byte and read the acknowledge bit
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  data byte to write
 * @return     status code
 *             - 0 slave acknowledged (SOFT_IIC_ACK)
 *             - 1 slave did not acknowledge (SOFT_IIC_NACK)
 *             - 2 handle is null
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       msb first
 */
uint8_t soft_iic_write_byte(soft_iic_handle_t *handle, uint8_t data);

/**
 * @brief      read one byte and send the acknowledge bit
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  ack 0 sends ack (more bytes follow), 1 sends nack (last byte)
 * @param[out] *data pointer to the read data
 * @return     status code
 *             - 0 success
 *             - 2 handle or data is null
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       msb first
 */
uint8_t soft_iic_read_byte(soft_iic_handle_t *handle, uint8_t ack, uint8_t *data);

/**
 * @brief      write bytes to a 7-bit slave
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  addr 7-bit device address
 * @param[in]  *buf pointer to the write data
 * @param[in]  len write length (must not be 0)
 * @return     status code
 *             - 0 success
 *             - 1 write failed
 *             - 2 handle / buffer / length is invalid
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       start + address + data + stop
 */
uint8_t soft_iic_write(soft_iic_handle_t *handle, uint8_t addr, uint8_t *buf, uint16_t len);

/**
 * @brief      read bytes from a 7-bit slave
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  addr 7-bit device address
 * @param[out] *buf pointer to the read buffer
 * @param[in]  len read length (must not be 0)
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 handle / buffer / length is invalid
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       start + address + data + stop, the last byte is nacked
 */
uint8_t soft_iic_read(soft_iic_handle_t *handle, uint8_t addr, uint8_t *buf, uint16_t len);

/**
 * @brief      write a register block
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  addr 7-bit device address
 * @param[in]  reg register sub-address
 * @param[in]  *buf pointer to the write data
 * @param[in]  len write length (0 writes the sub-address only)
 * @return     status code
 *             - 0 success
 *             - 1 write failed
 *             - 2 handle / buffer / length is invalid
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       start + address(w) + reg + data + stop
 */
uint8_t soft_iic_write_reg(soft_iic_handle_t *handle, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief      read a register block
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  addr 7-bit device address
 * @param[in]  reg register sub-address
 * @param[out] *buf pointer to the read buffer
 * @param[in]  len read length (must not be 0)
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 handle / buffer / length is invalid
 *             - 3 handle is not inited
 *             - 4 scl is held low
 * @note       start + address(w) + reg + repeated start + address(r) + data + stop;
 *             the repeated start keeps the slave auto-increment state, a
 *             stop/start pair would clear it
 */
uint8_t soft_iic_read_reg(soft_iic_handle_t *handle, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief      probe a 7-bit address
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[in]  addr 7-bit device address
 * @param[in]  trials probe times (0 is treated as 1)
 * @return     status code
 *             - 0 device acknowledged
 *             - 1 device did not acknowledge
 *             - 2 handle is null
 *             - 3 handle is not inited
 * @note       each trial is a full start/address/stop transaction
 */
uint8_t soft_iic_is_device_ready(soft_iic_handle_t *handle, uint8_t addr, uint8_t trials);

/**
 * @brief      recover a bus that a slave holds low
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @return     status code
 *             - 0 success (both lines released)
 *             - 1 recovery failed
 *             - 2 handle is null
 *             - 3 handle is not inited
 * @note       clocks scl at most 9 times and then sends a stop condition
 */
uint8_t soft_iic_bus_recover(soft_iic_handle_t *handle);

/**
 * @brief      scan the 7-bit address space
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @param[out] *addr_list pointer to the address list buffer
 * @param[in]  *count in: list capacity, out: found device count
 * @return     status code
 *             - 0 success
 *             - 2 handle / list / count is null
 *             - 3 handle is not inited
 * @note       scans 0x08 to 0x77
 */
uint8_t soft_iic_scan(soft_iic_handle_t *handle, uint8_t *addr_list, uint8_t *count);

/**
 * @brief     get the driver information
 * @param[out] *info pointer to a soft_iic_info_t structure
 * @return    status code
 *             - 0 success
 *             - 2 info is null
 * @note      none
 */
uint8_t soft_iic_info(soft_iic_info_t *info);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
