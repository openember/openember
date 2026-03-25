# Mongoose (cesanta/mongoose) dependency via FetchContent
#
# Provides:
# - openember_get_mongoose()
# - variables:
#   - MONGOOSE_INCLUDE_DIRS
#   - MONGOOSE_LIBRARIES  (target name)
#
include(FetchContent)

function(openember_get_mongoose)
    if(NOT DEFINED OPENEMBER_MONGOOSE_LOCAL_SOURCE)
        set(OPENEMBER_MONGOOSE_LOCAL_SOURCE "" CACHE PATH "Optional local path override for mongoose")
    endif()

    FetchContent_GetProperties(openember_mongoose)
    if(NOT openember_mongoose_POPULATED)
        if(OPENEMBER_MONGOOSE_LOCAL_SOURCE)
            FetchContent_Declare(
                openember_mongoose
                SOURCE_DIR ${OPENEMBER_MONGOOSE_LOCAL_SOURCE}
                OVERRIDE_FIND_PACKAGE
            )
        else()
            FetchContent_Declare(
                openember_mongoose
                URL ${OPENEMBER_MONGOOSE_URL}
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                OVERRIDE_FIND_PACKAGE
            )
        endif()

        FetchContent_MakeAvailable(openember_mongoose)
    endif()

    # Upstream ships mongoose.c / mongoose.h. Build a minimal static library.
    if(NOT TARGET mongoose)
        add_library(mongoose STATIC
            "${openember_mongoose_SOURCE_DIR}/mongoose.c"
        )
        target_include_directories(mongoose PUBLIC
            "${openember_mongoose_SOURCE_DIR}"
        )
        set_target_properties(mongoose PROPERTIES
            POSITION_INDEPENDENT_CODE ON
        )
    endif()

    set(MONGOOSE_INCLUDE_DIRS "${openember_mongoose_SOURCE_DIR}" PARENT_SCOPE)
    set(MONGOOSE_LIBRARIES mongoose PARENT_SCOPE)
endfunction()

