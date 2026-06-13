# CPDN Controller: BOINC File Treatment Through main()

## Overview

The CPDN controller manages BOINC input and output files using a two-directory model:

- **Project directory**: `projects/climateprediction.net/` — Physical BOINC archives with internal names (`jf_*`)
- **Slot directory**: `slots/0/` — Working directory containing logical symlinks to physical archives, plus extracted files

### Key Principle

**Never overwrite BOINC logical files in the slot directory** because BOINC uses these symlinks to resolve the physical archive names on task restart. If a logical file is overwritten, restart recovery becomes impossible.

---

## File Flow Through main() - OpenIFS Example

This section traces the complete journey of BOINC input files through the controller, from initialization through model execution and upload.

### Step 1: BOINC Initialization (Lines ~500–535 in cpdn_main.cpp)

The controller initializes BOINC to obtain essential configuration:

```cpp
// From cpdn_main.cpp, main()
retval = init_boinc( bconfig );
```

**Result**: `bconfig` structure contains:
- `slot_path` = `/path/to/slots/0` — Working directory for this task
- `project_dir` = `/path/to/projects/climateprediction.net` — BOINC project directory
- `app_name` = `"test_model"` (or `"oifs_43r3_omp_l159"`, etc.)
- `app_version` = From BOINC init_data.xml
- `standalone` flag — Whether running under BOINC or standalone

### Step 2: Model Instance Creation (Lines ~560–570 in cpdn_main.cpp)

The controller creates a model-specific controller instance based on the app name:

```cpp
auto model_ctrl = create_model_control( bconfig.app_name, bconfig.app_version );
```

**See**: `src/cpdn_main.cpp` lines 75–100 for the factory function `create_model_control()`

**For OpenIFS**:
```cpp
} else if ( model_name == "oifs_43r3_omp_l159" || ... ) {
    model = std::make_unique<OpenIFSControl>( "ECMWF", model_name, model_version, "oifs_43r3_omp_model.exe" );
```

This returns an `OpenIFSControl` instance, which knows:
- Model executable name: `oifs_43r3_omp_model.exe`
- Control file location: `fort.4` (namelist)
- Input/output file patterns
- Model-specific setup requirements

### Step 3: App Bundle Staging (Lines ~618–650 in cpdn_main.cpp)

The app bundle contains the executable and control file. It's staged as a BOINC logical input file.

**Logical filename constructed in main()**:
```cpp
fs::path app_bundle_path = bconfig.slot_path;
app_bundle_path /= std::string( bconfig.app_name ) + "_" + tconfig.memberid + "_" + 
                   tconfig.filename_startdate + "_" + std::to_string( (int)num_days ) + 
                   "_" + tconfig.batch + "_" + tconfig.workunit + ".zip";
```

**Example filename in slot**:
```
~/slots/0/test_model_member001_20250610_10_b001_wu12345.zip (logical name)
  └─ Points back to: ~/projects/climateprediction.net/jf_12345abc (physical BOINC archive)
```

**Staging process** (line ~639):
```cpp
auto app_bundle_stage = stage_boinc_input_file( app_bundle_path, 
                                                 bconfig.slot_path, 
                                                 fs::path( "." ), 
                                                 "app_bundle" );
```

**What happens inside `stage_boinc_input_file()`** (see `src/cpdn_control.cpp` lines 511–545):

1. **Resolve BOINC filename** (`resolve_boinc_input_file()`):
   ```
   test_model_member001_20250610_10_b001_wu12345.zip 
     → ~/projects/climateprediction.net/jf_12345abc
   ```
   BOINC resolves the logical filename through the symlink.

2. **Verify checksum** (`verify_project_zip_md5()`):
   ```
   MD5 check: ~/projects/climateprediction.net/jf_12345abc
   ```
   Ensures the archive wasn't corrupted in download.

3. **Copy to slot and unzip** (`stage_model_input_archive()`):
   ```
   Copy: ~/projects/climateprediction.net/jf_12345abc → ~/slots/0/jf_12345abc
   Unzip: jf_12345abc → ~/slots/0/
   ```

