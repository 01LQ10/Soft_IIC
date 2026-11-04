# SOFT_IIC — 平台无关的软件（位拨）IIC 主机驱动

低成本方案板子上 SC7A20H（LGA-12，`SDA=PA2` / `SCL=PA3`，外部 4.7K 上拉到 3V0）的
软件 IIC 通路，用来和硬件 I2C1 做 A/B 对拍、验证 IIC 链路是否可用。

只做主机、只做 7 位地址；不做 10 位地址、多主、从机模式、SMBus PEC。

## 1. 目录

```
BSP/SOFT_IIC/
├── src/driver_soft_iic.c              # 协议状态机 + 顶部端口层（内联专用，零 HAL 符号）
├── src/driver_soft_iic.h              # handle / 返回码 / API
├── interface/driver_soft_iic_port_template.h      # 内联模式端口契约模板（不参与编译）
├── example/driver_soft_iic_basic.c/.h             # init / scan / probe / dump
├── test/driver_soft_iic_scan_test.c               # 真机扫描测试，默认 OFF
└── CMakeLists.txt                                 # SOFT_IIC::SOFT_IIC
```

本分支（inline）是内联模式专用版本，平台文件只有一个：
`User/Inc/driver_soft_iic_port_py32.h`（static inline 端口层；槽位模式的平台文件
`User/Src/driver_soft_iic_interface.c` 在 interface 分支）。**换平台只需要替换这一个端口头。**

## 2. 分层与 LibDriver 规范

| 层 | 文件 | 允许依赖 |
|---|---|---|
| 驱动 | `src/` | 只有 `stdint.h` / `string.h` / 自身头 / 下方端口层 |
| 端口层 | `src/driver_soft_iic.c` 顶部 | `#include SOFT_IIC_PORT_HEADER`（内联专用） |
| 端口契约模板 | `interface/driver_soft_iic_port_template.h` | 不参与编译 |
| 平台实现 | 工程侧 `driver_soft_iic_port_py32.h` | 自己 include 的平台头，**自包含** |

- `src/` 里 `grep -iE "HAL_|stm32|GPIO"` 为空；端口层靠 `#include SOFT_IIC_PORT_HEADER`
  一行把平台头引进来。
- 引脚访问是 `static inline` 直写 `BSRR`/`BRR`/`IDR`，**零间接调用**，
  但**单实例**（引脚编译期固定）。
- 槽位模式（handle 函数指针槽位、**多实例**、**可 mock**）在 interface 分支。

### 端口契约（8 项）

```
SOFT_IIC_PORT_PIN_INIT()          SOFT_IIC_PORT_PIN_DEINIT()
SOFT_IIC_SCL_WRITE(handle, lvl)   SOFT_IIC_SDA_WRITE(handle, lvl)
SOFT_IIC_SCL_READ(handle)         SOFT_IIC_SDA_READ(handle)
SOFT_IIC_DELAY_US(handle, us)     SOFT_IIC_PORT_LOG(fmt, ...)   ← 变参
```

`PIN_INIT`/`PIN_DEINIT` 由 `soft_iic_init()`/`soft_iic_deinit()` 自动调用，
所以调用方（`example/`、业务接口层）不需要再碰引脚层。

### 本分支构建与槽位模式

```cmake
cmake --preset py32f030f28u6 -B build_inline -DPIXPILL_SOFT_IIC_PORT_INLINE=ON
cmake --build build_inline
```

CMake 会给 `soft_iic` 目标加 `SOFT_IIC_PORT_INLINE=1` +
`SOFT_IIC_PORT_HEADER="driver_soft_iic_port_py32.h"`（PUBLIC，所以一起编译的
`User/Src/*.c` 也看到同一开关），并补上 `User/Inc` 与 HAL/CMSIS 的 include 路径。
本分支用默认选项（OFF）构建会直接 `#error` 编译失败（fail-fast，防误用槽位模式）：
`inline 分支只支持内联模式（SOFT_IIC_PORT_INLINE=1）；槽位模式在 interface 分支`。

内联模式下 `nm pixpill.elf | grep soft_iic_interface_` 应为空（已用此验证槽位 .c 未链接），
且 `soft_iic_write_byte` 里没有 `blx r` 间接调用（槽位模式有 10 处）。

槽位模式（函数指针槽位、多实例、可 mock）在 interface 分支；
本分支换平台时抄 `interface/driver_soft_iic_port_template.h`。

