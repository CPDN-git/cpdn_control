#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: test_with_vcpkg.sh [options]

Rerun tests against an existing vcpkg-configured build directory.

Options:
  --build-dir DIR    Build directory to test. Default: <repo>/build
  --functional       Run only tests labeled 'functional'
  --unit-only        Exclude tests labeled 'functional' (default)
  -h, --help         Show this help text
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

build_dir="${repo_root}/build"
ctest_mode="unit"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --functional)
            ctest_mode="functional"
            shift
            ;;
        --unit-only)
            ctest_mode="unit"
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

build_dir="$(cd "${build_dir}" && pwd)"

if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
    echo "Build directory does not contain CMakeCache.txt: ${build_dir}" >&2
    exit 2
fi

if ! grep -q "VCPKG_TARGET_TRIPLET" "${build_dir}/CMakeCache.txt"; then
    echo "Build directory does not appear to be configured with vcpkg: ${build_dir}" >&2
    exit 2
fi

if [[ "${ctest_mode}" == "functional" ]]; then
    echo "[test_with_vcpkg] Running functional tests from ${build_dir}"
    (
        cd "${build_dir}"
        ctest --output-on-failure -V -L functional
    )
else
    echo "[test_with_vcpkg] Running unit/non-functional tests from ${build_dir}"
    (
        cd "${build_dir}"
        ctest --output-on-failure -V -LE functional
    )
fi
