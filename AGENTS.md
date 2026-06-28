# AI Agent Notes (Codex / Claude)

This repository builds the **CPDN controller** executable used to run/manage climateprediction.net (CPDN) model tasks under **BOINC**. The controller is a C++17 app built with **CMake**, plus a small Python-driven **functional test harness** that mimics a BOINC slot/workunit layout.

## Architecture (where to look)

- `src/cpdn_main.cpp`
  - Program entry point (`main`) and high-level flow: BOINC init, argument parsing, model selection, run loop.
  - Contains the model factory `create_model_control(...)` mapping `app_name/model_name -> ModelControl`.
  - Now stages model input archives through a model-owned manifest rather than hardcoded OpenIFS filename handling in `main()`.
  - Currently also contains **experimental** step-diagnostics glue that runs `diagnostics.exe` before result files are moved out of the slot directory. This is a temporary integration and is expected to move under model-class control later.
- `src/parse_args.h`, `src/parse_args.cpp`
  - CLI11-based command-line parsing for controller/task arguments.
  - The `--filename_startdate` and `--filename_fclen` options are CPDN filename-resolution metadata only; they are not authoritative model runtime controls.
  - Uses vendored CLI11 headers under `third_party/CLI11/`.
- `src/cpdn_control.cpp`, `src/cpdn_control.h`
  - Core controller logic used by the release/debug executables and unit tests.
  - BOINC API call sites in controller helpers now log the failing BOINC function name, numeric return code, and `boincerror(...)` string locally before returning control to the caller.
  - Includes the child-process cleanup helper used by `finish_task(...)` to terminate an active model child before calling `boinc_finish(...)`.
- `api/model_input_manifest.h`
  - Shared manifest/context types for model-declared BOINC input archives.
- `api/model_control.h`
  - Abstract interface for model-specific controllers (`ModelControl`).
  - Includes the model input manifest hook used to describe logical BOINC filenames and slot unzip destinations.
  - `ModelControl::parse_control_input()` now owns model control-file opening/parsing/validation and returns a structured `ModelControlInputData` result to `main()`.
  - `ModelControl::get_current_step(...)` now reports the current model step as an integer step count.
- `models/`
  - Model-specific helpers/implementations.
  - `models/test/` contains the `test_model` and controller glue used by functional tests.
  - `models/openifs/` contains OpenIFS helper utilities.
  - `models/wrf/` contains WRF control, timestamp/step parsing, restart pruning, and WRF output filename generation.
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
  - Includes staging/checksum coverage for BOINC project archives such as `t_verify_project_zip_md5.cpp` and `t_stage_model_input_archive.cpp`.
- `tests/functional/`
  - End-to-end/“workunit style” tests driven by Python scripts:
  - `setup_test.py` creates a fake BOINC directory layout (`projects/climateprediction.net/`, `slots/0/`, input zips, `init_data.xml`, etc).
  - The harness now models BOINC logical input files as `<soft_link>...</soft_link>` files in the slot that resolve to `jf_*` archives in the project directory.
  - `run.py` copies built binaries into the test workdir and launches the controller.
  - `validate_test.py` checks outputs.

## Build prerequisites

### 1) BOINC libraries

The preferred BOINC dependency path is now the repo-local `vcpkg` manifest in:

- `vcpkg/vcpkg.json`
- `vcpkg/vcpkg-configuration.json`
- `vcpkg/triplets/`
- `vcpkg/overlays/boinc/`

Use `vcpkg` first:

- Linux:
  - `scripts/setup_vcpkg.sh --triplet x64-linux-cpdn-static`
  - `scripts/build_with_vcpkg.sh --vcpkg-root /home/glenn/github/vcpkg`
- Windows:
  - configure with `-DCMAKE_TOOLCHAIN_FILE=/PATH/TO/vcpkg/scripts/buildsystems/vcpkg.cmake`
  - use `-DVCPKG_TARGET_TRIPLET=x64-windows-cpdn-static`
- macOS Apple Silicon:
  - use `-DVCPKG_TARGET_TRIPLET=arm64-osx-cpdn`

The repo pins the default BOINC version through the `vcpkg` baseline. If a BOINC release needs to be rolled back, prefer a manifest override rather than switching the repo back to a local BOINC build.

The repo also carries a small BOINC overlay port under `vcpkg/overlays/boinc/`.
That overlay is intentional and should remain the default on Linux, Windows, and macOS because this project uses `boinc` and `boincapi` but does not use BOINC's `boinc_zip` library.

Why the overlay exists:

- `cpdn_control` uses the in-repo `cpdn_zip`, not BOINC `boinc_zip`
- the upstream `vcpkg` BOINC port still builds and exports `boinc_zip`
- carrying the overlay keeps the package surface aligned with the repo's actual dependency needs
- the overlay should be reviewed when updating the pinned BOINC version or `vcpkg` baseline

Keep the local BOINC install only as a fallback when the `vcpkg` package path is unavailable or broken. The manual path still expects:

- headers in `include/`
- libraries in `lib/`

