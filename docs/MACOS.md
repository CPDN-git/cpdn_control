# macOS Port Notes

This document tracks the current state of the Apple Silicon macOS port for `cpdn_control`.

Current scope:
- target Apple Silicon only
- use the repo-local `vcpkg` BOINC dependency path first
- keep `BOINC_DIR` as the manual fallback
- keep the manual GitHub Actions build as the first-line validation path
- run unit tests in CI
- keep functional tests disabled in CI for now

## Current Status

The repository has an Apple Silicon build and unit-test workflow. Its presence
does not establish native runtime validation of every process-control change.

Already in place:
- `CMakeLists.txt` already derives `PLATFORM=arm64-apple-darwin` on Apple Silicon
- `cmake/BoincConfig.cmake` already prefers `find_package(boinc CONFIG)` and allows shared BOINC libraries on Apple
- `lib/cpdn_cpu_time.cpp` already has a macOS implementation using `proc_pid_rusage`
- `tests/functional/CMakeLists.txt` already injects `DYLD_LIBRARY_PATH` when BOINC runtime libraries exist
- the repo already has pinned `vcpkg` manifest metadata and repo-owned triplets for Linux, Windows, and macOS
- the repo now has the Apple Silicon `arm64-osx-cpdn` triplet checked in under `vcpkg/triplets/`
- the repo now has a manual macOS GitHub Actions workflow at `.github/workflows/macos_build.yml`
- that workflow bootstraps pinned `vcpkg`, installs BOINC through the repo-local manifest, builds `cpdn_zip`, configures the controller with `CPDN_REQUIRE_STATIC_BOINC=OFF`, builds `unit_tests`, and runs the full macOS unit test suite
- the workflow uploads both logs and the built Apple controller artifact on success

This means the first macOS porting phase has moved beyond planning and into build/test validation with the current repo-owned workflow.

## Apple Platform Naming

Apple naming is slightly inconsistent across tools, so this repo should keep the distinctions explicit.

- CPU architecture:
  - Apple Silicon: `arm64`
  - Intel Mac: `x86_64`
- Repo binary/platform naming:
  - use `arm64-apple-darwin`
  - this is the value already derived by the top-level `CMakeLists.txt`
- `vcpkg` triplet naming:
  - `vcpkg` uses `osx` rather than `apple-darwin`
  - for this repo, use the overlay triplet `arm64-osx-cpdn`
- Deployment target:
  - this is the minimum macOS version the built binaries may run on
  - if we want to support the first Apple Silicon machines, target macOS `11.0`

Reason for `11.0`:
- the first M1 Macs arrived with macOS Big Sur, which is macOS 11
- using `11.0` is the cleanest baseline if the goal is "support Apple Silicon from the start of the M1 era"

## Build Policy

For macOS, the repo should not try to mirror the Linux fully static build.

Policy for the Apple build:
- prefer the `vcpkg` BOINC package
- allow BOINC shared libraries on Apple
- keep `BOINC_DIR` as fallback if the package path is unavailable or broken
- keep using the in-repo `cpdn_zip` library rather than BOINC's `boinc_zip`

This matches the current CMake defaults:
- `CPDN_REQUIRE_STATIC_BOINC=OFF` on Apple
- no `-static` release link option on Apple

## Code Review Findings

The following separates implemented build support from native validation still
needed for process-control changes.

Likely already sufficient:
- [CMakeLists.txt](/home/glenn/github/cpdn_control/CMakeLists.txt)
  - already derives the correct Apple platform string
  - already avoids Linux-only static release linking on Apple
- [cmake/BoincConfig.cmake](/home/glenn/github/cpdn_control/cmake/BoincConfig.cmake)
  - already allows shared BOINC libraries on Apple
  - already keeps the manual `BOINC_DIR` fallback
  - already contains defensive logic for incomplete imported-target include metadata
- [lib/cpdn_cpu_time.cpp](/home/glenn/github/cpdn_control/lib/cpdn_cpu_time.cpp)
  - already has an Apple-specific process CPU-time implementation
