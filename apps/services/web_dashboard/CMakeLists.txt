project(web_dashboard VERSION 1.0.0 LANGUAGES CXX)

set( WEB_MODULE_SRCS 
    main.cpp
)

set(_web_root "${OPENEMBER_WEB_DASHBOARD_ROOT_DIR}")
if(NOT _web_root OR _web_root STREQUAL "")
    set(_web_root "${CMAKE_CURRENT_SOURCE_DIR}/web_root")
elseif(NOT IS_ABSOLUTE "${_web_root}")
    set(_web_root "${CMAKE_SOURCE_DIR}/${_web_root}")
endif()

OPENEMBER_ADD_EXECUTABLE(TARGET ${PROJECT_NAME} 
    SRCS     ${WEB_MODULE_SRCS} 
    LIBS     ${OPENEMBER_LIBRARIES} ${MONGOOSE_LIBRARIES}
    INCLUDES ${OPENEMBER_INCLUDE_DIRS} ${MONGOOSE_INCLUDE_DIRS}
    DESTINATION bin 
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    OPENEMBER_WEB_DASHBOARD_ROOT_DIR="${_web_root}"
)

file(COPY web_root DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/../..")