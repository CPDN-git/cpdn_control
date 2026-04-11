# Windows Port Notes

Target: Windows 10+, x86_64, MSVC/Visual Studio.

This note tracks the current state of the Windows port after the recent controller refactors. The repo now has a usable Windows compile-probe workflow and a platform-specific child-process seam, but it is still not ready for a full Windows BOINC runtime.

## Current status

Already in place:

- CMake derives a Windows platform triplet of `x86_64-pc-windows-msvc` and rejects 32-bit Windows builds.
- BOINC library discovery in `cmake/BoincConfig.cmake` accepts Windows-style `.lib` files.
- The repo has a manual Windows compile-probe workflow at `.github/workflows/windows_build_probe.yml`.
- That workflow currently uses compile-only BOINC stubs and disables tests so it can surface MSVC build errors before a real Windows BOINC build exists.
- Linux-specific compile and link flags such as `-pthread`, `-static`, and `-fsanitize=address` are not applied to the main controller targets on Windows.
- `test_model` now uses platform-aware compile options instead of unconditional `-g -Wall`.
- `lib/cpdn_cpu_time.cpp` has a Windows implementation using `GetProcessTimes`.
- `lib/logging_utils.cpp` already uses `localtime_s` on Windows.
- `set_env_var(...)` now uses `_putenv_s(...)` on Windows.
- `api/progressfile_handler.cpp` now uses `_getpid()` on Windows.
- `src/cpdn_main.cpp` no longer depends on `mkdir` and the old unconditional POSIX directory headers for its upload-directory setup.

## Process Control Status

The controller no longer keeps all child-process logic inside a POSIX-only `launch_process(...)` implementation.

Now in place:

- a shared child-process handle type with a portable process id
- a POSIX backend in `src/process_control_posix.cpp`
- a Windows backend in `src/process_control_windows.cpp`
- shared controller policy in `src/cpdn_control.cpp` for:
  - `launch_process(...)`
  - `check_child_status(...)`
  - `handle_boinc_client_status(...)`

The Windows backend now implements:

- `CreateProcessW` for launch
- explicit child-only environment handoff
- `WaitForSingleObject(..., 0)` and `GetExitCodeProcess` for polling
- `TerminateProcess` for forced shutdown
- suspend/resume via Tool Help thread enumeration and `SuspendThread` / `ResumeThread`

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

To move beyond the probe:

- provide a real Windows BOINC install/artifact under `BOINC_DIR/include` and `BOINC_DIR/lib`
- disable `CPDN_USE_BOINC_STUBS`
- re-enable the relevant test/build stages in the workflow

### 3. Some test coverage is still not Windows-ready

The launch/status unit test has been refactored to use the new cross-platform process-control seam, but broader Windows test support is still incomplete.

Known follow-up areas:

- `t_run_process_with_timeout.cpp` still assumes the helper exists on Windows
- several tests still use Unix-centric environment helpers directly
- the Windows probe workflow still disables unit and functional tests on purpose

### 4. Application package naming still assumes Linux/macOS strings

`move_and_unzip_app_file(...)` in `src/cpdn_control.cpp` still hardcodes:

- `x86_64-apple-darwin`
- `aarch64-poky-linux`
- `x86_64-pc-linux-gnu`

There is still no Windows branch there. That should be aligned with the shared `PLATFORM` triplet instead of maintaining separate hardcoded platform strings.

## Practical Meaning For GitHub Actions

For now, the Windows workflow is useful as a development probe:

- trigger it manually
- inspect the uploaded build logs
- fix the next MSVC compile error
- repeat

Do not promote it to required CI yet.

Before enabling a real Windows CI job:

1. Provide real Windows BOINC headers/libs or a BOINC build job/artifact.
2. Replace the compile-only stub mode with a real `BOINC_DIR`.
3. Implement the Windows branch of `run_process_with_timeout(...)`.
4. Re-enable and port the relevant tests.
5. Validate runtime behavior, not just compilation.

Prefer dynamic BOINC linkage on Windows unless a fully static BOINC build is confirmed to work.
