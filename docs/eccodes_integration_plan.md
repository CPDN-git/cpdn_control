# ecCodes Integration Plan

## Purpose

This document records the agreed approach for adding ECMWF ecCodes support to
`cpdn_control` for future OpenIFS GRIB metadata work.

The current plan is deliberately narrower than the earlier vendored-source
proposal: `cpdn_control` will consume an externally built ecCodes installation
as a normal CMake package instead of carrying the full ecCodes source tree
inside this repository.

## Agreed Direction

The following points are now treated as the working design:

1. ecCodes is built and installed outside `cpdn_control`.
2. `cpdn_control` finds ecCodes with `find_package(eccodes CONFIG REQUIRED)`.
3. ecCodes is linked only on the OpenIFS build path, not as a blanket
   dependency of the whole controller interface.
4. Linux is the only supported ecCodes integration path for now.
5. AEC support remains enabled for Linux ecCodes builds.
6. `cpdn_control` keeps a Linux-only `find_package(libaec)` fallback because
   static ecCodes exports may still reference `libaec::aec` transitively.
7. The helper used to build ecCodes lives in this repo under `scripts/`, but it
   remains an optional developer convenience script rather than part of the
   main controller build graph.

## Why The Vendored ecCodes Plan Was Dropped

After review, vendoring ecCodes into `cpdn_control` was judged too heavy for
this repository:

- ecCodes is a substantial third-party codebase with its own release cycle
- it would add upstream build-system policy and patch maintenance here
- it widens the dependency footprint of a repo whose main responsibility is the
  CPDN controller
- it makes the controller repo responsible for more third-party behaviour than
  is justified by the current OpenIFS integration needs

Using ecCodes as an external installed dependency is the cleaner seam.

## External Build Helper

This repo now provides:

- `scripts/build_eccodes.sh`

The helper script is intended to:

- build ecCodes from an external checkout such as `${HOME}/github/eccodes`
- use the repo-local `cmake/Findlibaec.cmake`
- build static ecCodes on Linux
- install ecCodes into a chosen prefix

It is a convenience helper only. The canonical controller integration remains
the installed ecCodes package discovered by CMake.

## Linux AEC Requirement

On Linux, the host system must have the AEC development package installed
before building ecCodes with `-DENABLE_AEC=ON`.

Ubuntu/Debian package:

- `libaec-dev`

Reason:

- Ubuntu's `libaec-dev` provides the library archive and headers
- but it does not provide a CMake package config file
- therefore this repo carries `cmake/Findlibaec.cmake` so both the external
  ecCodes helper and `cpdn_control` can resolve `libaec`

## ecCodes Build Policy

The Linux helper build should continue to use this constrained feature set
unless a later OpenIFS requirement changes it:

```cmake
-DBUILD_SHARED_LIBS=OFF
-DENABLE_FORTRAN=OFF
-DENABLE_NETCDF=OFF
-DENABLE_JPG=OFF
-DENABLE_PNG=OFF
-DENABLE_LARGE_FILE_SUPPORT=OFF
-DENABLE_GRIB_THREADS=OFF
-DENABLE_ECCODES_THREADS=OFF
-DENABLE_PYTHON=OFF
-DENABLE_AEC=ON
-DENABLE_USE_SHARED_LIB_AEC=OFF
```

Notes:

- the target library is a static `libeccodes.a`
- `libaec` is also expected to be linked statically on Linux

## Controller-Side CMake Shape

`cpdn_control` should keep the ecCodes integration small and reversible.

### New wrapper

- `cmake/EcCodesConfig.cmake`

Responsibilities:

1. define `CPDN_ENABLE_ECCODES`
2. enforce the current Linux-only policy
3. prepend the repo `cmake/` directory so `Findlibaec.cmake` is available
4. call `find_package(libaec REQUIRED)` on Linux
5. call `find_package(eccodes CONFIG REQUIRED)`
6. expose a narrow interface target for the OpenIFS path

### Link scope

ecCodes should not be added to the public link interface of `cpdn_control`.

Instead:

- OpenIFS model objects compile against an OpenIFS-specific dependency target
- final executables/tests that consume those OpenIFS objects also link that
  OpenIFS dependency target

This keeps the dependency narrow while still allowing static final links to
resolve correctly.

## Configure `cpdn_control`

After ecCodes has been installed externally, configure the controller with the
ecCodes package prefix or config directory available to CMake.

Typical Linux examples:

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/home/glenn/github/eccodes-install/2.46.2 \
  -DCPDN_ENABLE_ECCODES=ON
```

or:

```bash
cmake -S . -B build \
  -Deccodes_DIR=/home/glenn/github/eccodes-install/2.46.2/lib/cmake/eccodes \
  -DCPDN_ENABLE_ECCODES=ON
```

If ecCodes support is not wanted for a build:

```bash
cmake -S . -B build -DCPDN_ENABLE_ECCODES=OFF
```

## Non-Goals For This Phase

Still out of scope:

- vendoring ecCodes under `third_party/`
- adding a `vcpkg` ecCodes port
- Windows ecCodes support
- macOS ecCodes support
- redesigning the OpenIFS GRIB definitions/samples runtime layout
- adding ecCodes API usage outside the OpenIFS code path

## Follow-Up Work

Once the external package path is stable:

1. add the first actual ecCodes call sites under `models/openifs/`
2. confirm the installed ecCodes package exports all required transitive link
   metadata cleanly
3. revisit whether the explicit Linux `libaec` fallback is still needed for all
   supported ecCodes install layouts
4. evaluate Windows and macOS separately rather than assuming Linux policy will
   transfer unchanged
