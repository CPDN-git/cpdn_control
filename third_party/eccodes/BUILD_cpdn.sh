#! /usr/bin/env bash
#
#  eccodes build script

set -e

if [ ! -d build ]; then
   mkdir build
fi

# configure
cd build

cmake -DCMAKE_INSTALL_PREFIX=. \
        -DCMAKE_BUILD_TYPE=Release   \
        -DENABLE_NETCDF=OFF    \
        -DENABLE_JPG=OFF       \
        -DENABLE_PNG=OFF       \
        -DENABLE_LARGE_FILE_SUPPORT=OFF  \
        -DENABLE_GRIB_THREADS=OFF  \
        -DENABLE_ECCODES_THREADS=OFF  \
        -DENABLE_FORTRAN=OFF   \
        -DENABLE_PYTHON=OFF    \
        -DBUILD_SHARED_LIBS=OFF  \
        -DENABLE_AEC=ON        \
        ..

# compile
cmake --build . -j2




