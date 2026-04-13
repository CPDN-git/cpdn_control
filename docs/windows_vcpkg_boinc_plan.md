# BOINC via vcpkg Design And Migration Plan

This note is now the repo-level design and migration plan for supplying the BOINC dependency through `vcpkg`.
It started as a Windows-only plan, but the design questions are now mostly shared across platforms.

The main requirement is unchanged:

- `cpdn_control` release builds must not depend on BOINC shared libraries being preinstalled on remote volunteer hosts
- BOINC linkage for supported deployment builds must therefore remain static
- `vcpkg` is acceptable only if it preserves that deployment model

This document describes:

- the proposed shared dependency design
- how BOINC version pinning and rollback should work
- the current Windows state
- the Linux migration plan
- the deferred macOS follow-up

## Goals

- manage BOINC as a repo-local dependency instead of relying on a shared manual BOINC build used by multiple repositories
- keep static BOINC linkage as a hard requirement for deployment builds
- keep the BOINC dependency path reproducible and version-pinned
- make BOINC version upgrades explicit and reversible
- keep a manual `BOINC_DIR` fallback for local recovery and non-`vcpkg` environments
- avoid adopting the BOINC `boinc_zip` library; continue using the in-repo `cpdn_zip`

## Non-Negotiable Requirements

- Linux and Windows release builds must link BOINC statically
- the final controller executable must not require BOINC runtime libraries to be installed on the host
- CI should verify that requirement rather than assuming it
- the functional test harness should reflect the deployment contract for release-style builds

## Proposed Repo Design

### 1. Use repo-local `vcpkg` manifest mode

The repo should move to a checked-in `vcpkg` manifest configuration:

- `vcpkg/vcpkg.json`
- `vcpkg/vcpkg-configuration.json`

This keeps dependency selection inside the repo and removes the need for a shared external BOINC build tree.

The repo should continue to allow a manual `BOINC_DIR` fallback, but that should become the escape hatch rather than the preferred path.

### 2. Keep BOINC discovery behind a single CMake seam

`cmake/BoincConfig.cmake` should remain the only place that knows how BOINC is found.

Preferred resolution order:

1. try `find_package(boinc CONFIG QUIET)` when the build is using `vcpkg`
2. if that succeeds, use the imported BOINC targets
3. otherwise fall back to `BOINC_DIR/include` and `BOINC_DIR/lib`
4. fail with a clear message if neither path works

That keeps the main `CMakeLists.txt` simpler and avoids platform-specific BOINC logic leaking into the rest of the build.

### 3. Make static linkage an explicit repo policy

The repo should not rely on incidental defaults to preserve static BOINC linkage.

Recommended policy:

- release linkage requirements should be expressed explicitly in repo-owned CMake options and `vcpkg` triplets
- CI should fail if BOINC resolves to shared libraries where static linkage is required
- CI should also fail if the final release executable still has BOINC runtime dependencies

The current Linux build already does this implicitly by:

- selecting BOINC `.a` files in `cmake/BoincConfig.cmake`
- linking the release executable with `-static`

That behaviour should be preserved, but made clearer.

### 4. Use repo-owned static `vcpkg` triplets

The straightforward maintainable approach is to keep the static-linkage policy under repo control.

Recommended structure:

- `vcpkg/triplets/x64-linux-static-release.cmake`
- `vcpkg/triplets/x64-windows-static-release.cmake` if the built-in Windows triplet is not sufficient for the repo's exact needs

Minimum triplet expectations:

- prefer static library linkage
- optionally build release-only ports for CI/release paths
- keep any repo-specific flags in version-controlled triplet files rather than in workflow shell commands

If the upstream BOINC port does not honour the repo's static requirement on Linux, the next maintainable step should be an overlay BOINC port rather than abandoning the `vcpkg` path entirely.

### 5. Keep BOINC version control inside the `vcpkg` path

The repo should always build against the latest BOINC stable release that has been explicitly approved for `cpdn_control`.

That should be implemented with:

