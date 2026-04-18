find_path(libaec_INCLUDE_DIR
    NAMES libaec.h)

set(_cpdn_saved_find_library_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
if(DEFINED libaec_USE_STATIC_LIBS AND libaec_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

find_library(libaec_LIBRARY
    NAMES aec libaec)

set(CMAKE_FIND_LIBRARY_SUFFIXES "${_cpdn_saved_find_library_suffixes}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libaec
    REQUIRED_VARS libaec_LIBRARY libaec_INCLUDE_DIR)

if(libaec_FOUND)
    set(libaec_LIBRARIES "${libaec_LIBRARY}")
    set(libaec_INCLUDE_DIRS "${libaec_INCLUDE_DIR}")

    if(NOT TARGET libaec::aec)
        add_library(libaec::aec UNKNOWN IMPORTED)
        set_target_properties(libaec::aec PROPERTIES
            IMPORTED_LOCATION "${libaec_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${libaec_INCLUDE_DIR}")
    endif()
endif()

mark_as_advanced(libaec_INCLUDE_DIR libaec_LIBRARY)
