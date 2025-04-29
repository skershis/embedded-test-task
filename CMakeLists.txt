cmake_minimum_required(VERSION 3.14)
project(embedded_app)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(mqtt)
add_subdirectory(gpio)

add_executable(embedded-app
    main.cpp
    application.cpp
)

target_link_libraries(embedded-app
    mqtt
    gpio
    mosquitto
    pthread
)
