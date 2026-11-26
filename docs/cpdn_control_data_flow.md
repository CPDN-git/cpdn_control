# CPDN Control Data Flow

## Scope and ownership

This note describes the current controller interfaces and execution order. Use
[the file-flow guide](boinc_file_flow_summary.md) for archive paths and
[BOINC exit behaviour](boinc_exit_behaviour.md) for process ownership, exit
classification, and the lifecycle review decisions and remaining validation limits.

The shared controller selects and monitors a model, manages BOINC interaction,
and stages uploads. The model adapter owns its control-file format, input
manifest, restart interpretation, output readiness, and success checks. A model
need not use a Fortran namelist.

## Input sources

| Source | Reader | Destination and use |
| --- | --- | --- |
| BOINC `APP_INIT_DATA` | `init_boinc()` in `src/cpdn_control.cpp` | `BoincConfig`: app identity/version, project and BOINC directories, result/workunit names, CPU allocation, standalone mode. |
| Current working directory | `init_boinc()` | `BoincConfig::slot_path`; the controller runs in the BOINC slot. |
| Controller CLI | `parse_args()` then `process_args()` in `src/parse_args.cpp` | `TaskConfig`: batch, workunit, member ID, filename label, upload interval. |
| Optional trailing `--nthreads` | `get_app_config_nthreads()` in `src/cpdn_main.cpp` | Validated against the model's thread range; overrides `BoincConfig::ncpus`. |
| Model-owned input manifest | `ModelControl::get_input_manifest(workunit)` | Logical archive names and relative unzip directories. Available before model control input is parsed. |
| Model control file | `ModelControl::parse_control_input()` | Validated `ModelControlInputData`; model-specific values remain private to the adapter. |
| Model restart state and controller progress file | Model restart hooks and `initialize_task_state_from_restart()` | Initial `TaskState`, reconciled model step, restored CPU baseline and upload state. |
| BOINC runtime messages | BOINC API background processing; `boinc_get_status()` copies received flags | `BoincRuntime::client_status`, separate from child-process status. |

`boinc_get_status()` reads the latest received status snapshot; it does not
synchronously drain pending client IPC messages. Startup checks reduce unwanted
launches, and controller-owned cleanup handles requests observed after launch.

## Configuration boundaries

### BOINC environment: `BoincConfig`

`app_name` selects the model adapter and application archive. `app_version` is
reformatted for archive naming. `project_dir`, `boinc_dir`, `wu_name`,
`result_name`, and `ncpus` come from BOINC initialization. The CPU count is
clamped to at least one before any valid thread override is applied.

`slot_path` comes from the current working directory. `standalone` records
whether BOINC client interaction is available. `app_files` is declared but is
not currently populated.

### Task policy: `TaskConfig` and `ParseResult`

| Field | Source | Meaning |
| --- | --- | --- |
| `batch` | `--batch` | CPDN batch identifier. |
| `workunit` | `--workunit` | CPDN workunit identifier. |
| `memberid` | `--memberid` | CPDN member identifier. |
| `filename_label` | `--filename_label` | Opaque filename component used to resolve the task app bundle before model input is available. |
| `upload_interval` | `--upload_interval` | Controller upload interval in integer model steps; zero disables result uploads, including final uploads, but does not disable trickles. |

The parser accepts the five options above. `ParseResult::app_name` is a remaining
unused member; the model identity comes from BOINC. The parser permits extra
arguments so the separately handled `--nthreads` argument can be supplied by
BOINC task configuration. Removed fields such as `ancil_files`, `model_args`,
`fclen`, and `exptid` are not part of the current task configuration contract.

Do not infer dates, forecast duration, or OpenIFS `CUSTOP` from `filename_label`.
Upload policy comes from the CLI, not a CPDN comment header in `fort.4`.

### Model timing: `ModelControlInputData`

The adapter returns:

- `ok`, `source_file`, and structured error step/field/message information;
- `timestep_seconds`;
- `output_interval`, `restart_interval`, and `total_steps` in model steps;
- `forecast_length_time`, the forecast duration in seconds.

These values describe the whole run, including steps completed before a restart.
For example, WRF can rewrite restart start dates while preserving original-run
control information for timing calculations.

OpenIFS parses `fort.4` privately. It validates `CNMEXP` and stores the experiment
identifier in the OpenIFS adapter for filename generation. It converts `UTSTEP`,
`NFRPOS`, `NFRRES`, and `CUSTOP` into the shared timing result. `CUSTOP` can be a
step count or a supported hour/day duration. Negative OpenIFS output/restart
intervals represent hours and are converted to steps. Resolution, grid, and
experiment identifiers are not fields of the shared result.