- a pinned `builtin-baseline` in `vcpkg/vcpkg.json` for the normal approved BOINC version
- a BOINC-specific `overrides` entry in `vcpkg/vcpkg.json` when rollback to an older known-good BOINC version is required

That gives two controlled modes:

- normal mode:
  - update the baseline to adopt a newer approved BOINC stable release
- rollback mode:
  - pin `boinc` to a known-good version through `overrides` without leaving the `vcpkg` workflow

The local `BOINC_DIR` fallback should remain available, but only as a last resort if the `vcpkg` route itself is broken.

### 6. Continue using `cpdn_zip`

`cpdn_control` should continue linking the in-repo `cpdn_zip` library from `zip/`.

The BOINC `boinc_zip` library should not be adopted here:

- it is not required by this repo
- this repo already has a dedicated zip path
- there are known BOINC zip issues on Windows

### 7. Export enough BOINC runtime-path information for tests only where needed

The functional test harness currently uses `BOINC_LIB_DIR` to build runtime library search paths.

That is acceptable for:

- development configurations
- platforms where shared-library BOINC testing is still part of the bring-up process

It is not representative of the intended Linux release deployment model.

So the repo should distinguish between:

- release-style validation:
  - no BOINC runtime library injection should be required
- development or transitional test configurations:
  - runtime BOINC library paths may still be injected if the configuration genuinely uses shared BOINC libraries

## Proposed CMake Cleanup

### 1. Simplify BOINC selection

Once the `vcpkg` path is generalized, the main `CMakeLists.txt` should not need platform-specific BOINC branching.

`configure_boinc(...)` should supply:

- imported BOINC targets or manual library paths
- BOINC include information when needed
- BOINC runtime library directory only when tests actually need it

### 2. Tidy the current cache-variable confusion

The current Linux release link behaviour works, but it is confusing because:

- `RELEASE_LINK_OPTIONS` is created as a cache variable
- then it is later overridden as a normal variable
- the final target sees `-static`, while `CMakeCache.txt` still appears to show an empty value

This should be cleaned up.

Recommended direction:

- use cache variables only for genuine user-tunable inputs
- compute target defaults in normal variables
- add explicit repo options where a user-facing toggle is actually useful

Examples:

- `CPDN_REQUIRE_STATIC_BOINC`
- `CPDN_LINK_RELEASE_STATIC`

That makes the final link behaviour visible and reduces misleading cache state.

## Shared Implementation Plan

### Phase 1. Introduce repo-owned `vcpkg` metadata

Add:

- `vcpkg/vcpkg.json`
- `vcpkg/vcpkg-configuration.json`
- repo-owned triplets under a version-controlled directory

Initial dependency scope should stay minimal:

- `boinc`

### Phase 2. Generalize BOINC resolution in CMake

Update `cmake/BoincConfig.cmake` so the BOINC dependency is resolved through:

- imported targets from `find_package(boinc CONFIG QUIET)` when available
- otherwise the current manual `BOINC_DIR` fallback

At the same time:

- make static-link requirements explicit
- stop relying on confusing cache shadowing for release link defaults

### Phase 3. Add static-link verification

For deployment builds, add validation that:

- BOINC resolves to static libraries where required
- the final executable does not depend on BOINC shared libraries at runtime

Linux examples:

- inspect `BOINC_LIB` and `BOINC_API`
- inspect the final executable with `ldd`, `readelf -d`, or similar

Equivalent Windows checks should confirm the deployment assumptions for that platform.

### Phase 4. Move CI to `vcpkg`

Once static verification is in place:

- switch the Windows workflow fully to the repo-local manifest path
- replace the Linux BOINC source build in CI with the repo-local manifest path

The workflows should cache:

- the `vcpkg` checkout/tool directory
- installed packages for the selected triplet
- optionally the binary cache if adopted later

### Phase 5. Align tests with the deployment model

Update the functional test harness so release-style Linux validation does not depend on BOINC runtime library injection.

If transitional shared-library test modes remain useful, keep them explicit and separate from the release-style path.

