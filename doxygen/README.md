# Doxygen documentation

This directory contains the repo-owned Doxygen configuration for `cpdn_control`.

## Goals

- generate API documentation directly from the current source tree
- provide caller and call graphs for the controller code
- include internal `static` helpers because much of the controller flow is in file-local free functions
- reuse existing markdown from `README.md` and `docs/` instead of maintaining a second documentation tree

## Generated output

The generated HTML output is written to:

```text
build/doxygen/html/
```

The main landing page is:

```text
build/doxygen/html/index.html
```

Generated HTML is not committed to git.

## Prerequisites

- `doxygen`
- Graphviz `dot`

## Build command

Configure the project as usual, then run:

```bash
cmake --build build --target doxygen
```

If `doxygen` is not installed, the target prints a clear error and fails.

## What is included

The Doxygen configuration is intentionally focused on the project-owned controller code and selected markdown:

- `README.md`
- `docs/`
- `src/`
- `lib/`
- `api/`
- `models/`

## What is excluded

To keep the symbol tree and graphs readable, the following are excluded:

- `build/`
- `third_party/`
- `vcpkg/`
- `tests/unit/`
- `old_src/`
- `zip/ZipLib/extlibs/`

## Maintenance contract

Keep the Doxygen documentation in step with the code:

1. When adding or materially changing controller/model helper functions, keep the Doxygen-style comment headers current.
2. When changing major execution flow or ownership boundaries, update the relevant existing markdown in `docs/` or `README.md` so the generated docs still explain the current behavior.
3. After significant changes to controller flow, BOINC behavior, model seams, or upload/finalization paths, rerun:

```bash
cmake --build build --target doxygen
```

4. Treat Doxygen config changes like build-system changes: keep them small, explicit, and repo-owned.

## Notes

- The most useful graphs are expected to be around `main()`, BOINC shutdown helpers, upload/finalization flow, and the model-control seam.
- Doxygen call graphs are static-analysis based; they are useful for structure and navigation but should not be treated as a runtime trace.
- In Doxygen caller/callee graphs, a red-bordered node means the graph has been truncated and not all relationships are shown. In this repo that is usually due to `DOT_GRAPH_MAX_NODES` in `Doxyfile.in`, which keeps very large graphs such as `main()` readable.
