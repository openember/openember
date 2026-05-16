# OpenEmber HAL — Linux (C++17)
add_library(openember_hal STATIC
    src/linux/file.cpp
    src/linux/uart.cpp
    src/linux/gpio.cpp
    src/linux/i2c.cpp
    src/linux/spi.cpp
    src/linux/can.cpp
    src/linux/sbus.cpp
    src/linux/onewire.cpp
)

target_include_directories(openember_hal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(openember_hal PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(openember_hal PUBLIC openember_osal)
