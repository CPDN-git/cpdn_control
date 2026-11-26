# CPDN Controller: BOINC File Flow

## Directories and archive roles

The controller runs in a BOINC slot. The project directory contains downloaded
archives and BOINC result destinations. A task-specific directory under the
project directory temporarily holds staged model outputs.

| Role | Example or pattern | Treatment |
| --- | --- | --- |
| Controller binary | `cpdn_control_<version>_<platform>` | Launched by BOINC; not extracted from the model executable archive. |
| Application executable archive | `<app_name>_app_<app_version>_<platform>.zip` | Copied from the project directory into the slot, extracted there, and the slot archive removed after successful extraction. |
| Task app bundle | `<app_name>_<memberid>_<filename_label>_<batch>_<workunit>.zip` | BOINC logical input, normally containing model control files and task-specific inputs. Resolved and staged through the `jf_*` input path. |
| Model manifest archives | For example `ic_ancil_<workunit>.zip` | BOINC logical inputs; model declares each name and unzip destination. |
| Controller progress | `cpdn_progressfile.txt` in the slot | Persists accumulated CPU time, model-step and upload progress, and completion state. |
| Staged outputs | `<project_dir>/<app_name>_<workunit>/` | Holds successfully copied model outputs before packaging. |

The executable archive and the task app bundle are distinct inputs. The generic
controller obtains the executable name from the model adapter; it does not
assume that the task app bundle contains that executable.

## Preserve BOINC logical inputs

A slot logical input can be a native symlink or a BOINC `<soft_link>` file. The
functional harness uses the latter. Both identify a physical project archive
such as `jf_<md5>`.

The controller must preserve the logical input on first run and restart. It
resolves the name, validates the project archive checksum, copies that physical
archive to the selected slot destination, and extracts from the copy. It must
not copy downloaded archive bytes over the logical link file.

For example, an illustrative OpenIFS input layout is:

```text
projects/climateprediction.net/
    <app_name>_app_<app_version>_<platform>.zip
    jf_<app_bundle_md5>
    jf_<initial_conditions_md5>
    jf_<ifsdata_md5>
slots/0/
    <app_name>_<memberid>_<filename_label>_<batch>_<workunit>.zip
        -> project jf_<app_bundle_md5>
    ic_ancil_<workunit>.zip
        -> project jf_<initial_conditions_md5>
    ifsdata_<workunit>.zip
        -> project jf_<ifsdata_md5>
```

`filename_label` is opaque filename metadata used before model control input is
available. It is not a forecast duration or a model start date to parse.

## Startup order

Function names below refer to [main()](../src/cpdn_main.cpp) and
[controller staging helpers](../src/cpdn_control.cpp); source line numbers are
intentionally omitted because they change as the controller evolves.

1. `init_boinc()` determines app identity, project/slot paths and standalone mode.
   `create_model_control()` selects the model adapter. CLI parsing supplies the
   task identifiers, upload interval and optional thread override.
2. `check_boinc_before_launch(..., "before_staging")` logs the latest received
   BOINC status. Observed shutdown prevents launch; suspension defers startup.
   The getter reads received flags and does not synchronously drain pending IPC.
3. `stage_and_unzip_app_file()` copies and extracts the application executable
   archive. Unlike task/manifest staging, this helper constructs the project
   archive path directly and does not use the `jf_*` checksum helper.
4. `stage_boinc_input_file()` stages the task app bundle, typically unpacking
   model control files into the slot root.
5. The controller calls `setup_directories(slot_path)`, obtains
   `get_input_manifest(workunit)`, then stages every manifest archive. The
   manifest is available before model control input is parsed.
6. The model's `setup(slot_path)` performs any required edits, such as WRF restart
   namelist preparation. `parse_control_input()` then returns validated timing
   data for the whole run, including already completed steps on restart.
7. `initialize_task_state_from_restart()` reconciles model restart metadata with
   the progress file. Initial progress is written and upload/trickle helpers are
   constructed.
8. The controller checks executable presence and permissions, logs a
   `before_launch` status check, and launches the model in its owned process
   group or Job Object. It polls again immediately after launch before entering
   the normal loop.

A clear startup status snapshot cannot guarantee that no client message is
waiting to be processed. The controller's final exit path therefore attempts
owned-process cleanup even when a shutdown request is observed after launch.

## Physical archive staging

For task app bundles and manifest inputs, the helper sequence is:

