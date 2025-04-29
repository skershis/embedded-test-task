
add_library(gpio
    gpio_manager.cpp
)
target_include_directories(gpio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