**Result in slot after app bundle unpack**:
```
~/slots/0/
├─ test_model_member001_20250610_10_b001_wu12345.zip ← PRESERVED (BOINC logical symlink)
├─ jf_12345abc ← Physical copy (now safe to discard)
├─ oifs_43r3_omp_model.exe ← Extracted executable
├─ fort.4 ← Extracted control file (namelist)
└─ ... (other extracted files)
```

**Important**: The logical filename `test_model_member001...zip` is **never overwritten**. It remains as a symlink pointing back to the original `jf_*` archive in the project directory. This is essential for restart recovery—if a task is interrupted and restarted, BOINC can re-resolve this logical filename and find the original archive.

### Step 4: Model Setup & Control File Parsing (Lines ~665–690 in cpdn_main.cpp)

The model is given an opportunity to perform any setup before its control file is parsed:

```cpp
auto model_setup_result = model_ctrl->setup();
```

Then the model parses its control file (e.g., `fort.4` for OpenIFS):

```cpp
auto control_input = model_ctrl->parse_control_input();
```

**See**: `models/openifs/oifs_control.cpp` lines 124–210 for OpenIFS control file parsing

**What `parse_control_input()` does**:
- Opens `fort.4` in the slot directory (unpacked from the app bundle)
- Reads Fortran namelist key-value pairs
- Extracts critical fields:
  - `CNMEXP` — Experiment ID (4 characters)
  - `UTSTEP` — Model timestep in seconds
  - `CUSTOP` — Total number of model steps
  - `NFRPOS` — Output file frequency (in steps)
  - `NFRRES` — Restart dump frequency (in steps)

**Example fort.4 snippet**:
```fortran
&NAML
 CNMEXP = "PL01",
 UTSTEP = 3600.0,
 CUSTOP = 40,
 NFRPOS = 1,
 NFRRES = 10,
/
```

**Returned to main()** (line ~680):
```cpp
const int timestep_seconds = control_input.timestep_seconds;        // 3600
const int output_interval = control_input.output_interval;          // 1
int restart_interval_steps = control_input.restart_interval;        // 10
const int total_steps = control_input.total_steps;                  // 40
const double total_length_of_simulation_time = 
    control_input.forecast_length_time;                             // 40 * 3600 = 144000 secs
```

### Step 5: Model Declares Input File Manifest (Line ~692 in cpdn_main.cpp)

The model declares which BOINC input files it needs and where each archive should be unpacked:

```cpp
auto input_manifest = model_ctrl->get_input_manifest( tconfig.workunit );
```

**See**: `models/openifs/oifs_control.cpp` lines 83–92 for OpenIFS manifest

**OpenIFS manifest for workunit `wu12345`**:
```cpp
return {
    { "ic_ancil_wu12345.zip", fs::path( "." ) },           // Unpack to slot root
    { "ifsdata_wu12345.zip", ifsdata_dir },                 // Unpack to ./ifsdata/
    { "clim_data_wu12345.zip", climdata_dir },              // Unpack to ./climdata/
};
```

The manifest is a simple data structure:
- **Logical BOINC filename**: Constructed by appending workunit ID (e.g., `ic_ancil_wu12345.zip`)
- **Unzip destination**: Relative path in the slot (e.g., `"ifsdata"` means `~/slots/0/ifsdata/`)

This design keeps `main()` generic—the model declares its own input requirements, and the controller executes the same generic staging process for each file.

### Step 6: Model Input Files Staged (Lines ~695–703 in cpdn_main.cpp)

The controller stages each file in the manifest using the same process:

```cpp
auto manifest_stage = stage_model_input_manifest( input_manifest, bconfig.slot_path );
```

**See**: `src/cpdn_control.cpp` lines 546–560 for `stage_model_input_manifest()`

**What happens inside** (pseudocode):
```cpp
for ( const auto& archive : manifest ) {
    fs::path logical_file = slot_path / archive.logical_name;
    
    auto result = stage_boinc_input_file( logical_file,           // ic_ancil_wu12345.zip
                                          slot_path,               // ~/slots/0
                                          archive.unzip_relative_dir,  // "." or "ifsdata"
                                          archive.logical_name );
    if ( !result.ok ) return result;
}
```

#### Detailed Example: Initial Conditions Archive (`ic_ancil_wu12345.zip`)

