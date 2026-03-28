# 应用层示例：hello_node、msgbus 双节点、msgbus NNG 转发器等

if(NOT DEFINED OPENEMBER_MODULE_TEMPLATE OR OPENEMBER_MODULE_TEMPLATE)
    add_subdirectory(hello_node)
endif()

if(NOT DEFINED OPENEMBER_EXAMPLE_NETWORK_SOCKETS OR OPENEMBER_EXAMPLE_NETWORK_SOCKETS)
    add_subdirectory(network_sockets)
endif()

if(NOT DEFINED OPENEMBER_EXAMPLE_MSGBUS_TWO_NODES OR OPENEMBER_EXAMPLE_MSGBUS_TWO_NODES)
    add_subdirectory(msgbus_two_nodes)
endif()

if(NOT DEFINED OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER OR OPENEMBER_EXAMPLE_MSGBUS_NNG_FORWARDER)
    if(OPENEMBER_MSGBUS_USE_NNG)
        add_subdirectory(msgbus_nng_forwarder)
    endif()
endif()

if(OPENEMBER_COMPONENT_MQTT AND OPENEMBER_EXAMPLE_MQTT_EMQX)
    add_subdirectory(mqtt_emqx_client)
endif()
