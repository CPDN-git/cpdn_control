# CPDN Control Data Flow

## Purpose

This note maps where setup data currently comes from before it is used by `cpdn_control`.

There are two broad classes of values in the current code:

1. Controller-facing values needed by `cpdn_control` itself.
2. Model-facing values needed to prepare and run the model process.

Today these are split across BOINC init data, controller command-line arguments, CPDN-injected metadata inside `fort.4`, normal Fortran namelist entries inside `fort.4`, and a number of values derived in `main()`.

## Main Input Sources

| Source | Where read | Notes |
| --- | --- | --- |
| BOINC init data (`APP_INIT_DATA`) | `init_boinc()` in `src/cpdn_control.cpp` | Supplies BOINC/task environment values. |
| Controller CLI args | `parse_args()` in `src/parse_args.cpp`, then `process_args()` in `src/cpdn_main.cpp` | Supplies CPDN task identifiers and forecast length. |
| Raw trailing `--nthreads` arg | Directly inspected in `main()` in `src/cpdn_main.cpp` | Bypasses `ParseResult` and `TaskConfig`. |
| CPDN header metadata in `fort.4` | Parsed in `main()` in `src/cpdn_main.cpp` | Comment-style `!KEY=VALUE` lines near top of file. |
| Standard Fortran namelist values in `fort.4` | Parsed in `main()` in `src/cpdn_main.cpp` | Used for model timing/output scheduling. |
| Derived values | Various calculations in `main()` | Used for filenames, scheduling, progress, upload cadence, and state tracking. |

## Current Flow Summary

| Stage | Data pulled in | Output of stage |
| --- | --- | --- |
| BOINC init | app version/name, project dir, result/workunit names, CPU count, standalone flag | `BoincConfig` |
| CLI parse | batch, workunit, memberid, startdate, forecast length | `ParseResult` then `TaskConfig` |
| CLI override | `--nthreads` if present at end of argv | `nthreads`, `nthreads_int`, also overwrites `bconfig.ncpus` |
| `fort.4` CPDN header parse | archive names and metadata like `UPLOAD_INTERVAL` | loose locals in `main()` |
| `fort.4` namelist parse | `UTSTEP`, `NFRPOS`, `NFRRES` | loose locals in `main()` |
| Derived calculations | `num_days`, `total_nsteps`, `trickle_freq`, upload paths, etc. | more loose locals in `main()` |
| Runtime monitoring | child pid, progress, upload counters, current step/cpu/fraction done | `TaskState` plus some additional locals |

## `BoincConfig` Field Sources

| Field | Current source | Classification | Notes |
| --- | --- | --- | --- |
| `app_version` | `APP_INIT_DATA.app_version` in `init_boinc()`, then reformatted | BOINC | Stored as string and dotted manually. |
| `app_name` | `APP_INIT_DATA.app_name` in `init_boinc()` | BOINC | Used to choose model type and app zip name. |
| `project_dir` | `APP_INIT_DATA.project_dir` in `init_boinc()` | BOINC | Trailing slash appended. |
| `wu_name` | `APP_INIT_DATA.wu_name` in `init_boinc()` | BOINC | Used by trickle handler. |
| `result_name` | `APP_INIT_DATA.result_name` in `init_boinc()` | BOINC | Currently not central in `main()`. |
| `ncpus` | `APP_INIT_DATA.ncpus` in `init_boinc()` | BOINC, then overridden | Minimum clamped to 1. Later overwritten if trailing `--nthreads` is present. |
| `boinc_dir` | `APP_INIT_DATA.boinc_dir` in `init_boinc()` | BOINC | Trailing slash appended. Not currently used much. |
| `app_files` | No current population | None / unused | Declared but not currently filled. |
| `slot_path` | `std::filesystem::current_path()` in `init_boinc()` | Derived | BOINC gives slot number, current code derives full path from cwd. |
| `standalone` | `boinc_is_standalone()` in `init_boinc()` | BOINC | Controls BOINC upload/trickle/reporting behavior. |