**In slot directory** (BOINC symlink):
```
~/slots/0/ic_ancil_wu12345.zip → ~/projects/climateprediction.net/jf_67890def
```

**Staging process**:
1. **Resolve** → `jf_67890def`
2. **Verify checksum** on `jf_67890def`
3. **Copy** to slot: `jf_67890def → ~/slots/0/jf_67890def`
4. **Unzip** to root (manifest says `"."`): 
   ```
   ~/slots/0/jf_67890def → ~/slots/0/
   (Initial condition GRIB files unpacked to root)
   ```

#### Detailed Example: IFS Data Archive (`ifsdata_wu12345.zip`)

**In slot directory** (BOINC symlink):
```
~/slots/0/ifsdata_wu12345.zip → ~/projects/climateprediction.net/jf_24681357
```

**Staging process**:
1. **Resolve** → `jf_24681357`
2. **Verify checksum** on `jf_24681357`
3. **Copy** to slot: `jf_24681357 → ~/slots/0/jf_24681357`
4. **Unzip** to `ifsdata/` (manifest says `"ifsdata"`):
   ```
   ~/slots/0/jf_24681357 → ~/slots/0/ifsdata/
   (IFS grid and physics data files unpacked to ./ifsdata/)
   ```

#### Detailed Example: Climate Data Archive (`clim_data_wu12345.zip`)

**In slot directory** (BOINC symlink):
```
~/slots/0/clim_data_wu12345.zip → ~/projects/climateprediction.net/jf_13579246
```

**Staging process**:
1. **Resolve** → `jf_13579246`
2. **Verify checksum** on `jf_13579246`
3. **Copy** to slot: `jf_13579246 → ~/slots/0/jf_13579246`
4. **Unzip** to `climdata/` (manifest says `"climdata"`):
   ```
   ~/slots/0/jf_13579246 → ~/slots/0/climdata/
   (Climate/SST data files unpacked to ./climdata/)
   ```

**Result in slot after all input staging**:
```
~/slots/0/
├─ test_model_member001_20250610_10_b001_wu12345.zip ← PRESERVED (app bundle logical)
├─ ic_ancil_wu12345.zip ← PRESERVED (initial conditions logical)
├─ ifsdata_wu12345.zip ← PRESERVED (IFS data logical)
├─ clim_data_wu12345.zip ← PRESERVED (climate data logical)
├─ (jf_* physical copies now discardable, not needed after unzip)
├─ oifs_43r3_omp_model.exe ← Executable
├─ fort.4 ← Model control file
├─ ifsdata/ ← Extracted directory
│  ├─ ti_grib_p_ml_128_cv.grb ← IFS pressure-level grid
│  ├─ ti_grib_p_ml_319_cv.grb ← Higher-resolution grid variant
│  ├─ ti_grib_q_ml_128_cv.grb ← Humidity model
│  └─ ... (other IFS data files)
├─ climdata/ ← Extracted directory
│  ├─ t_an_sst.grb ← Sea surface temperature
│  ├─ u_an_sst.grb ← SST anomaly
│  └─ ... (other climate data)
└─ (Initial condition GRIB files in root)
   ├─ ic_initial_state.grb
   ├─ ic_perturbation.grb
   └─ ... (other IC files)
```

**Key insight**: All four logical BOINC files are **preserved intact** in the slot. They remain as symlinks pointing to the original `jf_*` archives in the project directory. On task restart, BOINC can re-resolve these logical filenames and find the archived input data.

### Step 7: Model Execution (Lines ~705+ in cpdn_main.cpp)

The model now runs in the slot directory with access to all staged files:

```cpp
Model process reads from slot:
  ├─ ~/slots/0/fort.4 ← Control file (namelist)
  ├─ ~/slots/0/ifsdata/* ← IFS grid and physics data
  ├─ ~/slots/0/climdata/* ← Climate/SST data
  ├─ ~/slots/0/*.grb ← Initial condition GRIB files
  └─ ~/slots/0/oifs_43r3_omp_model.exe ← Executable

Model process writes to slot:
  ├─ ~/slots/0/ICMSHOIFS+000000 ← Output (timestep 0)
  ├─ ~/slots/0/ICMSHOIFS+001000 ← Output (timestep 1000)
  ├─ ~/slots/0/ICMSHOIFS+002000 ← Output (timestep 2000)
  └─ ~/slots/0/ifs.stat ← Progress status file
```

