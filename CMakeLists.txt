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

# Export compile_commands.json for VS Code SonarCube linter extension (and other tools)
# so we can easily see what compilation commands will be used.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

# Require C++17 standard
# This will apply to ALL targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(CPDN_BUILD_UNIT_TESTS "Build the unit test executable and register unit tests." ON)
option(CPDN_BUILD_FUNCTIONAL_TESTS "Register functional tests." ON)
if (APPLE)
    set(_CPDN_REQUIRE_STATIC_BOINC_DEFAULT OFF)
else()
    set(_CPDN_REQUIRE_STATIC_BOINC_DEFAULT ON)
endif()
option(CPDN_REQUIRE_STATIC_BOINC "Require BOINC static libraries for supported deployment builds." ${_CPDN_REQUIRE_STATIC_BOINC_DEFAULT})
option(CPDN_LINK_RELEASE_STATIC "Link the Linux release controller executable statically." ON)
unset(_CPDN_REQUIRE_STATIC_BOINC_DEFAULT)

# Enforce 64-bit builds on Windows.
if (WIN32 AND CMAKE_SIZEOF_VOID_P LESS 8)
    message(FATAL_ERROR "32-bit Windows builds are not supported. Please use a 64-bit generator/toolchain.")
endif()

# Match the Windows vcpkg BOINC package, which is built with the static MSVC runtime.
if (MSVC AND WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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
    set(CPDN_RELEASE_COMPILE_OPTIONS /W3)
    set(CPDN_RELEASE_LINK_OPTIONS)
    set(CPDN_DEBUG_COMPILE_OPTIONS /W3)
    set(CPDN_DEBUG_LINK_OPTIONS)
else()
    set(CPDN_RELEASE_COMPILE_OPTIONS -g -Wall)
    set(CPDN_RELEASE_LINK_OPTIONS)
    set(CPDN_DEBUG_COMPILE_OPTIONS -fsanitize=address -ggdb3 -Wall)
    set(CPDN_DEBUG_LINK_OPTIONS -fsanitize=address)
    if (NOT APPLE)
        list(APPEND CPDN_RELEASE_COMPILE_OPTIONS -pthread)
        list(APPEND CPDN_DEBUG_COMPILE_OPTIONS -pthread)
        if (CPDN_LINK_RELEASE_STATIC)
            list(APPEND CPDN_RELEASE_LINK_OPTIONS -static)
        endif()
    endif()
endif()

include(cmake/GitHelpers.cmake)
include(cmake/TargetHelpers.cmake)

set(RELEASE_TARGET_NAME cpdn_release)
set(DEBUG_TARGET_NAME   cpdn_debug)

#--------------------------- Documentation -----------------------------------

find_program(CPDN_DOXYGEN_EXECUTABLE NAMES doxygen)
find_program(CPDN_DOT_EXECUTABLE NAMES dot)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen)

set(CPDN_DOXYGEN_HAVE_DOT NO)
if(CPDN_DOT_EXECUTABLE)
    set(CPDN_DOXYGEN_HAVE_DOT YES)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/doxygen/Doxyfile
    @ONLY
)

if(CPDN_DOXYGEN_EXECUTABLE)
    add_custom_target(doxygen
        COMMAND ${CPDN_DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxygen/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen
        COMMENT "Generating Doxygen documentation"
        VERBATIM
    )
else()
    add_custom_target(doxygen
        COMMAND ${CMAKE_COMMAND} -E echo "Doxygen executable not found. Install doxygen and rerun this target."
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Doxygen executable not available"
        VERBATIM
    )
endif()

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
add_subdirectory(third_party/fmt)

#--------------------------- Include paths -----------------------------------

# Add the source include dirs (local headers only)
set(LOCAL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/api
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11
)

#--------------------------- Model control sources -----------------------------------

add_subdirectory(models)

if (WIN32)
    set(CPDN_PROCESS_CONTROL_SOURCE ./lib/process_control_windows.cpp)
else()
    set(CPDN_PROCESS_CONTROL_SOURCE ./lib/process_control_posix.cpp)
endif()

#--------------------------- Libraries ---------------------------------------

# Unit tests need just these source files; create a library for them to link against.
add_library(cpdn_control 
        ./src/cpdn_control.cpp
        ./src/control_start.cpp
        ./src/parse_args.cpp
        ./src/upload_manager.cpp
        ${CPDN_PROCESS_CONTROL_SOURCE}
        ./lib/logging_utils.cpp
        ./lib/utils.cpp
        ./lib/cpdn_cpu_time.cpp
        ./api/trickle_handler.cpp
        ./api/progressfile_handler.cpp
)

# Add the model object files to the controller library
# (merges model helpers without creating extra archives per model)
target_sources(cpdn_control PRIVATE $<TARGET_OBJECTS:cpdn_models>)

# Add external header paths for boinc and cpdnzip.
# The vcpkg BOINC path supplies includes via imported targets, while the
# manual BOINC_DIR path still needs an explicit include directory here.
set(CPDN_EXTERNAL_INCLUDE_DIRS ${CPDNZIP_INCLUDE_DIR})
if(BOINC_INCLUDE_DIR)
    list(APPEND CPDN_EXTERNAL_INCLUDE_DIRS ${BOINC_INCLUDE_DIR})
endif()
target_include_directories(cpdn_control PUBLIC ${CPDN_EXTERNAL_INCLUDE_DIRS})

# Add local header paths (needed by executables that include project headers)
target_include_directories(cpdn_control PUBLIC ${LOCAL_INCLUDE_DIRS})

# Link the external libraries
# Marked PUBLIC so downstream executables inherit the requirement.
target_link_libraries(cpdn_control PUBLIC
                    ${BOINC_LINK_LIBS}
                    ${CPDNZIP_LIB}
                    Threads::Threads
                    fmt::fmt
)

# this executes git to retrieve the current hash of the branch and sets a macro the code uses.
get_git_hash(CODE_VERSION_HASH)
target_compile_definitions(cpdn_control PUBLIC
                    CODE_VERSION="${CODE_VERSION_HASH}"
                    CPDN_PLATFORM="${PLATFORM}"
)

#--------------------------- Executables -------------------------------------

# Configure release and debug targets using the helper function
# (If more executables are added, keep using add_cpdn_executable to stay consistent.)
add_cpdn_executable(${RELEASE_TARGET_NAME} ./src/cpdn_main.cpp ${RELEASE_OUTPUT_NAME} 
                    "${CPDN_RELEASE_COMPILE_OPTIONS}" 
                    "${CPDN_RELEASE_LINK_OPTIONS}")

add_cpdn_executable(${DEBUG_TARGET_NAME} ./src/cpdn_main.cpp ${DEBUG_OUTPUT_NAME}
                    "${CPDN_DEBUG_COMPILE_OPTIONS}" 
                    "${CPDN_DEBUG_LINK_OPTIONS}")

# Test model executable - standalone program for simulating the atmospheric model
add_executable(test_model ./models/test/test_model.cpp)

# Compile options for test_model (debug symbols and warnings)
if (MSVC)
    target_compile_options(test_model PRIVATE /W3)
else()
    target_compile_options(test_model PRIVATE -g -Wall)
endif()


#------- UNIT TESTS CONFIGURATION

#  Enable CTest support for the project
enable_testing()

# Include the unit tests
if (CPDN_BUILD_UNIT_TESTS)
    add_subdirectory(tests/unit)
endif()

# Include the functional tests
if (CPDN_BUILD_FUNCTIONAL_TESTS)
    add_subdirectory(tests/functional)
endif()
