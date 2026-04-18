
External third-party dependencies.
=================================

CLI11 : Command line parser for C++11 and beyond.
        https://github.com/CLIUtils/CLI11
        Custom license.

fmt : Modern formatting library for C++.
      https://github.com/fmtlib/fmt
      Using MIT license.
      Vendored CMakeLists.txt includes a small local compatibility patch so
      it can be built by this project with CMake 3.16.

eccodes : ECMWF GRIB file commands and libraries. Used by OpenIFS family of models
      Cloned from Glenn Carver's fork of the ECMWF github eccodes repo.
      Checkout eccodes version: 2.46.2
      See CMake cache for list of options enabled.
