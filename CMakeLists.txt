cmake_minimum_required(VERSION 3.15)

# 注意：如果使用toolchain.cmake，toolchain必须在project()之前加载
# example: cmake -DCMAKE_TOOLCHAIN_FILE=./toolchain-arm-none-eabi.cmake ...

project(SOFT_IIC LANGUAGES C)

# ===================== 可配置编译开关 =====================
# TEST 默认 OFF：test/*.c 需要真机总线，固件无人引用（符合《预编译裁剪规范》全部走 -D）。
# EXAMPLE 默认 ON：example/driver_soft_iic_basic.c 是 bring-up 的运行依赖
# （main.c 用 basic 层做扫描/探活/寄存器 dump）。
option(SOFT_IIC_BUILD_TEST "Build test source code" OFF)
option(SOFT_IIC_BUILD_EXAMPLE "Build example source code" ON)

# ===================== 核心静态库目标 =====================
# src/ 平台无关（零 HAL 符号）；端口层由 -DSOFT_IIC_PORT_HEADER=<平台头> 提供
# （本仓库为 User/Inc/driver_soft_iic_port_py32.h，内联模式专用）。
add_library(soft_iic STATIC
    src/driver_soft_iic.c
)

# 别名，上层项目推荐使用 SOFT_IIC::SOFT_IIC 链接
add_library(SOFT_IIC::SOFT_IIC ALIAS soft_iic)

target_compile_features(soft_iic PUBLIC c_std_99)

# 裸机静态库，关闭PIE，不要生成位置无关代码（单片机不需要）
set_target_properties(soft_iic PROPERTIES
    POSITION_INDEPENDENT_CODE OFF
)

# 对外公开头文件路径：src 驱动头、interface 接口层声明
target_include_directories(soft_iic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/interface
)

# ===================== example 示例代码（默认 ON） =====================
if(SOFT_IIC_BUILD_EXAMPLE)
    target_sources(soft_iic PRIVATE
        example/driver_soft_iic_basic.c
    )
    target_include_directories(soft_iic PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/example
    )
endif()

# ===================== test 测试代码（可选编译，默认 OFF） =====================
if(SOFT_IIC_BUILD_TEST)
    if(NOT SOFT_IIC_BUILD_EXAMPLE)
        message(FATAL_ERROR "SOFT_IIC_BUILD_TEST 需要 SOFT_IIC_BUILD_EXAMPLE=ON（测试复用 basic 层）")
    endif()
    target_sources(soft_iic PRIVATE
        test/driver_soft_iic_scan_test.c
    )
    target_include_directories(soft_iic PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/test
    )
endif()
