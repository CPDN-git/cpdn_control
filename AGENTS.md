# AI Agent Notes (Codex / Claude)

This repository builds the **CPDN controller** executable used to run/manage climateprediction.net (CPDN) model tasks under **BOINC**. The controller is a C++17 app built with **CMake**, plus a small Python-driven **functional test harness** that mimics a BOINC slot/workunit layout.

## Architecture (where to look)

- `src/cpdn_main.cpp`
  - Program entry point (`main`) and high-level flow: BOINC init, argument parsing, model selection, run loop.
  - Contains the model factory `create_model_control(...)` mapping `app_name/model_name -> ModelControl`.
  - Currently also contains **experimental** step-diagnostics glue that runs `diagnostics.exe` before result files are moved out of the slot directory. This is a temporary integration and is expected to move under model-class control later.
- `src/parse.h`, `src/parse.cpp`
  - CLI11-based command-line parsing for `--cpdn_*` and `--model_*` arguments.
  - Uses vendored CLI11 headers under `tools/CLI11/`.
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
  - `lib/utils.h`, `lib/utils.cpp` now include a general `run_process_with_timeout(...)` helper used for short synchronous external programs that must run in a specific working directory and may be validated by checking an output file timestamp/update.
- `zip/`
  - Standalone CMake project that builds the `cpdn_zip` static library (wrapper around ZipLib).
  - The top-level controller build expects this to be installed into `zip/install`.
- `tests/unit/`
  - CTest-driven unit tests (single `unit_tests` executable invoked with different arguments).
  - Includes `t_run_process_with_timeout.cpp` plus `timed_process_helper.cpp` for exercising timed external-process execution.
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

## Current experimental area

- `src/cpdn_main.cpp` contains temporary logic for running an external `diagnostics.exe` program on completed OpenIFS output before `move_result_file()` removes those files from the slot directory.
- The current implementation only looks for the `ICMSH...` file from `get_output_filenames(...)` and builds a hard-coded experimental argument list for diagnostics. Treat this as provisional, not as a stable interface.
- If you are extending or refactoring this area, prefer moving the diagnostics decision-making and argument construction into model-specific code rather than growing more OpenIFS-specific logic in `main()`.
- The diagnostics path currently depends on `TrickleHandler` consuming `trickle_data` from the slot directory, so any redesign that stages work elsewhere must either copy `trickle_data` back or update that contract deliberately.

## CPU Time And Progress File Semantics

- `TaskState::prior_acc_cpu_time` is the accumulated CPU time restored from `cpdn_progressfile.txt` when a controller restarts after an interrupted run.
- `TaskState::current_cpu_time` is the live total CPU time for the task during the current controller run. It should be treated as:
  - `current_cpu_time = prior_acc_cpu_time + cpdn_cpu_time(child_pid)`
- On a fresh run, both values start at zero.
- On a restart, the progress file is read first, `prior_acc_cpu_time` is restored, and `current_cpu_time` must be seeded from it before the first progress-file write.
- The progress file stores the accumulated total CPU time so far, not just the prior-run baseline.
- `prior_acc_cpu_time` must remain the restored baseline for the lifetime of the current controller process; do not update it inside the main loop.
- If this area is refactored, preserve the distinction:
  - `prior_acc_cpu_time` = accumulated CPU time from earlier controller runs
  - `current_cpu_time` = total accumulated CPU time including the currently running child process

## Repo conventions for AI-assisted changes

- Prefer small, focused patches; avoid drive-by refactors/formatting.
- Keep build logic in CMake (don’t hardcode machine-specific paths in code).
- Formatting: use the repo `.clang-format` when you need to format code; avoid reformatting unrelated files/sections.
- When changing controller behavior, add/adjust a unit test where practical; use functional tests for end-to-end behavior.
- The debug controller binary enables ASan; prefer it for test runs and bug hunting.
- When touching the experimental diagnostics path, keep the change narrowly scoped unless the task is explicitly to migrate it into the model classes.
- AI-authored commits should prefix the commit subject with the model/version identifier, for example `GPT-5.4: ...`, so repository history clearly records the source of the change.
- Any reference to `ACTION.md` or `ACTIONS.md` should be treated as a reference to `AGENTS.md`; those filenames are common typos.

## Functional Test Limits

- The current functional test harness is suitable for checking controller startup, file movement, and basic end-to-end execution, but it is not a strong test of `current_cpu_time` behaviour.
- `models/test/test_model.cpp` spends much of its runtime sleeping, so wall-clock runtime in the functional test is not a reliable proxy for process CPU time.
- A lightweight functional validation may check that the progress file contains a parsable non-negative accumulated CPU time value, but that does not prove restart accumulation logic.
- A proper end-to-end test for CPU accounting should:
  - force an interrupted controller run,
  - restart from the saved progress file and model restart state,
  - and verify that the accumulated CPU time after restart is greater than the value saved before interruption.
- Defer implementing that stronger restart-based functional test until the surrounding development work stabilizes.
