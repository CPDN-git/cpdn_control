# ecCodes Integration Plan

## Purpose

This document defines a concrete implementation plan for adding ECMWF ecCodes
to `cpdn_control` so future OpenIFS work can read metadata from GRIB input
files.

The immediate aim is to establish a low-risk, cross-platform integration path
for Linux, Windows, and macOS without blocking on upstream `vcpkg` support.

## Current Decisions

The following decisions are treated as agreed for this planning stage:

1. Do not wait for an upstream `vcpkg` port. ecCodes should be integrated as a
   repo-managed third-party dependency for now.
2. Do not keep a nested git repository under `third_party/`.
3. Keep any ecCodes fork and active development clone outside this repo.
4. Vendor a clean source snapshot into `third_party/eccodes/`, following the same
   general repo-owned pattern already used for third-party code under `third_party/`.
5. Start with the C and C++ library use case only.
6. Do not enable Fortran support.
7. Do not enable NetCDF support.
8. Do not enable AEC/CCSDS in the first integration pass.
9. JPEG support is optional and should default to off unless the build proves
   materially simpler with it on.
10. On Windows, attempt a static library build first. If that fails for clear
    upstream reasons, fall back to a shared-library build and package the DLL
    deliberately.

## Why Not `vcpkg`

At the time of writing, ecCodes is not available as an official upstream
`vcpkg` port. Carrying a repo-local overlay port would be possible, but it
would add packaging and maintenance work before the project has validated the
library configuration it actually wants on all three target platforms.

For this repo, a vendored-source integration is the simpler first step because:

- the repo already vendors third-party code under `third_party/`
- ecCodes uses CMake upstream
- the build options need deliberate platform-specific policy
- Windows support includes caveats that should be validated directly before
  adding package-manager indirection

If the integration stabilises later, a `vcpkg` overlay port can still be added
as a follow-up.

## Goals

- build ecCodes from source as part of the repo build
- support Linux, Windows, and macOS
- keep the first integration pass small and reversible
- avoid widening the dependency surface across unrelated controller code
- preserve the existing OpenIFS runtime conventions for GRIB definitions and
  samples unless ecCodes proves to require a different contract
- document provenance and local patches clearly

## Non-Goals

Out of scope for the first integration pass:

- adding ecCodes-based GRIB parsing to controller logic immediately
- redesigning the OpenIFS runtime layout for GRIB definition/sample files
- enabling Fortran bindings
- enabling NetCDF conversion features
- enabling AEC/CCSDS support
- adding a full `vcpkg` overlay port
- solving every possible ecCodes optional-feature combination on Windows

## Recommended Dependency Management Model

### Upstream fork and local clone

Use a personal fork of `ecmwf/eccodes` only as an external maintenance vehicle.
That fork is useful if local fixes are needed for:

- Windows static linking
- CMake option cleanup
- packaging fixes
- later AEC enablement work

Keep the fork and working clone outside `cpdn_control`.

### Repo import model

Import a source snapshot into:

- `third_party/eccodes/`

Do not import the `.git/` directory and do not use a git submodule.

Add a small repo-local provenance file such as:

- `third_party/eccodes/README.cpdn.md`

That file should record:

- upstream repository URL
- imported tag or commit hash
- import date
- whether the import is unmodified or patched
- a short summary of any local patches

This keeps third-party provenance explicit without introducing a nested repo.

## Recommended Build Strategy

### Stage 1: validate upstream builds outside this repo

Before wiring ecCodes into `cpdn_control`, validate a constrained upstream build
from a separate checkout on each target platform.

Initial configuration target:

```cmake
-DENABLE_FORTRAN=OFF
-DENABLE_NETCDF=OFF
-DENABLE_AEC=OFF
-DENABLE_JPG=OFF
```

Windows first attempt:

```cmake
-DBUILD_SHARED_LIBS=OFF
```

If Windows static build fails for clear upstream reasons, retry with:

