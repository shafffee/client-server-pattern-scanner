add_executable(client_app 
    main.cpp file_reader.cpp)

target_link_libraries(client_app PRIVATE common)