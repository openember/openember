project(OTA VERSION 1.0.0 LANGUAGES CXX)

set( OTA_MODULE_SRCS 
    main.cpp
)

OPENEMBER_ADD_EXECUTABLE(TARGET ${PROJECT_NAME} 
    SRCS     ${OTA_MODULE_SRCS} 
    LIBS     ${OPENEMBER_LIBRARIES}
    INCLUDES ${OPENEMBER_INCLUDE_DIRS}
    DESTINATION bin 
)
