# Adding a new model

This note describes the minimum code changes currently required to add a new model implementation to the CPDN controller codebase.

Assumption for this first version:

- the new model, called `NewModel`, behaves in a very similar way to OpenIFS
- it uses the existing `ModelControl` abstraction
- it fits the current controller flow for startup, restart handling, input staging, step polling, and output upload

This is intentionally a bare-bones guide. Add detail later as the code matures.

## 1. Add a new model directory under `models/`

Create a new directory, for example:

- `models/newmodel/`

Add the model-specific source and header files there. For a model similar to OpenIFS, the minimum expected files are likely:

- `models/newmodel/newmodel_control.h`
- `models/newmodel/newmodel_control.cpp`

If the model needs additional utility functions for filenames, restart parsing, log parsing, environment setup, or other model-specific behaviour, add companion files such as:

- `models/newmodel/newmodel_utils.h`
- `models/newmodel/newmodel_utils.cpp`

## 2. Implement a `ModelControl` subclass

Create a new class derived from `ModelControl`, similar to `OpenIFSControl` in:

- [models/openifs/oifs_control.h](/home/glenn/github/cpdn_control/models/openifs/oifs_control.h)
- [models/openifs/oifs_control.cpp](/home/glenn/github/cpdn_control/models/openifs/oifs_control.cpp)

The new class must override the current pure virtual interface in:

- [api/model_control.h](/home/glenn/github/cpdn_control/api/model_control.h)

At a minimum this means implementing:

- `print_logs(...)`
- `check_model_success()`
- `get_input_manifest(...)`
- `parse_control_input()`
- `get_current_step(...)`
- `get_output_filenames(...)`
- `get_copyable_output_filenames(...)`
- `setup_directories(...)`
- `is_output_filename(...)`
- `is_restart_filename(...)`
- `get_log_filenames()`
- `restart_exists()`
- `parse_restart(...)`

The new class will also need the necessary private member variables, for example:

- control-input filename
- restart-control filename
- key log filenames
- output filename pattern(s)

## 3. Return the model from `create_model_control(...)`

Update:

- [src/cpdn_main.cpp](/home/glenn/github/cpdn_control/src/cpdn_main.cpp)

Add a new branch in `create_model_control(...)` so the controller can construct the new model control class when the BOINC app/model name matches `NewModel`.

This is the step that connects the new model into the executable at runtime.

## 4. Add the new model sources to the build

Update:

- [models/CMakeLists.txt](/home/glenn/github/cpdn_control/models/CMakeLists.txt)

Add the new source files to the `cpdn_models` object library so they are compiled and linked into the controller.

For a model similar to OpenIFS, this typically means adding:

- `models/newmodel/newmodel_control.cpp`
- any `newmodel_utils.cpp` or similar helper files

## 5. Implement model input staging via `get_input_manifest(...)`

The controller now stages model input archives through the model manifest returned by:

- `get_input_manifest(...)`

This manifest uses:

- [api/model_input_manifest.h](/home/glenn/github/cpdn_control/api/model_input_manifest.h)

If `NewModel` uses BOINC logical input archives in the same general way as OpenIFS, implement the model-specific logical filenames and unzip destinations there.

If `NewModel` depends on those values, use them in the same way as OpenIFS for now.

## 6. Implement model control-input parsing in the model class

`main()` no longer parses model control files directly. The model class must own:

- control-input filename selection
- file existence/opening
- parsing
- validation
- conversion into `ModelControlInputData`

So `NewModel` must implement `parse_control_input()` in its own control class.

Populate the shared result with validated timing for the whole run:

- `timestep_seconds`;
- `output_interval`, `restart_interval`, and `total_steps` in model steps;
- `forecast_length_time` in seconds;
- `ok` and the source file, or structured error step/field/message information.

On restart, these describe the original run, including completed steps, rather
than only the remaining forecast. See WRF's handling of its rewritten namelist.

Keep model-specific resolution, grid and experiment identifiers private to the
adapter. OpenIFS reads `CNMEXP` into its own `experiment_id` for filename
construction; these values are not fields of `ModelControlInputData`. Upload
cadence comes from `TaskConfig::upload_interval` on the CLI, not model input.

The startup order is executable archive, task app bundle,
`setup_directories(slot_path)`, manifest staging, `setup(slot_path)`, then
`parse_control_input()`. The manifest must therefore be available before control
input is parsed. A model with no special directory preparation still implements
`setup_directories(...)`, returning success for its no-op case.

## 7. Implement restart and step polling hooks

The controller startup and run loop currently depend on model hooks for:

- restart file existence
- restart metadata parsing
- current step polling
- per-step output filename generation

So `NewModel` must provide behaviour for:

- `restart_exists()`
- `parse_restart(...)`
- `get_current_step(...)`
- `get_output_filenames(...)`
- `get_copyable_output_filenames(...)`

