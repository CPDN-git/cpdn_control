# AI Agent Notes (Codex / Claude)

This repository builds the **CPDN controller** executable used to run/manage climateprediction.net (CPDN) model tasks under **BOINC**. The controller is a C++17 app built with **CMake**, plus a small Python-driven **functional test harness** that mimics a BOINC slot/workunit layout.

## Architecture (where to look)

- `src/cpdn_main.cpp`
  - Program entry point (`main`) and high-level flow: BOINC init, argument parsing, model selection, run loop.
  - Contains the model factory `create_model_control(...)` mapping `app_name/model_name -> ModelControl`.
- `src/cpdn_control.cpp`, `src/cpdn_control.h`
  - Core controller logic used by the release/debug executables and unit tests.
- `api/model_control.h`
  - Abstract interface for model-specific controllers (`ModelControl`).
- `models/`
  - Model-specific helpers/implementations.
  - `models/test/` contains the `test_model` and controller glue used by functional tests.
  - `models/openifs/` contains OpenIFS helper utilities.
  - Built as an **object library** (`cpdn_models`) and folded into the main `cpdn_control` library (see `models/CMakeLists.txt`).
- `lib/`
  - Shared utilities (filesystem helpers, CPU-time helpers, etc).
- `zip/`
  - Standalone CMake project that builds the `cpdn_zip` static library (wrapper around ZipLib).
  - The top-level controller build expects this to be installed into `zip/install`.
- `tests/unit/`
  - CTest-driven unit tests (single `unit_tests` executable invoked with different arguments).
- `tests/functional/`
  - End-to-end/“workunit style” tests driven by Python scripts:
  - `setup_test.py` creates a fake BOINC directory layout (`projects/`, `slots/0/`, input zips, `init_data.xml`, etc).
  - `run.py` copies built binaries into the test workdir and launches the controller.
  - `validate_test.py` checks outputs.

## Build prerequisites

### 1) BOINC libraries

The controller links against BOINC libraries. By default, the top-level `CMakeLists.txt` expects BOINC installed at:

- `../boinc-8.0.2-x86_64`
  - headers in `include/`
  - libs in `lib/` (static `.a` preferred)

Override at configure time if your BOINC install path differs:

- `cmake -S . -B build -DBOINC_DIR=/path/to/boinc-install`

### 2) `cpdn_zip` library (required)

The top-level build expects:

- headers in `zip/install/include`
- library in `zip/install/lib` (found via `cmake/CPDNzipConfig.cmake`)

Build/install it first:

- `cmake -S zip -B zip/build -DCMAKE_INSTALL_PREFIX=zip/install`
- `cmake --build zip/build -j`
- `cmake --build zip/build --target install`

## Build the controller

From repo root:

- `cmake -S . -B build -DBOINC_DIR=/path/to/boinc-install` (optional if using default)
- `cmake --build build -j`

Outputs are placed in `build/` (not installed). Notable binaries:

- `cpdn_control_1.0.0_x86_64-pc-linux-gnu` (release-ish target; links with `-static` by default)
- `cpdn_control_1.0.0_x86_64-pc-linux-gnu-debug` (AddressSanitizer enabled)
- `test_model` (simple model simulator used by functional tests)
- `unit_tests`

## Running tests

All tests are registered with CTest by the top-level build.

### Unit tests

- `ctest --test-dir build -V`
- Run one test by name: `ctest --test-dir build -R RCFTest -V`

### Functional tests

Functional tests are labeled `functional` (see `tests/functional/CMakeLists.txt`):

- `ctest --test-dir build -L functional -V`

Notes:
- Functional tests require `python3`.
- `tests/functional/CMakeLists.txt` currently sets `BOINC_LIB_DIR` as `${CMAKE_SOURCE_DIR}/../boinc-8.0.2-x86_64/lib` for `LD_LIBRARY_PATH` during the run step. If BOINC is installed elsewhere, update this or make it configurable alongside `BOINC_DIR`.

## Adding a new model integration

Typical steps:

- Implement a new `ModelControl` subclass (see `api/model_control.h`).
- Add sources under `models/<your_model>/...` and include them in `models/CMakeLists.txt` (object library `cpdn_models`).
- Update the model factory in `src/cpdn_main.cpp` (`create_model_control`) to return your new class for the appropriate model/app name.
- Add targeted unit tests in `tests/unit/` and/or a functional fixture in `tests/functional/fixtures/`.

## Repo conventions for AI-assisted changes

- Prefer small, focused patches; avoid drive-by refactors/formatting.
- Keep build logic in CMake (don’t hardcode machine-specific paths in code).
- Formatting: use the repo `.clang-format` when you need to format code; avoid reformatting unrelated files/sections.
- When changing controller behavior, add/adjust a unit test where practical; use functional tests for end-to-end behavior.
- The debug controller binary enables ASan; prefer it for test runs and bug hunting.
