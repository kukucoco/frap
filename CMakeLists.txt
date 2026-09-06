cmake_minimum_required(VERSION 3.28)
project(frap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# 稳定 ABI 的插件 SDK（C 接口，插件作者无需链接 Qt / frap_core）
# ---------------------------------------------------------------------------
set(FRAP_SDK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

# 调试模式下生成 release 之外额外的默认 build/plugins 输出目录
set(FRAP_PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/plugins)
file(MAKE_DIRECTORY ${FRAP_PLUGIN_OUTPUT_DIR})

# ---------------------------------------------------------------------------
# 核心库
# ---------------------------------------------------------------------------
add_subdirectory(src/core)

# ---------------------------------------------------------------------------
# 插件宿主进程（extension host）
# ---------------------------------------------------------------------------
add_subdirectory(src/host)

# ---------------------------------------------------------------------------
# 主程序
# ---------------------------------------------------------------------------
add_subdirectory(src/app)

# ---------------------------------------------------------------------------
# 内置插件（同样走插件通道，作为 SDK 示例）
# ---------------------------------------------------------------------------
add_subdirectory(src/plugins)

# ---------------------------------------------------------------------------
# 崩溃报告查看器
# ---------------------------------------------------------------------------
add_subdirectory(src/crash)

# ---------------------------------------------------------------------------
# 无头集成测试
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# 将默认配置复制到构建目录，方便开发期直接运行
# ---------------------------------------------------------------------------
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/settings.json
               ${CMAKE_BINARY_DIR}/config/settings.json COPYONLY)

message(STATUS "frap: 插件输出目录 = ${FRAP_PLUGIN_OUTPUT_DIR}")
