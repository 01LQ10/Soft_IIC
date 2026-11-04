# SOFT_IIC — 平台无关的 GPIO 模拟 IIC 主机驱动

纯 C 实现的软件 IIC（GPIO 位拨）主机驱动：两根开漏引脚 + 微秒延时，不依赖任何硬件 I2C 外设。`src/` 零平台符号，引脚操作经 handle 的函数指针槽位注入，支持多实例与 host mock 测试。

**功能边界**：只做主机、只做 7 位地址；不做 10 位地址、多主、从机模式、SMBus PEC。

---

## 1. 目录结构

```
SOFT_IIC/
├── src/
│   ├── driver_soft_iic.c              # 协议状态机（零 HAL 符号，不 include 接口头）
│   └── driver_soft_iic.h              # handle / 返回码 / API
├── interface/
│   ├── driver_soft_iic_interface.h          # 引脚层声明（仅 src 与平台实现引用）
│   └── driver_soft_iic_interface_template.c # 移植模板（不参与编译）
├── example/
│   └── driver_soft_iic_basic.c/.h           # init / scan / probe / dump
├── test/
│   └── driver_soft_iic_scan_test.c          # 真机扫描测试，默认 OFF
└── CMakeLists.txt                           # SOFT_IIC::SOFT_IIC
```

平台实现文件由使用方工程提供，换平台时按 `interface/driver_soft_iic_interface_template.c` 抄一份实现即可（见 §7）。

---

## 2. 分层与 LibDriver 规范

| 层 | 文件 | 允许依赖 |
|---|---|---|
| 驱动 | `src/` | 只有 `stdint.h` / `string.h` / 自身头 |
| 接口 | `interface/*.h` | 纯声明 |
| example | `example/` | 只调 `src/` 公共 API（经 `soft_iic_link()` 绑定） |
| test | `test/` | `src/` + `interface/`（自带 gs_handle 与 LINK 绑定） |
| 平台实现 | 工程侧 `driver_soft_iic_interface.c` | HAL / 平台头 |

**约束**：

- `src/` 里 `grep -iE "HAL_|stm32|GPIO"` 为空，`grep -i driver_soft_iic_interface` 也为空；引脚访问一律走 handle 的函数指针槽位（LibDriver 注入模式）。
- 绑定入口是 `src/` 声明的 `soft_iic_link(handle)`，由平台实现文件提供，内部用 `DRIVER_SOFT_IIC_LINK_*` 把每个槽位填满。
- 支持**多实例**（每条总线一个 handle）与**可 mock**（host 单元测试直接给假引脚函数）。

---

## 3. 端口契约（handle 的 8 个平台槽位）

| handle 槽位 | LINK 宏 | 平台函数 |
|---|---|---|
| `pin_init` | `DRIVER_SOFT_IIC_LINK_PIN_INIT` | `soft_iic_interface_pin_init` |
| `pin_deinit` | `DRIVER_SOFT_IIC_LINK_PIN_DEINIT` | `soft_iic_interface_pin_deinit` |
| `scl_write` | `DRIVER_SOFT_IIC_LINK_SCL_WRITE` | `soft_iic_interface_scl_write` |
| `sda_write` | `DRIVER_SOFT_IIC_LINK_SDA_WRITE` | `soft_iic_interface_sda_write` |
| `scl_read` | `DRIVER_SOFT_IIC_LINK_SCL_READ` | `soft_iic_interface_scl_read` |
| `sda_read` | `DRIVER_SOFT_IIC_LINK_SDA_READ` | `soft_iic_interface_sda_read` |
| `delay_us` | `DRIVER_SOFT_IIC_LINK_DELAY_US` | `soft_iic_interface_delay_us` |
| `debug_print` | `DRIVER_SOFT_IIC_LINK_DEBUG_PRINT` | `soft_iic_interface_debug_print` |

**绑定规则**：

- `src/` 里只有 `handle->xxx(...)` 直接调用，没有中间层宏。
- 平台侧实现 `soft_iic_link()` 完成全部绑定（先 `DRIVER_SOFT_IIC_LINK_INIT` 清零）。
- `pin_init` / `pin_deinit` 由 `soft_iic_init()` / `soft_iic_deinit()` 自动调用，所以 `example/`、`test/`、业务接口层都不需要碰引脚层。
- `debug_print` 为 NULL 时 `soft_iic_init()` 直接返回 3（先于任何打印）；其余槽位为 NULL 时打印 `soft iic: pin slot is null.` 并返回 3。

> 换平台时抄 `interface/driver_soft_iic_interface_template.c`，只需实现这 8 个函数加 `soft_iic_link()`，`src/` 完全不动。

---

## 4. 时序

