# Windows BOINC via vcpkg Plan

Target branch for this work: `win_port_vpkg`, created from the current head of `win_port`.

This note started as the implementation plan for replacing the temporary Windows BOINC stubs with a real BOINC dependency provided by `vcpkg`. It now also records what has been completed and what still remains.

## Goals

- provide a real BOINC build for the Windows GitHub Actions workflow
- remove the BOINC stubs from the repo once the real Windows BOINC path is proven
- keep Linux and macOS on the current BOINC dependency path for now
- keep the integration reversible and low-risk while the Windows port is still in progress

## Completed Work

- `cpdn_control` currently builds against a manually provided BOINC install via `BOINC_DIR`
- `cmake/BoincConfig.cmake` now supports:
  - a Windows `find_package(boinc CONFIG)` path for vcpkg
  - the existing `BOINC_DIR/include` and `BOINC_DIR/lib` fallback for non-vcpkg builds
- the BOINC stubs and `CPDN_USE_BOINC_STUBS` option have been removed from the repo
- the pinned vcpkg BOINC port builds BOINC `8.2.9`, installs headers to `include/boinc`, and exports CMake targets:
  - `unofficial::boinc::boinc`
  - `unofficial::boinc::boincapi`
  - `unofficial::boinc::boinc_zip`
- the Windows workflow now:
  - bootstraps `vcpkg` under `${{ github.workspace }}/vcpkg`
  - restores/caches that vcpkg tree
  - installs BOINC from the pinned vcpkg port
  - configures this repo with the vcpkg toolchain
  - builds the controller and `unit_tests`
  - is configured to run the full Windows unit test suite
- the main repo and the separate `zip` project both now use the static MSVC runtime on Windows so they match the static vcpkg BOINC build
- the Windows build is now successful in GitHub Actions
- `lib/utils.cpp` now has a real Windows `run_process_with_timeout(...)` implementation using `CreateProcessW`, a child-only environment block, timeout handling, Job Object cleanup, and optional combined stdout/stderr capture

Notes:

- For `cpdn_control`, using the latest BOINC is acceptable. There is no need to stay on local BOINC `8.0.2`.
- `libsched` is not needed by this repo, so the narrower vcpkg BOINC packaging is a good fit here.
- The `boincapi` name comes from the vcpkg port's CMake shim, not from this repo.
- `cpdn_control` should continue to use its own `cpdn_zip` library from `zip/`. The BOINC `boinc_zip` library should not be adopted here because there are known problems with the BOINC-supplied zip library on Windows.

## Resulting Design

### 1. Add vcpkg-aware BOINC resolution to the CMake framework

Keep `cmake/BoincConfig.cmake` as the single dependency seam.

The BOINC dependency is now resolved in this order:

1. On Windows, try `find_package(boinc CONFIG QUIET)`.
2. If that succeeds, use the vcpkg-exported targets.
3. Otherwise, fall back to the existing `BOINC_DIR/include` and `BOINC_DIR/lib` search.
4. If neither path works, fail with a clear message.

Expected target mapping for the vcpkg path:

- `unofficial::boinc::boinc`
- `unofficial::boinc::boincapi`

The controller build should not need `unofficial::boinc::boinc_zip`.
It should continue to link the existing in-repo `cpdn_zip` library instead.

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

The Windows workflow now:

1. bootstrap or restore `vcpkg`
2. install BOINC and its dependencies for the Windows triplet
3. configure this repo with the vcpkg toolchain file
4. build the controller and `unit_tests`
5. run the Windows unit tests
6. keep functional tests disabled for now

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

## Deferred Items

Still not done:

- enabling functional tests on Windows
- validating end-to-end Windows BOINC runtime behavior rather than just build plus unit tests

Not planned as part of the current Windows work:

- migrating Linux to vcpkg
- migrating macOS to vcpkg

## Linux and macOS follow-up

Linux:

- possible later, but not recommended as part of the current Windows-focused change
- the current BOINC path already works and should remain the default there for now

macOS:

- worth reconsidering after the Windows vcpkg flow is stable
- the vcpkg BOINC port appears structured to support macOS as well
- if the Windows build-plus-unit-test path remains clean, the same package-based approach may become attractive for a later macOS port

## Acceptance criteria

The initial vcpkg integration should now be considered successful because:

- the Windows workflow can install a real BOINC package via vcpkg
- `cpdn_control` configures and compiles against real BOINC on `windows-latest`
- `unit_tests` also compile in that workflow
- the BOINC stubs have been removed from the repo
- Linux and macOS builds remain unaffected

## Next Steps

Recommended next order from here:

1. Validate the full Windows unit-test run in GitHub Actions and clear any remaining platform-specific failures.
2. Decide whether any further Windows unit tests need platform gating or cleanup.
3. Only after that, consider enabling functional tests on Windows.
4. Validate end-to-end Windows BOINC runtime behavior, not just build plus unit tests.
