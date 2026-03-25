# 应用层示例：hello_node、pub/sub 双节点、msgbus NNG 转发器等

if(NOT DEFINED OPENEMBER_MODULE_TEMPLATE OR OPENEMBER_MODULE_TEMPLATE)
    add_subdirectory(hello_node)
endif()

add_subdirectory(network_sockets)

# pub/sub 双节点演示（依赖当前选择的 pubsub transport）
if(OPENEMBER_PUBSUB_BACKEND STREQUAL "zmq")
    set(OPENEMBER_PUBSUB_DEMO_PUB_URL "tcp://*:5560")
    set(OPENEMBER_PUBSUB_DEMO_SUB_URL "tcp://127.0.0.1:5560")
elseif(OPENEMBER_PUBSUB_BACKEND STREQUAL "nng")
    set(OPENEMBER_PUBSUB_DEMO_PUB_URL "tcp://127.0.0.1:5560")
    set(OPENEMBER_PUBSUB_DEMO_SUB_URL "tcp://127.0.0.1:5560")
elseif(OPENEMBER_PUBSUB_BACKEND STREQUAL "lcm")
    set(OPENEMBER_PUBSUB_DEMO_PUB_URL "udpm://239.255.76.67:7667")
    set(OPENEMBER_PUBSUB_DEMO_SUB_URL "udpm://239.255.76.67:7667")
else()
    set(OPENEMBER_PUBSUB_DEMO_PUB_URL "udp://127.0.0.1:7560")
    set(OPENEMBER_PUBSUB_DEMO_SUB_URL "udp://127.0.0.1:7560")
endif()

if(NOT DEFINED OPENEMBER_EXAMPLE_PUBSUB_TWO_NODES OR OPENEMBER_EXAMPLE_PUBSUB_TWO_NODES)
    if(BUILD_PUBSUB_ZMQ OR BUILD_PUBSUB_NNG OR BUILD_PUBSUB_LCM)
        add_subdirectory(pubsub_two_nodes)
    endif()
endif()

if(NOT DEFINED OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER OR OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER)
    if(OPENEMBER_MSGBUS_USE_NNG)
        add_subdirectory(msgbus_nng_forwarder)
    endif()
endif()
