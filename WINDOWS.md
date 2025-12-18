# Windows Port Notes (Codex Agent)

Target: Windows 10+, focus on MSVC/Visual Studio (x86_64). Code is unchanged; this lists remaining Windows-specific work.

- BOINC deps
  - Build BOINC for Windows (MSVC, x64). Export headers and `.lib` (or `.dll` + import libs). Provide an install layout similar to `BOINC_DIR/include` and `BOINC_DIR/lib`.
  - Confirm whether static libs are available; if not, ensure `.dll` deployment and `PATH` adjustments during tests.

- Platform strings/binaries
  - Use the shared `PLATFORM` triplet; for MSVC builds expect `x86_64-pc-windows-msvc`. Align any generated artifact names or fixtures with this when adding Windows support to tests.

- POSIX-only APIs
  - `/proc`, `fork/exec`, `waitpid`, `kill`, `signal`, `chmod`, `setenv`, and many `<unistd.h>` usages are Unix-only. Implement Windows equivalents (process launch/monitoring, CPU time, permissions) or introduce abstraction layers before enabling runtime on Windows.
  - `cpdn_linux_cpu_time` and its unit test are Linux-specific; add a Windows implementation (e.g., `GetProcessTimes`) and gate/adjust the test.

- Paths and permissions
  - `std::filesystem` is fine, but explicit permission calls (`chmod`, `set_exec_perms`) need Windows-aware handling (e.g., `_chmod` or skipping).
  - Environment helpers (`set_env_var`) should use `_putenv_s` on Windows.

- Threading/link flags
  - `-pthread`, `-static`, `-fsanitize` are removed in CMake for Windows, but verify any future flags remain compiler-appropriate (`/W` etc.).

- Functional tests
  - Python harness assumes Linux naming and uses `LD_LIBRARY_PATH`; add `PATH` handling for `.dll` discovery and Windows-safe path joining. Parameterize binary names with `PLATFORM`.
  - File linking/symlinks in tests may need adjustments or developer mode; avoid symlinks where possible.

- CI
  - Add a Windows GitHub Actions job once BOINC Windows artifacts exist; ensure `ctest` uses appropriate env (`PATH` with BOINC DLLs). Avoid static linking unless confirmed available.
