#  CMakeLists.txt for cpdn_controller
#
#  Sets up the build configuration for the CPDN Controller app,
#  and linking against the BOINC and CPDNZIP libraries.
#
#  Steps:
#    1. mkdir build (or remove it for fresh build).
#    2. cd build
#    3. cmake ..
#    4. cmake --build . --verbose (omit --verbose if not interested in compile commands)
#    5. ctest -V    (run unit tests, -V is verbose, optional)
#
#    Final executables can be found in the 'build' subdir.
#
#  Structure of a CMakeLists.txt file broadly follows:
#    1. Set up variables and configurations.
#    2. Create targets: add_library(), add_library().
#    3. Apply properties to targets: target_link_options(), target_compile_options().
#
#      Glenn Carver, CPDN, Oct, 2025.


cmake_minimum_required(VERSION 3.16)

project(cpdn_control VERSION 1.0.0  DESCRIPTION "CPDN BOINC task controller"  LANGUAGES C CXX)

# Require C++17 standard
# This will apply to ALL targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Configuration
set(PLATFORM  "x86_64-pc-linux-gnu"  CACHE STRING "Platform and operating system.")

set(RELEASE_OUTPUT_NAME "cpdn_control_${CMAKE_PROJECT_VERSION}_${PLATFORM}")
set(DEBUG_OUTPUT_NAME   "cpdn_control_${CMAKE_PROJECT_VERSION}_${PLATFORM}-debug")

# Compiler configuration
set(RELEASE_COMPILE_OPTIONS -g -pthread -Wall CACHE STRING "Compile options for release target.")
set(RELEASE_LINK_OPTIONS    -static           CACHE STRING "Linker options for release target.")
set(DEBUG_COMPILE_OPTIONS   -fsanitize=address -ggdb3 -pthread -Wall CACHE STRING "Compile options for debug target.")
set(DEBUG_LINK_OPTIONS      -fsanitize=address                       CACHE STRING "Linker options for debug target.")

include(cmake/GitHelpers.cmake)
include(cmake/TargetHelpers.cmake)

set(RELEASE_TARGET_NAME cpdn_release)
set(DEBUG_TARGET_NAME   cpdn_debug)

#--------------------------- Dependencies -----------------------------------

# --- CONFIGURATION FOR BOINC LIBRARY (headers & lib)
include(cmake/BoincConfig.cmake)
set(BOINC_DIR "../boinc-8.0.2-x86_64" CACHE STRING "Root directory of BOINC install." ) 
configure_boinc(${BOINC_DIR})

# --- CONFIGURATION FOR CPDNZIP LIBRARY (headers & lib)
include(cmake/CPDNzipConfig.cmake)
configure_cpdnzip()

# Other packages required
find_package(Threads REQUIRED)

#--------------------------- Include paths -----------------------------------

# Add the source include dirs (local headers only)
set(LOCAL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/api
)

#--------------------------- Model sources -----------------------------------

add_subdirectory(models)

#--------------------------- Libraries ---------------------------------------

# Unit tests need just these source files; create a library for them to link against.
add_library(cpdn_control 
        ./src/cpdn_control.cpp
        ./lib/utils.cpp
        ./lib/cpdn_linux_cpu_time.cpp
        ./api/trickle_handler.cpp
)

# Add the model object files to the controller library
# (merges model helpers without creating extra archives per model)
target_sources(cpdn_control PRIVATE $<TARGET_OBJECTS:cpdn_models>)

# Add external header paths for boinc and cpdnzip
# These paths are PUBLIC because the main code needs them to compile.
target_include_directories(cpdn_control PUBLIC 
                    ${BOINC_INCLUDE_DIR}
                    ${CPDNZIP_INCLUDE_DIR}
)

# Add local header paths (needed by executables that include project headers)
target_include_directories(cpdn_control PUBLIC ${LOCAL_INCLUDE_DIRS})

# Link the external libraries
# Marked PUBLIC so downstream executables inherit the requirement.
target_link_libraries(cpdn_control PUBLIC
                    ${BOINC_API} ${BOINC_LIB}
                    ${CPDNZIP_LIB}
)

# this executes git to retrieve the current hash of the branch and sets a macro the code uses.
get_git_hash(CODE_VERSION_HASH)
target_compile_definitions(cpdn_control PUBLIC CODE_VERSION="${CODE_VERSION_HASH}")

#--------------------------- Executables -------------------------------------

# Configure release and debug targets using the helper function
# (If more executables are added, keep using add_cpdn_executable to stay consistent.)
add_cpdn_executable(${RELEASE_TARGET_NAME} ./src/cpdn_main.cpp ${RELEASE_OUTPUT_NAME} 
                    "${RELEASE_COMPILE_OPTIONS}" 
                    "${RELEASE_LINK_OPTIONS}")

add_cpdn_executable(${DEBUG_TARGET_NAME} ./src/cpdn_main.cpp ${DEBUG_OUTPUT_NAME}
                    "${DEBUG_COMPILE_OPTIONS}" 
                    "${DEBUG_LINK_OPTIONS}")

# Test model executable - standalone program for simulating the atmospheric model
add_executable(test_model ./models/test/test_model.cpp)

# Compile options for test_model (debug symbols and warnings)
target_compile_options(test_model PRIVATE -g -Wall)


#------- UNIT TESTS CONFIGURATION

#  Enable CTest support for the project
enable_testing()

# Include the unit tests
add_subdirectory(tests/unit)

# Include the functional tests
add_subdirectory(tests/functional)
