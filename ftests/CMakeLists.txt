# CMakeLists.txt for the funcational tests
#
#     Glenn Carver, CPDN, 2025.
#
#  Configurations for the tests can be found in the fixtures directory.
#  We use a ctest 'label' for better control of running the tests.
#  'ctest -L functional' will just run these tests.
#  'ctest -E functional' will exclude these tests from any other tests.
#

set(TEST_NAME test1)
set(TEST_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME})
set(TEST_FIXTURE ${TEST_WORK_DIR}/fixtures/ftest1.json)
set(BOINC_LIB_DIR "${CMAKE_SOURCE_DIR}/../boinc-8.0.2-x86_64/lib")

file(MAKE_DIRECTORY ${TEST_WORK_DIR})
file(MAKE_DIRECTORY ${TEST_WORK_DIR}/fixtures)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fixtures/ftest1.json ${TEST_FIXTURE} COPYONLY)

# Test 1 Setup
add_test(
    NAME FTest1_Setup
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/setup_test.py ${TEST_FIXTURE}
    WORKING_DIRECTORY ${TEST_WORK_DIR}
)

# Test 1 Execute
add_test(
    NAME FTest1
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/run.py
            --config ${TEST_FIXTURE}
            --build-dir ${CMAKE_BINARY_DIR}
            --boinc-lib-dir ${BOINC_LIB_DIR}
    WORKING_DIRECTORY ${TEST_WORK_DIR}
)

# Test 1 Validate
add_test(
    NAME FTest1_Validate
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/validate_test.py ${TEST_FIXTURE}
    WORKING_DIRECTORY ${TEST_WORK_DIR}
)

# Set dependencies and environment
set_tests_properties(FTest1 PROPERTIES
    DEPENDS FTest1_Setup
    ENVIRONMENT "LD_LIBRARY_PATH=${BOINC_LIB_DIR}:$ENV{LD_LIBRARY_PATH}")
set_tests_properties(FTest1_Validate PROPERTIES
    DEPENDS FTest1)

# Add label to each test to enable control by ctest -L functional
set_tests_properties(FTest1 FTest1_Setup FTest1_Validate PROPERTIES LABELS "functional")

# Placeholder teardown (kept commented to allow inspection of artifacts)
# add_test(NAME FTest1_Teardown ... )
