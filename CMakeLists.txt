# ============================================================================
# FlyOS 子系统 - 根 CMake 配置文件
# ============================================================================
# 本文件负责：
# 1. 校验 FlyOS 目标平台配置是否合法
# 2. 输出 FlyOS 当前配置摘要
# 3. 按需包含 FlyOS 内核与 FlyKit 子目录
# 4. 约束上层必须显式提供 FLYOS_PLATFORM
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# ----------------------------------------------------------------------------
# 1. 平台参数校验
# ----------------------------------------------------------------------------

# 支持的平台列表
set(SUPPORTED_PLATFORMS "stm32f4" "s32k344")

# 检查是否已定义 FLYOS_PLATFORM
if(NOT DEFINED FLYOS_PLATFORM)
    string(JOIN ", " PLATFORMS_STR ${SUPPORTED_PLATFORMS})
    message(FATAL_ERROR
        "未定义 FLYOS_PLATFORM。\n"
        "请在顶层 CMakeLists.txt 中显式设置该变量。\n"
        "当前支持的平台：${PLATFORMS_STR}\n"
        "示例：set(FLYOS_PLATFORM \"s32k344\" CACHE STRING \"目标平台\")"
    )
endif()

# 校验平台取值是否合法
if(NOT FLYOS_PLATFORM IN_LIST SUPPORTED_PLATFORMS)
    string(JOIN ", " PLATFORMS_STR ${SUPPORTED_PLATFORMS})
    message(FATAL_ERROR
        "不支持的 FLYOS_PLATFORM：'${FLYOS_PLATFORM}'\n"
        "当前支持的平台：${PLATFORMS_STR}"
    )
endif()

# ----------------------------------------------------------------------------
# 2. 配置摘要输出
# ----------------------------------------------------------------------------

message(STATUS "========================================")
message(STATUS "FlyOS 配置摘要")
message(STATUS "========================================")
message(STATUS "目标平台：      ${FLYOS_PLATFORM}")
message(STATUS "构建类型：      ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")

# ----------------------------------------------------------------------------
# 3. 添加子目录
# ----------------------------------------------------------------------------

add_subdirectory(flyos)

if(USE_FLYKIT)
    add_subdirectory(flykit)
endif()