- [tests/functional/CMakeLists.txt](/home/glenn/github/cpdn_control/tests/functional/CMakeLists.txt)
  - already handles `DYLD_LIBRARY_PATH`

Areas still needing real build validation:
- [lib/process_control_posix.cpp](/home/glenn/github/cpdn_control/lib/process_control_posix.cpp)
  - stack-limit changes are intentionally skipped on Apple
  - launches establish a private group with `setpgid(...)` before reporting success
  - `waitid(..., WNOWAIT)` observes leader exit while retaining its PID/group identity until termination is requested
  - confirmation reaps the leader and checks group presence with a two-second monotonic wait budget
  - validate running/suspended children, descendants after leader exit, zombie-only group presence, and immediate executable overwrite natively
- BOINC imported target metadata from `vcpkg`
  - the Windows work found that the BOINC package metadata can be incomplete
  - macOS may need the same compatibility path
- runtime validation
  - Linux test results for the shutdown correction do not establish native Apple behaviour
  - the workflow must be run for the changed `waitid`, process-group and descendant-cleanup paths
  - the root [README.md](/home/glenn/github/cpdn_control/README.md) already describes macOS build support; build availability and real BOINC runtime validation remain separate claims

## GitHub Actions Status

The repo now has a separate manual-only Apple Silicon workflow:
- `workflow_dispatch` only
- Apple Silicon GitHub-hosted runner input (`macos-14`, `macos-15`, or `macos-latest`)
- bootstraps pinned `vcpkg`
- installs BOINC with the repo's `arm64-osx-cpdn` triplet
- builds `cpdn_zip`
- configures and builds `cpdn_control`
- runs the full unit test suite
- keeps functional tests disabled for now

Runner note:
- the current workflow uses the standard GitHub-hosted Apple Silicon runner labels such as `macos-14`
- if those labels are unavailable to the repository, use a self-hosted Apple Silicon runner instead

## Apple Build Settings

The checked-in workflow configures:

- `VCPKG_TARGET_TRIPLET=arm64-osx-cpdn` (workflow variable `VCPKG_TRIPLET`)
- `CMAKE_OSX_ARCHITECTURES=arm64`
- `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`
- `PLATFORM=arm64-apple-darwin`
- `CPDN_BUILD_UNIT_TESTS=ON`
- `CPDN_BUILD_FUNCTIONAL_TESTS=OFF`
- `CPDN_REQUIRE_STATIC_BOINC=OFF`
- `VCPKG_INSTALLED_DIR=<repo>/vcpkg_installed`

Use the workflow as the source of truth for its current runner and build settings.

## Controller-owned shutdown

The shared exit helper in `src/task_lifecycle.cpp` owns model cleanup on all
controlled exits, including BOINC quit/abort/heartbeat loss. The POSIX backend
is shared with Linux, but macOS behaviour must be validated natively. Normal
model descendants inherit the private process group; descendants that deliberately
leave it are outside the process-group guarantee. Recovery of old orphans is not
implemented.

The two-second budget bounds cleanup confirmation, not total shutdown latency.
Synchronous diagnostics have a separate execution timeout and can delay the next
BOINC status check. Review accepted the current finalization ordering and
unconfirmed-cleanup upload policy for OpenIFS and WRF because they use threads
and create no model subprocess descendants. Diagnostics now reject output on
failed or expired helper cleanup. See the decisions and future-model limits in
[BOINC exit behaviour](boinc_exit_behaviour.md).

The Linux-specific subreaper used by descendant unit tests is not compiled on
macOS. Native descendant tests therefore also depend on the system reaper's
behaviour; a lingering zombie can keep a group-presence probe positive without
keeping an executable image busy.

## Next Steps

1. Keep the manual macOS workflow passing as BOINC, `vcpkg`, and runner images change.
2. Decide when macOS functional tests are practical, especially around BOINC runtime library setup and test fixture expectations.
3. Run native lifecycle tests for the shutdown correction and record the results separately from Linux validation.
4. Decide whether the macOS workflow should remain manual-only or become a broader CI signal later.
