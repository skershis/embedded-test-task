# mqtt/CMakeLists.txt
add_library(mqtt
    mqtt_client.cpp
)
target_include_directories(mqtt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/generic
)