The model updates `ifs.stat` as it progresses. The controller periodically reads this file to track progress (see `models/openifs/oifs_control.cpp` lines 268–295 for `get_current_step()`).

### Step 8: Result Collection & Upload Preparation

As the model runs, the controller monitors for output files that match the expected pattern.

**Output file pattern** (see `models/openifs/oifs_control.cpp` lines 242–260):
```cpp
bool parse_oifs_output_filename( std::string_view filename )
{
    // Pattern: ICMGG[A-Z]{4}+[0-9]{6} or ICMSH[A-Z]{4}+[0-9]{6}
    if ( filename.size() != 16 ) return false;
    
    const std::string_view prefix = filename.substr( 0, 5 );
    if ( prefix != "ICMGG" && prefix != "ICMSH" && prefix != "ICMUA" ) 
        return false;
    
    // Check remaining pattern...
}
```

**Example output files**:
- `ICMSHOIFS+000000` (3-hourly output at step 0)
- `ICMSHOIFS+001000` (3-hourly output at step 1000)
- `ICMSHOIFS+002000` (3-hourly output at step 2000)

**Zip and upload** (see `src/cpdn_main.cpp` lines 242–290 for `zip_and_send_upload()`):

```cpp
// Collect all matching output files
std::vector<fs::path> files_to_zip;
int result_code = add_upload_files( bconfig.slot_path, files_to_zip, *model_ctrl );

// Zip them
fs::path upload_archive = bconfig.project_dir / 
                          ( result_base_name + "_0.zip" );
zip_and_delete( upload_archive.string(), files_to_zip );

// Submit upload to BOINC
boinc_upload_file( "upload_file_0.zip" );
```

**Result**:
```
~/projects/climateprediction.net/
├─ jf_12345abc ← Original app bundle
├─ jf_67890def ← Original initial conditions
├─ jf_24681357 ← Original IFS data
├─ jf_13579246 ← Original climate data
└─ test_model_wu12345_result_0.zip ← Uploaded results archive
   (contains all ICMSH*.* output files)
```

---

## File Directory States Timeline

### Initial State (from BOINC)

**Project directory** (downloaded by BOINC):
```
projects/climateprediction.net/
├─ jf_12345abc ← App bundle archive
├─ jf_67890def ← Initial conditions archive
├─ jf_24681357 ← IFS data archive
└─ jf_13579246 ← Climate data archive
```

**Slot directory** (BOINC creates symlinks):
```
slots/0/
├─ test_model_member001_20250610_10_b001_wu12345.zip → jf_12345abc (symlink)
├─ ic_ancil_wu12345.zip → jf_67890def (symlink)
├─ ifsdata_wu12345.zip → jf_24681357 (symlink)
└─ clim_data_wu12345.zip → jf_13579246 (symlink)
```

### After Staging (before model runs)

**Project directory** (unchanged):
```
projects/climateprediction.net/
├─ jf_12345abc ← Original archive
├─ jf_67890def ← Original archive
├─ jf_24681357 ← Original archive
└─ jf_13579246 ← Original archive
```

**Slot directory** (files extracted, logical links preserved):
```
slots/0/
├─ test_model_member001_20250610_10_b001_wu12345.zip ← PRESERVED (symlink)
├─ ic_ancil_wu12345.zip ← PRESERVED (symlink)
├─ ifsdata_wu12345.zip ← PRESERVED (symlink)
├─ clim_data_wu12345.zip ← PRESERVED (symlink)
├─ oifs_43r3_omp_model.exe ← Extracted
├─ fort.4 ← Extracted
├─ ifsdata/ ← Extracted directory with model data
│  ├─ ti_grib_p_ml_128_cv.grb
│  ├─ ti_grib_p_ml_319_cv.grb
│  └─ ...
├─ climdata/ ← Extracted directory with climate data
│  ├─ t_an_sst.grb
│  ├─ u_an_sst.grb
│  └─ ...
└─ (Initial condition GRIB files)
   ├─ ic_initial_state.grb
   └─ ...
```

