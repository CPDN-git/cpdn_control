# Code complexity refactor tracking

This document records the measured code-complexity changes during the recent controller refactors.

Measurements were taken with:

- `pmccabe`
- `/home/glenn/.local/bin/lizard`

## Target for `main()`

The goal is not a tiny `main()` made up of many thin helpers. For this controller, `main()` is expected to remain the top-level orchestration flow for:

- BOINC lifecycle
- model launch and polling
- restart/bootstrap handling
- upload/trickle coordination
- final shutdown

The target is therefore a readable orchestration function with clear subtask boundaries, not the smallest possible metric.

Current working target for `[src/cpdn_main.cpp](/home/glenn/github/cpdn_control/src/cpdn_main.cpp)` `main()`:

- `pmccabe` roughly `40-55`
- `lizard` NLOC roughly `220-300`

That range is intended as a practical stopping point for this codebase. If `main()` is pushed much below that, there is a risk of over-extraction and weaker cohesion unless the new helper boundaries are very clearly justified.

## Baseline: before namelist refactor

Measured on 2026-04-09.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 120 | 120 | 516 | Primary complexity hotspot |
| `src/cpdn_main.cpp` | `process_args()` | 10 | 10 | 49 | Secondary helper, moderate complexity |
| `src/cpdn_control.cpp` | `resolve_boinc_input_file()` | 10 | 10 | 33 | Highest complexity in controller helpers |
| `src/cpdn_control.cpp` | `handle_boinc_client_status()` | 9 | 9 | 42 | Moderate complexity |
| `models/openifs/oifs_control.cpp` | `restart_ctl_read()` | 5 | 5 | 19 | Low complexity in model layer |

## After namelist parser extraction

Measured on 2026-04-10 after moving `fort.4` parsing into `OpenIFSControl::parse_control_input()`.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 88 | 88 | 399 | Still the primary hotspot, but materially reduced |
| `src/cpdn_main.cpp` | `process_args()` | 10 | 10 | 49 | Essentially unchanged |
| `src/cpdn_control.cpp` | `resolve_boinc_input_file()` | 10 | 10 | 33 | Unchanged controller helper hotspot |
| `src/cpdn_control.cpp` | `handle_boinc_client_status()` | 9 | 9 | 42 | Unchanged controller helper |
| `models/openifs/oifs_control.cpp` | `parse_control_input()` | 37 | 37 | 118 | New model-layer parsing hotspot created by the extraction |

Comparison against baseline:

| Function | Before `pmccabe` | After `pmccabe` | Change | Before `lizard` NLOC | After `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 120 | 88 | -32 (`-26.7%`) | 516 | 399 | -117 (`-22.7%`) |

## After restart/progress bootstrap extraction

Measured on 2026-04-10 after extracting the restart/progress-file reconciliation block into `initialize_task_state_from_restart()`.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 74 | 74 | 359 | Primary hotspot further reduced |
| `src/cpdn_main.cpp` | `initialize_task_state_from_restart()` | 19 | 19 | 62 | New startup-state helper extracted from `main()` |
| `src/cpdn_main.cpp` | `process_args()` | 10 | 10 | 49 | Unchanged |
| `src/cpdn_control.cpp` | `resolve_boinc_input_file()` | 10 | 10 | 33 | Unchanged controller helper hotspot |
| `models/openifs/oifs_control.cpp` | `parse_control_input()` | 37 | 37 | 118 | Largest non-`main()` hotspot |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 88 | 74 | -14 (`-15.9%`) | 399 | 359 | -40 (`-10.0%`) |

Comparison against the original baseline:

| Function | Baseline `pmccabe` | Current `pmccabe` | Change | Baseline `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 120 | 74 | -46 (`-38.3%`) | 516 | 359 | -157 (`-30.4%`) |

## After zip/upload flow deduplication

Measured on 2026-04-11 after extracting the shared zip/wait/upload/status sequence into `zip_and_send_upload()`.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 77 | 77 | 365 | Main remains the dominant hotspot |
| `src/cpdn_main.cpp` | `initialize_task_state_from_restart()` | 19 | 19 | 62 | Unchanged startup-state helper |
| `src/cpdn_main.cpp` | `zip_and_send_upload()` | 8 | 8 | 53 | New shared upload-send helper |
| `src/cpdn_control.cpp` | `resolve_boinc_input_file()` | 10 | 10 | 33 | Unchanged controller helper hotspot |
| `models/openifs/oifs_control.cpp` | `parse_control_input()` | 37 | 37 | 118 | Largest non-`main()` hotspot |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 74 | 77 | +3 (`+4.1%`) | 359 | 365 | +6 (`+1.7%`) |

Comparison against the original baseline:

