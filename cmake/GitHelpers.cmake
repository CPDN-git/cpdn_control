# Retrieves the short hash of the current repository HEAD.
# Usage: get_git_hash(<out-var>)
function(get_git_hash OUT_VAR)
    if(NOT OUT_VAR)
        message(FATAL_ERROR "get_git_hash requires an output variable name.")
    endif()

    set(DEFAULT_VALUE "unknown")
    execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE _git_result
        OUTPUT_VARIABLE _git_hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(_git_result EQUAL 0 AND _git_hash)
        string(SUBSTRING "${_git_hash}" 0 8 _git_hash_short)
        set(${OUT_VAR} "${_git_hash_short}" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "${DEFAULT_VALUE}" PARENT_SCOPE)
    endif()
endfunction()
