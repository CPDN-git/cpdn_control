# Windows Port Notes

Target: Windows 10+, x86_64, MSVC/Visual Studio.

This note tracks the current state of the Windows port after the recent controller refactors. The repo now has a usable Windows compile-probe workflow and a platform-specific child-process seam, but it is still not ready for a full Windows BOINC runtime.

## Current status

Already in place:

- CMake derives a Windows platform triplet of `x86_64-pc-windows-msvc` and rejects 32-bit Windows builds.
- BOINC library discovery in `cmake/BoincConfig.cmake` accepts Windows-style `.lib` files.
- The repo has a manual Windows compile-probe workflow at `.github/workflows/windows_build_probe.yml`.
- That workflow currently uses compile-only BOINC stubs, builds `unit_tests` for extra compile coverage, and still skips functional tests so it can surface MSVC build errors before a real Windows BOINC build exists.
- Linux-specific compile and link flags such as `-pthread`, `-static`, and `-fsanitize=address` are not applied to the main controller targets on Windows.
- `test_model` now uses platform-aware compile options instead of unconditional `-g -Wall`.
- `lib/cpdn_cpu_time.cpp` has a Windows implementation using `GetProcessTimes`.
- `lib/logging_utils.cpp` already uses `localtime_s` on Windows.
- `set_env_var(...)` now uses `_putenv_s(...)` on Windows.
- `api/progressfile_handler.cpp` now uses `_getpid()` on Windows.
- `src/cpdn_main.cpp` no longer depends on `mkdir` and the old unconditional POSIX directory headers for its upload-directory setup.
- `move_and_unzip_app_file(...)` now uses the shared CMake `PLATFORM` triplet rather than hardcoded Linux/macOS filename suffixes.

## Temporary Probe Infrastructure

The current Windows workflow is still a compile probe, not a runtime validation job.

Temporary pieces now in use:

- `CPDN_USE_BOINC_STUBS=ON`
- compile-only BOINC headers from `cmake/boinc_stub/include`
- `unit_tests` built for extra compile coverage
- functional tests disabled

This is useful for short-term porting work, but the BOINC stubs should not remain in the code indefinitely.

They should be removed or at least stopped from being the default Windows porting path soon, because long-lived stubs create drift risk:

- the controller can compile against interfaces that no longer match real BOINC headers/libs
- Windows-specific BOINC link/runtime issues stay hidden
- CI can start to look healthier than the real product state

So treat the stubs as a temporary bridge only, and plan to replace them with a real Windows BOINC build/artifact path once the next round of compile blockers is cleared.

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

### 1. Timed external-process execution is still stubbed on Windows

`lib/utils.cpp` still returns a default result from the Windows branch of `run_process_with_timeout(...)`.

This still blocks:

- the experimental diagnostics path in `src/external_diagnostics.cpp`
- meaningful Windows coverage for `t_run_process_with_timeout`

### 2. Full Windows runtime has not yet been validated with real BOINC libraries

The current workflow is intentionally a compile probe. It does not yet prove:

- link/runtime behavior against real Windows BOINC headers/libs
- BOINC DLL discovery and execution environment
- end-to-end controller behavior under a Windows BOINC-style task layout
- runtime behavior of the new Job Object suspend/resume and tree-termination logic under real model workloads

To move beyond the probe:

- provide a real Windows BOINC install/artifact under `BOINC_DIR/include` and `BOINC_DIR/lib`
- disable and then retire `CPDN_USE_BOINC_STUBS` from the active Windows workflow path
- re-enable the relevant test/build stages in the workflow

### 3. Some test coverage is still not Windows-ready

The launch/status unit test has been refactored to use the new cross-platform process-control seam, but broader Windows test support is still incomplete.

Known follow-up areas:

- `t_run_process_with_timeout.cpp` will still fail at runtime on Windows until `run_process_with_timeout(...)` is implemented there
- several tests still use Unix-centric environment helpers directly
- the Windows probe workflow still disables functional tests and does not run the Windows-built unit test binary
- the threaded suspend/resume and descendant-process termination paths need Windows-specific runtime coverage, not just compile success

## Next practical milestones

The current Windows workflow is still useful as a development probe:

- trigger it manually
- inspect the uploaded build logs
- fix the next MSVC compile error
- repeat

Do not promote it to required CI yet.

Recommended near-term order:

1. Clear the next compile/runtime portability issues while keeping changes small.
2. Add a real Windows BOINC install/artifact path to the workflow.
3. Stop using the BOINC stubs in the active Windows workflow path.
4. Implement the Windows branch of `run_process_with_timeout(...)`.
5. Re-enable and then run the relevant tests on Windows.
6. Validate runtime behavior, not just compilation.

Prefer dynamic BOINC linkage on Windows unless a fully static BOINC build is confirmed to work.
