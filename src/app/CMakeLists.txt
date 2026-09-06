# 覆盖层 UI 库（QML 版本）
add_library(frap_ui STATIC
    qml_backend.h
    qml_backend.cpp
    frame_image.h
    frame_image.cpp
    icon_provider.h
    icon_provider.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickShapes)
find_package(LayerShellQt REQUIRED)

target_include_directories(frap_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FRAP_SDK_INCLUDE_DIR}
)

target_link_libraries(frap_ui PUBLIC
    frap_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickShapes
    LayerShellQt::Interface
)

# QML 模块（qmldir 驱动，FrameImage 运行时注册）
set(FRAP_QML_DIR ${CMAKE_CURRENT_SOURCE_DIR}/qml)
qt_add_resources(frap_ui "qmlmod"
    PREFIX "/Frap"
    BASE ${FRAP_QML_DIR}
    FILES
        ${FRAP_QML_DIR}/qmldir
        ${FRAP_QML_DIR}/Theme.qml
        ${FRAP_QML_DIR}/CaptureController.qml
        ${FRAP_QML_DIR}/main.qml
        ${FRAP_QML_DIR}/ToolButton.qml
        ${FRAP_QML_DIR}/TranslationPanel.qml
        ${FRAP_QML_DIR}/FloatingToolBar.qml
        ${FRAP_QML_DIR}/OptionsBar.qml
        ${FRAP_QML_DIR}/CropOverlay.qml
        ${FRAP_QML_DIR}/AnnotationOutline.qml
        ${FRAP_QML_DIR}/Magnifier.qml
        ${FRAP_QML_DIR}/Readout.qml
        ${FRAP_QML_DIR}/Toast.qml
)

# 主程序
qt_add_executable(frap main.cpp)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2 QuickShapes Widgets)

target_link_libraries(frap PRIVATE
    frap_ui
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)

target_include_directories(frap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FRAP_SDK_INCLUDE_DIR}
)

set_target_properties(frap PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
