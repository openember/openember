# OpenEmber HAL — Linux 初版（file / uart）
add_library(openember_hal STATIC
    src/linux/oe_file.c
    src/linux/oe_uart.c
    src/linux/oe_gpio.c
)

target_include_directories(openember_hal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(openember_hal PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(openember_hal PUBLIC openember_osal)
