# Windows BOINC via vcpkg Plan

Target branch for this work: `win_port_vpkg`, created from the current head of `win_port`.

This note describes a small-step plan for replacing the temporary Windows BOINC stubs with a real BOINC dependency provided by `vcpkg`, initially for the Windows GitHub Actions build only.

## Goals

- provide a real BOINC build for the Windows GitHub Actions workflow
- remove `CPDN_USE_BOINC_STUBS` from the main Windows build path as soon as the real BOINC path is working
- keep Linux and macOS on the current BOINC dependency path for now
- keep the integration reversible and low-risk while the Windows port is still in progress

## Current position

- `cpdn_control` currently builds against a manually provided BOINC install via `BOINC_DIR`
- `cmake/BoincConfig.cmake` expects:
  - headers in `include/`
  - libraries in `lib/`
  - libraries named `boinc` and `boinc_api`
- the Windows workflow currently uses `CPDN_USE_BOINC_STUBS=ON`
- the pinned vcpkg BOINC port builds BOINC `8.2.9`, installs headers to `include/boinc`, and exports CMake targets:
  - `unofficial::boinc::boinc`
  - `unofficial::boinc::boincapi`
  - `unofficial::boinc::boinc_zip`

Notes:

- For `cpdn_control`, using the latest BOINC is acceptable. There is no need to stay on local BOINC `8.0.2`.
- `libsched` is not needed by this repo, so the narrower vcpkg BOINC packaging is a good fit here.
- The `boincapi` name comes from the vcpkg port's CMake shim, not from this repo.
- `cpdn_control` should continue to use its own `cpdn_zip` library from `zip/`. The BOINC `boinc_zip` library should not be adopted here because there are known problems with the BOINC-supplied zip library on Windows.

## Proposed approach

### 1. Add vcpkg-aware BOINC resolution to the CMake framework

Keep `cmake/BoincConfig.cmake` as the single dependency seam.

Update it so the BOINC dependency is resolved in this order:

1. If `CPDN_USE_BOINC_STUBS=ON`, use the existing compile-only stubs.
2. Otherwise, try `find_package(boinc CONFIG QUIET)`.
3. If that succeeds, use the vcpkg-exported targets.
4. Otherwise, fall back to the existing `BOINC_DIR/include` and `BOINC_DIR/lib` search.
5. If neither path works, fail with a clear message.

Expected target mapping for the vcpkg path:

- `unofficial::boinc::boinc`
- `unofficial::boinc::boincapi`

The controller build should not need `unofficial::boinc::boinc_zip`.
It should continue to link the existing in-repo `cpdn_zip` library instead.

This change should be scoped so:

- Windows can use the vcpkg package path
- Linux and macOS can continue to use `BOINC_DIR` unchanged
- developer builds without vcpkg still work

### 2. Keep the install location outside the repo if desired

A local directory such as `/home/glenn/github/vpkg` is acceptable for developer use.

For GitHub Actions, the important requirement is that the vcpkg root and installed package paths are stable enough to cache. The path does not need to live inside this repo.

Two practical options:

- local/developer machines:
  - `/home/glenn/github/vpkg`
- GitHub Actions:
  - a directory under `${{ github.workspace }}` for simplicity

The implementation should avoid hardcoding a Linux-specific absolute path into the repo CMake.

### 3. Add a Windows GitHub Actions workflow path that builds BOINC via vcpkg

The Windows workflow should:

1. bootstrap or restore `vcpkg`
2. install BOINC and its dependencies for the Windows triplet
3. configure this repo with the vcpkg toolchain file
4. build the controller and `unit_tests`
5. keep functional tests disabled for now

Once this path works, remove `CPDN_USE_BOINC_STUBS=ON` from the main Windows workflow path.

## GitHub Actions caching plan

The workflow should cache the vcpkg state so BOINC is not rebuilt from scratch on every run.

Recommended cache contents:

- the vcpkg checkout/tool directory
- the vcpkg `installed/` tree for the selected Windows triplet
- optionally the vcpkg binary cache directory if used

Recommended cache key inputs:

- runner OS
- target triplet
- vcpkg commit or release tag
- the BOINC package selection input used by the workflow
- files that affect dependency resolution, such as the workflow file and any future vcpkg manifest/config files

Example cache dimensions:

- `windows-latest`
- `x64-windows-static` or whichever triplet is chosen
- pinned vcpkg commit

Important:

- the cache is compatible with a vcpkg directory outside the repo
- the cache should be treated as an accelerator, not as the only source of BOINC
- the workflow must still be able to install BOINC from scratch on a cold cache
- for the GitHub Actions implementation, prefer placing the vcpkg tree under `${{ github.workspace }}` because it is the simpler option to reason about and cache

## Suggested implementation order

1. Create branch `win_port_vpkg` from `win_port`.
2. Update `cmake/BoincConfig.cmake` to support both:
   - vcpkg `find_package(boinc CONFIG)`
   - existing manual `BOINC_DIR` fallback
3. Update the Windows workflow to bootstrap `vcpkg`, install BOINC, and configure with the vcpkg toolchain file.
4. Add caching for the vcpkg root and installed triplet artifacts.
5. Remove `CPDN_USE_BOINC_STUBS=ON` from the active Windows workflow path.
6. Run the workflow and fix the next real BOINC-backed Windows build issue.
7. Only after the BOINC-backed Windows compile path is stable, consider whether to keep or delete the BOINC stubs entirely.

## Deferred items

Not part of the first implementation:

- migrating Linux to vcpkg
- migrating macOS to vcpkg
- implementing the Windows `run_process_with_timeout(...)` path
- enabling functional tests on Windows
- deleting the BOINC stubs immediately before the vcpkg path is proven

## Linux and macOS follow-up

Linux:

- possible later, but not recommended as part of the first change
- the current BOINC path already works and should remain the default there for now

macOS:

- worth reconsidering after the Windows vcpkg flow is stable
- the vcpkg BOINC port appears structured to support macOS as well
- if Windows integration is clean, the same package-based approach may become attractive for a later macOS port

## Acceptance criteria

The first implementation should be considered successful if:

- the Windows workflow can install a real BOINC package via vcpkg
- `cpdn_control` configures and compiles against real BOINC on `windows-latest`
- `unit_tests` also compile in that workflow
- the BOINC stubs are no longer needed for the main Windows workflow path
- Linux and macOS builds remain unaffected