## 3. 时序

IIC **只约束最小脉宽，没有最大周期约束**，所以任何额外开销都只会拉长位周期、
不会造成时序违规。所有半周期（含 START/STOP 的建立与保持时间）统一由
`handle->half_delay_us` 派生：

| 参数 | 默认值 | 说明 |
|---|---|---|
| `half_delay_us` | 4 us | 每半个 bit 的延时；单次调用实测约 6.2 us（见下） |
| `stretch_timeout_us` | 10000 us | 从机拉低 SCL（时钟延展）的最长等待 |

每个 bit 的耗时不是 `2 × half_delay_us`，而是 **`3 × half_delay_us`**：
`a_soft_iic_write_bit()` / `a_soft_iic_read_bit()` 会在 SCL 拉低前、SDA 变更后、
SCL 拉高后各等一次（即 tLOW = 2 份、tHIGH = 1 份），这是 SCL 低电平占总周期
2/3 的来源。

单次 `delay_us(us)` 实测 = `us + 2.2 us` 左右。这 2.2 us 是 SysTick 轮询循环
本身的粒度（48 MHz、1 个 flash 等待周期下，读 `SysTick->VAL` 比较一次到退出
至少要一个循环体），不是可以靠调参消掉的常量。**因此别在热路径里再引入除法**：

- 教训（2026-11-04）：`delay_us` 里每次调用都算 `SystemCoreClock / 1000000U`，
  M0+ 没有硬件除法，`__aeabi_uidiv` 每次多花约 2.8 us，SCL 从预期的 ~80 kHz
  掉到实测 **32 kHz**（500 次 6 字节读要 1285 ms）。改成 init 时校准一次、
  热路径只做乘法之后回到 920 ms / ~45 kHz。

上板实测（48 MHz，`half_delay_us=4`）：500 次 6 字节寄存器读 920 ms →
**约 45 kHz SCL**，折算每个 bit ≈ 22 us（比 `3 × 6.2 = 18.6 us` 多出的部分是
每次引脚回调的开销与 ACK 位的收尾）。

**端口模式 A/B（2026-11-04 上板实测，同一测试）**：槽位 920 ms ≈ 45.1 kHz /
内联 986 ms ≈ 42.1 kHz —— **内联不提速**。瓶颈是 `delay_us` 每次约 2.2 us 的
固定开销（每 bit 调 3 次），被内联消除的 `blx r` 引脚间接调用只有约 0.4 us/bit
（占比 <6%，差值落在单次测量抖动内）。内联的收益在体积（−268 B）、零间接调用、
换平台只写一个文件，**不在速率**；想提速调 `half_delay_us`，不要换端口模式。

想更快时下调 `half_delay_us`（`soft_iic_set_half_delay()` 或
`soft_iic_basic_get_handle()` 后改字段）。下表是按同一套实测开销外推的估算值：

| `half_delay_us` | bit 周期（估算） | 约 SCL | tHIGH（估算） | 备注 |
|---|---|---|---|---|
| 4（默认） | ≈ 22 us（实测） | ~45 kHz（实测） | ≈ 6.2 us | 保守值，建立时间最充裕 |
| 2 | ≈ 14 us | ~70 kHz | ≈ 4.2 us | tHIGH 刚好仍 ≥ 标准模式 4.0 us |

再往下（1 us）tHIGH 会掉到约 3.2 us，低于标准模式 100 kHz 的 4.0 us 下限，
只有对 400 kHz 快速模式的从机（tHIGH ≥ 0.6 us，SC7A20H 属于此类）才合适。

## 4. 返回码

| 码 | 含义 |
|---|---|
| 0 | 成功（`soft_iic_write_byte` 表示 ACK） |
| 1 | 总线操作失败（NACK / 总线忙 / 恢复失败） |
| 2 | 参数无效 |
| 3 | 未初始化 / 引脚槽位为 NULL |
| 4 | 超时（时钟延展超时、SCL 被拉死） |

## 5. 用法

```c
#include "driver_soft_iic_basic.h"

uint8_t list[SOFT_IIC_BASIC_MAX_DEVICES];
uint8_t count = sizeof(list);
uint8_t who = 0U;

soft_iic_basic_init();                    /* 引脚层 + 总线初始化 */
soft_iic_basic_scan(list, &count);        /* 扫描 0x08..0x77 */
soft_iic_basic_read_reg(0x18, 0x0F, &who, 1);  /* SC7A20H WHO_AM_I，期望 0x11 */
soft_iic_basic_deinit();
```

