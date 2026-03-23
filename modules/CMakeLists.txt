set(FOLDER_NAME "modules")

# 可扩展模块层（当前先迁入原 components/Algorithm）
if(NOT DEFINED OPENEMBER_FEATURE_ALGORITHM OR OPENEMBER_FEATURE_ALGORITHM)
    add_subdirectory(Algorithm)
endif()