### After Model Completes

**Slot directory** (model outputs present):
```
slots/0/
├─ (all previous files remain unchanged)
├─ ICMSHOIFS+000000 ← Model output
├─ ICMSHOIFS+001000 ← Model output
├─ ICMSHOIFS+002000 ← Model output
├─ ifs.stat ← Model status log
└─ ... (other model logs and restart files)
```

**Project directory** (results uploaded):
```
projects/climateprediction.net/
├─ jf_12345abc ← Original archive
├─ jf_67890def ← Original archive
├─ jf_24681357 ← Original archive
├─ jf_13579246 ← Original archive
└─ test_model_wu12345_result_0.zip ← Uploaded results
   (contains ICMSH*.*, ifs.stat, etc.)
```

---

## Key Design Principles

### 1. Logical Files Never Overwritten

The BOINC symlinks in the slot (e.g., `ic_ancil_wu12345.zip`) are **always preserved** and never overwritten. This is the only way BOINC can recover the original archive on task restart.

**Code reference**: `src/cpdn_control.cpp` lines 511–545 specifically avoids unzipping into the logical filename, always unzipping into a separate copy or subdirectory.

### 2. Generic main() via Model Manifest

Instead of hardcoding OpenIFS filenames in `main()`, the model declares its requirements:

```cpp
// OpenIFS (see models/openifs/oifs_control.cpp lines 83-92)
ModelInputManifest OpenIFSControl::get_input_manifest( const std::string& wu )
{
    return {
        { "ic_ancil_" + wu + ".zip", fs::path( "." ) },
        { "ifsdata_" + wu + ".zip", ifsdata_dir },
        { "clim_data_" + wu + ".zip", climdata_dir },
    };
}
```

The controller then executes the **same generic staging logic** for each file:

```cpp
// main() (cpdn_main.cpp lines 695-703) - model-agnostic
auto input_manifest = model_ctrl->get_input_manifest( tconfig.workunit );
auto manifest_stage = stage_model_input_manifest( input_manifest, bconfig.slot_path );
```

This separation allows new models to be added without modifying `main()`.

### 3. Manifest-Driven Unpacking

Each entry in the manifest specifies both:
- **Logical BOINC filename**: What to request from BOINC
- **Unzip destination**: Where to extract the archive

```cpp
struct ModelInputArchive {
    std::string logical_name;                    // e.g., "ic_ancil_wu12345.zip"
    std::filesystem::path unzip_relative_dir;    // e.g., "." or "ifsdata"
};
```

This allows a single model to have inputs unpacked to different directories without requiring `main()` to know those details.

### 4. Rich Error Context for Failures

When staging fails, the result includes enough information for meaningful error reporting:

```cpp
struct InputStageResult {
    bool ok = true;
    std::string logical_file;           // What was requested (e.g., "ic_ancil_wu12345.zip")
    std::string resolved_project_file;  // What BOINC found (e.g., "jf_67890def")
    std::string destination_archive;    // Destination in slot
    std::string step;                   // Failed at: "resolve", "verify", "copy", or "unzip"
    std::string message;                // Human-readable error
};
```

**Code reference**: `src/cpdn_control.h` lines 108–115

When a failure is reported to stderr (see `src/cpdn_main.cpp` lines 118–134), it includes:
```
Failed to stage model input archive for logical file 'ic_ancil_wu12345.zip'
  resolved to 'jf_67890def'
  via slot archive './jf_67890def'
  at step 'unzip'
  : permission denied
```

This detailed context is essential for debugging tasks running on remote volunteer hosts where reproducing the environment is difficult.

---

## File Handling on Restart

If a task is interrupted and restarted:

1. **BOINC restores the symlinks** in the slot directory pointing to the original `jf_*` archives in the project directory
2. **Controller re-initializes** and calls `init_boinc()` again
3. **Model manifest is re-requested**, yielding the same logical filenames
4. **Staging logic checks** if files are already extracted and skips redundant work
5. **Model reads checkpoint files** to resume from the last saved state

The preservation of logical BOINC files is the critical enabler of this restart recovery.

---

## Staging Sequence Diagram