| Function | Baseline `pmccabe` | Current `pmccabe` | Change | Baseline `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 120 | 77 | -43 (`-35.8%`) | 516 | 365 | -151 (`-29.3%`) |

## After moving controller arg processing into parse_args module

Measured on 2026-04-11 after moving `process_args(...)` from `src/cpdn_main.cpp` into `src/parse_args.cpp`.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 69 | 69 | 339 | Main reduced again after removing controller arg translation from this file |
| `src/cpdn_main.cpp` | `initialize_task_state_from_restart()` | 19 | 19 | 62 | Unchanged startup-state helper |
| `src/cpdn_main.cpp` | `zip_and_send_upload()` | 8 | 8 | 53 | Unchanged shared upload-send helper |
| `src/cpdn_control.cpp` | `resolve_boinc_input_file()` | 10 | 10 | 33 | Unchanged controller helper hotspot |
| `models/openifs/oifs_control.cpp` | `parse_control_input()` | 37 | 37 | 118 | Largest non-`main()` hotspot |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 77 | 69 | -8 (`-10.4%`) | 365 | 339 | -26 (`-7.1%`) |

Comparison against the original baseline:

| Function | Baseline `pmccabe` | Current `pmccabe` | Change | Baseline `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 120 | 69 | -51 (`-42.5%`) | 516 | 339 | -177 (`-34.3%`) |

## After timestamped stderr logging wrapper

Measured on 2026-04-11 after installing a timestamping `std::cerr` streambuf wrapper and converting the remaining controller `fprintf(stderr, ...)` call sites to `std::cerr`.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 69 | 69 | 340 | Main complexity unchanged; line count changed slightly |
| `src/cpdn_main.cpp` | `initialize_task_state_from_restart()` | 19 | 19 | 62 | Unchanged startup-state helper |
| `src/cpdn_main.cpp` | `zip_and_send_upload()` | 8 | 8 | 53 | Unchanged shared upload-send helper |
| `src/cpdn_control.cpp` | `resolve_boinc_input_file()` | 10 | 10 | 33 | Unchanged controller helper hotspot |
| `models/openifs/oifs_control.cpp` | `parse_control_input()` | 37 | 37 | 118 | Largest non-`main()` hotspot |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 69 | 69 | `0` | 339 | 340 | +1 (`+0.3%`) |

Comparison against the original baseline:

| Function | Baseline `pmccabe` | Current `pmccabe` | Change | Baseline `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 120 | 69 | -51 (`-42.5%`) | 516 | 340 | -176 (`-34.1%`) |

## After extracting startup seam into `control_start`

Measured on 2026-04-11 after moving `initialize_task_state_from_restart()` from `src/cpdn_main.cpp` into `src/control_start.cpp` and adding direct unit coverage for the startup/restart state machine.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 69 | 69 | 340 | Main complexity unchanged; orchestration logic still dominates |
| `src/control_start.cpp` | `initialize_task_state_from_restart()` | 19 | 19 | 62 | Startup/restart seam extracted and directly unit-tested |
| `src/cpdn_main.cpp` | `zip_and_send_upload()` | 8 | 8 | 53 | Unchanged shared upload-send helper |
| `src/cpdn_control.cpp` | `resolve_boinc_input_file()` | 10 | 10 | 33 | Unchanged controller helper hotspot |
| `models/openifs/oifs_control.cpp` | `parse_control_input()` | 37 | 37 | 118 | Largest non-`main()` hotspot |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 69 | 69 | `0` | 340 | 340 | `0` |

Comparison against the original baseline:

| Function | Baseline `pmccabe` | Current `pmccabe` | Change | Baseline `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 120 | 69 | -51 (`-42.5%`) | 516 | 340 | -176 (`-34.1%`) |

## After extracting cross-platform process-control seam

Measured on 2026-04-11 after moving child launch/poll/suspend/resume/terminate implementation into `lib/process_control_posix.cpp` and `lib/process_control_windows.cpp`, while keeping BOINC policy in `src/cpdn_control.cpp`.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 67 | 67 | 342 | Main reduced slightly after child-handle integration and upload-dir cleanup |
| `src/cpdn_main.cpp` | `zip_and_send_upload()` | 8 | 8 | 53 | Unchanged helper complexity |
| `src/cpdn_control.cpp` | `handle_boinc_client_status()` | 17 | 17 | 61 | BOINC policy is still here and got more complex as platform actions were abstracted underneath it |
| `src/cpdn_control.cpp` | `resolve_boinc_input_file()` | 10 | 10 | 33 | Unchanged controller helper hotspot |
| `lib/process_control_posix.cpp` | `poll_child_process()` | 10 | 11 | 38 | New POSIX backend hotspot |
| `lib/process_control_posix.cpp` | `start_child_process()` | 9 | 9 | 44 | New POSIX backend launch helper |
| `src/control_start.cpp` | `initialize_task_state_from_restart()` | 19 | 19 | 62 | Unchanged startup-state helper |
| `models/openifs/oifs_control.cpp` | `parse_control_input()` | 37 | 37 | 118 | Largest non-`main()` hotspot remains in the model layer |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 69 | 67 | -2 (`-2.9%`) | 340 | 342 | +2 (`+0.6%`) |

