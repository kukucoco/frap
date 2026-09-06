# frap_core：主程序与插件宿主共用的核心库
add_library(frap_core STATIC
    protocol.h
    rpc_peer.h
    logging.h
    logging.cpp
    settings.h
    settings.cpp
    plugin_manifest.h
    plugin_manager.h
    plugin_manager.cpp
    session.h
    session.cpp
    render_ops.h
    render_ops.cpp
    crash_handler.h
    crash_handler.cpp
)

target_include_directories(frap_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FRAP_SDK_INCLUDE_DIR}
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

target_link_libraries(frap_core PUBLIC
    Qt6::Core
    Qt6::Gui
)

# 插件宿主需要把 core 的导出符号保留，因此 core 也参与构建
set_target_properties(frap_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# 编译期注入宿主绝对路径，便于任何环境（含测试/安装）定位宿主
target_compile_definitions(frap_core PUBLIC
    FRAP_HOST_PATH="${CMAKE_BINARY_DIR}/frap-host"
)
