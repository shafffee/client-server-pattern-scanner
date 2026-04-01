add_executable(stats_viewer_app main.cpp)

target_include_directories(stats_viewer_app PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(stats_viewer_app PRIVATE common)