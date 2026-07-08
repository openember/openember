# Platform 层：OSAL / LPIO
if(OPENEMBER_ENABLE_OSAL)
    add_subdirectory(osal)
endif()

if(OPENEMBER_ENABLE_LPIO)
    add_subdirectory(lpio)
endif()
