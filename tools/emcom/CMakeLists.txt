add_executable(emcom
    main.cpp
)

target_link_libraries(emcom PRIVATE openember_lpio)

set_target_properties(emcom PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS emcom RUNTIME DESTINATION bin OPTIONAL)
