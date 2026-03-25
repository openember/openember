# OpenEmber OSAL — Linux pthread 实现（初版）
add_library(openember_osal STATIC
    src/linux/oe_mutex.c
    src/linux/oe_thread.c
    src/linux/oe_time.c
    src/linux/oe_cond.c
    src/linux/oe_event.c
    src/linux/oe_sem.c
    src/linux/oe_shm.c
    src/linux/oe_socket.c
    src/linux/oe_pipe.c
    src/linux/oe_fifo.c
)

target_include_directories(openember_osal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(openember_osal PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

find_package(Threads REQUIRED)
target_link_libraries(openember_osal PUBLIC Threads::Threads)
