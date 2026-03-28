# SQLite amalgamation：third_party/ sqlite-amalgamation-<num>.zip → build/_deps/sqlite-amalgamation-<num>/

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_prepare_sqlite_source out_var)
    if(OPENEMBER_SQLITE_LOCAL_SOURCE)
        set(_root "${OPENEMBER_SQLITE_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_root "${OPENEMBER_SQLITE_CACHE_KEY}" "${OPENEMBER_SQLITE_STAGE_DIR_NAME}"
            "${OPENEMBER_SQLITE_URL}" "sqlite3.c" "")
    endif()

    if(NOT EXISTS "${_root}/sqlite3.c")
        file(GLOB _cand LIST_DIRECTORIES true "${_root}/*")
        foreach(_d IN LISTS _cand)
            if(IS_DIRECTORY "${_d}" AND EXISTS "${_d}/sqlite3.c")
                set(_root "${_d}")
                break()
            endif()
        endforeach()
    endif()

    if(NOT EXISTS "${_root}/sqlite3.c")
        message(FATAL_ERROR "sqlite3.c not found under extracted SQLite tree")
    endif()

    set(${out_var} "${_root}" PARENT_SCOPE)
endfunction()

function(openember_get_sqlite)
    openember_prepare_sqlite_source(_root)

    if(NOT TARGET sqlite3)
        add_library(sqlite3 SHARED "${_root}/sqlite3.c")
        target_include_directories(sqlite3 PUBLIC "${_root}")
    endif()

    if(NOT TARGET sqlite3-static)
        add_library(sqlite3-static STATIC "${_root}/sqlite3.c")
        set_target_properties(sqlite3-static PROPERTIES OUTPUT_NAME sqlite3)
        target_include_directories(sqlite3-static PUBLIC "${_root}")
    endif()

    set(SQLITE_INCLUDE_DIRS "${_root}" PARENT_SCOPE)
    set(SQLITE_LIBRARIES sqlite3 PARENT_SCOPE)
endfunction()
