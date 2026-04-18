# TargetHelpers.cmake
#
# Helper functions for creating and configuring executable targets
# with consistent compile and link options.

# Function to reduce duplication when creating cpdn executable targets
# 
# Parameters:
#   target_name   - The CMake target name
#   target_src    - The source file with the main() function for the target
#   output_name   - The output executable name
#   compile_opts  - List of compile options (can be empty)
#   link_opts     - List of link options (can be empty)
#
function(add_cpdn_executable target_name target_src output_name compile_opts link_opts)
    add_executable(${target_name} ${target_src})
    set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${output_name})
    target_link_libraries(${target_name} PRIVATE
        cpdn_control
        cpdn_openifs_deps
    )
    
    if(compile_opts)
        set(_cpdn_compile_opts ${compile_opts})
        if(APPLE OR WIN32)
            list(REMOVE_ITEM _cpdn_compile_opts "-pthread")
        endif()
        if(_cpdn_compile_opts)
            target_compile_options(${target_name} PRIVATE ${_cpdn_compile_opts})
        endif()
    endif()
    
    if(link_opts)
        set(_cpdn_link_opts ${link_opts})
        if(APPLE OR WIN32)
            list(REMOVE_ITEM _cpdn_link_opts "-pthread")
        endif()
        if(_cpdn_link_opts)
            target_link_options(${target_name} PRIVATE ${_cpdn_link_opts})
        endif()
    endif()
endfunction()
