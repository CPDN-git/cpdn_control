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

# Export compile_commands.json for VS Code SonarCube linter extension (and obther tools).
# And so we can easily see exactly what compilation commands will be used.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

# Require C++17 standard
# This will apply to ALL targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enforce 64-bit builds on Windows.
if (WIN32 AND CMAKE_SIZEOF_VOID_P LESS 8)
    message(FATAL_ERROR "32-bit Windows builds are not supported. Please use a 64-bit generator/toolchain.")
endif()

# Configuration
# Derive a sensible default platform triplet but allow overrides.
if (NOT DEFINED PLATFORM)
    set(_CPDN_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    if (APPLE)
        if (_CPDN_ARCH STREQUAL "arm64" OR _CPDN_ARCH STREQUAL "aarch64")
            set(_CPDN_ARCH "arm64")
        endif()
        set(_CPDN_OS "apple-darwin")
    elseif (WIN32)
        if (_CPDN_ARCH STREQUAL "AMD64" OR _CPDN_ARCH STREQUAL "x86_64")
            set(_CPDN_ARCH "x86_64")
        endif()
        set(_CPDN_OS "pc-windows-msvc")
    elseif (UNIX)
        if (_CPDN_ARCH STREQUAL "aarch64")
            set(_CPDN_ARCH "aarch64")
        else()
            set(_CPDN_ARCH "x86_64")
        endif()
        set(_CPDN_OS "pc-linux-gnu")
    else()
        set(_CPDN_OS "${CMAKE_SYSTEM_NAME}")
    endif()
    set(PLATFORM "${_CPDN_ARCH}-${_CPDN_OS}" CACHE STRING "Platform and operating system.")
    unset(_CPDN_ARCH)
    unset(_CPDN_OS)
endif()

set(RELEASE_OUTPUT_NAME "cpdn_control_${CMAKE_PROJECT_VERSION}_${PLATFORM}")
set(DEBUG_OUTPUT_NAME   "cpdn_control_${CMAKE_PROJECT_VERSION}_${PLATFORM}-debug")

# Compiler configuration
if (MSVC)
    set(RELEASE_COMPILE_OPTIONS /W3 CACHE STRING "Compile options for release target.")
    set(RELEASE_LINK_OPTIONS    ""   CACHE STRING "Linker options for release target.")
    set(DEBUG_COMPILE_OPTIONS   /W3 CACHE STRING "Compile options for debug target.")
    set(DEBUG_LINK_OPTIONS      ""   CACHE STRING "Linker options for debug target.")
else()
    set(RELEASE_COMPILE_OPTIONS -g -Wall CACHE STRING "Compile options for release target.")
    set(RELEASE_LINK_OPTIONS    ""     CACHE STRING "Linker options for release target.")
    set(DEBUG_COMPILE_OPTIONS   -fsanitize=address -ggdb3 -Wall CACHE STRING "Compile options for debug target.")
    set(DEBUG_LINK_OPTIONS      -fsanitize=address      CACHE STRING "Linker options for debug target.")
    if (NOT APPLE)
        list(APPEND RELEASE_COMPILE_OPTIONS -pthread)
        list(APPEND DEBUG_COMPILE_OPTIONS -pthread)
        if (NOT RELEASE_LINK_OPTIONS)
            set(RELEASE_LINK_OPTIONS -static)
        endif()
    endif()
endif()

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

# --- CONFIGURATION FOR FMT LIBRARY (vendored source)
# Vendored fmt includes a small local CMake compatibility patch so it still
# configures on hosts limited to CMake 3.16.
set(FMT_DOC OFF CACHE BOOL "Disable fmt documentation targets." FORCE)
set(FMT_TEST OFF CACHE BOOL "Disable fmt test targets." FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install targets." FORCE)
add_subdirectory(tools/fmt)

#--------------------------- Include paths -----------------------------------

# Add the source include dirs (local headers only)
set(LOCAL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/api
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/CLI11
)

#--------------------------- Model control sources -----------------------------------

add_subdirectory(models)

#--------------------------- Libraries ---------------------------------------

# Unit tests need just these source files; create a library for them to link against.
add_library(cpdn_control 
        ./src/cpdn_control.cpp
        ./src/external_diagnostics.cpp
        ./src/parse_args.cpp
        ./lib/logging_utils.cpp
        ./lib/utils.cpp
        ./lib/cpdn_cpu_time.cpp
        ./api/trickle_handler.cpp
        ./api/progressfile_handler.cpp
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
                    Threads::Threads
                    fmt::fmt
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
