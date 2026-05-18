add_executable(sbus-receiver
    main.cpp
)

target_link_libraries(sbus-receiver PRIVATE openember_lpio)

set_target_properties(sbus-receiver PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS sbus-receiver RUNTIME DESTINATION bin OPTIONAL)
