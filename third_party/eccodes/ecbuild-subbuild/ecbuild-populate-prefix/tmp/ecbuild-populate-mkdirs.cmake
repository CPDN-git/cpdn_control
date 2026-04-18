# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/glenn/github/eccodes/ecbuild-src"
  "/home/glenn/github/eccodes/ecbuild-build"
  "/home/glenn/github/eccodes/ecbuild-subbuild/ecbuild-populate-prefix"
  "/home/glenn/github/eccodes/ecbuild-subbuild/ecbuild-populate-prefix/tmp"
  "/home/glenn/github/eccodes/ecbuild-subbuild/ecbuild-populate-prefix/src/ecbuild-populate-stamp"
  "/home/glenn/github/eccodes/ecbuild-subbuild/ecbuild-populate-prefix/src"
  "/home/glenn/github/eccodes/ecbuild-subbuild/ecbuild-populate-prefix/src/ecbuild-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/glenn/github/eccodes/ecbuild-subbuild/ecbuild-populate-prefix/src/ecbuild-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/glenn/github/eccodes/ecbuild-subbuild/ecbuild-populate-prefix/src/ecbuild-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
