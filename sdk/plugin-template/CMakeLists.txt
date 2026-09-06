cmake_minimum_required(VERSION 3.16)
project(my_tool_highlight)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 指向 frap 的 sdk/ 目录（按需修改为你的实际路径）
set(FRAP_SDK_DIR "/path/to/frap/sdk")

add_library(tool_highlight SHARED tool_highlight.cpp)
target_include_directories(tool_highlight PRIVATE ${FRAP_SDK_DIR})

set_target_properties(tool_highlight PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 可选：构建后直接复制到用户插件目录
# add_custom_command(TARGET tool_highlight POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#             $<TARGET_FILE:tool_highlight>
#             $ENV{HOME}/.config/frap/plugins/)
