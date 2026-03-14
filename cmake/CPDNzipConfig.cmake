# Helper to configure the CPDNZIP dependency for this project.
function(configure_cpdnzip)
    set(CPDNZIP_LIB_NAME "cpdn_zip")

    set(
        CPDNZIP_INCLUDE_DIR
        "zip/install/include"
        CACHE PATH "Path to CPDNZIP library headers."
    )
    set(
        CPDNZIP_LIB_DIR
        "zip/install/lib"
        CACHE PATH "Path to CPDNZIP library files."
    )

    find_library(CPDNZIP_LIB NAMES ${CPDNZIP_LIB_NAME} HINTS ${CPDNZIP_LIB_DIR})

    if (NOT CPDNZIP_LIB)
        message(
            FATAL_ERROR
            "Could not find CPDNZIP library ${CPDNZIP_LIB_NAME}. Check CPDNZIP_LIB_DIR."
        )
    endif()

    set(CPDNZIP_LIB ${CPDNZIP_LIB} PARENT_SCOPE)
    set(CPDNZIP_INCLUDE_DIR ${CPDNZIP_INCLUDE_DIR} PARENT_SCOPE)
    set(CPDNZIP_LIB_DIR ${CPDNZIP_LIB_DIR} PARENT_SCOPE)
endfunction()
