# Input File Refactor Plan

## Purpose

This document defines a concrete refactor plan for BOINC-delivered model input files.

The immediate aim is to simplify the input-file setup logic in `src/cpdn_main.cpp`, remove OpenIFS-specific filename handling from controller code, stop relying on CPDN-injected custom lines in `fort.4` for input archive names, preserve the BOINC-created logical link files in the slot directory, and replace the current custom soft-link parsing with BOINC-standard filename resolution.

## Scope

In scope:

- refactor input-file staging for the model input archives copied from the BOINC project directory
- move model-specific input naming rules into model code
- use BOINC filename resolution instead of custom `>...<` parsing
- verify the MD5 of resolved `jf_*` files before unzip
- always copy, then always unzip, on both first run and restart
- preserve support for archives that unzip into subdirectories, including OpenIFS climate data

Out of scope for this refactor:

- redesign of app-bundle unpacking beyond a local variable rename
- replacing all `fort.4` parsing
- removing existing use of `fort.4` for timing and scheduling values
- full external model metadata system

## Agreed Decisions

1. Keep the BOINC app bundle separate from the model input archive flow.
2. Rename `namelist_zip_path` and related locals in `main()` to `app_bundle_path` / `app_bundle` to reflect actual contents.
3. Add a model-owned input manifest to `ModelControl`.
4. The OpenIFS implementation may initially hardcode the required logical input archive names.
5. The manifest should describe the logical BOINC filename and where the archive is to be unpacked.
6. The controller should create any required destination subdirectory based on manifest data.
7. Use `boinc_resolve_filename[_s]()` instead of custom soft-link parsing.
8. Only the resolved `jf_*` project files need MD5 verification in this refactor.
9. Always copy the resolved archive into the slot, then always unzip it. Do not clear the destination first.
10. The BOINC-created logical files in the slot must remain read-only inputs and must not be overwritten.

## Design Summary

The controller should no longer discover model input files by parsing custom metadata in `fort.4`. Instead:

1. `main()` asks the selected `ModelControl` instance for an input manifest for the current task context.
2. Each manifest entry tells the controller:
   - the logical BOINC filename expected in the slot
   - the relative unzip destination inside the slot
3. Generic controller code resolves the logical filename to the physical BOINC project file using `boinc_resolve_filename_s()`.
4. Generic controller code validates that the resolved project file name matches the expected `jf_<md5>` pattern and that the file's MD5 matches the derived value.
5. Generic controller code copies the physical file into the slot using the same `jf_*` filename as the source archive.
6. Generic controller code unzips the copied archive into the requested destination directory.

The copied archive must not overwrite the BOINC-created logical file in the slot. Preserving that logical file preserves the link back to the real file in `projects/climateprediction.net` for future restarts and for re-resolution via BOINC.

This keeps filename patterns and archive placement rules in the model layer, while keeping BOINC file resolution, copying, checksum verification, and unzip behaviour in generic controller code.

## Read-Only BOINC Logical Files

The current code path does overwrite the logical slot file when `destination` equals the logical file path in `copy_and_unzip()`. In `src/cpdn_control.cpp`, the copy step uses:

```cpp
fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
```

where `destination` is currently passed as `ic_ancil_zip`, `ifsdata_destination`, or `climate_data_destination`.

That behaviour is acceptable for the current code only because it intentionally replaces the logical file with the real archive. Under the new design this must stop. The copy destination should be derived from the resolved physical file basename `jf_*`, not from the logical BOINC filename. This preserves the logical file and should work correctly.

## Proposed Data Types

Use a dedicated header:

- new header: `api/model_input_manifest.h`

Suggested types:

```cpp
struct ModelInputArchive {
    std::string logical_name;
    std::filesystem::path unzip_relative_dir;
};
```

```cpp
using ModelInputManifest = std::vector<ModelInputArchive>;
```

Notes:

- `workunit_id` is needed now for OpenIFS logical filename construction
- add a short code comment in the model implementation noting that a new model may populate the same manifest from a different source and should not assume a Fortran namelist exists

## Proposed `ModelControl` Interface

Add a new pure virtual method:

```cpp
virtual ModelInputManifest get_input_manifest(const std::string& workunit_id) const = 0;
```

Notes:

- this keeps filename construction and destination rules in the model class
- this avoids forcing `main()` to know OpenIFS prefixes such as `ic_ancil`, `ifsdata`, and `clim_data`
- this avoids a temporary OpenIFS special case in `main()`

OpenIFS implementation:

```cpp
ModelInputManifest OpenIFSControl::get_input_manifest(const std::string& workunit_id) const;
```

Initial OpenIFS entries:

- logical name: `ic_ancil_<workunit>.zip`
  - unzip dir: `.`
- logical name: `ifsdata_<workunit>.zip`
  - unzip dir: `ifsdata`
- logical name: `clim_data_<workunit>.zip`
  - unzip dir: `climdata`

