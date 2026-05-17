function(lpio_add_example target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE openember_lpio)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/serial_echo.cpp")
    lpio_add_example(lpio_serial_echo serial_echo.cpp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/i2c_scan.cpp")
    lpio_add_example(lpio_i2c_scan i2c_scan.cpp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/can_logger.cpp")
    lpio_add_example(lpio_can_logger can_logger.cpp)
endif()
