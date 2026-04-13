# Helper to configure the BOINC dependency for this project.

function(_cpdn_get_imported_library_location target_name out_var)
    set(${out_var} "" PARENT_SCOPE)

    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(_aliased_target ${target_name} ALIASED_TARGET)
    if(_aliased_target)
        set(_target_to_query ${_aliased_target})
    else()
        set(_target_to_query ${target_name})
    endif()

    set(_cpdn_location_properties
        IMPORTED_LOCATION_RELEASE
        IMPORTED_LOCATION_RELWITHDEBINFO
        IMPORTED_LOCATION_MINSIZEREL
        IMPORTED_LOCATION_DEBUG
        IMPORTED_LOCATION_NOCONFIG
        IMPORTED_LOCATION
    )

    if(WIN32)
        list(PREPEND _cpdn_location_properties
            IMPORTED_IMPLIB_RELEASE
            IMPORTED_IMPLIB_RELWITHDEBINFO
            IMPORTED_IMPLIB_MINSIZEREL
            IMPORTED_IMPLIB_DEBUG
            IMPORTED_IMPLIB_NOCONFIG
            IMPORTED_IMPLIB
        )
    endif()

    foreach(_property_name IN LISTS _cpdn_location_properties)
        get_target_property(_candidate ${_target_to_query} ${_property_name})
        if(_candidate AND NOT _candidate MATCHES "-NOTFOUND$")
            set(${out_var} ${_candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

function(_cpdn_get_imported_include_dirs target_name out_var)
    set(${out_var} "" PARENT_SCOPE)

    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(_aliased_target ${target_name} ALIASED_TARGET)
    if(_aliased_target)
        set(_target_to_query ${_aliased_target})
    else()
        set(_target_to_query ${target_name})
    endif()

    get_target_property(_cpdn_include_dirs ${_target_to_query} INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _cpdn_include_dirs OR _cpdn_include_dirs MATCHES "-NOTFOUND$")
        return()
    endif()

    set(_cpdn_normalized_include_dirs)
    foreach(_cpdn_include_dir IN LISTS _cpdn_include_dirs)
        if(NOT _cpdn_include_dir)
            continue()
        endif()

        get_filename_component(_cpdn_include_leaf "${_cpdn_include_dir}" NAME)
        if(_cpdn_include_leaf STREQUAL "boinc")
            get_filename_component(_cpdn_include_dir "${_cpdn_include_dir}" DIRECTORY)
        endif()

        list(APPEND _cpdn_normalized_include_dirs "${_cpdn_include_dir}")
    endforeach()

    list(REMOVE_DUPLICATES _cpdn_normalized_include_dirs)
    set(${out_var} "${_cpdn_normalized_include_dirs}" PARENT_SCOPE)
endfunction()

function(_cpdn_guess_package_include_dirs_from_library library_path out_var)
    set(${out_var} "" PARENT_SCOPE)

    if(NOT library_path)
        return()
    endif()

    get_filename_component(_cpdn_library_dir "${library_path}" DIRECTORY)
    set(_cpdn_candidate_roots
        "${_cpdn_library_dir}/.."
        "${_cpdn_library_dir}/../.."
        "${_cpdn_library_dir}/../../.."
    )

    set(_cpdn_candidate_include_dirs)
    foreach(_cpdn_candidate_root IN LISTS _cpdn_candidate_roots)
        get_filename_component(_cpdn_candidate_root "${_cpdn_candidate_root}" ABSOLUTE)
        if(EXISTS "${_cpdn_candidate_root}/include/boinc/boinc_api.h")
            list(APPEND _cpdn_candidate_include_dirs "${_cpdn_candidate_root}/include")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _cpdn_candidate_include_dirs)
    set(${out_var} "${_cpdn_candidate_include_dirs}" PARENT_SCOPE)
endfunction()

function(_cpdn_library_is_static library_path out_var)
    if(NOT library_path)
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()

    get_filename_component(_cpdn_library_ext "${library_path}" EXT)
    string(TOLOWER "${_cpdn_library_ext}" _cpdn_library_ext)
    if(_cpdn_library_ext STREQUAL ".a" OR _cpdn_library_ext STREQUAL ".lib")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(_cpdn_runtime_dir_for_library library_path out_var)
    if(NOT library_path)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    _cpdn_library_is_static("${library_path}" _cpdn_is_static_library)
    if(_cpdn_is_static_library)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    get_filename_component(_cpdn_runtime_dir "${library_path}" DIRECTORY)
    set(${out_var} "${_cpdn_runtime_dir}" PARENT_SCOPE)
endfunction()

function(_cpdn_use_boinc_package out_found_var out_message_var)
    set(${out_found_var} FALSE PARENT_SCOPE)
    set(${out_message_var} "" PARENT_SCOPE)

    find_package(boinc CONFIG QUIET)
    if(NOT boinc_FOUND)
        return()
    endif()

    set(_cpdn_boinc_target unofficial::boinc::boinc)
    set(_cpdn_boinc_api_target unofficial::boinc::boincapi)
    if(NOT TARGET ${_cpdn_boinc_target} OR NOT TARGET ${_cpdn_boinc_api_target})
        set(${out_message_var} "BOINC package config did not define the expected imported targets" PARENT_SCOPE)
        return()
    endif()

    _cpdn_get_imported_library_location(${_cpdn_boinc_target} _cpdn_boinc_location)
    _cpdn_get_imported_library_location(${_cpdn_boinc_api_target} _cpdn_boinc_api_location)
    _cpdn_get_imported_include_dirs(${_cpdn_boinc_target} _cpdn_boinc_include_dirs)
    if(NOT _cpdn_boinc_include_dirs)
        _cpdn_get_imported_include_dirs(${_cpdn_boinc_api_target} _cpdn_boinc_include_dirs)
    endif()
    if(NOT _cpdn_boinc_include_dirs)
        _cpdn_guess_package_include_dirs_from_library("${_cpdn_boinc_api_location}" _cpdn_boinc_include_dirs)
    endif()
    if(NOT _cpdn_boinc_include_dirs)
        _cpdn_guess_package_include_dirs_from_library("${_cpdn_boinc_location}" _cpdn_boinc_include_dirs)
    endif()

    if(CPDN_REQUIRE_STATIC_BOINC)
        _cpdn_library_is_static("${_cpdn_boinc_location}" _cpdn_boinc_is_static)
        _cpdn_library_is_static("${_cpdn_boinc_api_location}" _cpdn_boinc_api_is_static)
        if(NOT _cpdn_boinc_is_static OR NOT _cpdn_boinc_api_is_static)
            set(${out_message_var}
                "BOINC package config resolved to non-static libraries; falling back to BOINC_DIR search"
                PARENT_SCOPE
            )
            return()
        endif()
    endif()

    if(NOT _cpdn_boinc_include_dirs)
        set(${out_message_var}
            "BOINC package config did not expose usable include directories; falling back to BOINC_DIR search"
            PARENT_SCOPE
        )
        return()
    endif()

    _cpdn_runtime_dir_for_library("${_cpdn_boinc_api_location}" _cpdn_runtime_dir)
    if(NOT _cpdn_runtime_dir)
        _cpdn_runtime_dir_for_library("${_cpdn_boinc_location}" _cpdn_runtime_dir)
    endif()

    set(BOINC_INCLUDE_DIR "${_cpdn_boinc_include_dirs}" PARENT_SCOPE)
    set(BOINC_LIB_DIR "" PARENT_SCOPE)
    set(BOINC_RUNTIME_LIB_DIR "${_cpdn_runtime_dir}" PARENT_SCOPE)
    set(BOINC_LIB ${_cpdn_boinc_target} PARENT_SCOPE)
    set(BOINC_API ${_cpdn_boinc_api_target} PARENT_SCOPE)
    set(BOINC_LINK_LIBS
        ${_cpdn_boinc_api_target}
        ${_cpdn_boinc_target}
        PARENT_SCOPE
    )
    set(${out_found_var} TRUE PARENT_SCOPE)
    set(${out_message_var} "Using BOINC package from CMake package config" PARENT_SCOPE)
endfunction()

function(configure_boinc boinc_dir)
    _cpdn_use_boinc_package(_cpdn_have_boinc_package _cpdn_boinc_message)
    if(_cpdn_boinc_message)
        message(STATUS "${_cpdn_boinc_message}")
    endif()
    if(_cpdn_have_boinc_package)
        # _cpdn_use_boinc_package() sets these in this function scope.
        # Promote them again so the caller's directory scope sees the resolved
        # package targets, include dirs, and runtime-dir metadata.
        set(BOINC_INCLUDE_DIR ${BOINC_INCLUDE_DIR} PARENT_SCOPE)
        set(BOINC_LIB_DIR ${BOINC_LIB_DIR} PARENT_SCOPE)
        set(BOINC_RUNTIME_LIB_DIR ${BOINC_RUNTIME_LIB_DIR} PARENT_SCOPE)
        set(BOINC_LIB ${BOINC_LIB} PARENT_SCOPE)
        set(BOINC_API ${BOINC_API} PARENT_SCOPE)
        set(BOINC_LINK_LIBS ${BOINC_LINK_LIBS} PARENT_SCOPE)
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

    if(CPDN_REQUIRE_STATIC_BOINC)
        _cpdn_library_is_static("${BOINC_LIB}" _cpdn_manual_boinc_is_static)
        _cpdn_library_is_static("${BOINC_API}" _cpdn_manual_boinc_api_is_static)
        if(NOT _cpdn_manual_boinc_is_static OR NOT _cpdn_manual_boinc_api_is_static)
            message(FATAL_ERROR
                "Static BOINC libraries are required, but BOINC_DIR resolved to non-static BOINC libraries."
            )
        endif()
    endif()

    _cpdn_runtime_dir_for_library("${BOINC_API}" _cpdn_runtime_dir)
    if(NOT _cpdn_runtime_dir)
        _cpdn_runtime_dir_for_library("${BOINC_LIB}" _cpdn_runtime_dir)
    endif()

    # Promote variables so the parent scope can use them when linking.
    set(BOINC_LIB ${BOINC_LIB} PARENT_SCOPE)
    set(BOINC_API ${BOINC_API} PARENT_SCOPE)
    set(BOINC_LINK_LIBS ${BOINC_API} ${BOINC_LIB} PARENT_SCOPE)
    set(BOINC_INCLUDE_DIR ${BOINC_INCLUDE_DIR} PARENT_SCOPE)
    set(BOINC_LIB_DIR ${BOINC_LIB_DIR} PARENT_SCOPE)
    set(BOINC_RUNTIME_LIB_DIR ${_cpdn_runtime_dir} PARENT_SCOPE)
endfunction()
