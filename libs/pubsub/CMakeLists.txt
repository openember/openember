# ember_pubsub：优先 libzmq，否则内置 UDP

find_package(Threads REQUIRED)

if(OPENEMBER_PUBSUB_BACKEND STREQUAL "zmq")
    add_library(ember_pubsub STATIC ember_pubsub_zmq.c)
    target_include_directories(ember_pubsub PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(ember_pubsub PUBLIC
        ${OPENEMBER_LIBZMQ_LIBRARIES}
        Threads::Threads
    )
    target_include_directories(ember_pubsub PUBLIC ${OPENEMBER_LIBZMQ_INCLUDE_DIRS})
    if(OPENEMBER_LIBZMQ_CFLAGS_OTHER)
        target_compile_options(ember_pubsub PRIVATE ${OPENEMBER_LIBZMQ_CFLAGS_OTHER})
    endif()
elseif(OPENEMBER_PUBSUB_BACKEND STREQUAL "nng")
    add_library(ember_pubsub STATIC ember_pubsub_nng.c)
    target_include_directories(ember_pubsub PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(ember_pubsub PUBLIC
        ${OPENEMBER_NNG_LIBRARIES}
        Threads::Threads
    )
    target_include_directories(ember_pubsub PUBLIC ${OPENEMBER_NNG_INCLUDE_DIRS})
elseif(OPENEMBER_PUBSUB_BACKEND STREQUAL "lcm")
    add_library(ember_pubsub STATIC ember_pubsub_lcm.c)
    target_include_directories(ember_pubsub PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(ember_pubsub PUBLIC
        ${OPENEMBER_LCM_LIBRARIES}
        Threads::Threads
    )
    target_include_directories(ember_pubsub PUBLIC ${OPENEMBER_LCM_INCLUDE_DIRS})
else()
    add_library(ember_pubsub STATIC ember_pubsub_udp.c)
    target_include_directories(ember_pubsub PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(ember_pubsub PUBLIC Threads::Threads)
endif()
