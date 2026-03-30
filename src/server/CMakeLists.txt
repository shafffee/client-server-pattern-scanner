add_executable(server_app
    main.cpp
    config.cpp
)

target_include_directories(server_app PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party
)