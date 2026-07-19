# Namelist input handling refactor plan

## Purpose

This document defines the next refactor stage for the part of `main()` that reads the model control input file, currently `fort.4`.

The immediate aim is to reduce the length and complexity of `main()` in `src/cpdn_main.cpp` by moving model-specific parsing into the model layer, while keeping controller-side scheduling, validation and error reporting explicit and robust.

For this stage:

- use the existing `ModelControl` abstraction in `api/model_control.h`
- implement the model-specific parsing in `models/openifs/oifs_control.h` / `models/openifs/oifs_control.cpp`
- add helper types and helper functions only where they clearly reduce complexity
- keep the change small and avoid designing a broader external model metadata system yet

## Aims

- `src/cpdn_main.cpp` should become more model-independent.
- `main()` should not assume Fortran namelist syntax long term.
- the model layer should own opening, reading, parsing and validating its control input file
- controller-visible failures should carry enough context to be useful on remote volunteer hosts
- the refactor should create a clean seam for later tidy-up without forcing premature generalisation

## Scope

In scope:

- refactor the current `fort.4` parsing block in `src/cpdn_main.cpp`
- move model-owned parsing and validation into the model class
- introduce a small parsed-input result struct for data needed by `main()`
- keep forecast-length parsing in the model layer as model-owned runtime information
- keep CPDN filename metadata on the CLI separate from model runtime control values
- document outstanding unit-semantics issues rather than solving them incompletely in this stage

Out of scope for this refactor:

- redesign of the CPDN-injected comment lines at the top of `fort.4`
- a full `model.xml` or equivalent external model metadata system
- redesign of upload interval semantics

## Code Complexity

Complexity tracking for this refactor sequence now lives in
[docs/code_complexity_refactor_tracking.md](/home/glenn/github/cpdn_control/docs/code_complexity_refactor_tracking.md).

Short version:

- the original `main()` hotspot was reduced materially by moving namelist parsing into the model layer
- the later restart/progress bootstrap extraction reduced `main()` further
- the current largest non-`main()` hotspots are `OpenIFSControl::parse_control_input()` and the newly extracted `initialize_task_state_from_restart()`

## Design

### Design summary

1. `main()` should ask the model layer to parse its control input file directly.
2. The model layer should own the control-input filename as an internal detail, along with existence checks, opening, parsing, validation and conversion of model-specific values from that file.
3. The model layer should return a small parsed-input result containing only the values currently needed by `main()`.
4. Do not introduce a general `ModelConfig` struct yet. That is broader than this refactor needs.
5. Keep `TaskConfig` as the task/controller identity struct for now, including the CPDN filename tokens used to resolve downloads.
6. Parse forecast-length information from the model namelist as model-owned runtime data, but do not compare it with the CPDN filename token from the CLI.
7. Treat the model namelist as definitive for model runtime; the CLI filename tokens exist only to resolve CPDN download filenames before the model input is parsed.
8. Keep `CNMEXP` handling as it is for now, but record the current split in usage between controller/task naming and model output naming.
9. Treat upload interval redesign as a follow-up item rather than folding it into this refactor without a clear unit contract.
10. Include a modest tidy of step/time tracking in `main()` so units and names are explicit.

### Parsed-input result for this stage

The new parsed result should be narrowly scoped to values needed by the controller now.

Suggested contents:

- model forecast-length information derived from the namelist
- `timestep`
- `output_interval`
- `restart_interval`
- `upload_interval`
- `total_steps`
- `experiment_id` from `CNMEXP`
- any parse/validation failure context

This is intentionally not a full cross-model schema. It is a local seam to remove parsing complexity from `main()`.

### Ownership

Ownership after the refactor should be:

- `main()`
  - asks the model to parse its control input file
  - validates controller-level assumptions that depend on task setup
  - derives controller scheduling values from the parsed result
  - logs and terminates on structured parse/validation errors

- model class
  - determines and owns the control-input filename through existing model metadata
  - checks existence and opens the file
  - parses model-specific syntax
  - validates model-specific field presence and format
  - returns controller-facing parsed values in a structured result

### Error shape

Do not return a bare `bool` for this parser path.

The parser result should support:

- source file path
- failed step such as `exists`, `open`, `parse`, or `validate`
- field/key name when available
- human-readable message

This matches the existing direction taken for BOINC input staging failures.

### Forecast-length and filename metadata

Keep the CPDN filename metadata in `TaskConfig` because the app-bundle naming path still needs it before `fort.4` is available.

Implemented direction:

- the model namelist remains the model-side source of truth for runtime length
- the CLI uses one `--filename_label` value to resolve the CPDN app-bundle filename
- that label is opaque archive-location metadata, not model configuration
- it is not passed into the model and should not be validated against `CUSTOP`

### Upload interval

The current handling is inconsistent and should not be normalised by guesswork in this refactor.

Current understanding:

- `UPLOAD_INTERVAL` in `fort.4` is CPDN/task-specific, not model-specific
- controller CLI already has an `--upload_interval` parameter
- current comments and usage still show unresolved unit confusion

Planned direction:

- remove `UPLOAD_INTERVAL` from `fort.4` in a later change
- provide upload interval entirely through controller/task configuration
- keep this as an explicit outstanding issue in this plan until unit semantics are reviewed properly

### `exptid` / `CNMEXP`

For this refactor, keep the current code path conceptually unchanged and document the split:

- the controller-side task/config `exptid` is tied to CPDN naming conventions for downloads and related task artefacts
- the model-side `CNMEXP` value in `fort.4` is what OpenIFS uses for its own file naming

Short-term decision:

- continue parsing `CNMEXP` from `fort.4`
- continue passing the existing value through current code paths for now
- note in the plan and in code comments later that this area should be revisited in a future tidy-up