## `TaskConfig` Field Sources

| Field | Current source | Classification | Notes |
| --- | --- | --- | --- |
| `batch` | `--batch` via `ParseResult` -> `process_args()` | Controller CLI | CPDN task identifier. |
| `workunit` | `--workunit` via `ParseResult` -> `process_args()` | Controller CLI | CPDN task identifier. |
| `memberid` | `--memberid` via `ParseResult` -> `process_args()` | Controller CLI | CPDN task identifier / UMID. |
| `startdate` | `--startdate` via `ParseResult` -> `process_args()` | Controller CLI | Used in archive naming. |
| `exptid` | Hard-coded to `"NSET"` in `process_args()` | Placeholder / incorrect source | Comments already note this should come from the model/namelist, not CLI. |
| `fclen` | `--fcast_len` via `ParseResult` -> `process_args()` | Controller CLI | Used to derive `num_days` and construct namelist zip name. Comment in code says this should come from `fort.4`. |

## `ParseResult` Fields Not Properly Folded Into Config

These are parsed but are either ignored or only partially used:

| Field | Current source | Current status |
| --- | --- | --- |
| `app_name` | `--app_name` | Parsed but not used; BOINC `app_name` is used instead. |
| `upload_interval` | `--upload_interval` | Parsed but ignored; current upload interval comes from CPDN header metadata in `fort.4`. |
| `ancil_files` | `--cpdn_ancil_files` | Parsed but ignored. |
| `model_args` | `--model_args` | Parsed but ignored. |

This is one sign that controller setup data is currently split between an intended CLI schema and the legacy `fort.4` metadata path.

## `fort.4` Values Read In `main()`

### Normal Fortran namelist values

| Local variable in `main()` | `fort.4` key | Used for | Classification |
| --- | --- | --- | --- |
| `timestep` | `UTSTEP` | Convert steps to seconds, derive trickle and total step counts | Shared controller/model timing |
| `ICM_file_interval` | `NFRPOS` | Intended to describe output production cadence | Shared, but currently underused |
| `restart_interval` | `NFRRES` | Restart/progress restart logic | Shared controller/model timing |

### Important values present in the model domain but not sourced cleanly for controller use

| Value | Current controller handling | Note |
| --- | --- | --- |
| `exptid` / `EXPTID` | Not read from `fort.4`; controller uses hard-coded `TaskConfig.exptid = "NSET"` | This is the clearest current mismatch. The model/test code reads `EXPTID` directly from `fort.4`, but the controller does not. |
| Forecast length / stop condition | Controller uses CLI `--fcast_len` via `TaskConfig.fclen` | Code comments already note this should come from the model configuration / namelist instead of CLI. |

## Derived Values in `main()` That Are Not In Config Structs

These are important values, but they are currently loose locals rather than grouped configuration/state.

| Variable | Derived from | Role |
| --- | --- | --- |
| `nthreads` | `bconfig.ncpus`, optionally overridden by trailing `--nthreads` | Model launch env setup |
| `nthreads_int` | Parsed integer form of `nthreads` | Progress/fraction calculations |
| `num_days` | `atof(tconfig.fclen.c_str())` | Simulation length in days |
| `namelist_zip_path` / `namelist_zip` | `app_name`, `memberid`, `startdate`, `fclen`, `batch`, `workunit` | Locate task namelist zip |
| `namelist_file` | `slot_path` + `fort.4` | Main model/controller metadata file |
| `trickle_freq` | `timestep`, `total_nsteps` via `TrickleHandler::get_trickle_frequency()` | Controller trickle cadence |
| `total_nsteps` | `num_days`, `timestep` | Total step count estimate |
| `total_length_of_simulation` | `num_days` | Total length in seconds |
| `upload_dir` | `project_dir`, `app_name`, `workunit` | Temp staging directory for uploads |
| `result_base_name` | `BoincConfig` + `TaskConfig` | Upload/trickle naming |
| `model_exe` | `slot_path`, model selection | Child executable path |
| `diag_exe` | `slot_path`, fixed filename | Optional diagnostics path |

