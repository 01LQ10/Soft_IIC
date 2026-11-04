/**
 * @file      driver_soft_iic_basic.h
 * @brief     driver software iic basic example header file
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

#ifndef DRIVER_SOFT_IIC_BASIC_H
#define DRIVER_SOFT_IIC_BASIC_H

#include "driver_soft_iic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup soft_iic_example_driver soft iic example driver function
 * @brief    soft iic example driver modules
 * @{
 */

#define SOFT_IIC_BASIC_MAX_DEVICES (16U) /**< scan list capacity */

/**
 * @brief     initialize the basic example
 * @return    status code
 *            - 0 success
 *            - 1 initialization failed
 * @note      binds the platform pin layer and starts the bus
 */
uint8_t soft_iic_basic_init(void);

/**
 * @brief     deinitialize the basic example
 * @return    status code
 *            - 0 success
 *            - 1 deinitialization failed
 * @note      releases the bus before closing the pin layer
 */
uint8_t soft_iic_basic_deinit(void);

/**
 * @brief      scan the bus
 * @param[out] *addr_list pointer to the address list buffer
 * @param[in]  *count in: list capacity, out: found device count
 * @return     status code
 *             - 0 success
 *             - 1 scan failed
 *             - 2 parameter is invalid
 * @note       none
 */
uint8_t soft_iic_basic_scan(uint8_t *addr_list, uint8_t *count);

/**
 * @brief     probe one 7-bit address
 * @param[in] addr 7-bit device address
 * @return    status code
 *            - 0 device acknowledged
 *            - 1 device did not acknowledge
 *            - 2 parameter is invalid
 * @note      none
 */
uint8_t soft_iic_basic_probe(uint8_t addr);

/**
 * @brief      read a register block
 * @param[in]  addr 7-bit device address
 * @param[in]  reg register sub-address
 * @param[out] *buf pointer to the read buffer
 * @param[in]  len read length
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 parameter is invalid
 * @note       none
 */
uint8_t soft_iic_basic_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief      write a register block
 * @param[in]  addr 7-bit device address
 * @param[in]  reg register sub-address
 * @param[in]  *buf pointer to the write buffer
 * @param[in]  len write length
 * @return     status code
 *             - 0 success
 *             - 1 write failed
 *             - 2 parameter is invalid
 * @note       none
 */
uint8_t soft_iic_basic_write_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief      dump a register block to the log
 * @param[in]  addr 7-bit device address
 * @param[in]  start_reg first register
 * @param[in]  count register count (at most 16 per line)
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 parameter is invalid
 * @note       for bring-up only
 */
uint8_t soft_iic_basic_dump(uint8_t addr, uint8_t start_reg, uint8_t count);

/**
 * @brief      get the internal handle
 * @param[out] **handle pointer to the handle pointer
 * @return     status code
 *             - 0 success
 *             - 2 parameter is invalid
 * @note       lets the application tune half_delay_us or time raw transactions
 */
uint8_t soft_iic_basic_get_handle(soft_iic_handle_t **handle);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
