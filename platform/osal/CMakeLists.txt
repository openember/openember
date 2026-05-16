# OpenEmber OSAL — Linux C++17 实现
add_library(openember_osal STATIC
    src/linux/mutex.cpp
    src/linux/thread.cpp
    src/linux/time.cpp
    src/linux/cond.cpp
    src/linux/event.cpp
    src/linux/sem.cpp
    src/linux/shm.cpp
    src/linux/socket_ops.cpp
    src/linux/socket.cpp
    src/linux/pipe.cpp
    src/linux/fifo.cpp
    src/linux/signals.cpp
    src/linux/mq.cpp
)

target_include_directories(openember_osal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(openember_osal PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

find_package(Threads REQUIRED)
target_link_libraries(openember_osal PUBLIC Threads::Threads rt)
