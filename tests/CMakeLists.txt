# 测试
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Test)

# 无头框架集成测试（宿主/协议/渲染/崩溃重启）
qt_add_executable(core_framework_test core_framework_test.cpp)
target_link_libraries(core_framework_test PRIVATE frap_core)
set_target_properties(core_framework_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

add_test(NAME core_framework_test
         COMMAND core_framework_test ${FRAP_PLUGIN_OUTPUT_DIR})
set_tests_properties(core_framework_test PROPERTIES TIMEOUT 30)

# QML 交互测试（见 qmltest/CMakeLists.txt）
add_subdirectory(qmltest)

# UI 渲染预览工具（离线输出 preview.png，便于验收/迭代界面）
qt_add_executable(ui_preview ui_preview.cpp)
target_link_libraries(ui_preview PRIVATE
    frap_ui
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick)
set_target_properties(ui_preview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
