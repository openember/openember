# 示例子工程（根据根目录选择的 pubsub transport backend）
#
# 注意：
# - ZMQ：支持 tcp://*:PORT（wildcard）
# - NNG：URL wildcard * 不一定有效，这里使用 127.0.0.1

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

if(BUILD_PUBSUB_ZMQ OR BUILD_PUBSUB_NNG OR BUILD_PUBSUB_LCM)
    add_subdirectory(pubsub_two_nodes)
endif()

if(OPENEMBER_MSGBUS_USE_NNG)
    add_subdirectory(msgbus_nng_forwarder)
endif()
