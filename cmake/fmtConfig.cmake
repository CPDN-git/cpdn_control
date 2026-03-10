# Helper to configure the fmt dependency for this project.
# fmt is vendored in tools/fmt/ and has been built as a static library

function(configure_fmt)
    set(FMT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt/include")
    set(FMT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt/lib")
    
    # Find the compiled fmt library
    find_library(FMT_LIB NAMES fmt HINTS ${FMT_LIB_DIR} REQUIRED)

    # Create an interface library target for fmt
    if (NOT TARGET fmt::fmt)
        add_library(fmt::fmt INTERFACE IMPORTED)
        set_target_properties(fmt::fmt PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FMT_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${FMT_LIB}"
        )
    endif()

    set(FMT_INCLUDE_DIR ${FMT_INCLUDE_DIR} PARENT_SCOPE)
    set(FMT_LIB ${FMT_LIB} PARENT_SCOPE)
endfunction()