Override at configure time if the manual BOINC install path differs:

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

Preferred `vcpkg` path:

- `scripts/build_with_vcpkg.sh --vcpkg-root /home/glenn/github/vcpkg`

Equivalent manual CMake flow:

- `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/PATH/TO/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux-cpdn-static`
- `cmake --build build -j`

Fallback manual BOINC flow:

- `scripts/build_with_local_boinc.sh --boinc-dir /path/to/boinc-install`

or:

- `cmake -S . -B build -DBOINC_DIR=/path/to/boinc-install`
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
- `tests/functional/CMakeLists.txt` now uses `BOINC_RUNTIME_LIB_DIR` when BOINC runtime libraries exist.
- For Linux static BOINC builds this is typically empty, which is the desired release-style behaviour.
- For Windows and macOS package builds, the test harness may still need the BOINC runtime directory injected via `PATH` or `DYLD_LIBRARY_PATH`.

## Adding a new model integration

Typical steps:

- Implement a new `ModelControl` subclass (see `api/model_control.h`).
- Implement `get_input_manifest(...)` for the new model so `main()` does not need model-specific BOINC input naming or unpack rules.
- Add sources under `models/<your_model>/...` and include them in `models/CMakeLists.txt` (object library `cpdn_models`).
- Update the model factory in `src/cpdn_main.cpp` (`create_model_control`) to return your new class for the appropriate model/app name.
- Add targeted unit tests in `tests/unit/` and/or a functional fixture in `tests/functional/fixtures/`.

Notes:
- Do not assume future models will use a Fortran namelist. A new model may populate the same manifest from a different source.
- If a model has a control file such as `fort.4`, keep the filename and parsing logic private to the model implementation rather than exposing a controller-side getter for it.

## Current experimental area

- `src/cpdn_main.cpp` contains temporary logic for running an external `diagnostics.exe` program on completed OpenIFS output before `move_result_file()` removes those files from the slot directory.
- The current implementation only looks for the `ICMSH...` file from `get_output_filenames(...)` and builds a hard-coded experimental argument list for diagnostics. Treat this as provisional, not as a stable interface.
- If you are extending or refactoring this area, prefer moving the diagnostics decision-making and argument construction into model-specific code rather than growing more OpenIFS-specific logic in `main()`.
- The diagnostics path currently depends on `TrickleHandler` consuming `trickle_data` from the slot directory, so any redesign that stages work elsewhere must either copy `trickle_data` back or update that contract deliberately.

## BOINC Input Staging Semantics

- BOINC logical input files in the slot, such as `ic_ancil_*.zip`, `ifsdata_*.zip`, and `clim_data_*.zip`, are read-only controller inputs and must not be overwritten.
- The controller should resolve those logical files through `boinc_resolve_filename_s(...)`, validate the resolved `jf_*` project archive, copy that `jf_*` archive into the slot, and unzip from the copied archive.
- Preserve the BOINC logical file on both first run and restart so BOINC resolution still works after a restart.
- `src/cpdn_main.cpp` should stay generic here. Model-specific filename construction and unzip destinations belong in the model manifest, not in `main()`.

## Failure Reporting Requirements

- For operational failures that can occur on remote volunteer hosts, do not collapse the result to a bare `bool` unless no better option exists.
- Prefer structured results that carry enough context for `main()` to emit a high-signal error:
  - logical BOINC filename
  - resolved physical `jf_*` file when available
  - destination slot archive/path when available
  - failed step such as resolve, checksum, copy, mkdir, or unzip
  - human-readable failure message
- Low-level helpers may still log locally, but the caller should be able to produce a single summary error line with the exact file and failure step.
- This is especially important for BOINC input staging and other startup failures where the task is executing remotely and reproducing the environment may be difficult.

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

## Step And Progress Semantics

- Treat model step counts and elapsed model time as different concepts with different names.
- `TaskState::current_step`, `TaskState::last_completed_step`, and `TaskState::last_trickle_step` are integer model step counts.
- `TaskState::last_upload_step` is the last model step already covered by an upload interval.
- The progress file now writes `last_completed_step` and `last_upload_step`.

## WRF Output Filename Semantics

- `WRFControl::get_output_filenames(step)` currently emits the innermost-domain output file only, not all WRF domains.
- The innermost domain is the highest configured WRF domain number (`max_dom`), but the cached prefix vector is zero-based. Preserve that distinction when editing filename generation.
- `WRFControl::get_copyable_output_filenames(current_step)` returns every WRF output filename considered safe to copy as of the observed model step, including the initial step-0 output set when applicable.
- Defensive filtering of empty per-step filename lists is acceptable at the controller seam, but the model controller remains responsible for generating valid output filenames.

## CPDN Filename Metadata

- The controller still needs CPDN task metadata on the command line to resolve downloaded filenames before the model input is parsed.
- `TaskConfig::filename_startdate` and `TaskConfig::filename_fclen` are filename tokens for that purpose only.
- Do not compare `filename_fclen` directly with model runtime controls such as `CUSTOP`; they belong to different contracts.