For the climate-data case, unpack into a generic name directory and create links to the supported horizontal resolution + grid_type named directories.

Comment guidance for the refactor:
- note in comments that future models may source manifest inputs from something other than `fort.4`, for example `model.xml` or another model-owned config source

## Proposed Controller Helpers

The current `get_tag()` and `copy_and_unzip()` functions should be replaced by BOINC-native staging helpers.

### New helper responsibilities

1. Resolve logical slot filename to physical project file path

Suggested helper:

```cpp
bool resolve_boinc_input_file(const std::string& logical_name, std::filesystem::path& physical_path);
```

Implementation notes:

- call `boinc_resolve_filename_s(logical_name.c_str(), resolved)`
- treat the resolved value as the physical file path for current live-task behaviour
- fail if the resolved file does not exist
- if future platforms expose symlinks instead of soft-link files, this helper is the right place to normalize that behaviour

2. Validate resolved project file naming and checksum

Suggested helper:

```cpp
bool verify_project_zip_md5(const std::filesystem::path& project_file);
```

Implementation notes:

- expect filename form `jf_<32-hex>`
- derive expected checksum from the filename suffix after `jf_`
- compute actual MD5 of the file contents
- compare before unzip
- log both path and checksum mismatch details on failure

3. Ensure unzip destination exists

Suggested helper:

```cpp
bool ensure_directory(const std::filesystem::path& dir);
```

4. Copy and unzip a resolved archive

Suggested helper:

```cpp
bool stage_model_input_archive(
    const std::filesystem::path& source_project_file,
    const std::filesystem::path& slot_path,
    const std::filesystem::path& unzip_relative_dir,
    std::string_view type_label
);
```

Implementation notes:

- destination copy path should be:
  - `slot_path / unzip_relative_dir / source_project_file.filename()`
- this preserves the BOINC logical link file in the slot
- always copy with overwrite
- always unzip after copy
- create parent directories if needed

5. Drive staging of the full manifest

Suggested helper:

```cpp
bool stage_model_input_manifest(
    const ModelInputManifest& manifest,
    const std::filesystem::path& slot_path
);
```

This keeps `main()` shorter by moving loop-and-stage behaviour into one function.

## Functions Affected

### `src/cpdn_main.cpp`

Planned changes:

- rename `namelist_zip_path` to `app_bundle_path`
- rename `namelist_zip` to `app_bundle`
- remove the OpenIFS-specific ancillary archive block from lines currently around 585-837
- stop reading these CPDN header keys for input-file staging:
  - `IC_ANCIL_FILE`
  - `IFSDATA_FILE`
  - `CLIMATE_DATA_FILE`
- stop constructing:
  - `ic_ancil_zip`
  - `ifsdata_folder`
  - `ifsdata_zip`
  - `ifsdata_destination`
  - `climate_data_path`
  - `climate_data_zip`
  - `climate_data_destination`
- call:
  - `auto input_manifest = model_ctrl->get_input_manifest(workunit_id);`
  - `stage_model_input_manifest(...)`

Keep in `main()` for now:

- `fort.4` parsing for `UTSTEP`, `NFRPOS`, `NFRRES`, `CNMEXP`
- any still-needed metadata not yet moved behind model control

### `src/cpdn_control.cpp`

Planned changes:

- delete `get_tag()`
- delete `copy_and_unzip()`
- add BOINC-based resolver helper(s)
- add MD5 verification helper(s)
- add copy/unzip staging helper(s)

### `src/cpdn_control.h`

Planned changes:

- remove declarations for:
  - `std::string get_tag(...)`
  - `int copy_and_unzip(...)`
- add declarations for the new staging helpers

### `api/model_input_manifest.h`

New file:

- define `ModelInputArchive`
- define `ModelInputManifest`

### `api/model_control.h`

Planned changes:

- include `api/model_input_manifest.h`
- add new pure virtual method `get_input_manifest(...)`

### `models/openifs/oifs_control.h`

Planned changes:

- declare OpenIFS override of `get_input_manifest(...)`

### `models/openifs/oifs_control.cpp`

Planned changes:

- implement OpenIFS manifest construction
- hardcode the current logical filename patterns here

## New and Deleted Functions

### New

Likely new functions in `src/cpdn_control.cpp`:

- `resolve_boinc_input_file(...)`
- `verify_project_zip_md5(...)`
- `ensure_directory(...)`
- `stage_model_input_archive(...)`
- `stage_model_input_manifest(...)`

Likely new method in model interface:

- `ModelControl::get_input_manifest(...)`

### Deleted

- `get_tag(...)`
- `copy_and_unzip(...)`

## MD5 Verification Notes

The narrow refactor assumes:

- BOINC physical input archives in the project directory are named `jf_<md5>`
- the file contents are the ZIP payload

Verification steps:

