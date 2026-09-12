project(hardware_interface_examples VERSION 0.1.0 LANGUAGES CXX)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_hardware_mock_listener
    SRCS     mock_listener.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
    DESTINATION bin
)

set_property(TARGET openember_hardware_mock_listener PROPERTY CXX_STANDARD 17)
set_property(TARGET openember_hardware_mock_listener PROPERTY CXX_STANDARD_REQUIRED ON)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_joint_command_sender
    SRCS     joint_command_sender.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
    DESTINATION bin
)

set_property(TARGET openember_joint_command_sender PROPERTY CXX_STANDARD 17)
set_property(TARGET openember_joint_command_sender PROPERTY CXX_STANDARD_REQUIRED ON)
