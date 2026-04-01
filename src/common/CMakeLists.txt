add_library(common
    port.cpp
    net_utils.cpp
    fd_utils.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)