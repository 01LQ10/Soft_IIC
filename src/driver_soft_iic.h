/**
 * @file      driver_soft_iic.h
 * @brief     driver software iic header file
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
 * 平台无关的软件（位拨）IIC 主机驱动：
 *   - 只做 7 位地址主机模式，支持 START / 重复 START / STOP、ACK 判定、
 *     从机时钟延展等待与超时、9 时钟总线恢复；
 *   - 引脚访问全部经 handle 的函数指针槽位（LibDriver 注入模式），
 *     本文件（src/）不出现任何 HAL / 平台头；
 *   - 端口层（8 个宏）在 driver_soft_iic.c 顶部，编译期二选一：
 *     槽位模式（SOFT_IIC_PORT_INLINE=0，默认，多实例、可 mock）或
 *     内联模式（=1，由 -DSOFT_IIC_PORT_HEADER=<平台头> 提供 static inline 实现，
 *     零间接调用、单实例）。
 *
 * 返回码约定（LibDriver 风格）：
 *   - 0 成功（对 soft_iic_write_byte 表示从机应答 ACK）
 *   - 1 总线操作失败（NACK / 总线忙 / 总线恢复失败）
 *   - 2 参数无效（handle 或缓冲区为 NULL、长度非法）
 *   - 3 未初始化 / 引脚槽位为 NULL
 *   - 4 超时（时钟延展超时或总线被从机拉死）
 *
 * 时序说明：IIC 只约束最小脉宽、没有最大周期约束，所以位周期只会被拉长、
 * 不会违规。所有半位周期统一由 handle->half_delay_us 派生（含 START/STOP
 * 的建立/保持时间），默认 4 us，48 MHz 下实际有效总线速率约 100 kHz。
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

#define SOFT_IIC_DEFAULT_HALF_DELAY_US (4U)      /**< 半位周期默认值（us），实际约 100 kHz */
#define SOFT_IIC_DEFAULT_STRETCH_TIMEOUT_US (10000U) /**< 时钟延展等待超时默认值（us） */
#define SOFT_IIC_DEFAULT_TRIALS (3U)             /**< 设备就绪探测默认次数 */
#define SOFT_IIC_MAX_ADDRESS (0x7FU)             /**< 7 位从机地址上限 */
#define SOFT_IIC_SCAN_ADDRESS_START (0x08U)      /**< 扫描起始地址（保留地址段之后） */
#define SOFT_IIC_SCAN_ADDRESS_END (0x77U)        /**< 扫描结束地址（保留地址段之前） */

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
