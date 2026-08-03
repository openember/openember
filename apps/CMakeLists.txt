set(FOLDER_NAME "apps")

# Product application entry point. OpenEmber keeps this directory for
# downstream projects to add product-specific nodes without mixing them with
# framework examples, system nodes, services, or tools.

if(OPENEMBER_ENABLE_LINK AND OPENEMBER_ENABLE_MSGS)
    if(OPENEMBER_APP_SMART_DEVICE_DEMO)
        add_subdirectory(smart_device_demo)
    endif()
endif()