```
main()
├─ init_boinc() 
│  └─ Obtain: slot_path, project_dir, app_name, app_version
├─ create_model_control(app_name, app_version)
│  └─ Return: OpenIFSControl instance
├─ [Application setup/argument processing]
├─ STAGE APP BUNDLE
│  └─ stage_boinc_input_file(app_bundle_logical, slot_path, ".", "app_bundle")
│     ├─ resolve_boinc_input_file(app_bundle) → jf_12345abc
│     ├─ verify_project_zip_md5(jf_12345abc)
│     ├─ copy jf_12345abc → slot/jf_12345abc
│     └─ unzip jf_12345abc → slot/
│        ├─ oifs_43r3_omp_model.exe
│        └─ fort.4
├─ model_ctrl->setup()
│  └─ OpenIFS: setup slot directories, setup_namelist edits, etc.
├─ model_ctrl->parse_control_input()
│  └─ Read fort.4, extract CNMEXP, UTSTEP, CUSTOP, NFRPOS, NFRRES
├─ model_ctrl->get_input_manifest(workunit_id)
│  └─ Return: [
│     { "ic_ancil_wu12345.zip", "." },
│     { "ifsdata_wu12345.zip", "ifsdata" },
│     { "clim_data_wu12345.zip", "climdata" }
│  ]
├─ STAGE MANIFEST FILES
│  └─ stage_model_input_manifest(manifest, slot_path)
│     ├─ FOR ic_ancil_wu12345.zip:
│     │  └─ stage_boinc_input_file(ic_ancil, slot_path, ".", "ic_ancil")
│     │     ├─ resolve → jf_67890def
│     │     ├─ verify
│     │     ├─ copy → slot/jf_67890def
│     │     └─ unzip → slot/
│     ├─ FOR ifsdata_wu12345.zip:
│     │  └─ stage_boinc_input_file(ifsdata, slot_path, "ifsdata", "ifsdata")
│     │     ├─ resolve → jf_24681357
│     │     ├─ verify
│     │     ├─ copy → slot/jf_24681357
│     │     └─ unzip → slot/ifsdata/
│     └─ FOR clim_data_wu12345.zip:
│        └─ stage_boinc_input_file(clim_data, slot_path, "climdata", "clim_data")
│           ├─ resolve → jf_13579246
│           ├─ verify
│           ├─ copy → slot/jf_13579246
│           └─ unzip → slot/climdata/
├─ START MODEL PROCESS
│  └─ spawn oifs_43r3_omp_model.exe with environment vars
├─ MONITOR PROGRESS
│  └─ Periodically call model_ctrl->get_current_step() to read ifs.stat
├─ HANDLE MODEL OUTPUT
│  └─ When model completes:
│     ├─ Verify model success via model_ctrl->check_model_success()
│     ├─ Collect output files matching is_output_filename() pattern
│     └─ Zip and upload results
└─ task_finish()
   └─ boinc_finish(exit_code)
```

---

## References

- **Main entry point**: [src/cpdn_main.cpp](../src/cpdn_main.cpp)
  - `main()` function: Lines 502–1000+ (approximate)
  - `create_model_control()`: Lines 75–100
  - App bundle staging: Lines 618–650
  - Control input parsing: Lines 665–690
  - Model input manifest staging: Lines 695–703

- **Staging functions**: [src/cpdn_control.cpp](../src/cpdn_control.cpp)
  - `stage_boinc_input_file()`: Lines 511–545
  - `stage_model_input_manifest()`: Lines 546–560
  - Error reporting: `report_input_stage_failure()` in cpdn_main.cpp lines 118–134

- **OpenIFS model specifics**: [models/openifs/oifs_control.cpp](../models/openifs/oifs_control.cpp)
  - `get_input_manifest()`: Lines 83–92
  - `parse_control_input()`: Lines 124–210
  - `get_current_step()`: Lines 268–295
  - `check_model_success()`: Lines 43–66

- **Data structures**: 
  - `ModelInputManifest`: [api/model_input_manifest.h](../api/model_input_manifest.h)
  - `ModelControl` interface: [api/model_control.h](../api/model_control.h)
  - `InputStageResult`: [src/cpdn_control.h](../src/cpdn_control.h) lines 108–115