1. Resolve logical filename to physical project path.
2. Extract expected MD5 from the resolved filename.
3. Compute the file's MD5 from disk.
4. Abort if they differ.
5. Only after that copy and unzip the file.

This avoids any need to parse `client_state.xml` during the current refactor.

## Test Changes

### Unit tests

Remove or replace tests tied to deleted behaviour:

- remove `tests/unit/t_get_tag.cpp`

Add new focused tests:

1. filename-to-expected-MD5 parsing
   - valid `jf_<md5>`
   - invalid names
   - wrong length
   - non-hex characters

2. MD5 verification helper
   - matching checksum
   - mismatching checksum

3. manifest staging helpers where feasible
   - destination directory creation
   - archive copied to requested destination
   - logical BOINC file left untouched

Depending on how BOINC calls are wrapped, `boinc_resolve_filename_s()` itself may be best covered by integration-style tests rather than unit tests.

### Functional tests

Update `tests/functional/setup_test.py`:

- stop generating custom `>...<` pseudo-soft-link files
- generate BOINC-standard `<soft_link>...</soft_link>` content, or real symlinks if appropriate for the platform and harness
- ensure the resolved physical project files are named `jf_<md5>`
- keep the model input archives as ZIP payloads with no `.zip` suffix on the physical project file
- preserve the logical BOINC files in the slot after controller staging

Functional validation should cover:

1. first run:
   - all required model input archives are copied and unzipped

2. restart:
   - input archives are copied again from project directory
   - unzip is attempted again
   - logical BOINC files remain usable for re-resolution

3. checksum failure:
   - controller aborts before unzip

## Implementation Order

Recommended patch order:

1. rename `namelist_zip_path` / `namelist_zip` to `app_bundle_path` / `app_bundle`
2. add `api/model_input_manifest.h`
3. add `ModelControl::get_input_manifest(...)`
4. implement OpenIFS manifest
5. add BOINC resolver and MD5 helper functions
6. add archive staging helpers
7. replace the current input-file block in `main()`
8. remove `get_tag()` and `copy_and_unzip()`
9. update unit tests
10. update functional harness to BOINC-standard soft-link representation

This order keeps the code buildable at each step.

## `model.xml` Assessment

### Idea

A future `model.xml` unpacked from the app bundle could describe:

- input archive patterns
- destination directories
- model-specific setup values such as climate-data directory dependencies
- optional model metadata such as parameter file names or output filename patterns

The controller could read the XML and pass a parsed representation to the model instance, or the model instance could parse it directly.

### Potential benefits

- less hardcoded model setup in C++
- easier addition of new model variants without recompiling controller logic
- clearer separation between generic controller code and model-specific setup data
- a clean place to hold model configuration dependencies that do not sit naturally on the controller command line

### Costs

- new XML format to define and maintain
- XML parsing code added to the controller or model layer
- server-side work to generate and distribute the file
- validation burden for malformed or incomplete model metadata
- more moving parts for a refactor whose near-term goal is just to clean up input staging

### Recommendation

Do not introduce `model.xml` in this refactor.

Reason:

- it is likely over-engineering for the current problem

Longer-term target:

- keep the new manifest API stable enough that a future implementation can switch from hardcoded OpenIFS entries to XML-driven entries without changing `main()`

## Longer-Term / Future Work

1. Move model configuration dependencies out of `fort.4` where practical.
2. Decide whether climate-data directory dependencies belong in:
   - a future `model.xml`
   - a model-owned runtime config object
   - or a small structured `--model` argument if XML proves too heavy
3. Reduce `fort.4` parsing in `main()` to only values genuinely needed there.
4. Use the new manifest API as the migration seam for broader model support.
5. If the project adds one more model, hardcoded model manifest implementations may still be acceptable.
6. If the project adds two mixed models with materially different setup needs, reassess whether hardcoded C++ configuration is starting to become awkward.
7. If the project adds around four substantially different models, especially if some do not use a Fortran namelist, prioritize introducing a `model.xml` or equivalent model-owned configuration source.
8. If `model.xml` is adopted later, use it to describe:
   - required input archives and naming rules
   - destination directories
   - control/input file names
   - restart-control conventions
   - model-specific setup dependencies now being passed via manifest.
9. If `model.xml` is adopted later, keep `main()` unchanged where possible and move the change into model/config loading so the generic controller path remains stable.

The important thing in the current refactor is to create the seam, not to fully generalise the model configuration system in one step.

## Expected Outcome

After the refactor:

- `main()` will be shorter and less OpenIFS-aware
- BOINC filename resolution will use standard BOINC mechanisms
- custom `>...<` soft-link parsing will be gone
- `fort.4` will no longer be used to carry model input archive names
- the BOINC-created logical files in the slot will be preserved
- the controller will have one generic path for resolving, verifying, copying, and unzipping model input archives
- the model layer will own the declaration of which input archives are required and where they should be unpacked
