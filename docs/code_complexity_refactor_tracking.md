# Code complexity refactor tracking

This document records the measured code-complexity changes during the recent controller refactors.

Measurements were taken with:

- `pmccabe`
- `/home/glenn/.local/bin/lizard`

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

## Current observations

- `main()` is still the dominant complexity hotspot, but it is materially smaller than the original baseline.
- Complexity has been moved to more coherent ownership boundaries:
  - `OpenIFSControl::parse_control_input()` for model-specific control-file parsing
  - `initialize_task_state_from_restart()` for controller restart/progress bootstrap
  - `zip_and_send_upload()` removes duplicated upload-send mechanics, but the overall `main()` metric did not improve further because the caller-side branching and file collection still remain in `main()`.
- `process_args(...)` was a good cohesion move as well as a useful reduction in `main()` complexity; the parsing/validation split now lives together in `src/parse_args.cpp`.
- The next likely low-hanging fruit remains the per-step processing and upload/trickle block inside `main()`.
