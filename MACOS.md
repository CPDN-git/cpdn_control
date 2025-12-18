# macOS Port Notes (Codex Agent)

Target: Apple Silicon (arm64), macOS 13+. Goal: keep code unchanged for now; this doc lists remaining mac-specific work.

- BOINC deps
  - Build BOINC for macOS (arm64) as dynamic libs (`.dylib`); set `BOINC_DIR` in configure. Consider a GitHub Action job to produce/upload these artifacts.
  - If static builds are desired, confirm availability; current CMake prefers `.dylib` on Apple.

- Binary naming/platform strings
  - Ensure a single platform triplet is reused everywhere (CMake cache `PLATFORM`). Use `arm64-apple-darwin` for Apple Silicon when generating binary names and fixture assets.

- CPU time helpers
  - `lib/cpdn_linux_cpu_time.cpp` and related unit test rely on `/proc/<pid>/stat` (Linux-only). Add a macOS implementation using `getrusage`/`proc_pid_rusage`, and gate/adjust the unit test to exercise the mac path.
  - `cpu_time` in `lib/utils.cpp` includes a BOINC call on Apple but does not include BOINC headers; either include the header or rework to use the portable path above.

- Process/exec and limits
  - In `launch_process` (`src/cpdn_control.cpp`), the stack limit is skipped on Apple. Confirm desired behavior; consider setting a sane macOS-specific stack size instead of skipping.

- Packaging/asset naming
  - Zip naming in `move_and_unzip_app_file` assumes Linux strings except for a mac branch; align with the CMake-derived platform triplet and include Apple Silicon (`arm64`) naming.

- Tests and runners
  - Functional tests set `LD_LIBRARY_PATH`; add `DYLD_LIBRARY_PATH` for macOS.
  - Test fixtures and `run.py` hard-code Linux binary names (`…x86_64-pc-linux-gnu(-debug)`). Parameterize by `PLATFORM`/arch when copying binaries and creating fake app packages.

- CI
  - Add a macOS GitHub Actions job once BOINC/mac artifacts are available; reuse the shared `PLATFORM` triplet and avoid `-static` on macOS.
