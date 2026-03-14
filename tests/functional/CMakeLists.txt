# CMakeLists.txt for the functional tests
#
#     Glenn Carver, CPDN, 2025.
#
#  Configurations for the tests can be found in the fixtures directory.
#  We use a ctest 'label' for better control of running the tests.
#  'ctest -L functional' will just run these tests.
#  'ctest -LE functional' will exclude these tests from any other tests.
#

if (NOT DEFINED BOINC_LIB_DIR)
    if (DEFINED BOINC_DIR)
        set(BOINC_LIB_DIR "${BOINC_DIR}/lib")
    else()
        message(FATAL_ERROR "BOINC_LIB_DIR is not set. Configure from the repo root so BOINC_DIR/BOINC_LIB_DIR are available.")
    endif()
endif()

function(add_functional_test TEST_NAME FIXTURE_REL)
    # Working directory per test
    set(TEST_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME})
    file(MAKE_DIRECTORY ${TEST_WORK_DIR})
    file(MAKE_DIRECTORY ${TEST_WORK_DIR}/fixtures)

    # Copy fixture into test workdir
    get_filename_component(FIXTURE_NAME ${FIXTURE_REL} NAME)
    set(TEST_FIXTURE ${TEST_WORK_DIR}/fixtures/${FIXTURE_NAME})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${FIXTURE_REL} ${TEST_FIXTURE} COPYONLY)

    # Setup
    add_test(
        NAME ${TEST_NAME}_Setup
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/setup_test.py ${TEST_FIXTURE}
        WORKING_DIRECTORY ${TEST_WORK_DIR}
    )

    # Execute
    add_test(
        NAME ${TEST_NAME}
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/run.py
                --config ${TEST_FIXTURE}
                --build-dir ${CMAKE_BINARY_DIR}
                --boinc-lib-dir ${BOINC_LIB_DIR}
        WORKING_DIRECTORY ${TEST_WORK_DIR}
    )

    # Validate
    add_test(
        NAME ${TEST_NAME}_Validate
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/validate_test.py ${TEST_FIXTURE}
        WORKING_DIRECTORY ${TEST_WORK_DIR}
    )

    # Dependencies, env, labels
    if (WIN32)
        set(_cpdn_path_sep "\\;")
    else()
        set(_cpdn_path_sep ":")
    endif()
    set(_cpdn_env_vars
        "LD_LIBRARY_PATH=${BOINC_LIB_DIR}:$ENV{LD_LIBRARY_PATH}"
        "DYLD_LIBRARY_PATH=${BOINC_LIB_DIR}:$ENV{DYLD_LIBRARY_PATH}"
        "PATH=${BOINC_LIB_DIR}${_cpdn_path_sep}$ENV{PATH}"
        "CPDN_PLATFORM=${PLATFORM}"
    )

    set_tests_properties(${TEST_NAME} PROPERTIES
        DEPENDS ${TEST_NAME}_Setup
        ENVIRONMENT "${_cpdn_env_vars}")
    set_tests_properties(${TEST_NAME}_Validate PROPERTIES
        DEPENDS ${TEST_NAME})
    set_tests_properties(${TEST_NAME} ${TEST_NAME}_Setup ${TEST_NAME}_Validate PROPERTIES
        ENVIRONMENT "${_cpdn_env_vars}"
        LABELS "functional")
endfunction()

add_functional_test(FTest1 fixtures/ftest1.json)