IIC **只约束最小脉宽，没有最大周期约束**，所以任何额外开销都只会拉长位周期、不会造成时序违规。所有半周期（含 START/STOP 的建立与保持时间）统一由 `handle->half_delay_us` 派生：

| 参数 | 默认值 | 说明 |
|---|---|---|
| `half_delay_us` | 4 us | 每半个 bit 的延时；单次调用实测约 6.2 us |
| `stretch_timeout_us` | 10000 us | 从机拉低 SCL（时钟延展）的最长等待 |

### 4.1 位周期构成

每个 bit 的耗时不是 `2 × half_delay_us`，而是 **`3 × half_delay_us`**：`a_soft_iic_write_bit()` / `a_soft_iic_read_bit()` 会在 SCL 拉低前、SDA 变更后、SCL 拉高后各等一次（即 tLOW = 2 份、tHIGH = 1 份），这是 SCL 低电平占总周期 2/3 的来源。

### 4.2 delay_us 的固定开销与除法教训

单次 `delay_us(us)` 实测 = `us + 2.2 us` 左右。这 2.2 us 是 SysTick 轮询循环本身的粒度（48 MHz、1 个 flash 等待周期下，读 `SysTick->VAL` 比较一次到退出至少要一个循环体），不是可以靠调参消掉的常量。**因此别在热路径里再引入除法**：

> **教训（2026-11-04）**：`delay_us` 里每次调用都算 `SystemCoreClock / 1000000U`，M0+ 没有硬件除法，`__aeabi_uidiv` 每次多花约 2.8 us，SCL 从预期的 ~80 kHz 掉到实测 **32 kHz**（500 次 6 字节读要 1285 ms）。改成 init 时校准一次、热路径只做乘法之后回到 920 ms / ~45 kHz。

### 4.3 上板实测（48 MHz，`half_delay_us=4`）

500 次 6 字节寄存器读 920 ms → **约 45 kHz SCL**，折算每个 bit ≈ 22 us（比 `3 × 6.2 = 18.6 us` 多出的部分是每次引脚回调的开销与 ACK 位的收尾）。

**端口模式 A/B（2026-11-04 上板实测，同一测试）**：

| 模式 | 耗时 | SCL |
|---|---|---|
| 槽位 | 920 ms | ≈ 45.1 kHz |
| 内联 | 986 ms | ≈ 42.1 kHz |

**内联不提速**。瓶颈是 `delay_us` 每次约 2.2 us 的固定开销（每 bit 调 3 次），被内联消除的 `blx r` 引脚间接调用只有约 0.4 us/bit（占比 <6%，差值落在单次测量抖动内）。内联的收益在体积（−268 B）、零间接调用、换平台只写一个文件，**不在速率**；想提速调 `half_delay_us`，不要换端口模式。

### 4.4 提速估算

想更快时下调 `half_delay_us`（`soft_iic_set_half_delay()` 或 `soft_iic_basic_get_handle()` 后改字段）。下表是按同一套实测开销外推的估算值：

| `half_delay_us` | bit 周期（估算） | 约 SCL | tHIGH（估算） | 备注 |
|---|---|---|---|---|
| 4（默认） | ≈ 22 us（实测） | ~45 kHz（实测） | ≈ 6.2 us | 保守值，建立时间最充裕 |
| 2 | ≈ 14 us | ~70 kHz | ≈ 4.2 us | tHIGH 刚好仍 ≥ 标准模式 4.0 us |
| 1 | — | — | ≈ 3.2 us | 低于标准模式 100 kHz 的 4.0 us 下限，仅适合 400 kHz 快速模式从机（tHIGH ≥ 0.6 us） |

---

## 5. 返回码

| 码 | 含义 |
|---|---|
| 0 | 成功（`soft_iic_write_byte` 表示 ACK） |
| 1 | 总线操作失败（NACK / 总线忙 / 恢复失败） |
| 2 | 参数无效 |
| 3 | 未初始化 / 引脚槽位为 NULL |
| 4 | 超时（时钟延展超时、SCL 被拉死） |

---

## 6. 用法

```c
#include "driver_soft_iic_basic.h"

uint8_t list[SOFT_IIC_BASIC_MAX_DEVICES];
uint8_t count = sizeof(list);
uint8_t value = 0U;

soft_iic_basic_init();                          /* 引脚层 + 总线初始化 */
soft_iic_basic_scan(list, &count);              /* 扫描 0x08..0x77 */
soft_iic_basic_write_reg(0x50, 0x10, 0xAA, 1);  /* 向 0x50 的 0x10 寄存器写 1 字节 */
soft_iic_basic_read_reg(0x50, 0x10, &value, 1); /* 读回校验 */
soft_iic_basic_deinit();
```

