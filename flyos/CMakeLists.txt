# ============================================================================
# FlyOS 内核 - CMake 配置文件
# ============================================================================
# 本文件负责：
# 1. 根据 FLYOS_PLATFORM 选择对应的内核移植层
# 2. 组织内核核心源码与平台端口源码
# 3. 生成 flyos_kernel 静态库目标
# 4. 关联平台相关 HAL 或 SDK 依赖
# ============================================================================

message(STATUS "========================================")
message(STATUS "正在配置 FlyOS 内核")
message(STATUS "========================================")

# ----------------------------------------------------------------------------
# 1. 平台配置映射
# ----------------------------------------------------------------------------
# 新增平台时需要同步完成以下工作：
# 1. 添加 PLATFORM_MACRO_<platform> 的唯一整数值
# 2. 添加 PLATFORM_DEP_<platform> 的 HAL 或 SDK 目标名
# 3. 在 port/<platform>/ 下补齐端口实现
# 4. 在上层 CMakeLists.txt 的 SUPPORTED_PLATFORMS 中登记
# ----------------------------------------------------------------------------

# 平台对应的编译宏取值
set(PLATFORM_MACRO_stm32f4 1)
set(PLATFORM_MACRO_s32k344 2)

# 平台对应的 HAL 或 SDK 目标依赖
set(PLATFORM_DEP_stm32f4 "stm32f4_hal")
set(PLATFORM_DEP_s32k344 "s32k344_sdk")

# ----------------------------------------------------------------------------
# 2. 内核核心源码
# ----------------------------------------------------------------------------

set(FLYOS_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/flyos_kernel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/flyos_ipc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/flyos_mem.c
)

# ----------------------------------------------------------------------------
# 3. 平台端口选择
# ----------------------------------------------------------------------------

set(FLYOS_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/port/${FLYOS_PLATFORM})

if(NOT EXISTS ${FLYOS_PORT_DIR})
    message(FATAL_ERROR
        "未找到平台端口目录。\n"
        "期望路径：${FLYOS_PORT_DIR}\n"
        "请确认平台 ${FLYOS_PLATFORM} 的端口实现已经存在。"
    )
endif()

set(FLYOS_PORT_SOURCES ${FLYOS_PORT_DIR}/flyos_port.c)
set(FLYOS_PORT_ASM_SOURCES ${FLYOS_PORT_DIR}/flyos_port_asm.s)

message(STATUS "端口目录：${FLYOS_PORT_DIR}")

# ----------------------------------------------------------------------------
# 4. 内核库目标定义
# ----------------------------------------------------------------------------

add_library(flyos_kernel STATIC
    ${FLYOS_CORE_SOURCES}
    ${FLYOS_PORT_SOURCES}
    ${FLYOS_PORT_ASM_SOURCES}
)

target_include_directories(flyos_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/port
    ${FLYOS_PORT_DIR}
)

# ----------------------------------------------------------------------------
# 5. 平台相关配置
# ----------------------------------------------------------------------------

# 注入平台编译宏
target_compile_definitions(flyos_kernel PUBLIC
    FLYOS_PLATFORM=${PLATFORM_MACRO_${FLYOS_PLATFORM}}
)

# 关联平台 HAL 或 SDK 依赖
# 依赖目标允许缺省，但缺省后平台功能可能受限
set(PLATFORM_DEP ${PLATFORM_DEP_${FLYOS_PLATFORM}})
if(TARGET ${PLATFORM_DEP})
    target_link_libraries(flyos_kernel PUBLIC ${PLATFORM_DEP})
    message(STATUS "已关联依赖：${PLATFORM_DEP}")
else()
    message(WARNING "未找到依赖目标 ${PLATFORM_DEP}，部分平台功能可能不可用")
endif()

message(STATUS "========================================")
message(STATUS "FlyOS 内核配置完成")
message(STATUS "========================================")