Important current-state note:

- the present codebase does not yet parse a separate controller CLI `exptid`
- `TaskConfig::exptid` is currently populated from `CNMEXP`
- therefore the conceptual split described here is real at the system level, but not yet cleanly represented in code

### Resolution and timing fields

For this codebase, it is acceptable to treat these fields as expected controller inputs for current meteorological forecast models:

- horizontal resolution
- vertical resolution
- grid type
- timestep
- output interval
- restart interval

This does not mean every future model must expose them with identical semantics, only that the present refactor may use them without over-engineering a more abstract type system.

### Step and time normalization

This refactor should include a modest tidy of the current step/time tracking in `main()`.

Definitive model-time inputs:

- model `timestep`: seconds per model step, read from `UTSTEP`
- current model step count: returned by `get_current_step()`

From those two integer values, elapsed model time in seconds can be derived when needed.

Naming and type rules for this tidy:

- step counts should be stored in integer variables, not `std::string`
- convert step counts to `std::string` only at external boundaries where required
- variables representing a step count should use a `step` / `_step` suffix
- variables representing elapsed time in seconds should use a `_time` suffix
- time values should be understood to be in seconds
- where elapsed time is stored rather than derived on demand, prefer `double`

Short-term in-memory direction:

- keep one integer for the current model step count
- keep one integer for the last completed step count
- derive elapsed model time from `step_count * timestep` where needed
- avoid carrying parallel string and integer forms of the same step through the main loop

Specific tidy goals:

- rename confusing variables so their unit is obvious
- stop using names like `current_step` for values that are actually elapsed time in seconds
- tidy the progress-file schema to avoid mixing step counts and elapsed time without clear naming
- fix the current off-by-one mismatch between the step used to trigger upload/trickle work and the elapsed time passed into that work

Keep this tidy modest:

- do not redesign the full restart/progress system in this stage
- do not widen scope into a full BOINC progress accounting refactor
- keep follow-up cleanup notes in this markdown file where further work is deferred

## Planned changes

### `src/cpdn_main.cpp`

Planned changes:

- remove direct opening and line-by-line parsing of `fort.4`
- call a new model-layer parser method to obtain the parsed-input result
- keep controller-side derivation of:
  - restart interval in steps if hour-based values are still allowed
  - `total_nsteps`
  - `trickle_freq`
- keep CLI filename metadata separate from parsed model runtime controls
- normalize current in-memory step/time handling so step counts are integer step variables and elapsed time is derived explicitly
- rename local variables so step counts and elapsed-time values are visually distinct
- remove avoidable string-to-int churn for current step tracking inside the main loop
- fix the current off-by-one mismatch in upload and trickle scheduling

### `api/model_control.h`

Planned changes:

- add a new pure virtual method for parsing and validating the model control input file
- define the method around a structured result, not a bare success/failure flag
- remove the public `get_parameter_input_file()` accessor if it is no longer used elsewhere
- simplify the base-class constructor if the control-input filename becomes purely model-private implementation detail

Do not add or refer to `api/model_control.cpp` for this work.

### `models/openifs/oifs_control.h` and `models/openifs/oifs_control.cpp`

Planned changes:

- implement the new parser override for the OpenIFS control input
- move the current `fort.4` field extraction logic from `main()` into the OpenIFS model layer
- keep OpenIFS-specific parsing and validation in the model implementation
- keep the OpenIFS control-input filename as model-private state if no controller-side caller needs it

### `src/cpdn_control.h`

Planned changes:

- do not introduce a broad `ModelConfig` struct yet
- if needed, add a small controller-facing parsed-input struct here or in a nearby API header
- leave the CPDN filename metadata in `TaskConfig`
- keep the existing `TaskConfig::exptid` field for now and later add a clarifying comment describing how it relates to `CNMEXP`
- rename step/time fields in `TaskState` where needed so stored units are explicit

### `api/progressfile_handler.cpp`

Planned changes:

- tidy progress-file field naming so step counts and elapsed-time values are not mixed under ambiguous names
- preserve restart behaviour while making persisted units clearer
- keep the progress-file change narrowly scoped to naming and unit clarity for this refactor

## Outstanding issues for next iteration

- upload interval unit semantics remain unresolved and need a focused review
- `TaskConfig::exptid` versus `CNMEXP` remains muddled and should be revisited in a later tidy-up
- if future models diverge materially from OpenIFS-style timing/grid assumptions, revisit whether a broader config object or enum-backed fields are justified
- the current code does not yet expose the controller-side `exptid` / model-side `CNMEXP` split as two separate values
- after the modest tidy in this refactor, any remaining restart/progress redesign should be handled as a separate follow-up

## Review of this revised plan

After the revisions above, the plan is materially clearer and simpler, but a few areas still need explicit care during implementation.

Remaining points to watch:

- restart and output interval semantics for negative values should stay documented
  - the current code treats negative restart interval values as hours
  - confirm whether the same rule applies to output interval values before moving logic
- the exact location of the new parsed-input struct still needs choosing
  - keep it close to the model-control API, not buried in `main()`
- if `get_parameter_input_file()` is removed, check that no other non-parser controller path still needs public access to the control-input filename
- the step/time tidy should stay disciplined
  - rename and normalize units now
  - defer broader restart/progress redesign unless a concrete bug requires it
- upload interval must remain clearly marked as unresolved
  - otherwise there is a risk of half-migrating it into the model parser again
- CLI filename metadata and model runtime controls must remain visibly separate
  - `--filename_label` is opaque archive-location metadata only
  - `CUSTOP` and `UTSTEP` remain model runtime controls

Overall assessment:

- the refactor boundary is now sensible
- the design is simpler than the original draft
- the remaining ambiguity is mostly around units and existing CPDN/task metadata contracts, not around class structure
