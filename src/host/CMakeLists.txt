# 插件宿主进程
find_package(Qt6 REQUIRED COMPONENTS Core)

qt_add_executable(frap-host main.cpp)

target_link_libraries(frap-host PRIVATE
    frap_core
)

target_include_directories(frap-host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FRAP_SDK_INCLUDE_DIR}
)

# 宿主需要可执行权限 + 与主程序同目录输出（主程序在同目录找宿主）
set_target_properties(frap-host PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
