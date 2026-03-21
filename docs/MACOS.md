# macOS Port Notes (Codex Agent)

Target: Apple Silicon (arm64), macOS 13+. Goal: keep code unchanged for now; this doc lists remaining mac-specific work.

- BOINC deps
  - Build BOINC for macOS (arm64) as dynamic libs (`.dylib`); set `BOINC_DIR` in configure. Consider a GitHub Action job to produce/upload these artifacts.
  - If static builds are desired, confirm availability; current CMake prefers `.dylib` on Apple.

- Binary naming/platform strings
  - Ensure a single platform triplet is reused everywhere (CMake cache `PLATFORM`). Use `arm64-apple-darwin` for Apple Silicon when generating binary names and fixture assets.

- CPU time helpers
  - Implemented macOS CPU time path using `proc_pid_rusage`; unit test now exercises non-Linux paths without `/proc` dependency. `cpu_time` uses the platform-aware helper.

- Process/exec and limits
  - In `launch_process` (`src/cpdn_control.cpp`), the stack limit is skipped on Apple. Confirm desired behavior; consider setting a sane macOS-specific stack size instead of skipping.

- Packaging/asset naming
  - Zip naming in `move_and_unzip_app_file` assumes Linux strings except for a mac branch; align with the CMake-derived platform triplet and include Apple Silicon (`arm64`) naming.

- Tests and runners
  - Functional tests now set `DYLD_LIBRARY_PATH` and pass `CPDN_PLATFORM`; Python harnesses pick platform-aware binary names and app packages.

- CI
  - Add a macOS GitHub Actions job once BOINC/mac artifacts are available; reuse the shared `PLATFORM` triplet and avoid `-static` on macOS.
