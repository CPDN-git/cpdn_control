# Windows Port Notes

Target: Windows 10+, x86_64, MSVC/Visual Studio.

This note tracks the current state of the Windows port after the recent controller refactors. The repo now has some Windows-aware build and test plumbing, but it is not yet ready for a full Windows build-and-run workflow.

## Current status

Already in place:

- CMake derives a Windows platform triplet of `x86_64-pc-windows-msvc` and rejects 32-bit Windows builds.
- BOINC library discovery in `cmake/BoincConfig.cmake` now accepts Windows-style `.lib` files.
- Linux-specific compile and link flags such as `-pthread`, `-static`, and `-fsanitize=address` are not applied to the main controller targets on Windows.
- The functional test harness is now platform-aware:
  - `tests/functional/run.py` appends `.exe` when needed
  - `tests/functional/run.py` and `tests/functional/setup_test.py` use `CPDN_PLATFORM`
  - `tests/functional/CMakeLists.txt` prepends `BOINC_LIB_DIR` to `PATH` on Windows
  - the fixture generator models BOINC logical inputs using `<soft_link>...</soft_link>` files rather than filesystem symlinks
- `lib/cpdn_cpu_time.cpp` has a Windows implementation using `GetProcessTimes`.
- `lib/logging_utils.cpp` already uses `localtime_s` on Windows.

Still true:

- A working Windows BOINC build is still required under a `BOINC_DIR/include` and `BOINC_DIR/lib` layout.
- The controller runtime still depends heavily on POSIX process-control code and will not yet build or run cleanly on Windows.

## What The Refactors Changed

The recent refactors improved the Windows story indirectly:

- Model input staging is now driven by the model manifest rather than hardcoded controller logic in `main()`.
- BOINC input staging now resolves logical BOINC files, validates the `jf_*` project archive, copies that archive into the slot, and unzips from the copied archive.
- Functional tests now mirror BOINC logical input behaviour with `<soft_link>` files, which is a better fit for cross-platform testing than relying on real symlinks.

Those changes make the startup path more portable, but they do not remove the remaining Unix-specific runtime/process code.

## Remaining Windows Blockers

### 1. Process launch and child control are still POSIX-only

The main controller runtime in `src/cpdn_control.cpp` still uses Unix process APIs:

- `fork()`
- `execl()`
- `waitpid()`
- `kill()`
- `SIGKILL`, `SIGSTOP`, `SIGCONT`
- `setrlimit()`

Affected functions include:

- `launch_process(...)`
- `check_child_status(...)`
- `handle_boinc_client_status(...)`

This is the biggest runtime blocker. A Windows implementation will need `CreateProcess`-style launch and Windows-native suspend/resume/terminate/wait handling, or a small abstraction layer above platform-specific process control.

### 2. Timed external-process execution is not implemented on Windows

`lib/utils.cpp` contains `run_process_with_timeout(...)`, but the Windows branch currently returns the default `TimedProcessResult` immediately without spawning anything.

This matters for:

- the experimental diagnostics path in `src/external_diagnostics.cpp`
- the `t_run_process_with_timeout` unit test

Until this helper has a real Windows implementation, diagnostics support on Windows is effectively blocked.

### 3. Environment helper is still Unix-only

`set_env_var(...)` in `lib/utils.cpp` still calls `setenv(...)` unconditionally.

For Windows this should be changed to `_putenv_s(...)` or an equivalent wrapper. The current note that this helper "should use `_putenv_s`" is still an outstanding task, not something already completed.

### 4. A few source files still have direct Unix header or API dependencies

Current examples:

- `src/cpdn_main.cpp` includes `<dirent.h>`, `<sys/stat.h>`, and `<sys/types.h>` unconditionally.
- `api/progressfile_handler.cpp` includes `<unistd.h>` and writes `getpid()` into the progress file.
- `src/cpdn_control.h` includes `<sys/types.h>` for `pid_t`.

These are compile blockers for a straightforward MSVC build and should be hidden behind portable wrappers or guarded includes.

### 5. Some tests are still Unix-only

Several unit tests assume POSIX APIs directly:

- `tests/unit/t_launch_process.cpp` uses `<unistd.h>`, `setenv`, and signal-based expectations
- `tests/unit/t_run_process_with_timeout.cpp` uses `setenv` and `unsetenv`
- `tests/unit/launch_process_helper.cpp` raises `SIGTERM`

The CPU-time test already has a Windows path, but the process-control tests still need either Windows variants or conditional exclusion.

### 6. `test_model` compile options still need a Windows check

Top-level `CMakeLists.txt` still applies:

- `-g`
- `-Wall`

directly to `test_model` with:

```cmake
target_compile_options(test_model PRIVATE -g -Wall)
```

That is fine for GCC/Clang but not for MSVC. The main controller targets already route options through shared helpers; `test_model` should follow the same platform-aware pattern.

### 7. App package naming still assumes Linux/macOS

`move_and_unzip_app_file(...)` in `src/cpdn_control.cpp` still hardcodes:

- `x86_64-apple-darwin`
- `aarch64-poky-linux`
- `x86_64-pc-linux-gnu`

There is no Windows branch yet. That should be aligned with the shared `PLATFORM` triplet instead of maintaining separate hardcoded platform strings here.

## Practical Meaning For A GitHub Windows Workflow

Do not add the Windows GitHub Actions job yet as a normal required build. The repo has enough groundwork to define the workflow shape, but not enough to expect a successful build-and-test run.

Before enabling a real Windows workflow:

1. Provide Windows BOINC headers/libs in a predictable `BOINC_DIR`.
2. Remove the remaining MSVC compile blockers listed above.
3. Implement Windows process launch/control for the controller runtime.
4. Implement the Windows branch of `run_process_with_timeout(...)`.
5. Make the Unix-only unit tests conditional or port them.
6. Fix `test_model` compile options for MSVC.

Once that is done, the workflow should:

- run on `windows-latest`
- configure CMake with `BOINC_DIR`
- build normally with the MSVC generator/toolchain
- run `ctest`
- ensure BOINC `.dll` locations are added to `PATH`

Prefer dynamic BOINC linkage on Windows unless a fully static BOINC build is confirmed to work.
