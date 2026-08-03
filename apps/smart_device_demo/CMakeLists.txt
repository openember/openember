project(smart_device_demo VERSION 0.1.0 LANGUAGES CXX)

OPENEMBER_ADD_EXECUTABLE(TARGET openember_smart_device_demo
    SRCS     smart_device_demo.cpp
    LIBS     openember::core
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
    DESTINATION bin
)

set_property(TARGET openember_smart_device_demo PROPERTY CXX_STANDARD 17)
set_property(TARGET openember_smart_device_demo PROPERTY CXX_STANDARD_REQUIRED ON)
