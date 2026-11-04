/**
 * @file      driver_soft_iic_interface.h
 * @brief     driver software iic interface header file
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
 *
 * 本文件只有声明，不含任何平台头：平台实现放在工程侧
 * （本仓库为 User/Src/driver_soft_iic_interface.c，PY32F030 + HAL）。
 * 换平台时只需替换这一个实现文件，驱动本体（src/）不需要改动。
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
uint8_t soft_iic_interface_init(void);

/**
 * @brief     close the pin layer
 * @return    status code
 *            - 0 success
 *            - 1 deinitialization failed
 * @note      releases both lines so a hardware iic peripheral can take over
 */
uint8_t soft_iic_interface_deinit(void);

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

/**
 * @brief      link every pin slot of the handle to this platform
 * @param[in]  *handle pointer to a soft_iic_handle_t structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is null
 * @note       clears the handle first, then fills the pin and platform slots
 */
uint8_t soft_iic_interface_link(soft_iic_handle_t *handle);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