## Runtime / Progress Variables Outside The Input Structs

`TaskState` already groups much of the mutable runtime state. That is good, but there are still related values outside it.

### `TaskState` itself

`TaskState` is the current runtime state holder for:

- last known step/upload/trickle/cpu values
- model pid and child status
- completion/success flags
- fraction done

This is already the right direction. The main issue is that some state uses mixed units and only part of it is persisted to `progress_file`.

### Additional runtime locals in `main()`

| Variable | Current role | Comment |
| --- | --- | --- |
| `step` | Current step as a string from model polling | Repeatedly converted to int. This is one of the muddier variables. |
| `delay_count` | Loop delay counter | Upload polling cadence control. |
| `delay_max` | Active loop delay target | Toggled by diagnostics run. |
| `zfl` | List of files to zip for upload | Temporary upload working set. |
| `restart_ctl_exists` | Snapshot of restart-file presence before startup recovery logic | Narrow-scope state, ok as local. |

## Observations About The Current Split

### Controller-facing values

These are currently mainly:

- BOINC execution context from `BoincConfig`
- CPDN task identifiers from `TaskConfig`
- upload/trickle/restart cadence values parsed from `fort.4`
- runtime monitoring/progress state in `TaskState`

### Model-facing values

These are currently mainly:

- model executable path and model selection
- thread count / environment variables
- ancillary archive names and derived unpack directories
- `fort.4` itself
- restart control file (`rcf`)

### Where things are muddled

| Issue | Current symptom |
| --- | --- |
| Controller config is split across CLI and `fort.4` metadata | `TaskConfig` only holds some controller data; `upload_interval` and input archive names live as loose locals. |
| Model identity/config is split between BOINC app name, model class, and `fort.4` | `exptid` is the clearest example. |
| `TaskConfig` contains some values that code comments say should not really come from CLI | `fclen`, `exptid`. |
| `ParseResult` includes fields that are not actually part of the working data flow | `app_name`, `upload_interval`, `ancil_files`, `model_args`. |
| Runtime state uses mixed units and duplicated representations | `step` is in model steps as a string, while `TaskState.current_step` is seconds. |

## Minimal Refactoring Suggestions

Keep this simple. The smallest useful tidy-up would be:

1. Add a dedicated struct for values parsed from `fort.4`.
   Suggested scope: archive names, resolution metadata, `upload_interval`, `timestep`, `NFRPOS`, `NFRRES`, and eventually `exptid`.

2. Keep `TaskConfig` for controller CLI/task identity only.
   Good candidates to keep here: `batch`, `workunit`, `memberid`, `startdate`.
   Good candidates to remove later: `exptid`, `fclen`.

3. Keep `BoincConfig` strictly for BOINC/runtime environment data.
   This struct is already close to that role.

4. Add a small derived scheduling struct if desired.
   Suggested scope: `nthreads`, `nthreads_int`, `num_days`, `timestep`, `restart_interval`, `trickle_freq`, `total_nsteps`, `total_length_of_simulation`.
   This would remove a large cluster of loosely related locals from `main()`.

5. Normalize step/time handling.
   The simplest improvement would be to hold the in-memory current step as an integer step count, and derive seconds only where needed.

6. Move `exptid` sourcing into model/namelist handling.
   This looks like the highest-value cleanup because it is already known to be wrong in the current controller flow.

## Suggested Future Struct Split

One simple target structure could be:

- `BoincConfig`
  BOINC-provided runtime/environment values only.

- `TaskConfig`
  CPDN CLI/task identity values only.

- `Fort4Config` or `ModelInputConfig`
  Values parsed from `fort.4`, including both CPDN-injected metadata and model namelist values needed by the controller.

- `RunSchedule`
  Derived timing/scheduling values computed from `TaskConfig` + `Fort4Config` + `BoincConfig`.

- `TaskState`
  Mutable runtime progress and child-process state.

That would keep the current design direction but make the data flow much easier to follow and much easier to tidy later.
