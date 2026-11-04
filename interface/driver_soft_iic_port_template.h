/**
 * @file      driver_soft_iic_port_template.h
 * @brief     driver software iic inline port layer template (not built)
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
 * 内联模式的**端口契约模板**：本文件不参与编译，只用来回答一个问题——
 * 换平台/换引脚时要提供什么。照抄成 <你的平台>_port.h 即可，
 * 驱动本体（src/）和 example/ 一行都不用改。
 *
 * 启用方式（编译期二选一，默认走槽位模式）：
 *   -DSOFT_IIC_PORT_INLINE=1 -DSOFT_IIC_PORT_HEADER="your_port.h"
 *
 * 契约一共 8 项，且必须**完全自包含**：只用平台头自己 include 的东西，
 * 不得引用任何 .c 里的 extern 函数（host 侧要靠替换本头来跑协议测试）。
 *
 * 硬性要求：
 *   1. 全部实现写成 static inline（否则“零间接调用”就失去意义）；
 *   2. PIN_INIT 必须把引脚从任何占用它的硬件外设手里拿回来（本仓库是
 *      I2C1），并配成开漏/释放状态；
 *   3. 读回函数必须反映引脚真实电平（开漏模式下靠这个判 ACK 和时钟延展）；
 *   4. DELAY_US 必须与优化等级无关（不要用空循环凑时间）；
 *   5. 微秒延时的每微秒 tick 数要缓存，不能每次调用都算除法（M0+ 无硬件除法）。
 */

#ifndef DRIVER_SOFT_IIC_PORT_TEMPLATE_H
#define DRIVER_SOFT_IIC_PORT_TEMPLATE_H

#include <stdint.h>

/* ---- 1. 引脚初始化 / 释放 ------------------------------------------------
 * 返回 0 成功；非 0 会让 soft_iic_init() 直接返回 1。
 */
static inline uint8_t soft_iic_port_pin_init(void)
{
    /* release the pins from the hardware peripheral, configure open-drain,
     * then drive both lines high (released) */

    return 0;
}

static inline uint8_t soft_iic_port_pin_deinit(void)
{
    /* release both lines and hand the pins back to their reset state */

    return 0;
}

/* ---- 2. 四条引脚操作 -----------------------------------------------------
 * level: 0 = 拉低, 1 = 释放（开漏 + 上拉由外部提供高电平）
 */
static inline void soft_iic_port_scl_write(uint8_t level)
{
    (void)level;
}

static inline void soft_iic_port_sda_write(uint8_t level)
{
    (void)level;
}

/* 必须返回引脚真实电平：ACK 判定 */
static inline uint8_t soft_iic_port_sda_read(void)
{
    return 1;
}

/* 必须返回引脚真实电平：时钟延展检测 */
static inline uint8_t soft_iic_port_scl_read(void)
{
    return 1;
}

/* ---- 3. 微秒延时 ---------------------------------------------------------
 * 与优化等级无关；每微秒 tick 数只算一次。
 */
static inline void soft_iic_port_delay_us(uint32_t us)
{
    (void)us;
}

/* ---- 4. 日志出口 ---------------------------------------------------------
 * 变参，直接转发到平台 printf 即可。
 */
static inline void soft_iic_port_log(const char *const fmt, ...)
{
    (void)fmt;
}

/* ===================== 端口契约（8 个宏） =====================
 * handle 在内联模式下没有意义，用 (void)(handle) 保持与槽位模式相同的
 * 调用签名，这样驱动正文两种模式共用一份源码。
 */
#define SOFT_IIC_PORT_PIN_INIT() (soft_iic_port_pin_init())
#define SOFT_IIC_PORT_PIN_DEINIT() (soft_iic_port_pin_deinit())
#define SOFT_IIC_SCL_WRITE(handle, level) ((void)(handle), soft_iic_port_scl_write((uint8_t)(level)))
#define SOFT_IIC_SDA_WRITE(handle, level) ((void)(handle), soft_iic_port_sda_write((uint8_t)(level)))
#define SOFT_IIC_SCL_READ(handle) ((void)(handle), soft_iic_port_scl_read())
#define SOFT_IIC_SDA_READ(handle) ((void)(handle), soft_iic_port_sda_read())
#define SOFT_IIC_DELAY_US(handle, us) ((void)(handle), soft_iic_port_delay_us((uint32_t)(us)))
#define SOFT_IIC_PORT_LOG(...) soft_iic_port_log(__VA_ARGS__)

#endif /* DRIVER_SOFT_IIC_PORT_TEMPLATE_H */
