set(FOLDER_NAME "apps")

# In-tree product apps kept intentionally thin: turtle (+ optional smart_device).
# Full product stories (rover, agri) are external repos wired via
# OPENEMBER_EXTERNAL_APPS_ROOT — see docs/design/external-product-repos.md.

if(OPENEMBER_ENABLE_LINK AND OPENEMBER_ENABLE_MSGS)
    if(OPENEMBER_APP_SMART_DEVICE_DEMO)
        add_subdirectory(smart_device_demo)
    endif()
    if(OPENEMBER_APP_TURTLE_DEMO)
        add_subdirectory(turtle_demo)
    endif()
endif()
