# 崩溃报告查看器
find_package(Qt6 REQUIRED COMPONENTS Core)

qt_add_executable(crash_viewer crash_viewer.cpp)
target_link_libraries(crash_viewer PRIVATE frap_core)
target_include_directories(crash_viewer PRIVATE ${FRAP_SDK_INCLUDE_DIR})

set_target_properties(crash_viewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