```text
logical slot input
    -> resolve_boinc_input_file()
    -> physical project jf_* archive
    -> verify_project_zip_md5()
    -> stage_model_input_archive()
         copy jf_* into the selected slot unzip directory
         extract into that directory
         retain the copied archive in the current implementation
```

For a manifest entry `{ "ifsdata_<workunit>.zip", "ifsdata" }`, the physical
copy is placed in `slot/ifsdata/jf_*` and extracted into `slot/ifsdata/`. The
logical input `slot/ifsdata_<workunit>.zip` is preserved. An entry whose relative
destination is `"."` extracts into the slot root.

`InputStageResult` carries the logical input, resolved project file, destination
archive, failing operation, and message when available. `main()` reports that
context for remote-host failures. Operational errors should not be reduced to
an unexplained boolean.

## Model control and output ownership

`ModelControl::parse_control_input()` owns the filename, opening, parsing and
validation of a control file. OpenIFS uses `fort.4`; WRF owns `namelist.input`
and its restart-related copies/rewrites. The controller consumes shared timing
fields rather than parsing model-specific namelists.

OpenIFS keeps `CNMEXP` privately for output filename generation and converts its
namelist timing values into steps and seconds. The shared `ModelControlInputData`
contains timing and error information, not an experiment ID or upload policy.
The CLI `--upload_interval` controls upload cadence in model steps.

On an observed step change, the controller invokes `do_step_tasks(step, slot)`
first. OpenIFS may run diagnostics through
[external_diagnostics.cpp](../models/openifs/external_diagnostics.cpp), producing
`trickle_data` in the slot. Diagnostics reject and remove the payload on failed
or expired helper cleanup, execution failure, or stale output. Rejection remains
nonfatal to the model. `TrickleHandler` consumes accepted data in the slot.

The controller next asks `get_copyable_output_filenames(step)` for files the
model considers safe to copy. `get_output_filenames(step)` is a nominal naming
hook and is not a readiness guarantee. Copy failures are logged individually;
staging continues with other eligible files.

Successful copies are staged under `<project_dir>/<app_name>_<workunit>/`.
`UploadManager` collects them, creates result ZIPs in the project directory,
resolves BOINC logical upload names such as `upload_file_0.zip`, and submits
uploads when running under BOINC. Requested final logs and placeholders for
otherwise empty required upload slots are handled during finalization.
`upload_interval == 0` disables both intermediate and final result uploads;
trickles remain enabled.

## Restart behaviour

On restart, the controller repeats executable, task-bundle and manifest staging;
it does not generally skip extraction because files already exist. A surviving
process executing the same slot binary can therefore prevent executable
extraction with Linux `ETXTBSY`.

The preserved logical inputs let the controller resolve the original project
archives again. Model hooks own checkpoint discovery and restart-step parsing.
The shared startup helper reconciles those results with the controller progress
file and restores the CPU baseline before launch.

Startup does not discover or terminate orphans from earlier controller runs.
That recovery is outside scope. The controller tracks processes launched by its
current run, and cleanup is bounded rather than an absolute guarantee that a
process in a prolonged kernel wait has released its executable before restart.

## Finalization and exit

`finish_task_exit()` in [task_lifecycle.cpp](../src/task_lifecycle.cpp) attempts
model process-set cleanup on every controlled exit, closes ownership handles,
and unwinds the BOINC critical section. It calls `boinc_finish()` only for the
completion route. BOINC quit and heartbeat loss return zero without a finish
file; abort and ordinary failure use non-finish failure exits as appropriate.
Restartable BOINC shutdown does not initiate final upload handling.

The current normal-completion sequence performs `finalize(slot_path)`, model
success checking, final upload work and file-settle waits before final exit
attempts remaining-descendant cleanup. Cleanup failure does not currently
change the preselected completion code. Controller-error recovery instead
attempts cleanup before upload finalization, but skips all remaining final
uploads when cleanup is failed or unconfirmed, including zombie-only group
presence. Review accepted these policies for OpenIFS and WRF because the models
use threads and do not create model subprocess descendants; no further code
change is required for these two findings. Reassess the policies before adding
a model that creates subprocess workers.

See [BOINC exit behaviour](boinc_exit_behaviour.md) for the two-second cleanup
budget, received-status limitations, synchronous-helper shutdown latency,
regression-test scope, and the recorded review decisions.

## References

- [Controller configuration and data flow](cpdn_control_data_flow.md)
- [Model interface](../api/model_control.h)
- [Manifest types](../api/model_input_manifest.h)
- [Upload management](../src/upload_manager.cpp)
- [Model integration guide](Adding_new_model.md)