### Phase 6. Document update and rollback workflow

The repo should document:

- how to move to a newer approved BOINC stable release
- how to pin BOINC back to a known-good version using `vcpkg` overrides
- when to use the `BOINC_DIR` fallback instead

## Platform Sections

## Windows

### Current state

Already in place:

- `cmake/BoincConfig.cmake` supports a Windows `find_package(boinc CONFIG)` path
- the BOINC stubs and `CPDN_USE_BOINC_STUBS` option have been removed
- the Windows workflow now bootstraps `vcpkg`, installs BOINC from the pinned BOINC port, configures with the `vcpkg` toolchain, builds the controller and `unit_tests`, and is configured to run the full Windows unit test suite
- the repo and the separate `zip/` project now use the static MSVC runtime on Windows so they match the static BOINC package used there
- `run_process_with_timeout(...)` now has a Windows implementation

### Remaining Windows work

- validate the full Windows unit-test run in GitHub Actions after the recent timed-process changes
- decide whether any remaining Windows unit tests need platform cleanup or gating
- enable functional tests on Windows when the runtime path is ready
- validate end-to-end BOINC runtime behaviour, not just build plus unit tests

### Windows-specific notes

- `libsched` is not needed by this repo
- the `boincapi` target name comes from the BOINC `vcpkg` package, not from this repo
- if the built-in `x64-windows-static` triplet remains sufficient, there is no need to add a repo-owned Windows triplet immediately

## Linux

### Current state

The current Linux build still uses the manual BOINC install path.

Today it achieves static BOINC linkage by:

- preferring `.a` BOINC libraries in `cmake/BoincConfig.cmake`
- linking the release executable with `-static`

The current Linux CI still clones and builds BOINC from source.

### Linux migration target

Linux should move to the same repo-local `vcpkg` dependency model as Windows, but only if the static BOINC requirement is preserved.

The Linux `vcpkg` migration should therefore be accepted only when:

- BOINC is supplied as static libraries
- the final Linux release executable has no BOINC runtime-library dependency
- the functional/release validation path no longer relies on `LD_LIBRARY_PATH` for BOINC

### Linux-specific implementation notes

- a repo-owned Linux static triplet is strongly recommended
- if the upstream BOINC `vcpkg` port does not honour static linkage cleanly on Linux, add an overlay BOINC port
- do not switch Linux CI or default developer guidance until static validation is green

### Linux migration order

1. prototype the Linux static `vcpkg` triplet
2. verify BOINC is actually installed as static archives
3. verify the final controller has no BOINC shared-library dependency
4. update the functional test contract for release-style Linux validation
5. only then switch Linux CI and docs to the `vcpkg` path by default

## macOS

macOS remains deferred for now.

The current repo still treats macOS as a follow-up platform for BOINC dependency work.

The likely future direction is:

- use the same shared `vcpkg` design if the BOINC port and deployment model prove compatible
- keep the macOS-specific runtime and packaging questions separate from the Windows/Linux migration

Do not broaden the current migration scope to include macOS until the Windows and Linux static-link path is stable.

## Acceptance Criteria

The repo-level BOINC `vcpkg` migration should be considered successful only when:

- Windows and Linux use a repo-local BOINC dependency path by default
- deployment builds preserve static BOINC linkage
- CI verifies that the resulting executables do not rely on host-installed BOINC shared libraries
- BOINC version upgrades are pinned and reproducible
- BOINC rollback to a known-good version can be done inside the `vcpkg` workflow
- the manual `BOINC_DIR` path remains available as fallback

## Next Steps

Recommended next order from here:

1. validate the latest Windows workflow result and finish the Windows workflow rename once appropriate
2. add the repo-level `vcpkg` manifest and version-control design
3. simplify `cmake/BoincConfig.cmake` and tidy the current cache-variable confusion
4. prototype the Linux static `vcpkg` path and add static-link verification
5. only after that, switch Linux CI from source-built BOINC to `vcpkg`
