/**
 * @file      driver_soft_iic.c
 * @brief     driver software iic source file
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

#include "driver_soft_iic.h"

/* ============================ 端口层 ============================
 * 驱动正文只经下面 8 个宏访问硬件与日志，编译期二选一
 * （开关 SOFT_IIC_PORT_INLINE，由命令行 -D 传入）：
 *
 *   SOFT_IIC_PORT_INLINE=0（默认，槽位模式）
 *     宏映射到 handle 的函数指针槽位；引脚初始化/反初始化走 BSP interface
 *     层的 soft_iic_interface_init()/deinit()，平台实现文件为工程侧的
 *     User/Src/driver_soft_iic_interface.c。支持多实例（每条总线一个 handle），
 *     可在 host 上用假引脚做单元测试。
 *
 *   SOFT_IIC_PORT_INLINE=1（内联模式）
 *     整个端口层由 -DSOFT_IIC_PORT_HEADER=<平台头> 提供，平台头用
 *     static inline 直写寄存器（零间接调用），单实例（引脚编译期固定）。
 *     契约见 interface/driver_soft_iic_port_template.h，本仓库实现为
 *     User/Inc/driver_soft_iic_port_py32.h。
 *
 * 两种模式互不依赖，换模式或换平台都只需要提供/替换一个文件；
 * 本文件（src/）里不出现任何 HAL / 寄存器 / 引脚符号。
 */
#if SOFT_IIC_PORT_INLINE

#ifndef SOFT_IIC_PORT_HEADER
#error "SOFT_IIC_PORT_INLINE=1 时必须提供 -DSOFT_IIC_PORT_HEADER=<平台端口头>"
#endif

#include SOFT_IIC_PORT_HEADER

/* 内联模式没有 handle 日志槽位，日志出口由端口头提供 */
#define SOFT_IIC_LOG(handle, text) SOFT_IIC_PORT_LOG(text)

#else /* SOFT_IIC_PORT_INLINE == 0 */

#include "driver_soft_iic_interface.h"

#define SOFT_IIC_SCL_WRITE(handle, level) ((handle)->scl_write((uint8_t)(level)))
#define SOFT_IIC_SDA_WRITE(handle, level) ((handle)->sda_write((uint8_t)(level)))
#define SOFT_IIC_SCL_READ(handle) ((handle)->scl_read())
#define SOFT_IIC_SDA_READ(handle) ((handle)->sda_read())
#define SOFT_IIC_DELAY_US(handle, us) ((handle)->delay_us((uint32_t)(us)))
#define SOFT_IIC_PORT_PIN_INIT() (soft_iic_interface_init())
#define SOFT_IIC_PORT_PIN_DEINIT() (soft_iic_interface_deinit())

/* 错误日志统一走 handle 的 debug_print 槽位（不直接 printf），
 * 只使用字面量格式串，避免在 src/ 里引入 stdio 变参转发。 */
#define SOFT_IIC_LOG(handle, text)                      \
    do                                                  \
    {                                                   \
        if (((handle) != NULL) && ((handle)->debug_print != NULL)) \
        {                                               \
            (handle)->debug_print(text);                \
        }                                               \
    } while (0)