寄存器读必须是 **START + addr|W + reg + 重复 START + addr|R + 数据 + STOP**，
不能用 STOP+START（会清掉从机的子地址自增状态），`soft_iic_read_reg` 已按此实现。

## 6. 平台实现要点（PY32F030）

1. **先释放硬件 I2C1**：引脚层 `soft_iic_port_pin_init()`（端口头）里
   `HAL_I2C_DeInit(&hi2c1)` 再关 I2C 时钟，然后把 PA2/PA3 配成
   `GPIO_MODE_OUTPUT_OD`。否则外设仍占用引脚。
2. **开漏写 1 = 释放**：高电平由外部 4.7K 上拉提供；写 0 才真正拉低。
3. **IDR 回读**：开漏输出模式下 `IDR` 必须反映引脚真实电平，ACK 判定与时钟
   延展检测都依赖这一点。上板用扫描验证：能收到 0x18 的 ACK 即成立（本板实测唯一命中地址）。
4. **微秒延时**：轮询 `SysTick->VAL`（1 ms 重装 @48 MHz，20.83 ns 分辨率），
   与优化等级无关；不要用 `for()` 空循环凑时间。**每微秒的计数值只算一次**
   （见 §3 的除法教训），热路径里任何 `SystemCoreClock / 1e6` 都是 bug。
   实现是端口头的 `soft_iic_port_delay_us()`（函数内 static 懒校准）；槽位模式在
   interface 分支有同名副本，**跨分支改动请同步**，否则两个模式的时间基准会不一致。
5. **内联模式的延时函数刻意不内联**（`noinline`）：轮询循环约 100 字节，每位要调
   3–4 次，内联进每个调用点会让固件从 26500 B 膨胀到 31528 B；保持一次直调用后
   内联模式反而比槽位模式小 268 B（26232 B，因为引脚层函数与间接调用都没了）。
6. **不关中断**：主模式 I2C 只会被 ISR 拉长电平，合法。仅当总线上有 SMBus
   超时从机时才需要临界段。

## 7. 验证

- 离线（本分支，内联模式）：
  ```
  cc -std=c99 -Wall -Wextra -DSOFT_IIC_PORT_INLINE=1 \
     -DSOFT_IIC_PORT_HEADER='"soft_iic_port_sim_inline.h"' \
     test/soft_iic_host_test.c BSP/SOFT_IIC/src/driver_soft_iic.c \
     -I BSP/SOFT_IIC/src -I test -o test/soft_iic_host_test_inline && ./test/soft_iic_host_test_inline
  ```
  （PowerShell 里 `-D...=\"...\"` 的引号要写成 `'...=\"...\"'`，否则会被 shell 吃掉。）
  用开漏“线与”模型 + 假 SC7A20H 从机做协议级单元测试，exit 0 = pass；
  槽位模式的测试版本在 interface 分支。
- 上板（2026-11-04 实测通过）：`main.c` 先扫描（本板唯一命中 **0x18**，SDO 接地），
  再由 `APP_Sc7a20hOpen()` 锁定地址，走 SC7A20H 驱动读三轴（20 帧 @100 Hz 正常变化）、
  500 次 6 字节读测速、10 s soak（100/100 成功）。
- 对拍（2026-11-04 双后端均通过）：`-DPIXPILL_IIC_BACKEND_SOFT=ON/OFF` 切换软/硬后端，同一份测试各跑一遍。
  - 软 IIC：扫到 0x18 → 20 帧三轴 → 500×6 字节读 920 ms ≈ **45 kHz** SCL → soak 100/0。
  - 硬 I2C1（`build_hw` 独立目录）：init/三轴/soak 也是 100/0，`APP_AxisBenchmark` 被 `#if` 裁掉。
    坑：`HAL_I2C_DeInit()` → `MX_I2C1_Init()` 的关-再开复用路径在 PY32F0xx HAL 上不可靠（第二次 init 后
    首读 WHO_AM_I 值错 → `sc7a20h: device id is invalid.`），所以硬后端的 `iic_deinit()` 现在是保留外设、
    返回 0 —— 共享总线的所有权本来就该归 main。
