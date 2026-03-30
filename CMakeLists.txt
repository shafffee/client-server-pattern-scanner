cmake_minimum_required(VERSION 3.16)

project(kaspersky_test_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src/client)
add_subdirectory(src/server)
add_subdirectory(src/stats_viewer)