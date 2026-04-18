include_guard(GLOBAL)

option(CPDN_ENABLE_ECCODES "Enable external ecCodes package support for OpenIFS." OFF)

if(NOT TARGET cpdn_openifs_deps)
    add_library(cpdn_openifs_deps INTERFACE)
endif()

function(configure_eccodes)
    if(NOT CPDN_ENABLE_ECCODES)
        return()
    endif()

    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR
            "CPDN_ENABLE_ECCODES currently supports Linux only. "
            "Configure with -DCPDN_ENABLE_ECCODES=OFF on Windows or macOS until ecCodes support is implemented there.")
    endif()

    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

    # Linux ecCodes static exports may still reference libaec::aec transitively.
    set(libaec_USE_STATIC_LIBS ON)
    find_package(libaec REQUIRED)

    find_package(eccodes CONFIG REQUIRED)

    if(TARGET eccodes)
        set(_cpdn_eccodes_target eccodes)
    elseif(TARGET eccodes::eccodes)
        set(_cpdn_eccodes_target eccodes::eccodes)
    else()
        message(FATAL_ERROR
            "find_package(eccodes CONFIG) succeeded, but no usable ecCodes target was created. "
            "Point eccodes_DIR at the installed ecCodes package directory, or add the install prefix to CMAKE_PREFIX_PATH.")
    endif()

    if(NOT TARGET cpdn_eccodes)
        add_library(cpdn_eccodes INTERFACE)
    endif()

    target_link_libraries(cpdn_eccodes INTERFACE
        ${_cpdn_eccodes_target}
        libaec::aec
    )

    target_link_libraries(cpdn_openifs_deps INTERFACE cpdn_eccodes)
endfunction()
