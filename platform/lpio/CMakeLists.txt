cmake_minimum_required(VERSION 3.16)

# LPIO — Linux Peripheral I/O (C++20, RAII + Builder)
add_library(openember_lpio STATIC
    src/SerialPort.cpp
    src/I2CBus.cpp
    src/SPIBus.cpp
    src/CANBus.cpp
    src/GPIO.cpp
    src/OneWire.cpp
    src/PWM.cpp
    src/RS485.cpp
    src/SBUS.cpp
)

target_include_directories(openember_lpio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(openember_lpio PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

if(OPENEMBER_ENABLE_LPIO_EXAMPLES)
    add_subdirectory(examples)
endif()
