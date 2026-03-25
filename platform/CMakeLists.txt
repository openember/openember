# Platform 层：OSAL / HAL
if(OPENEMBER_ENABLE_HAL AND NOT OPENEMBER_ENABLE_OSAL)
    message(FATAL_ERROR "OPENEMBER_ENABLE_HAL requires OPENEMBER_ENABLE_OSAL")
endif()

if(OPENEMBER_ENABLE_OSAL)
    add_subdirectory(osal)
endif()

if(OPENEMBER_ENABLE_HAL)
    add_subdirectory(hal)
endif()