Step fields are integer model step counts. `parse_restart(...)` currently returns
a string which shared startup code converts into an integer.

`get_output_filenames(step)` supplies nominal names for model-internal use; it
does not establish copy readiness. `get_copyable_output_filenames(current_step)`
must return every file safe to copy as of the observed step. The controller can
skip observed steps, and a single file can contain several steps of output.

If the restart file format or current-step source differs from OpenIFS, that logic belongs in the new model implementation, not in `main()`.

## 8. Implement model log and success checks

The controller prints model logs and checks final success through the model class.

So `NewModel` must define:

- which log files are relevant
- how controller log printing should tail them
- what final success condition means

This belongs in:

- `get_log_filenames()`
- `print_logs(...)`
- `check_model_success()`

## 9. Review whether any temporary bridges or assumptions need extending

For a model very similar to OpenIFS, the existing controller seams may be enough.

Even so, when adding `NewModel`, review these areas:

- startup/restart seam in [src/control_start.h](/home/glenn/github/cpdn_control/src/control_start.h)
- any assumptions in [src/cpdn_main.cpp](/home/glenn/github/cpdn_control/src/cpdn_main.cpp) about output naming, restart cadence, or upload cadence

The goal should be:

- keep `main()` generic
- keep model-specific rules in the new model directory
- only extend shared interfaces when `NewModel` genuinely needs a new cross-model seam

## 10. Preserve controller-owned process cleanup

Use the shared launcher for the main executable. The controller owns its process
set on Linux, macOS, and Windows: a private POSIX process group or a Windows Job
Object. Valid launch ownership persists after leader exit until explicit cleanup
and handle closure. Identify processes by their recorded launch ownership, never
by a global executable-name search.

All controlled exits use `finish_task_exit()` in `src/task_lifecycle.cpp`.
Termination is immediate and confirmation has one two-second monotonic wait
budget for the model process set. BOINC quit/abort/heartbeat loss must not bypass
cleanup. Ordinary descendants must stay in the owned group/job; POSIX descendants
that deliberately detach are outside the guarantee. Orphan recovery from earlier
controller runs is outside scope.

Auxiliary synchronous programs should use `run_process_with_timeout()`, which
returns execution, output-freshness and cleanup outcomes. The helper execution
timeout and cleanup budget are separate. Check the cleanup result before
accepting helper output; the OpenIFS diagnostics acceptance path rejects failed
or expired cleanup even when execution succeeded and the file was refreshed.

Read [BOINC exit behaviour](boinc_exit_behaviour.md) before integrating a model
with descendants or auxiliary writers. Review accepted the current normal
completion ordering and error-finalization policy for OpenIFS and WRF because
their parallel workers are threads, not model subprocess descendants. Reassess
these policies before adding a model with subprocess workers. A leader exit or
successful helper return does not by itself prove every writer has stopped in
such an integration. The two-second cleanup budget does not bound
all time spent in model step hooks or other synchronous controller work.

Extend tests for model control input, manifest staging, output readiness and
restart handling. Include process-lifecycle tests for a model that uses
additional processes, and validate the affected backend natively on macOS and
Windows as well as Linux.

## Short checklist

Minimum code touchpoints for a new model similar to OpenIFS:

- add `models/newmodel/`
- implement `NewModelControl : ModelControl`
- add new model `.cpp` files to [models/CMakeLists.txt](/home/glenn/github/cpdn_control/models/CMakeLists.txt)
- update `create_model_control(...)` in [src/cpdn_main.cpp](/home/glenn/github/cpdn_control/src/cpdn_main.cpp)
- implement model manifest logic
- implement model control-input parsing
- implement restart/current-step/output/log/success hooks
- implement `setup_directories(...)` and copy-readiness reporting
- preserve shared process ownership and add appropriate lifecycle coverage

This is sufficient as a starting point for a first similar model integration.

## Future implementation ideas

The current design is more model-agnostic than earlier controller versions, but it is not yet fully declarative. Adding a new model is simpler than before, but it still requires a new code adapter rather than only a configuration file.

At this stage, that is acceptable. It is too early to introduce a model-properties file, template system, or other declarative approach until there is more evidence about which model behaviours are genuinely shared and which remain model-specific.

A sensible next step is:

- add one or two more real models through code first
- compare them with OpenIFS
- identify which parts are stable enough to become shared data rather than custom code

Areas that may later prove suitable for a model-owned properties file include:

- BOINC input archive names
- unzip destinations
- control-input filenames
- restart-control filenames
- simple output filename patterns

Other areas are more likely to remain code-based even in a future declarative design, for example:

- parsing model control input
- interpreting restart state and current progress
- step polling
- model-specific diagnostics or output handling

If a future declarative design is introduced, a mixed approach is likely to be better than making everything configurable:

- stable model data in a model-owned configuration file
- model-specific behaviour in code

This should only be attempted once several real model integrations exist and the common structure is clearer.