```cmake
-DBUILD_SHARED_LIBS=ON
```

This stage is intended to answer one narrow question first:

- can the core ecCodes C/C++ library be built reliably on Linux, Windows, and
  macOS with the feature set this project actually needs?

### Stage 2: vendor ecCodes into `third_party/`

Once the constrained builds are understood:

1. import the selected ecCodes snapshot into `third_party/eccodes/`
2. add the provenance note
3. keep any repo-local patches small and documented

### Stage 3: integrate ecCodes into top-level CMake

The preferred first approach is to build ecCodes directly from source in the
main CMake graph.

Recommended initial shape:

1. add a small CMake wrapper module under `cmake/`, for example
   `cmake/EcCodesConfig.cmake`, to centralise ecCodes-specific options
2. set ecCodes cache variables explicitly before adding the subdirectory
3. add `third_party/eccodes` with `EXCLUDE_FROM_ALL` if that works cleanly
4. expose a narrow imported or alias target for the rest of the repo to link
   against

The wrapper should own policy such as:

- Fortran off
- NetCDF off
- AEC off for phase 1
- JPG off by default
- Windows static-first policy
- any install/test/docs options that should be disabled for repo builds

### Why direct CMake integration is acceptable

ecCodes already uses CMake upstream, so the repo does not need to invent a new
build layer. The main risk is not “can CMake build it?” but “which upstream
options and platform caveats need to be pinned for this repo?”

That makes a repo-owned CMake wrapper a good fit.

## Proposed CMake Integration Shape

### New build wrapper

Add a helper module:

- `cmake/EcCodesConfig.cmake`

Suggested responsibilities:

1. define an option such as `CPDN_ENABLE_ECCODES`
2. define ecCodes build-policy defaults for this repo
3. add `third_party/eccodes` to the build only when enabled
4. expose:
   - include directories
   - the ecCodes library target to link
   - any runtime metadata needed by tests on Windows/macOS

### Top-level CMake usage

In `CMakeLists.txt`:

1. include the new `cmake/EcCodesConfig.cmake`
2. call a helper such as `configure_eccodes()`
3. append ecCodes to the link set only for targets that need it

Initial recommendation:

- do not link ecCodes to the whole `cpdn_control` library immediately
- instead, link it first to the model-side object/library that will contain the
  first ecCodes call sites

This keeps the dependency narrow and reduces the blast radius if platform
issues appear.

### Model-side integration point

The likely first consumer is OpenIFS-specific code under:

- `models/openifs/`

When actual ecCodes API usage begins, prefer one of:

- link `cpdn_models` to ecCodes directly, if usage remains model-local
- or introduce a small OpenIFS/ecCodes helper translation unit and link only
  that path

Do not add ecCodes calls to `main()` or generic controller code unless there is
    a clear cross-model reason.

## Runtime Data Contract

This repo already sets OpenIFS GRIB-related environment variables in:

- `models/openifs/oifs_utils.cpp`

Current values:

- `GRIB_SAMPLES_PATH`
- `GRIB_DEFINITION_PATH`

These currently point at files staged under the slot directory.

Initial recommendation:

1. preserve the existing OpenIFS runtime contract
2. use ecCodes against the same definitions/samples layout already expected by
   OpenIFS
3. avoid switching to ecCodes MEMFS or installed definitions in phase 1 unless
   the library proves not to work reliably with the current contract

Reasoning:

- it avoids introducing two independent definition/sample sources
- it reduces the chance of version skew between the model runtime and the GRIB
  library
- it keeps the first change focused on library integration rather than runtime
  layout redesign

## Platform Policy

### Linux

Expected to be the least risky platform.

Initial target:

- static build if upstream supports it cleanly in the selected configuration

### macOS

Expected to be viable with minimal special handling, but should still be
validated explicitly.

Initial target:

- use the same constrained feature set as Linux
- accept shared-library build if static causes avoidable friction

### Windows

This is the highest-risk platform and should be treated as such.

Initial target:

1. attempt static build first
2. if static fails, capture the exact cause
3. fall back to shared-library packaging only if needed

Important Windows assumptions for phase 1:

- no Fortran
- no NetCDF
- no AEC
- no requirement to solve every optional codec up front

## Testing Plan

### Dependency validation tests

Before the repo uses ecCodes in production code, validate:

1. configure succeeds
2. library build succeeds
3. a minimal C or C++ compile-and-link test succeeds
4. on Windows shared builds, the runtime library is found by a smoke test

### Repo-side tests after integration

For the first repo integration stage:

- Linux: full build and unit tests
- macOS: full build at minimum
- Windows: full build at minimum, plus a targeted runtime smoke test if shared
  libraries are required

Do not block the first integration on a full OpenIFS GRIB feature test suite.
That should follow after the library contract is stable.

## Implementation Phases

### Phase 0: external validation

1. fork `ecmwf/eccodes`
2. clone it outside this repo
3. validate Linux build
4. validate macOS build
5. validate Windows static attempt
6. if needed, validate Windows shared fallback
7. document the exact working option set

Exit criteria:

- a known-good option set exists for all required platforms, or
- the exact Windows blocker is understood well enough to choose a shared
  fallback deliberately

### Phase 1: source import

1. import ecCodes snapshot into `third_party/eccodes/`
2. add provenance note
3. add any minimal local patch set

Exit criteria:

- repo contains a stable vendored ecCodes source tree without nested git state

### Phase 2: build-system wiring

1. add `cmake/EcCodesConfig.cmake`
2. add a top-level build option such as `CPDN_ENABLE_ECCODES`
3. wire ecCodes into the build graph
4. keep linking narrow

Exit criteria:

- repo build can configure and build ecCodes on supported platforms

### Phase 3: minimal repo consumption

1. add one small OpenIFS-side compile-time use of ecCodes
2. keep the first runtime use narrow and testable
3. avoid broader controller refactors during this stage

Exit criteria:

- repo builds and links a real ecCodes call site on the intended platforms

### Phase 4: follow-up features

Potential follow-up work once baseline integration is stable:

- AEC/CCSDS enablement
- Windows static-build fixes if phase 1 had to use shared
- improved runtime smoke tests
- future reconsideration of `vcpkg` packaging

## Risks And Mitigations

### Risk: Windows static build fails

Mitigation:

- treat static as preferred, not assumed
- keep shared fallback acceptable for phase 1
- package DLLs explicitly if required

### Risk: ecCodes install/test logic pollutes the parent CMake build

Mitigation:

- centralise policy in `cmake/EcCodesConfig.cmake`
- disable unnecessary upstream options
- if `add_subdirectory()` proves messy, switch to a more isolated superbuild
  approach later

### Risk: runtime mismatch between ecCodes definitions and OpenIFS expectations

Mitigation:

- preserve the existing slot-staged definitions/samples contract first
- avoid MEMFS or alternate installed-data contracts in phase 1

### Risk: future local ecCodes patches become hard to track

Mitigation:

- keep fork outside the repo
- record imported commit and patch summary in `third_party/eccodes/README.cpdn.md`
- keep the local patch set minimal and reviewable

## Open Questions

These questions should be answered during phase 0 or phase 2:

1. What exact ecCodes target name should the repo link against after
   `add_subdirectory()`?
2. Does upstream ecCodes static linking work cleanly on Windows with the chosen
   feature set?
3. Does the repo need ecCodes-installed definition/sample files at all, or are
   the OpenIFS-provided slot files sufficient?
4. Are there any upstream CMake options that must be forced off to avoid
   unwanted install/test side effects in the parent build?

## Recommended Immediate Next Step

The next concrete step should be phase 0:

- validate a constrained ecCodes build from an external clone on Linux,
  macOS, and Windows before importing source into this repo

That keeps the first implementation move evidence-based and avoids committing
the repo to a packaging strategy before the platform realities are known.
