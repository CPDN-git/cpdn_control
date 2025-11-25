# CPDN controller to manage meterological/climate models running in climateprediction.net (CPDN)

This respository contains the instructions and code for building the application (controller)
used for managing the models in the climateprediction.net project under a BOINC framework. This code acts
as the interface between the BOINC client and the model. The BOINC client only knows about 
this program and only this program knows how to manage the model.

The design of this code allows it to be (mostly) agnostic about the underlying model. The
model details and configuration are read from input XML files.  A class architecture is
used to instantiate the model(s) handled by this code.

[![CTest Unit Tests](https://github.com/CPDN-git/cpdn_control/actions/workflows/run_unit_tests.yml/badge.svg)](https://github.com/CPDN-git/cpdn_control/actions/workflows/run_unit_tests.yml)
[![Controller CI](https://github.com/CPDN-git/cpdn_control/actions/workflows/controller_ci.yml/badge.svg)](https://github.com/CPDN-git/cpdn_control/actions/workflows/controller_ci.yml)

A number of prerequisite libraries are required.

## Prerequisite: BOINC library

Download and build the BOINC libraries which this code links with (BOINC is available from: https://github.com/BOINC/boinc). 
For instructions on building BOINC see: [BOINC github](https://github.com/BOINC/boinc/wiki/).

As only the libraries are required, the boinc client and manager can be disabled (reduces system packages required).

Install and build the boinc library to a directory outside this repository and note the install path.

Note the boinczip library is no longer used as the repository contains an updated compression/zip library.

In short:
```
    git clone https://github.com/BOINC/boinc.git
    cd boinc
    ./_autosetup
    ./configure --disable-server --disable-fcgi --disable-manager --disable-client  \
                --enable-libraries --disable-boinczip  \
                --prefix=/PATH_TO_BOINC_INSTALL/boinc-install  \
                CXXFLAGS='-O2'
    make install
    cd ..
```

This installs the boinc libraries and include files to the directory specified in the `--prefix` argument. 
It's preferable not to install into the same directory as the source. 
When compiling the controller, specify the location of the include files using the -I argument and the libraries using -L argument
on the compiler command line (see the CMakeLists.txt in the top dir).

## Prerequisite: ZipLib and cpdn_zip library

As of Oct 2025, the controller no longer uses the BOINC library zip routines.
These old routines are not memory safe; they use fixed sized buffers and unbounded string copy routines.

The 'zip' subdirectory now contains code to build `cpdn_zip` and `cpdn_unzip` functions to replace
`boinc_zip` and `boinc_unzip`.

### ZipLib

This project uses [ZipLib](https://github.com/gdcarver/ZipLib).
It's a modern C++ lightweight library based on STL streams. It's a fork
of the repository at https://github.com/DreamyCecil/ZipLib with fixes
and improved error reporting.

It supports zip, bzip2 and lzma compression techniques. bzip2 offers higher
compression at increased execution time and could be suitable for CPDN.
Currently only zip compression is used.

ZipLib is already installed and setup as required by the cpdn_zip wrapper code.
However, if ZipLib source needs to be upgraded follow these steps:

- To update ZipLib for this repo, download the ZipLib repo from github to a **temporary location** outside this repository.
- Copy the `Zip/LibSource/ZipLib` directory to `ZipLib` in this directory. 
- Also copy the README.md and LICENSE files for reference into ZipLib.

### cpdn_zip library

This is a simple wrapper around ZipLib to provide `cpdn_zip` and `cpdn_unzip` functions
now called by the control code.

It is built as a static library and combined with ZipLib object files and its 
compression library dependencies. cpdn_zip and unzip use the zip library by default.

CMake is the preferred build tool and the software is built in separate `build` and
`install` directories to the source. See `CMakeLists.txt` in the `zip` folder for
more details.

 To build the cpdn_zip library and optionally run a small test:

 1. cd cpdn_control/zip
 2. mkdir build
 3. cd build
 4. cmake -DCMAKE_INSTALL_PREFIX=../install ..
 5. make
 6. make install

 To run a simple test of the code:
 ```
 ./test_zip
 ```

## Build CPDN Controller executable.
### OpenIFS 43r3 model
#### Linux

If not already present, obtain the RapidXml header for parsing XML files. 
This is downloaded from the site: [RapidXml](http://rapidxml.sourceforge.net/).
We only need the file: 'rapidxml.hpp'. Download this file and put in the same folder as the code.

A Makefile is used to build the control code for the OpenIFS 43r3 model.

Ensure the Makefile uses ../zip/install/lib and ../zip/install/include and then run:
```
make clean
make
```
This creates 3 executables:
```
VERSION = 43r3_1.00
TARGET  = oifs_$(VERSION)_x86_64-pc-linux-gnu
DEBUG   = oifs_$(VERSION)_x86_64-pc-linux-gnu-debug
TEST    = oifs_43r3_test.exe
```
The DEBUG target is compiled with AddressSanitizer enabled and should be used for testing to check
for memory leaks and memory corruption.

The default 'TARGET' is the build intended for production.

The version number of the executable is best left as-is and changed when transferring to CPDN.

### Windows

Not yet ported to Windows.

#### ARM

To build OpenIFS on an ARM architecture machine modify the Makefile and set `-D_ARM` and the object file becomes `oifs_43r3_1.00_aarch64-poky-linux`.

#### macOS

Build the BOINC and cpdn_zip libraries using Xcode. Modify the Makefile to use `clang++` as the compiler and the object file as `oifs_43r3_100_x86_64-apple-darwin`.

## How to run the control code executable with OpenIFS

This creates the control code executable to control the OpenIFS model in the BOINC environment. 
In order for OpenIFS to run, its ancillary files need to be installed correctly. This is the responsibility of the control code.

The command to run the control in standalone mode with OpenIFS on Linux is:
```
    ./oifs_43r3_1.00_x86_64-pc-linux-gnu 2000010100 gw3a 0001 1 00001 1 oifs_43r3 1.00
```
Or for macOS:
```
    ./oifs_43r3_1.00_x86_64-apple-darwin 2000010100 gw3a 0001 1 00001 1 oifs_43r3 1.00
```

## Control code command line parameters

The command line parameters are: 
[0] compiled executable, 
[1] start date in YYYYMMDDHH format, 
[2] experiment id, 
[3] unique member id, 
[4] batch id, 
[5] workunit id, 
[6] FCLEN, 
[7] app name, 
[8] nthreads, 
[9] app version id.

Note, [9] is only used in standalone mode.

Currently supports version 43r3 of OpenIFS and it's model variants. 

The OpenIFS code is compiled separately and is installed alongside the OpenIFS controller in BOINC. 
To upgrade the controller code in the future to later versions of OpenIFS consideration will need 
to be made whether there are any changes to the command line parameters the compiled version of 
OpenIFS takes in, and whether there are changes to the structure and content of the supporting ancillary files.

