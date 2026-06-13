# Review: remaining model-specific behavior in `main()`

This note records the current findings from reviewing the `main()` control flow in `src/cpdn_main.cpp` and the controller helpers it calls, with the goal of identifying behavior that is still not fully abstracted behind `ModelControl`.

## 1. Output/upload flow still assumes OpenIFS-style filename derivation

- In `main()`, the controller asks the model for exact output filenames from `(step, experiment_id)`:
  - `src/cpdn_main.cpp:835`
  - `src/cpdn_main.cpp:868`
  - `src/cpdn_main.cpp:986`
- This fits OpenIFS because output filenames are deterministic from step and `CNMEXP`.
- It is already a poor fit for WRF-style datetime filenames and for any model where:
  - the emitted files depend on runtime configuration
  - the emitted files depend on which subdomains/nests are enabled
  - the controller should discover completed files rather than reconstruct them

### Why this matters

- The controller is still assuming a model contract shaped around OpenIFS.
- A more model-agnostic seam would let the model:
  - discover completed outputs for a progress point
  - enumerate upload candidates from the slot or upload directory
  - keep filename parsing and generation inside the model implementation


## 2. Experimental diagnostics path is still OpenIFS-specific

- DONE : all diagnostic code has been moved from cpdn_main() into model instance.


## 3. `ModelControlInputData` still encodes OpenIFS-style timing semantics

- The shared control-input result in `api/model_control.h:25` currently contains:
  - `timestep_seconds`
  - `output_interval`
  - `restart_interval`
  - `total_steps`
  - `forecast_length_time`
- `main()` then interprets these in an OpenIFS-shaped way:
  - derives trickle frequency from `timestep_seconds` and `total_steps`: `src/cpdn_main.cpp:668`
  - derives elapsed model time from step count and timestep seconds: `src/cpdn_main.cpp:846`

### Why this matters

- The controller contract is still strongly shaped by OpenIFS concepts.
- A new model with different cadence or timing semantics must currently adapt itself to this shared structure rather than expose its own cleaner model-owned scheduling data.
- A model that uses a variable timestep, or variable output/restart intervals will require recoding

### Actions required : None for now

- <b>For now this approach is sufficient.</b>
- Assume any model can be monitored using a fixed timestep in seconds with a forecast time in 
  number of steps.
- Assume restart and model output frequencies are also a fixed interval.
- For nested models, assume cpdn_control only monitors the top level model.
- This should cover most models.


## 4. App-bundle logical filename is still constructed in `main()`

- `main()` constructs the BOINC logical filename for the app bundle directly:
  - `src/cpdn_main.cpp:636`
- The naming rule is based on CPDN task tokens such as:
  - `app_name`
  - `memberid`
  - `filename_startdate`
  - `filename_fclen`
  - `batch`
  - `workunit`

### Why this matters

- This is still a hard-coded packaging convention in `main()`.
- If different model families ever require a different app-bundle naming rule, this becomes another model-specific branch outside the model seam.

### Actions required : None for now

- Assume all model implementations in CPDN will follow this approach for now.
- Low priority: could generalise the 'filename_startdate & filename_fclen' into a single string?


## 5. Progress smoothing is still based on an OpenIFS-tuned heuristic

- `model_frac_done()` in `src/cpdn_control.cpp:560` explicitly notes that it is “currently based on OpenIFS”.
- `main()` uses it unconditionally:
  - `src/cpdn_main.cpp:924`

### Why this matters

- This may be acceptable for now, but it is not truly model-agnostic behavior.
- If other models have very different step cadence or progress semantics, this likely belongs behind a model-owned policy hook.

### Actions : None for now but needs revising

- Should be revised so that the control code monitors the average time per step itself
  and provides a more accurate timing to the boinc client.


## 6. Model registration is still hard-coded in the factory

- The model factory in `src/cpdn_main.cpp:81` explicitly maps:
  - OpenIFS model names
  - WRF model name/version
  - test model

- No changes required. Code is acceptable as-is.

### Why this matters

- This means `main()` is not fully agnostic in the strictest sense.
- However, this is a central registry rather than controller-flow model logic, so it may remain acceptable unless a future external model descriptor or plugin mechanism is introduced.

## Summary

The most important remaining abstraction leaks are not isolated string literals or file names, but deeper controller assumptions that the model contract is based on:

- integer model steps
- controller-derived elapsed model time
- controller-owned upload/restart cadence math
- model outputs that can be regenerated from `(step, experiment_id)`
- DONE: OpenIFS-specific experimental diagnostics behavior

These are the highest-value seams to review if the next goal is to make adding non-OpenIFS models significantly easier.