#endif /* SOFT_IIC_PORT_INLINE */

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
    SOFT_IIC_DELAY_US(handle, handle->half_delay_us);
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

    while (SOFT_IIC_SCL_READ(handle) == 0U)
    {
        if (waited >= handle->stretch_timeout_us)
        {
            return 4; /* clock stretching timeout */
        }
        SOFT_IIC_DELAY_US(handle, 1U);
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
    SOFT_IIC_SDA_WRITE(handle, 1);
    SOFT_IIC_SCL_WRITE(handle, 1);
    a_soft_iic_half_delay(handle);

    if ((SOFT_IIC_SCL_READ(handle) == 0U) || (SOFT_IIC_SDA_READ(handle) == 0U))
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

    SOFT_IIC_SDA_WRITE(handle, 1); /* release sda, the slave decides */

    for (i = 0U; i < 9U; i++)
    {
        SOFT_IIC_SCL_WRITE(handle, 0);
        a_soft_iic_half_delay(handle);
        SOFT_IIC_SCL_WRITE(handle, 1);
        a_soft_iic_half_delay(handle);
    }

    /* finish with a stop condition so the slave state machine is reset */
    SOFT_IIC_SCL_WRITE(handle, 0);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SDA_WRITE(handle, 0);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SCL_WRITE(handle, 1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        return 1;
    }
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SDA_WRITE(handle, 1); /* stop condition */
    a_soft_iic_half_delay(handle);

    if ((SOFT_IIC_SCL_READ(handle) == 0U) || (SOFT_IIC_SDA_READ(handle) == 0U))
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
    SOFT_IIC_SCL_WRITE(handle, 0);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SDA_WRITE(handle, (bit != 0U) ? 1U : 0U);
    a_soft_iic_half_delay(handle); /* tSU;DAT */
    SOFT_IIC_SCL_WRITE(handle, 1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        return 4;
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
    SOFT_IIC_SCL_WRITE(handle, 0);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SDA_WRITE(handle, 1); /* release sda to the slave */
    a_soft_iic_half_delay(handle); /* tSU;DAT */
    SOFT_IIC_SCL_WRITE(handle, 1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        return 4;
    }
    *bit = SOFT_IIC_SDA_READ(handle);
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
#if !SOFT_IIC_PORT_INLINE
    if ((handle->scl_write == NULL) || (handle->sda_write == NULL) ||
        (handle->scl_read == NULL) || (handle->sda_read == NULL) ||
        (handle->delay_us == NULL))
    {
        SOFT_IIC_LOG(handle, "soft iic: pin slot is null.\n");
        return 3; /* slot is null */
    }
#endif
    if (handle->half_delay_us == 0U)
    {
        handle->half_delay_us = SOFT_IIC_DEFAULT_HALF_DELAY_US;
    }
    if (handle->stretch_timeout_us == 0U)
    {
        handle->stretch_timeout_us = SOFT_IIC_DEFAULT_STRETCH_TIMEOUT_US;
    }

    /* 引脚层必须先于任何总线操作：槽位模式下是 interface 层的引脚配置，
     * 内联模式下是端口头的 pin_init（把引脚从硬件外设手里拿回来并放开漏） */
    if (SOFT_IIC_PORT_PIN_INIT() != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: pin init failed.\n");
        return 1;
    }

    handle->inited = 1U;

    if (a_soft_iic_release_bus(handle) != 0U)
    {
        if (a_soft_iic_bus_recover(handle) != 0U)
        {
            SOFT_IIC_LOG(handle, "soft iic: bus is stuck.\n");
            handle->inited = 0U; /* rollback */

            return 1;
        }
    }

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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
        return 3; /* not inited */
    }

    SOFT_IIC_SDA_WRITE(handle, 1);
    SOFT_IIC_SCL_WRITE(handle, 1);
    handle->inited = 0U;

    (void)SOFT_IIC_PORT_PIN_DEINIT();

    return 0;
}