**寄存器读时序要求**：必须是 **START + addr|W + reg + 重复 START + addr|R + 数据 + STOP**，不能用 STOP+START（会清掉从机的子地址自增状态），`soft_iic_read_reg` 已按此实现。

---

## 7. 平台实现要点

1. **先释放占用引脚的硬件外设**：如果目标引脚此前被硬件 I2C 外设复用，引脚层 `soft_iic_interface_init()` 里先对外设 DeInit 并关其时钟，再把引脚配成开漏输出模式。否则外设仍占用引脚。
2. **开漏写 1 = 释放**：高电平由外部 4.7K 上拉提供；写 0 才真正拉低。
3. **IDR 回读**：开漏输出模式下 `IDR` 必须反映引脚真实电平，ACK 判定与时钟延展检测都依赖这一点。移植后可用扫描测试（`test/`）验证：任意从机返回 ACK 即成立。
4. **微秒延时**：轮询 `SysTick->VAL`（1 ms 重装 @48 MHz，20.83 ns 分辨率），与优化等级无关；不要用 `for()` 空循环凑时间。**每微秒的计数值只算一次**（见 §4.2 的除法教训），热路径里任何 `SystemCoreClock / 1e6` 都是 bug。参考实现见 §7.1。
5. **不关中断**：主模式 I2C 只会被 ISR 拉长电平，合法。仅当总线上有 SMBus 超时从机时才需要临界段。

### 7.1 delay_us 参考实现

移植模板（`interface/driver_soft_iic_interface_template.c`）里的 `soft_iic_interface_delay_us()` 是空 stub，但位拨 IIC 的**全部时序都靠这一个函数撑起来**，它是整个平台实现里最重要的部分，不能用空循环凑时间。下面是一份基于 Cortex-M0+ SysTick + HAL 的参考实现（SysTick 配成 1 ms 重装，48 MHz 下 LOAD=47999，分辨率 20.83 ns）：

```c
/* SysTick 每微秒的计数值，初始化时算一次。
 * M0+ 没有硬件除法器，SystemCoreClock / 1000000U 是软件除法（几百个周期），
 * 绝不能放进热路径（§4.2 的教训：每次多花 ~2.8 us，SCL 80 kHz -> 32 kHz）。 */
static uint32_t gs_ticks_per_us = 0U;

static void SOFT_IIC_CalibrateTiming(void)
{
    uint32_t ticks = SystemCoreClock / 1000000U;

    gs_ticks_per_us = (ticks == 0U) ? 1U : ticks;
}

void soft_iic_interface_delay_us(uint32_t us)
{
    uint32_t reload;
    uint32_t ticks;
    uint32_t start;
    uint32_t elapsed;
    uint32_t now;

    if (us == 0U)
    {
        return;
    }

    /* 热路径不做除法：每微秒的计数值在初始化时算好（懒校准兜底） */
    if (gs_ticks_per_us == 0U)
    {
        SOFT_IIC_CalibrateTiming();
    }
    reload = SysTick->LOAD + 1U;

    /* 超过一个 SysTick 周期时先走毫秒级延时，剩下的不足 1 ms 再轮询 */
    if ((us > 1000U) || (reload == 0U))
    {
        uint32_t ms = us / 1000U;

        if (ms != 0U)
        {
            HAL_Delay(ms);
        }
        us = us % 1000U;
        if ((us == 0U) || (reload == 0U))
        {
            return;
        }
    }

    ticks = us * gs_ticks_per_us;
    start = SysTick->VAL;
    do
    {
        now = SysTick->VAL;
        /* VAL 递减，回绕（now > start）时补上一个重装值 */
        elapsed = (start >= now) ? (start - now) : (reload + start - now);
    } while (elapsed < ticks);
}
```

**要点**：

- **计时源用自由运行计数器（SysTick）轮询**，不要用 `for()` 空循环——空循环的耗时随优化等级（`-O0/-O2/-O3`）变化，时序会漂移。
- **每微秒的计数值只算一次**（init 时校准并缓存 `gs_ticks_per_us`），热路径只做乘法；任何 `SystemCoreClock / 1e6` 出现在热路径都是 bug。
- **回绕处理**：SysTick 是递减计数器，轮询期间会从 0 回绕到 LOAD 重装，`now > start` 时补加 `reload = SysTick->LOAD + 1U`，否则跨重装点的延时会瞬间返回。
- **精度下限**：单次调用实测约 `us + 2.2 us`（轮询循环本身的粒度），这是 SCL 速率的瓶颈（§4）；想再快只能下调 `half_delay_us`（下限受 tHIGH 约束）。
- **换平台**：计时源可以换成别的（硬件定时器、DWT 等），但必须满足：确定性固定分辨率、与优化等级无关、热路径无除法。