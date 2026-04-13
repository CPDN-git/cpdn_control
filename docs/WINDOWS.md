# Windows Port Notes

Target: Windows 10+, x86_64, MSVC/Visual Studio.

This note tracks the current state of the Windows port after the recent controller refactors. The repo now has a usable Windows build workflow and a platform-specific child-process seam, but it is still not ready for a full Windows BOINC runtime.

## Current status

Already in place:

- CMake derives a Windows platform triplet of `x86_64-pc-windows-msvc` and rejects 32-bit Windows builds.
- BOINC library discovery in `cmake/BoincConfig.cmake` accepts Windows-style `.lib` files.
- `cmake/BoincConfig.cmake` now prefers a BOINC package path via `find_package(boinc CONFIG)` and keeps the existing manual `BOINC_DIR` fallback for non-`vcpkg` builds.
- The repo has a manual Windows build workflow at `.github/workflows/windows_build.yml`.
- The repo now has checked-in `vcpkg` metadata under `vcpkg/`, including `vcpkg/vcpkg.json`, `vcpkg/vcpkg-configuration.json`, `vcpkg/triplets/`, and the BOINC overlay port under `vcpkg/overlays/boinc/`.
- That workflow now bootstraps the external `vcpkg` tool under `${{ github.workspace }}/vcpkg_tool`, installs BOINC through the repo-local manifest under `${{ github.workspace }}/vcpkg`, uses the repo-owned Windows static triplet, builds `unit_tests`, and is configured to run the full Windows unit test suite.
- Linux-specific compile and link flags such as `-pthread`, `-static`, and `-fsanitize=address` are not applied to the main controller targets on Windows.
- `test_model` now uses platform-aware compile options instead of unconditional `-g -Wall`.
- `lib/cpdn_cpu_time.cpp` has a Windows implementation using `GetProcessTimes`.
- `lib/logging_utils.cpp` already uses `localtime_s` on Windows.
- `set_env_var(...)` now uses `_putenv_s(...)` on Windows.
- `run_process_with_timeout(...)` now has a Windows implementation using `CreateProcessW`, a child-only environment block, timeout handling, Job Object cleanup, and optional combined stdout/stderr capture.
- `api/progressfile_handler.cpp` now uses `_getpid()` on Windows.
- `src/cpdn_main.cpp` no longer depends on `mkdir` and the old unconditional POSIX directory headers for its upload-directory setup.
- `move_and_unzip_app_file(...)` now uses the shared CMake `PLATFORM` triplet rather than hardcoded Linux/macOS filename suffixes.
- the main repo and the separate `zip/` project now use the static MSVC runtime on Windows so they match the static BOINC package from vcpkg

## Windows BOINC Provisioning

The current Windows workflow is still not a full runtime-validation job, but it now uses a real BOINC dependency path and is configured to run the Windows unit tests.

Current Windows dependency path:

- `vcpkg` bootstrapped in the workflow
- BOINC installed from the repo-local pinned `vcpkg` manifest
- repo-owned static Windows triplet used for BOINC linkage
- `unit_tests` built and run in the workflow
- functional tests disabled

The old BOINC stubs have now been removed from the repo.

Note:

- the controller continues to use the in-repo `cpdn_zip` library from `zip/`
- the BOINC `boinc_zip` library provided by the vcpkg port is not used here because that BOINC zip path has known problems on Windows

## Process Control Status

The controller no longer keeps all child-process logic inside a POSIX-only `launch_process(...)` implementation.

Now in place:

- a shared child-process handle type with a portable process id
- a POSIX backend in `lib/process_control_posix.cpp`
- a Windows backend in `lib/process_control_windows.cpp`
- shared controller policy in `src/cpdn_control.cpp` for:
  - `launch_process(...)`
  - `check_child_status(...)`
  - `handle_boinc_client_status(...)`

The POSIX backend now keeps `setrlimit(...)` in the forked child, but all argument/environment preparation is done in the parent first. The child branch is intentionally limited to:

- `chdir(...)`
- `setrlimit(RLIMIT_CORE, ...)`
- `setrlimit(RLIMIT_STACK, ...)`
- `execve(...)`
- `_exit(...)` on failure

This keeps the old OpenIFS-compatible launch model while avoiding the earlier post-`fork()` C++ setup work in the child.

The Windows backend now implements:

- `CreateProcessW` for launch
- explicit child-only environment handoff
- case-insensitive environment-variable merge for Windows child environments
- `WaitForSingleObject(..., 0)` and `GetExitCodeProcess` for polling
- Job Object containment so descendants stay attached to the launched model tree
- `TerminateJobObject(...)` for forced shutdown of the full child tree
- suspend/resume across the attached job tree via Tool Help thread enumeration and `SuspendThread` / `ResumeThread`

The obsolete `process_env_overrides()` testing path has been removed. Model environment variables are now prepared as data and passed to the child launcher rather than being applied to the parent controller process.

## Remaining Windows Blockers

### 1. Full Windows runtime has not yet been validated with real BOINC libraries

The current workflow now compiles and runs unit tests against a real BOINC package, but it still does not prove:

- link/runtime behavior against real Windows BOINC headers/libs
- BOINC DLL discovery and execution environment
- end-to-end controller behavior under a Windows BOINC-style task layout
- runtime behavior of the new Job Object suspend/resume and tree-termination logic under real model workloads

To move beyond the current build-only validation:

- keep the vcpkg-based BOINC path working and stable in CI
- validate the re-enabled full Windows unit-test run in CI

### 2. Some test coverage is still not Windows-ready

The launch/status unit test has been refactored to use the new cross-platform process-control seam, but broader Windows test support is still incomplete.

Known follow-up areas:

- the full Windows unit-test run now needs GitHub Actions validation after re-enabling `RunProcessWithTimeoutTest`
- several tests still use Unix-centric assumptions or helpers directly
- the Windows workflow still disables functional tests
- the threaded suspend/resume and descendant-process termination paths need Windows-specific runtime coverage, not just compile success

## Next practical milestones

The current Windows workflow is still useful as a development build/test path:

- trigger it manually
- inspect the uploaded build logs
- fix the next MSVC compile error
- repeat

Do not promote it to required CI yet.

Recommended near-term order:

1. Validate the full Windows unit-test run in GitHub Actions and clear any remaining platform-specific failures.
2. Keep clearing any remaining Windows unit-test/runtime issues in small patches.
3. Consider when to enable functional tests on Windows.
4. Validate runtime behavior, not just compilation.

The current Windows path now uses the static vcpkg BOINC build, and the repo's MSVC runtime has been aligned to match it.
