# QML 控制器交互测试（offscreen）
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Test)

qt_add_executable(qml_controller_test qml_controller_test.cpp)

target_link_libraries(qml_controller_test PRIVATE
    frap_ui
    Qt6::Test
    Qt6::Qml
    Qt6::Quick
)

set_target_properties(qml_controller_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

add_test(NAME qml_controller_test
         COMMAND qml_controller_test ${FRAP_PLUGIN_OUTPUT_DIR})
set_tests_properties(qml_controller_test PROPERTIES
    TIMEOUT 90
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
