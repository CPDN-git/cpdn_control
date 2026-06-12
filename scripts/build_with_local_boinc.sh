#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: build_with_local_boinc.sh --boinc-dir /path/to/boinc-install [options]

One-shot Linux build using a manually built BOINC install via BOINC_DIR.

Options:
  --boinc-dir DIR    Path to the manual BOINC install. Can also be provided via BOINC_DIR.
  --build-dir DIR    Build directory for the controller. Default: <repo>/build
  --build-type TYPE  CMake build type. Default: Release
  --functional       Enable and run functional tests as part of the build
  --skip-tests       Configure and build, but do not run ctest
  --clean            Remove the controller build directory before configuring
  -h, --help         Show this help text
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

boinc_dir="${BOINC_DIR:-}"
build_dir="${repo_root}/build"
build_type="Release"
enable_functional="ON"
skip_tests="0"
clean_build="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --boinc-dir)
            boinc_dir="$2"
            shift 2
            ;;
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --build-type)
            build_type="$2"
            shift 2
            ;;
        --functional)
            enable_functional="ON"
            shift
            ;;
        --skip-tests)
            skip_tests="1"
            shift
            ;;
        --clean)
            clean_build="1"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "${boinc_dir}" ]]; then
    echo "BOINC_DIR is required. Pass --boinc-dir or set BOINC_DIR." >&2
    exit 2
fi

boinc_dir="$(cd "${boinc_dir}" && pwd)"
build_dir="$(mkdir -p "${build_dir}" && cd "${build_dir}" && pwd)"

if [[ "${clean_build}" == "1" ]]; then
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
fi

echo "[build_with_local_boinc] Configuring controller build in ${build_dir}"
cmake -S "${repo_root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE="${build_type}" \
    -DBOINC_DIR="${boinc_dir}" \
    -DCPDN_BUILD_FUNCTIONAL_TESTS="${enable_functional}"

echo "[build_with_local_boinc] Building controller"
cmake --build "${build_dir}"

if [[ "${skip_tests}" == "0" ]]; then
    echo "[build_with_local_boinc] Running tests"
    ctest --test-dir "${build_dir}" --output-on-failure -V
fi

