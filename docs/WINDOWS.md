# Windows Port Notes

Target: Windows 10+, x86_64, MSVC/Visual Studio.

This note tracks the current state of the Windows port after the recent controller refactors. The repo now has a usable Windows build-and-unit-test workflow, a platform-specific child-process seam, and a real `vcpkg` BOINC dependency path, but it is still not yet validated as a full Windows BOINC runtime end-to-end.

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
- `stage_and_unzip_app_file(...)` now uses the shared CMake `PLATFORM` triplet rather than hardcoded Linux/macOS filename suffixes.
- the main repo and the separate `zip/` project now use the static MSVC runtime on Windows so they match the static BOINC package from vcpkg

## Windows BOINC Provisioning

The current Windows workflow is still not a full runtime-validation job, but it now uses a real BOINC dependency path and runs the Windows unit tests.

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
  - `apply_boinc_suspend_resume(...)`
  - `check_boinc_before_launch(...)`
- common bounded termination confirmation in `lib/process_control.cpp`
- final exit decisions and controller-owned model cleanup in `src/task_lifecycle.cpp`

For comparison, the POSIX backend prepares model arguments and environment in
the parent, then the child establishes its process group with `setpgid(...)`,
changes working directory, redirects stdout through `freopen`/`setvbuf`, applies
resource limits, and calls `execve(...)` or `_exit(...)` on failure. Linux applies
the unlimited-stack setting; macOS skips that setting. The parent also establishes
or verifies the private process group before returning a successful launch.

The Windows backend now implements:

- `CreateProcessW` for launch
- explicit child-only environment handoff
- case-insensitive environment-variable merge for Windows child environments
- `WaitForSingleObject(..., 0)` and `GetExitCodeProcess` for leader polling while retaining process/job handles after leader exit
- Job Object containment so descendants stay attached to the launched model tree
- `TerminateJobObject(...)` for forced shutdown of the owned job tree
- `QueryInformationJobObject(..., JobObjectBasicAccountingInformation, ...)` to require zero active job processes as well as leader exit during confirmation
- bounded cleanup of a suspended root with `TerminateProcess(...)` if job assignment fails before it can be resumed
- suspend/resume across the attached job tree via Tool Help thread enumeration and `SuspendThread` / `ResumeThread`

The obsolete `process_env_overrides()` testing path has been removed. Model environment variables are now prepared as data and passed to the child launcher rather than being applied to the parent controller process.

## Controller-owned shutdown and confirmation

`CreateProcessW` creates the model suspended, and the launcher assigns it to its
Job Object before resuming execution. The controller retains ownership handles
after leader exit so remaining job processes can still be terminated. The Job
Object also has `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` as a fallback; controller
cleanup explicitly requests termination and attempts confirmation before closing
handles.

Every controlled final exit invokes shared model cleanup, including restartable
BOINC quit. Confirmation uses one two-second monotonic wait budget for the owned
process set. Failed job assignment or resume also attempts bounded cleanup before
returning a failed launch. Synchronous helper programs use the same confirmation
mechanism. Processes are selected by recorded ownership, not executable name;
old orphan discovery is outside scope.

The two-second budget does not cap total shutdown latency. BOINC status checks
read flags already received by the API, and synchronous model work can delay
controller-side polling. See [BOINC exit behaviour](boinc_exit_behaviour.md) for
the remaining review findings: normal uploads occur before final descendant
cleanup, error finalization skips uploads on unconfirmed cleanup, and diagnostics
currently ignore the helper cleanup result when accepting output. These are
current limits, not completed corrective work.

## Remaining Windows Blockers

### 1. Full Windows runtime has not yet been validated end-to-end under a real BOINC task environment

The current workflow now compiles and runs unit tests against a real BOINC package, but it still does not prove:

- BOINC DLL discovery and execution environment in a real deployed task layout
- end-to-end controller behavior under a Windows BOINC-style task layout
- runtime behavior of the new Job Object suspend/resume and tree-termination logic under real model workloads

To extend the current build-and-unit-test validation:

- keep the vcpkg-based BOINC path working and stable in CI
- validate the re-enabled full Windows unit-test run in CI

### 2. Some test coverage is still not Windows-ready

The launch/status unit test has been refactored to use the new cross-platform process-control seam, and the workflow now runs the Windows unit suite, but broader Windows test support is still incomplete.

Known follow-up areas:

- keep watching the full Windows unit-test run in GitHub Actions for regressions
- several tests still use Unix-centric assumptions or helpers directly
- the Windows workflow still disables functional tests
- run `TaskLifecycleTest` natively for running/suspended children, descendant cleanup after leader exit, process-set isolation and executable overwrite
- validate job-assignment/resume failures, retained-handle cleanup and zero-active-process confirmation natively; the shutdown correction has only been executed on Linux so far
- the deadline unit case uses a zero budget, so nonzero-deadline expiry remains a separate coverage gap
- broader shutdown timing needs Windows BOINC/service testing, not just compile success

## Next practical milestones

The current Windows workflow is useful as a development build/test path:

- trigger it manually
- inspect the uploaded build logs
- fix the next MSVC compile error
- repeat

Do not promote it to required CI yet unless the Windows runtime story becomes materially stronger.

Recommended near-term order:

1. Validate the full Windows unit-test run in GitHub Actions and clear any remaining platform-specific failures.
2. Keep clearing any remaining Windows unit-test/runtime issues in small patches.
3. Consider when to enable functional tests on Windows.
4. Validate runtime behavior, not just compilation.

The current Windows path now uses the static vcpkg BOINC build, and the repo's MSVC runtime has been aligned to match it.
