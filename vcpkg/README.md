# Repo Vcpkg Layout

This directory contains the repository-owned `vcpkg` metadata and policy for `cpdn_control`.

## Layout

- `vcpkg.json`
  - the project manifest
  - declares the top-level dependency set used by this repo
- `vcpkg-configuration.json`
  - registers the repo-local overlay triplets and overlay ports
- `triplets/`
  - repo-owned custom triplets for Linux, Windows, and Apple Silicon
  - these encode this repo's deployment policy rather than relying on upstream defaults
- `overlays/boinc/`
  - repo-local BOINC overlay port
  - keeps the BOINC package aligned with what `cpdn_control` actually uses

## Why An Overlay Port Is Used

This repo uses:

- `boinc`
- `boincapi`

This repo does not use:

- BOINC `boinc_zip`

`cpdn_control` uses the in-repo `cpdn_zip` library instead. The upstream `vcpkg` BOINC port still builds and exports `boinc_zip`, so this repo carries a small overlay port to remove that extra target and keep the dependency surface consistent across Linux, Windows, and macOS.

This adds a small maintenance burden, but it is deliberate:

- it avoids unnecessary BOINC build products
- it avoids platform-specific failures in the unused BOINC zip code path
- it keeps the package contract aligned with the controller code

## Updating The Pinned Versions

The default BOINC version used by this repo is controlled by the pinned `builtin-baseline` in [vcpkg.json](/home/glenn/github/cpdn_control/vcpkg/vcpkg.json).

Typical update flow:

1. update the `builtin-baseline` in `vcpkg.json`
2. review the upstream BOINC port for changes
3. compare those changes with `overlays/boinc/`
4. update the overlay port if needed
5. run the Linux, Windows, and macOS workflows again

If a BOINC release must be rolled back, prefer a `vcpkg` manifest override rather than abandoning the `vcpkg` path entirely.

## Notes

- the external `vcpkg` tool checkout used by local scripts and GitHub Actions is separate from this directory
- this directory contains only repo-owned metadata, not the `vcpkg` executable checkout