Comparison against the original baseline:

| Function | Baseline `pmccabe` | Current `pmccabe` | Change | Baseline `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 120 | 67 | -53 (`-44.2%`) | 516 | 342 | -174 (`-33.7%`) |

## After moving model environment setup behind `ModelControl::get_env_vars()`

Measured on 2026-06-12 after replacing the OpenIFS-specific `oifs_get_model_env_vars(...)` path with model-owned `get_env_vars(...)` overrides and switching child env storage to `KEY=VALUE` strings.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 67 | 67 | 323 | Main orchestration complexity unchanged; line count dropped modestly |
| `src/cpdn_control.cpp` | `launch_process()` | 4 | 4 | 22 | Launch path is simpler now that env construction is delegated to the model |
| `src/external_diagnostics.cpp` | `run_step_diagnostics()` | 9 | 9 | 49 | Diagnostics now reuses the model env getter rather than calling OpenIFS helpers directly |
| `models/openifs/oifs_control.cpp` | `OpenIFSControl::get_env_vars()` | 2 | 2 | 22 | Small model-owned env builder extracted from the old utility layer |
| `models/wrf/wrf_control.cpp` | `WRFControl::get_env_vars()` | 2 | 2 | 10 | New WRF env seam currently only supplies `OMP_NUM_THREADS` |
| `lib/process_control_posix.cpp` | `start_child_process()` | 11 | 11 | 51 | Slightly higher due to validation of `KEY=VALUE` env entries |
| `lib/utils.cpp` | `run_process_with_timeout()` | 27 | 48 | 170 | Timed-process env handling changed representation but remains the dominant utility hotspot |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 67 | 67 | `0` | 342 | 323 | -19 (`-5.6%`) |

## After replacing the regex filename seam with model-owned string matching

Measured on 2026-06-12 after removing `get_output_filename_regex()` from `ModelControl`, switching upload-file discovery to `is_output_filename(...)`, and moving OpenIFS/WRF filename shape checks into model-owned string helpers.

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 67 | 67 | 323 | Top-level orchestration complexity unchanged; upload scan no longer depends on `std::regex` |
| `src/cpdn_main.cpp` | `add_upload_files()` | 6 | 6 | 23 | Small helper still generic, but now delegates matching to the model instance |
| `models/openifs/oifs_control.cpp` | `parse_oifs_output_filename()` | 7 | 7 | 17 | Simple OpenIFS filename validator replaces the old regex seam |
| `models/openifs/oifs_control.cpp` | `OpenIFSControl::is_output_filename()` | 1 | 1 | 1 | Thin model-owned predicate built on the private parser |
| `models/wrf/wrf_control.cpp` | `is_wrf_datetime_suffix()` | 8 | 8 | 13 | Simple fixed-layout datetime validator for WRF filename shapes |
| `models/wrf/wrf_control.cpp` | `WRFControl::is_output_filename()` | 1 | 1 | 4 | WRF output matching is now contained in the model layer |
| `models/wrf/wrf_control.cpp` | `WRFControl::is_restart_filename()` | 1 | 1 | 4 | WRF restart matching follows the same contained seam |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 67 | 67 | `0` | 323 | 323 | `0` |

## After moving step diagnostics behind `OpenIFSControl::do_step_tasks()`

Measured on 2026-06-13 after:

- moving the diagnostics helper from `src/external_diagnostics.cpp` to `models/openifs/external_diagnostics.cpp`
- removing the direct `run_step_diagnostics(...)` call sites from `src/cpdn_main.cpp`
- routing both the polling-loop and final-step diagnostics trigger through `OpenIFSControl::do_step_tasks(...)`

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 67 | 67 | 318 | Top-level orchestration is slightly shorter; OpenIFS step diagnostics no longer leaks into `main()` |
| `models/openifs/oifs_control.cpp` | `OpenIFSControl::do_step_tasks()` | 2 | 2 | 8 | Small model-owned step hook now owns diagnostics dispatch and duplicate-step suppression |
| `models/openifs/external_diagnostics.cpp` | `run_step_diagnostics()` | 9 | 9 | 49 | Diagnostics helper keeps the same local complexity after moving under model ownership |

Comparison against the previous refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 67 | 67 | `0` | 323 | 318 | -5 (`-1.5%`) |
| `run_step_diagnostics()` | 9 | 9 | `0` | 49 | 49 | `0` |

## Current observations

- `main()` is still the dominant complexity hotspot, but it is materially smaller than the original baseline.
- Complexity has been moved to more coherent ownership boundaries:
  - `OpenIFSControl::parse_control_input()` for model-specific control-file parsing
  - `initialize_task_state_from_restart()` in `src/control_start.cpp` for controller restart/progress bootstrap
  - `zip_and_send_upload()` removes duplicated upload-send mechanics, but the overall `main()` metric did not improve further because the caller-side branching and file collection still remain in `main()`.
- The process-control refactor improved platform separation more than raw complexity numbers:
  - low-level POSIX child-process mechanics now live in `lib/process_control_posix.cpp`
  - a matching Windows implementation path exists in `lib/process_control_windows.cpp`
  - `handle_boinc_client_status()` became more complex because it still owns the BOINC-driven suspend/abort/quit policy while delegating the platform actions underneath
- The model-env refactor improved ownership more than top-level control-flow complexity:
  - model-specific launch environment assembly now lives behind `ModelControl::get_env_vars()`
  - both launch and diagnostics now use the same model-owned env seam
  - the new WRF override establishes the same seam without adding much complexity
- The filename-seam refactor follows the same containment pattern:
  - upload-file discovery now asks the model whether a filename matches
  - OpenIFS and WRF filename layouts are checked with simple string helpers rather than `std::regex`
  - model-specific parsing remains private to each model implementation instead of being lifted into `ModelControl`
- The step-diagnostics refactor follows the same containment pattern:
  - `main()` now triggers only the generic model step hook
  - OpenIFS keeps the diagnostics executable discovery and dispatch behind `setup(...)` and `do_step_tasks(...)`
  - the helper remains split into a separate OpenIFS-owned translation unit so it can evolve without growing `oifs_control.cpp`
- `process_args(...)` was a good cohesion move as well as a useful reduction in `main()` complexity; the parsing/validation split now lives together in `src/parse_args.cpp`.
- The timestamped logging refactor improved operability but, as expected, did not materially change control-flow complexity.
- The `control_start` extraction is valuable mainly because it creates a direct unit-test seam for a high-risk state machine, even though it does not reduce the `main()` metric further.
- The next likely low-hanging fruit is now more clearly the upload/trickle portion of the per-step block inside `main()` rather than the OpenIFS-specific diagnostics dispatch.

## After hardening process-control launch and containment

Measured on 2026-04-11 after:

- changing the POSIX backend so `argv`/`envp` are prepared in the parent and the child branch is limited to `chdir(...)`, `setrlimit(...)`, `execve(...)`, and `_exit(...)`
- adding a small child-status mapping helper in `src/cpdn_control.cpp`
- tightening the Windows backend design around environment handling and process-tree containment

| File | Function | `pmccabe` | `lizard` CCN | `lizard` NLOC | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `src/cpdn_main.cpp` | `main()` | 67 | 67 | 342 | Unchanged dominant hotspot |
| `src/cpdn_main.cpp` | `zip_and_send_upload()` | 8 | 8 | 53 | Unchanged helper complexity |
| `src/cpdn_control.cpp` | `handle_boinc_client_status()` | 17 | 17 | 61 | Unchanged BOINC policy hotspot |
| `src/cpdn_control.cpp` | `check_child_status()` | 7 | 7 | 24 | Small helper after child-status mapping extraction |
| `lib/process_control_posix.cpp` | `poll_child_process()` | 10 | 11 | 43 | POSIX backend polling hotspot unchanged |
| `lib/process_control_posix.cpp` | `start_child_process()` | 9 | 9 | 47 | Parent-side prep plus minimal child branch keeps the old launch model without child-side C++ setup |
| `src/control_start.cpp` | `initialize_task_state_from_restart()` | 19 | 19 | 62 | Unchanged startup-state helper |
| `models/openifs/oifs_control.cpp` | `parse_control_input()` | 37 | 37 | 118 | Largest non-`main()` hotspot remains in the model layer |

Comparison against the previous process-control refactor point:

| Function | Prior `pmccabe` | Current `pmccabe` | Change | Prior `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 67 | 67 | `0` | 342 | 342 | `0` |
| `lib/process_control_posix.cpp::start_child_process()` | 9 | 9 | `0` | 44 | 47 | +3 (`+6.8%`) |

Comparison against the original baseline:

| Function | Baseline `pmccabe` | Current `pmccabe` | Change | Baseline `lizard` NLOC | Current `lizard` NLOC | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `src/cpdn_main.cpp::main()` | 120 | 67 | -53 (`-44.2%`) | 516 | 342 | -174 (`-33.7%`) |
