# Examples: hello_node, core Link demos, transport, network, mqtt, references

if(NOT DEFINED OPENEMBER_EXAMPLE_HELLO_NODE OR OPENEMBER_EXAMPLE_HELLO_NODE)
    add_subdirectory(hello_node)
endif()

if(NOT DEFINED OPENEMBER_REFERENCE_SENSOR_DATA OR OPENEMBER_REFERENCE_SENSOR_DATA)
    add_subdirectory(references/sensor_data_reference)
endif()

if(NOT DEFINED OPENEMBER_EXAMPLE_NETWORK_SOCKETS OR OPENEMBER_EXAMPLE_NETWORK_SOCKETS)
    add_subdirectory(network_sockets)
endif()

if(OPENEMBER_COMPONENT_MQTT AND OPENEMBER_EXAMPLE_MQTT_EMQX)
    add_subdirectory(mqtt_emqx_client)
endif()

if(NOT DEFINED OPENEMBER_COMPONENT_TRANSPORT OR OPENEMBER_COMPONENT_TRANSPORT)
    if(NOT DEFINED OPENEMBER_EXAMPLE_TRANSPORT OR OPENEMBER_EXAMPLE_TRANSPORT)
        add_subdirectory(transport)
    endif()
endif()

if(OPENEMBER_ENABLE_LINK)
    if(NOT DEFINED OPENEMBER_EXAMPLE_CORE OR OPENEMBER_EXAMPLE_CORE)
        add_subdirectory(core)
    endif()
endif()
