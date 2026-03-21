# CPDN control application to manage meterological/climate models running in climateprediction.net (CPDN)

This respository contains the code and instructions for building the CPDN control application
used for managing the models in the CPDN project at the University of Oxford.  This code acts
as the interface between the BOINC client and the underlying model. Ideally the model does not (usually)
know that it is running under a BOINC client. The advantage of doing this is the model code
needs a minimal set of changes. The BOINC client only knows about this application and only 
this application knows how to manage the model.

The design of this code allows it to be (mostly) agnostic about the underlying model. The
model details and configuration are read from input XML files, then used to instantiate
a base class for each model type.

[![Controller CI](https://github.com/CPDN-git/cpdn_control/actions/workflows/controller_ci.yml/badge.svg)](https://github.com/CPDN-git/cpdn_control/actions/workflows/controller_ci.yml)
[![CodeQL Advanced](https://github.com/CPDN-git/cpdn_control/actions/workflows/codeql.yml/badge.svg)](https://github.com/CPDN-git/cpdn_control/actions/workflows/codeql.yml)

A number of prerequisite libraries are required detailed below.

## Prerequisite: BOINC library

Download and build the BOINC libraries required for linking. 
(BOINC is available from: https://github.com/BOINC/boinc). 
For instructions on building BOINC see: [BOINC github](https://github.com/BOINC/boinc/wiki/).

As only the BOINC libraries are required, the boinc client and manager can be disabled (reduces system packages required).

Install and build the boinc library to a directory **outside** this repository and note the install path.

The boinczip library is no longer used as this repository contains an improved compression/zip library.
The boinczip library is a buggy code that should not be used.

In short, outside this repo do:
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
Change the value of prefix to suit.
It's preferable not to install into the same directory as the source. 
When compiling the control application, specify the location of the include files using the -I argument and the 
libraries using -L argument on the compiler command line (see the CMakeLists.txt file in the top dir).

## Prerequisite: ZipLib and cpdn_zip library

As of Oct 2025, the control application no longer uses the BOINC library zip routines.
These old routines are not memory safe; they use fixed sized buffers and unbounded string copy routines.
They also do not work correctly under Windows (their low-level file handling breaks in Windows Update environments).

The 'zip' subdirectory in this repo now contains code to build `cpdn_zip` and `cpdn_unzip` functions to replace
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
However, if the ZipLib source needs to be upgraded follow these steps:

- To update ZipLib for this repo, download the ZipLib repo from github to a **temporary location** outside this repository.
- Copy the `Zip/LibSource/ZipLib` directory to `ZipLib` in this directory. 
- Also copy the README.md and LICENSE files for reference into ZipLib.

### cpdn_zip library

This is a simple wrapper around ZipLib to provide `cpdn_zip` and `cpdn_unzip` functions to the 
control application.

It's built as a static library and combined with ZipLib object files and its 
compression library dependencies. cpdn_zip/unzip use zip compression by default.

The software is built in separate `build` and
`install` directories to the source. See `CMakeLists.txt` in the `zip` folder for
more details.

 To build the cpdn_zip library and optionally run a small test:

 1. cd cpdn_control/zip
 2. mkdir build
 3. cd build
 4. cmake -DCMAKE_INSTALL_PREFIX=../install ..
 5. cmake --build .
 6. make install

 To run a simple test of the code:
 ```
 ./test_zip
 ```

## Prerequisite: fmt C++17 compatible library

The fmt library is a modern formatting library that provides C++20 `std::format`-like functionality for C++17.
It is in the repository under `tools/fmt/` and does not require separate installation.

To obtain the fmt library, execute the following command in the top-level directory:

```bash
cd tools/fmt
git clone https://github.com/fmtlib/fmt.git . --depth=1
```

Alternatively, if you have already cloned this repository and the `tools/fmt/` directory exists but is empty,
run the same git clone command from within that directory. The CMake build system will automatically detect
and configure fmt for use in the build.

### Prerequisite: RapidXML

[ Note: RapidXML will be needed for future development. It's not currently used]

If not already present, obtain the RapidXml code (as a header) for parsing XML files. 
This is downloaded from the site: [RapidXml](http://rapidxml.sourceforge.net/).
We only need the files: 'rapidxml.hpp' and the license.txt file. 
Download this file into the directory `tools/RapidXML`.

## Models

The `models` folder contains the model specific code for this application.

The meteorological and climate models themselves are not contained in this repository. They
are built separately.

Each model interface code should use the class structure described by the code in the `api`
directory. See that code for more details and the test model appication for an 
implementation example.

### Test model

A small test model code is available which behaves similarly to the OpenIFS 43r3 model. 
This is used to test functionality of the control app. See the `test` folder for 
more details.

The test model is built when the control application is built using CMake (see below).

## Build CPDN Controller

### Linux

CMake is used to build the controller application. Ensure the prerequisite steps
above have been completed.

BOINC libraries: These can either be specified by editing the CMakeLists.txt file directly
and changing the line:
```
set(BOINC_DIR "../boinc-8.0.2-x86_64" CACHE STRING "Root directory of BOINC install." )
```
Or make a temporary change on the cmake command line by using the -DBOINC_DIR argument.

#### Steps to build

In the top level directory:
1. mkdir build (or remove it for fresh build).
2. cd build
3. cmake ..
4. cmake --build . --verbose    # omit --verbose if not interested in compile commands
5. ctest -V                     # see below for more details

This creates 3 executables in the build directory:
```
VERSION = 43r3_1.00
TARGET  = oifs_$(VERSION)_x86_64-pc-linux-gnu
DEBUG   = oifs_$(VERSION)_x86_64-pc-linux-gnu-debug
```

The DEBUG target is compiled with AddressSanitizer enabled and should be used for testing to check
for memory leaks and memory corruption.

The default TARGET is the build intended for production.

The version number of the executable is best left as-is and changed when transferring to CPDN.

### Windows

Not yet ported to Windows. In progress.

#### macOS

Not ported.

[comment]: # (OLD: Build the BOINC and cpdn_zip libraries using Xcode. Modify the Makefile to use `clang++` as the compiler and the object file as `oifs_43r3_100_x86_64-apple-darwin`.)

#### ARM

Not ported.

[comment]: # (OLD: To build OpenIFS on an ARM architecture machine modify the Makefile and set `-D_ARM` and the object file becomes `oifs_43r3_1.00_aarch64-poky-linux`.)

## How to run the controller executable with OpenIFS

In order for OpenIFS to run, its ancillary inpur files need to be installed correctly from the
download directory on the client. This is the responsibility of the controller process.

The command line interface is still being refined as the controller code is migrated away
from the older positional-argument approach. The current implementation expects named
long-form options for the CPDN task metadata it uses during startup.

An example controller invocation is:
```bash
./cpdn_control_1.0.0_x86_64-pc-linux-gnu \
    --startdate=2000010100 \
    --memberid=0001 \
    --batch=1 \
    --workunit=00001 \
    --fcast_len=1
```

### Command line parameters

The currently required controller arguments are:

- `--startdate`: model start date in `YYYYMMDDHH` format
- `--memberid`: CPDN unique member id
- `--batch`: CPDN batch id
- `--workunit`: CPDN workunit id
- `--fcast_len`: model forecast length in days

Other information about the workunit, including the application name and version, comes
from the BOINC-supplied `init_data.xml` data read during controller startup.

## Testing

The project uses **CTest** (via CMake) for both unit tests and functional tests. 
After configuring and building, you can run tests from inside the `build/` directory, 
or from the repo root by using `ctest --test-dir build`.

### Unit testing

Unit tests are built as a single executable (`unit_tests`) and registered with CTest as multiple tests.
The unit tests test individual code functions in the code to ensure the correct and expected behaviour
is maintained (regression testing).

Build and run all unit tests:
```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Run all tests with verbose output (useful for debugging failures):
```
ctest --test-dir build -V
```

Show output only when a test fails (often a good default in CI):
```
ctest --test-dir build --output-on-failure --stop-on-failure
```

Run a single CTest test by name (example):
```
ctest --test-dir build -R RCFTest -V
```

Run **unit tests only** (exclude functional tests, which are labeled `functional`):
```
ctest --test-dir build -LE functional
```

Alternatively, run the unit test executable directly (useful when iterating on one case):
```
build/tests/unit/unit_tests "Read RCF File"
```

### Functional testing

Functional tests exercise the application end-to-end using the `test_model` program, 
which mimics the behaviour of the OpenIFS model from the controller’s point of view.

Functional tests live under `tests/functional/` and are driven by JSON fixture files 
in `tests/functional/fixtures/`. Each logical functional test is split into three CTest tests:

- `<TestName>_Setup` (create a BOINC-like work directory layout and inputs)
- `<TestName>` (run the controller against the prepared work directory)
- `<TestName>_Validate` (check outputs)

Run **only** the functional test suite:
```
ctest --test-dir build -L functional -V
```

Run everything except functional tests:
```
ctest --test-dir build -LE functional
```

Run just one functional scenario (example, all three steps):
```
ctest --test-dir build -R '^FTest1(_Setup|_Validate)?$' -V
```

Run a single phase of a functional test (examples):
```
ctest --test-dir build -R '^FTest1_Setup$' -V
ctest --test-dir build -R '^FTest1$' -V
ctest --test-dir build -R '^FTest1_Validate$' -V
```

## Coding style: Formatting via clang-format

This repo uses `clang-format` with a `.clang-format` file in the repo root to 
ensure the preferred coding style.

### VS Code
To use formatting in VS Code, load a code file into the editor and press: CTRL+SHIFT+I.

To setup VS Code to use clang-format:

Make clang-format the formatter for C/C++:
```
"[cpp]": { "editor.defaultFormatter": "ms-vscode.cpptools" },
"[c]":   { "editor.defaultFormatter": "ms-vscode.cpptools" }
```
Tell cpptools to use the .clang-format file:
```
"C_Cpp.clang_format_style": "file"
```
(Optional but recommended) Explicitly point to the clang-format binary so it’s consistent:
```
"C_Cpp.clang_format_path": "/path/to/clang-format"
```

If using the C/C++ Extension pack in VS Code, then check the bundled clang-format version
in the extensions folder: $HOME/.vscode/extensions/ms-vscode.cpptools-*/LLVM/bin/clang-format.

To automatically format a code file when saving from VS Code, do:
```
"editor.formatOnSave": true (formats on save)
```
Keep "C_Cpp.clang_format_style": "file" so it uses .clang-format.

Completion/indentation while typing still uses editor settings; clang-format is applied only during formatting actions.

### Terminal
To format a single file on the command line. From the top level of the repository do:
```
clang-format -i path/to/file.cpp
```

To format all source files from the repo root:
```
clang-format -i $(rg --files -g '*.cpp' -g '*.h' -g '*.hpp')
```

A recent version of clang-format is required that supports the custom _SpacesIn_ options.
