add_executable(server_app
    main.cpp
    config.cpp
    scanner.cpp
    statistics.cpp
)

target_include_directories(server_app PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(server_app PRIVATE common)