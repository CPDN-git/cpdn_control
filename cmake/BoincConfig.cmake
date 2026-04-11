# Helper to configure the BOINC dependency for this project.
function(configure_boinc boinc_dir)
    if(CPDN_USE_BOINC_STUBS)
        set(BOINC_INCLUDE_DIR
            "${CMAKE_SOURCE_DIR}/cmake/boinc_stub/include"
            CACHE PATH "Path to compile-only BOINC stub headers." FORCE
        )
        set(BOINC_LIB "" PARENT_SCOPE)
        set(BOINC_API "" PARENT_SCOPE)
        set(BOINC_INCLUDE_DIR ${BOINC_INCLUDE_DIR} PARENT_SCOPE)
        set(BOINC_LIB_DIR "" PARENT_SCOPE)
        message(STATUS "Using compile-only BOINC stub headers")
        return()
    endif()

    set(BOINC_LIB_NAME "boinc")
    set(BOINC_API_NAME "boinc_api")

    set(BOINC_INCLUDE_DIR
        "${boinc_dir}/include"
        CACHE PATH "Path to BOINC headers."
    )
    set(BOINC_LIB_DIR
        "${boinc_dir}/lib"
        CACHE PATH "Path to BOINC libraries."
    )

    # Prefer static libraries on Linux; allow shared on macOS where static may be unavailable.
    set(_BOINC_OLD_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    if(APPLE)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib" ".a")
    elseif(WIN32)
        # Prefer MSVC/MinGW import/static libs by default on Windows
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a")
    else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    endif()
    find_library(BOINC_LIB NAMES ${BOINC_LIB_NAME} HINTS ${BOINC_LIB_DIR})
    find_library(BOINC_API NAMES ${BOINC_API_NAME} HINTS ${BOINC_LIB_DIR})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_BOINC_OLD_SUFFIXES})

    if (NOT BOINC_LIB)
        message(FATAL_ERROR
            "Could not find BOINC library ${BOINC_LIB_NAME}. Check BOINC_LIB_DIR." )
    endif()

    if (NOT BOINC_API)
        message(FATAL_ERROR
            "Could not find BOINC API library ${BOINC_API_NAME}. Check BOINC_LIB_DIR." )
    endif()

    # Promote variables so the parent scope can use them when linking.
    set(BOINC_LIB ${BOINC_LIB} PARENT_SCOPE)
    set(BOINC_API ${BOINC_API} PARENT_SCOPE)
    set(BOINC_INCLUDE_DIR ${BOINC_INCLUDE_DIR} PARENT_SCOPE)
    set(BOINC_LIB_DIR ${BOINC_LIB_DIR} PARENT_SCOPE)
endfunction()
