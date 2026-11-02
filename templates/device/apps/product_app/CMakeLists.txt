if(NOT PRODUCT_BUILD_APP)
    return()
endif()

add_executable(openember___PRODUCT_SLUG___app main.cpp)
add_dependencies(openember_product openember___PRODUCT_SLUG___app)
target_link_libraries(openember___PRODUCT_SLUG___app PRIVATE openember::core)

set_target_properties(openember___PRODUCT_SLUG___app PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

install(TARGETS openember___PRODUCT_SLUG___app RUNTIME DESTINATION bin)
