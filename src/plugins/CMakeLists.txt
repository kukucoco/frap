# 内置插件（全部走标准插件通道，作为 SDK 示例）
#
# 插件仅依赖 sdk 头文件，不链接 Qt / frap_core —— 这正是插件社区
# 无需关心宿主 ABI 的原因。任何能编译 C++ 的环境都能写插件。

# 通用插件目标构造
function(sc_add_plugin target)
    add_library(${target} SHARED ${ARGN})
    target_include_directories(${target} PRIVATE ${FRAP_SDK_INCLUDE_DIR})
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        PREFIX ""                       # 输出 tool_rect.so 而非 libtool_rect.so
        LIBRARY_OUTPUT_DIRECTORY ${FRAP_PLUGIN_OUTPUT_DIR}
    )
endfunction()

sc_add_plugin(tool_rect tool_rect/tool_rect.cpp)
sc_add_plugin(tool_arrow tool_arrow/tool_arrow.cpp)
sc_add_plugin(tool_text tool_text/tool_text.cpp)
sc_add_plugin(tool_mosaic tool_mosaic/tool_mosaic.cpp)
sc_add_plugin(tool_ellipse tool_ellipse/tool_ellipse.cpp)
sc_add_plugin(tool_highlight tool_highlight/tool_highlight.cpp)
sc_add_plugin(tool_blur tool_blur/tool_blur.cpp)
sc_add_plugin(tool_number tool_number/tool_number.cpp)
sc_add_plugin(export_save export_save/export_save.cpp)
sc_add_plugin(export_clipboard export_clipboard/export_clipboard.cpp)

# 需要 Qt 的能力型插件（服务/采集，与标注工具的 Qt 无关形成对照）
find_package(Qt6 REQUIRED COMPONENTS Core Gui Network)

sc_add_plugin(service_translate service_translate/service_translate.cpp)
target_link_libraries(service_translate PRIVATE Qt6::Core Qt6::Network)

sc_add_plugin(capture_longshot capture_longshot/capture_longshot.cpp)
target_link_libraries(capture_longshot PRIVATE Qt6::Core Qt6::Gui)