## BOINC And Child Status Semantics

- Keep BOINC runtime state and child-process state separate.
- `TaskState::child_status` is only for the model child lifecycle:
  - `0` = running
  - `1` = exited normally
  - `3` = terminated by signal
  - `4` = stopped
  - `5` = not found by `waitpid()`
- `BoincRuntime::client_status` stores the latest `BOINC_STATUS` snapshot returned by `boinc_get_status()`.
- Do not overload `child_status` with BOINC meanings such as quit, abort, suspend, or no-heartbeat.
- `sleep_with_boinc_poll(...)` in `src/cpdn_main.cpp` is intended to poll BOINC state only. It should not directly stop, resume, or kill the child process.
- `handle_boinc_client_status(...)` in `src/cpdn_control.cpp` is the side-effecting BOINC handler. It may stop, resume, or kill the child based on `BoincRuntime::client_status`.
- `finish_task(...)` in `src/cpdn_main.cpp` now performs child-process cleanup as well: if the model child is still active, it should log that fact, terminate the child, then call `boinc_finish(...)`.
- Do not assume a non-BOINC controller error implies the child is already gone. If the controller is exiting after launch, preserve the invariant that the model child must not be left running.
- In `main()`, prefer the explicit sequence:
  - call `boinc_get_status(&bruntime.client_status)`
  - inspect or poll for a BOINC state change
  - call `handle_boinc_client_status(...)` if action is required
  - derive the controller exit via `finish_task(...)`, which terminates any active child then calls `boinc_finish(...)`
- If this area is refactored further, preserve the separation of responsibilities:
  - polling BOINC state
  - applying BOINC-driven process control
  - tracking child exit state
  - deciding the final controller exit code

## BOINC API Error Reporting

- When a `boinc_*` API call returns a non-zero error code, prefer logging the failure immediately at that call site with:
  - the BOINC function name
  - the numeric return code
  - the `boincerror(...)` string
- Keep that low-level BOINC error logging local to the helper making the call. Higher-level code may still add context or decide the task outcome, but should not be forced to reconstruct the BOINC error text itself.
- This convention currently applies to controller BOINC setup, BOINC filename resolution, upload submission/status, result-file copying, and trickle submission.

## Repo conventions for AI-assisted changes

- Prefer small, focused patches; avoid drive-by refactors/formatting.
- Keep build logic in CMake (don’t hardcode machine-specific paths in code).
- Formatting: use the repo `.clang-format` when you need to format code; avoid reformatting unrelated files/sections.
- When changing controller behavior, add/adjust a unit test where practical; use functional tests for end-to-end behavior.
- After any significant tidying or refactor, rerun the code-complexity scan with `pmccabe` and `/home/glenn/.local/bin/lizard` without waiting to be asked, and update [docs/code_complexity_refactor_tracking.md](/home/glenn/github/cpdn_control/docs/code_complexity_refactor_tracking.md).
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

## Future Work

- The current manifest refactor creates a clean seam for a future external model configuration file, but it does not implement one yet.
- See the future-work discussion in [docs/input_file_refactor_plan.md](/home/glenn/github/cpdn_control/docs/input_file_refactor_plan.md).
- Short version:
  - for one additional similar model, hardcoded model manifests may still be acceptable
  - for several substantially different models, especially if they do not share a Fortran namelist pattern, a `model.xml` or similar model-owned config file becomes more attractive
- If `model.xml` is introduced later, prefer using it to populate the existing manifest/config seam rather than pushing more model-specific rules back into `main()`.
- A future `model.xml` would likely describe input archives, unpack destinations, control-file names, output patterns, restart files, and model-specific setup dependencies.

## Documentation

- [docs/Adding_new_model.md](/home/glenn/github/cpdn_control/docs/Adding_new_model.md)
  Use this as the starting guide for integrating a new model through the current `ModelControl` seam.
- [docs/code_complexity_refactor_tracking.md](/home/glenn/github/cpdn_control/docs/code_complexity_refactor_tracking.md)
  Use this to track `main()` and related refactor complexity measurements over time; update it after significant tidy/refactor work.
- [docs/cpdn_control_data_flow.md](/home/glenn/github/cpdn_control/docs/cpdn_control_data_flow.md)
  Use this for a higher-level description of controller execution flow and data movement.
- [docs/input_file_refactor_plan.md](/home/glenn/github/cpdn_control/docs/input_file_refactor_plan.md)
  Use this for the broader input/configuration refactor context and longer-term future-work discussion.
- [docs/namelist_input_refactor_plan.md](/home/glenn/github/cpdn_control/docs/namelist_input_refactor_plan.md)
  Use this for the `fort.4` and model-control-input refactor history and design decisions.
- [docs/MACOS.md](/home/glenn/github/cpdn_control/docs/MACOS.md)
  Use this for macOS-specific build or environment notes.
- [docs/WINDOWS.md](/home/glenn/github/cpdn_control/docs/WINDOWS.md)
  Use this for Windows-specific build or environment notes.