uint8_t soft_iic_set_half_delay(soft_iic_handle_t *handle, uint16_t us)
{
    if ((handle == NULL) || (us == 0U))
    {
        return 2; /* invalid parameter */
    }

    handle->half_delay_us = us;

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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
        return 3; /* not inited */
    }

    SOFT_IIC_SDA_WRITE(handle, 1);
    SOFT_IIC_SCL_WRITE(handle, 1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: start failed, scl is held low.\n");
        return 4;
    }
    if (SOFT_IIC_SDA_READ(handle) == 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: start failed, bus is busy.\n");
        return 1; /* sda is held low by a slave, bus is busy */
    }
    a_soft_iic_half_delay(handle); /* tSU;STA */
    SOFT_IIC_SDA_WRITE(handle, 0);
    a_soft_iic_half_delay(handle); /* tHD;STA */
    SOFT_IIC_SCL_WRITE(handle, 0);
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
        return 3; /* not inited */
    }

    /* scl low first, then release sda: no stop condition can be generated */
    SOFT_IIC_SCL_WRITE(handle, 0);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SDA_WRITE(handle, 1);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SCL_WRITE(handle, 1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: restart failed, scl is held low.\n");
        return 4;
    }
    a_soft_iic_half_delay(handle); /* tSU;STA */
    SOFT_IIC_SDA_WRITE(handle, 0);
    a_soft_iic_half_delay(handle); /* tHD;STA */
    SOFT_IIC_SCL_WRITE(handle, 0);
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
        return 3; /* not inited */
    }

    SOFT_IIC_SCL_WRITE(handle, 0);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SDA_WRITE(handle, 0);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SCL_WRITE(handle, 1);
    res = a_soft_iic_wait_scl_high(handle);
    a_soft_iic_half_delay(handle); /* tSU;STO */
    SOFT_IIC_SDA_WRITE(handle, 1); /* stop condition */
    a_soft_iic_half_delay(handle); /* tBUF */
    if (res != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: stop failed, scl is held low.\n");

        return 4;
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
        return 3; /* not inited */
    }

    for (i = 0U; i < 8U; i++)
    {
        res = a_soft_iic_write_bit(handle, (uint8_t)((data & 0x80U) != 0U));
        if (res != 0U)
        {
            SOFT_IIC_LOG(handle, "soft iic: write byte failed.\n");

            return 4;
        }
        data = (uint8_t)(data << 1);
    }

    /* 9th clock: release sda and sample the acknowledge bit */
    res = a_soft_iic_read_bit(handle, &ack);
    if (res != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: write byte failed.\n");

        return 4;
    }
    if (ack != 0U)
    {
        return SOFT_IIC_NACK;
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
        return 3; /* not inited */
    }

    for (i = 0U; i < 8U; i++)
    {
        res = a_soft_iic_read_bit(handle, &bit);
        if (res != 0U)
        {
            SOFT_IIC_LOG(handle, "soft iic: read byte failed.\n");

            return 4;
        }
        value = (uint8_t)((value << 1) | bit);
    }

    /* 9th clock: the master drives ack (more bytes) or nack (last byte) */
    SOFT_IIC_SCL_WRITE(handle, 0);
    a_soft_iic_half_delay(handle);
    SOFT_IIC_SDA_WRITE(handle, (ack != 0U) ? 0U : 1U);
    a_soft_iic_half_delay(handle); /* tSU;DAT */
    SOFT_IIC_SCL_WRITE(handle, 1);
    if (a_soft_iic_wait_scl_high(handle) != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: read byte failed.\n");

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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
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
        SOFT_IIC_LOG(handle, "soft iic: write failed.\n");

        return 1;
    }
    if (soft_iic_stop(handle) != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: write failed.\n");

        return 1;
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
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
        SOFT_IIC_LOG(handle, "soft iic: read failed.\n");

        return 1;
    }
    if (soft_iic_stop(handle) != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: read failed.\n");

        return 1;
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
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
        SOFT_IIC_LOG(handle, "soft iic: write register failed.\n");

        return 1;
    }
    if (soft_iic_stop(handle) != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: write register failed.\n");

        return 1;
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
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
        SOFT_IIC_LOG(handle, "soft iic: read register failed.\n");

        return 1;
    }
    if (soft_iic_stop(handle) != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: read register failed.\n");

        return 1;
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
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

    return 1;
}

uint8_t soft_iic_bus_recover(soft_iic_handle_t *handle)
{
    if (handle == NULL)
    {
        return 2; /* invalid handle */
    }
    if (handle->inited == 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
        return 3; /* not inited */
    }

    if (a_soft_iic_bus_recover(handle) != 0U)
    {
        SOFT_IIC_LOG(handle, "soft iic: bus recover failed.\n");

        return 1;
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
        SOFT_IIC_LOG(handle, "soft iic: not inited.\n");
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
    (void)strncpy(info->chip_name, "soft iic", sizeof(info->chip_name) - 1U);
    (void)strncpy(info->manufacturer_name, "LQ", sizeof(info->manufacturer_name) - 1U);
    (void)strncpy(info->interface, "GPIO", sizeof(info->interface) - 1U);

    return 0;
}

/**
 * @}
 */