WRF owns `namelist.input` and its restart rewrites. Its model-specific timestamp
and domain handling remains outside `main()`.

## Current execution order

1. Initialize BOINC, create the model adapter, parse task arguments, and apply a
   valid thread override.
2. Read and log received BOINC status before expensive staging. Observed
   quit/abort/heartbeat loss prevents launch; suspension defers startup.
3. Stage the application executable archive, then the task app bundle.
4. Call `setup_directories(slot_path)`, obtain the model input manifest, and
   stage its remaining archives.
5. Call `setup(slot_path)` and `parse_control_input()` on the model adapter,
   then derive trickle cadence from the validated timing data.
6. Reconcile model restart metadata and `cpdn_progressfile.txt`, prepare transient
   controller-run state, and write initial progress.
7. Derive result naming and create `UploadManager` and `TrickleHandler`. The upload interval remains `TaskConfig` policy.
8. Validate executable presence/permissions, check and log received BOINC status
   immediately before launch, then launch an owned model process set.
9. Read and log status immediately after launch and apply child process control
   if needed. The run loop then polls at its normal cadence.
10. On observed step changes, invoke model step tasks, collect files the model
    declares safe to copy, process scheduled uploads and trickles, and write
    progress.
11. Follow the selected exit route through `finish_task_exit()` in
    `src/task_lifecycle.cpp`.

OpenIFS diagnostics are invoked through the model's step hook and implemented
in `models/openifs/external_diagnostics.cpp`. They run before the controller
collects that iteration's copyable outputs. Their generated `trickle_data`
remains in the slot for `TrickleHandler`.

## Runtime and progress state

`TaskState::current_step`, `last_completed_step`, `last_trickle_step`, and
`last_upload_step` are integer step counts. Elapsed model time is derived from
steps and `timestep_seconds`; it is not stored in these step fields.

`prior_acc_cpu_time` is the accumulated CPU baseline restored from an earlier
controller run. `current_cpu_time` includes that baseline plus the current model
child's CPU time. Startup seeds the live total from the baseline before the
first progress write. The progress file stores the accumulated total; the
baseline remains fixed during the current controller process. The current
progress file writes `control_pid`, accumulated CPU time under the historical
`prior_acc_cpu_time` key, `upload_file_number`, `last_completed_step`,
`last_upload_step`, and `model_completed`. `last_trickle_step` is an in-memory
field and is not currently persisted by this handler.

`child_status` describes only the model leader: running, exited, terminated,
suspended, or unavailable. It must not encode BOINC quit/abort/suspend meanings.
A terminal leader status does not prove that all descendants have exited.

`child_process` retains launch ownership after the leader exits. POSIX uses a
private process group and a waitable leader; Windows retains process and Job
Object handles. `child_cleanup` records the model cleanup attempt so recovery
and final exit do not restart its two-second confirmation budget. This state is
transient and is not restored from the progress file.

## Finalization review decisions

The existing normal-completion path performs model finalization and upload work
before the final exit helper attempts remaining-descendant cleanup. A failed or
expired cleanup result does not currently override its chosen completion code.
Controller-error recovery instead attempts cleanup before upload work and skips
all remaining upload finalization if termination is unconfirmed, including when
only zombie descendants keep a POSIX group present.

Review accepted these behaviours for the current OpenIFS and WRF models: their
parallel workers are threads and they create no model subprocess descendants.
No further code change is required for those two findings. Reassess the policies
before adding a model with subprocess workers. OpenIFS diagnostics separately
validate execution, cleanup and freshness before accepting `trickle_data`;
failed or expired helper cleanup discards the output without aborting the model.

The two-second cleanup confirmation budget does not bound total shutdown
latency: synchronous diagnostics can run for their separate 30-second execution
timeout before `main()` observes a BOINC request.

## Source references

- [Controller configuration and state](../src/cpdn_control.h)
- [Controller orchestration](../src/cpdn_main.cpp)
- [Startup reconciliation](../src/control_start.cpp)
- [Model interface](../api/model_control.h)
- [Input manifest types](../api/model_input_manifest.h)
- [Final exit handling](../src/task_lifecycle.cpp)
- [Input/configuration future work](input_file_refactor_plan.md)
