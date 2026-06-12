#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: build_with_vcpkg.sh --vcpkg-root /path/to/vcpkg [options]

One-shot Linux build using the repo-local vcpkg manifest and static BOINC triplet.

Options:
  --vcpkg-root DIR   Path to the vcpkg checkout. Can also be provided via VCPKG_ROOT.
  --build-dir DIR    Build directory for the controller. Default: <repo>/build
  --triplet NAME     vcpkg triplet. Default: x64-linux-cpdn-static
  --build-type TYPE  CMake build type. Default: Release
  --functional       Enable and run functional tests as part of the build
  --skip-tests       Configure and build, but do not run ctest
  --clean            Remove the controller build directory before configuring
  -h, --help         Show this help text
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
manifest_root="${repo_root}/vcpkg"

vcpkg_root="${VCPKG_ROOT:-}"
build_dir="${repo_root}/build"
vcpkg_triplet="x64-linux-cpdn-static"
build_type="Release"
enable_functional="OFF"
skip_tests="0"
clean_build="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --vcpkg-root)
            vcpkg_root="$2"
            shift 2
            ;;
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --triplet)
            vcpkg_triplet="$2"
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

if [[ -z "${vcpkg_root}" ]]; then
    echo "VCPKG_ROOT is required. Pass --vcpkg-root or set VCPKG_ROOT." >&2
    exit 2
fi

vcpkg_root="$(cd "${vcpkg_root}" && pwd)"
build_dir="$(mkdir -p "${build_dir}" && cd "${build_dir}" && pwd)"

if [[ "${clean_build}" == "1" ]]; then
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
fi

if [[ ! -x "${vcpkg_root}/vcpkg" ]]; then
    echo "[build_with_vcpkg] Bootstrapping vcpkg at ${vcpkg_root}"
    "${vcpkg_root}/bootstrap-vcpkg.sh"
fi

echo "[build_with_vcpkg] Installing manifest dependencies via vcpkg"
"${vcpkg_root}/vcpkg" install --x-manifest-root="${manifest_root}" --triplet="${vcpkg_triplet}"

echo "[build_with_vcpkg] Configuring controller build in ${build_dir}"
cmake -S "${repo_root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE="${build_type}" \
    -DCMAKE_TOOLCHAIN_FILE="${vcpkg_root}/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="${vcpkg_triplet}" \
    -DCPDN_BUILD_FUNCTIONAL_TESTS="${enable_functional}"

echo "[build_with_vcpkg] Building controller"
cmake --build "${build_dir}"

if [[ "${skip_tests}" == "0" ]]; then
    echo "[build_with_vcpkg] Running tests"
    ctest --test-dir "${build_dir}" --output-on-failure -V
fi
