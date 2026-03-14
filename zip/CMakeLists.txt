# CMakeLists.txt for cpdn_zip and its test program
# ================================================
#
#   Glenn Carver, assisted by Gemini 2.5 Pro.
#   Oct/2025.
#
#  Purpose
#  -------
#  This code was developed to replace the old boinc_zip and boinc_unzip
#  functions supplied with boinc. This old code is not memory safe.
#
#  ZipLib
#  ------
#  This project uses ZipLib downloaded from: https://github.com/DreamyCecil/ZipLib.
#  It's a modern C++ lightweight library based on STL streams.
#  To prepare ZipLib, download to a temporary location, and copy the 'Source/ZipLib' directory 
#  to 'ZipLib' in this directory. Also copy the README.md and LICENSE files for reference.
#  Fix: edit ZipLib/CMakeLists.txt and remove the line: 'target_compile_features(ZipLib_Sample PRIVATE cxx_std_11)'
#
#  cpdn_zip library
#  -----------------
#  This is a simple wrapper around ZipLib to provide cpdn_zip and cpdn_unzip functions.
#  It is built as a static library, and then combined with ZipLib and its dependencies.
#  cpdn_zip and unzip use the zip library by default but as ZipLib supports bzip2 & lzma
#  we can also be included.
#
# To build and run:
# 1. cd ../cpdn_control/zip
# 2. mkdir build
# 3. cd build
#    Configure with your desired installation path
# 4. cmake -DCMAKE_INSTALL_PREFIX=../install ..
# 5. make
# 6. make install
# 7. ./test_zip
# 8. In cpdn_control, make sure Makefile uses ../zip/install/lib and ../zip/install/include
# 9. make clean; make
#
# For cpdn_zip, use 'make clean' in the build directory or simply delete the zip/build and zip/install directories.

cmake_minimum_required(VERSION 3.16)

project(CpdnZip VERSION 1.0 LANGUAGES C CXX)

# Set the C++ standard to 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- ZipLib ---
# Add the downloaded ZipLib source as a subdirectory.
# This will create the 'ZipLib' target.
# The 'EXCLUDE_FROM_ALL' option prevents it from being built by default,
# but it will be built as a dependency of our test program.
add_subdirectory(ZipLib EXCLUDE_FROM_ALL)

# Disable LZMA support as not needed but retain (the default) & bzip2 (for testing).
# Using PUBLIC ensures this definition is inherited by any target that links to ZipLib.
# TODO: this doesn't seem to work; LZMA code is still built and added to libcpdn_zip.a?
target_compile_definitions(ZipLib PUBLIC ZIPLIB_NO_LZMA)

# --- cpdn_zip ---
# Define cpdn_zip library as a simple static library.
# It will be created, but we will replace it later by including all the ZipLib objects
add_library(cpdn_zip cpdn_zip.cpp)

# Explicitly add the current directory to the include path for cpdn_zip.
# This allows the compiler to find "ZipLib/ZipFile.h". The CMakeFiles that come
# with ZipLib do not do this automatically.
#target_include_directories(cpdn_zip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ZipLib)

# Link it against ZipLib to establish the dependency chain.
target_link_libraries(cpdn_zip PUBLIC ZipLib)

# --- Test Programs ---
# Define the test executables and link it against our library.
add_executable(test_zip test_zip.cpp)
add_executable(test_unzip test_unzip.cpp)
target_link_libraries(test_zip PRIVATE cpdn_zip)
target_link_libraries(test_unzip PRIVATE cpdn_zip)

# --- Combined Library Creation ---
# Add a custom command that runs AFTER the build is complete. 
# It finds all the static libraries (cpdn_zip, ZipLib, zlib, bzip2) 
# and combines them into a single, final libcpdn_zip.a in the build dir.

# Do NOT put {} inside the script, as CMake will misinterpret them.
set(COMBINE_SCRIPT "
#!/bin/bash
set -e
echo 'Creating combined static library libcpdn_zip.a...'
[ -d temp_lib_objs ] && rm -rf temp_lib_objs
mkdir temp_lib_objs

# For each archive, create a unique subdirectory and extract objects there
for lib in $(find . -name '*.a'); do
    lib_name=$(basename $lib .a); echo $lib_name
    mkdir -p temp_lib_objs/$lib_name
    cd temp_lib_objs/$lib_name
    ${CMAKE_AR} x ../../$lib
    cd ../..
done

# Create the new library from ALL object files in ALL subdirectories
${CMAKE_AR} cr libcpdn_zip.a temp_lib_objs/*/*.o
ranlib libcpdn_zip.a
rm -rf temp_lib_objs
")

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/combine_libs.sh "${COMBINE_SCRIPT}")

# Add the custom command to run the script just created.
add_custom_command(
            TARGET test_zip POST_BUILD
            COMMAND bash ${CMAKE_CURRENT_BINARY_DIR}/combine_libs.sh
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Bundling all static libraries into a single libcpdn_zip.a"
)

# --- Install Rules ---
# This section defines what happens when 'make install' is run.

#  Install the final combined static library
install(TARGETS cpdn_zip
        ARCHIVE DESTINATION lib
)

# Install the public header file for our library
install(FILES cpdn_zip.h
        DESTINATION include
)

# Install the necessary header files from ZipLib, which are a public dependency
install(DIRECTORY ZipLib/
        DESTINATION include/ZipLib
        FILES_MATCHING PATTERN "*.h"
)
