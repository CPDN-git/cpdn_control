#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: build_eccodes.sh [options]

Build and install ecCodes from an external checkout using the repo-local
Findlibaec.cmake helper.

Options:
  --source-dir DIR       ecCodes checkout. Default: ${HOME}/github/eccodes
  --build-dir DIR        ecCodes build directory. Default: <source>/build
  --install-prefix DIR   ecCodes install prefix. Default: /home/glenn/github/eccodes-install/<ref>
  --ref REF              Git ref to checkout before building. Default: 2.46.2
  --build-type TYPE      CMake build type. Default: Release
  --jobs N               Parallel build jobs. Default: 2
  --clean                Remove the ecCodes build directory before configuring
  -h, --help             Show this help text
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

eccodes_source_dir="${HOME}/github/eccodes"
eccodes_ref="2.46.2"
build_type="Release"
jobs="2"
clean_build="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source-dir)
            eccodes_source_dir="$2"
            shift 2
            ;;
        --build-dir)
            eccodes_build_dir="$2"
            shift 2
            ;;
        --install-prefix)
            eccodes_install_prefix="$2"
            shift 2
            ;;
        --ref)
            eccodes_ref="$2"
            shift 2
            ;;
        --build-type)
            build_type="$2"
            shift 2
            ;;
        --jobs)
            jobs="$2"
            shift 2
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

if [[ -z "${eccodes_build_dir:-}" ]]; then
    eccodes_build_dir="${eccodes_source_dir}/build"
fi

if [[ -z "${eccodes_install_prefix:-}" ]]; then
    eccodes_install_prefix="/home/glenn/github/eccodes-install/${eccodes_ref}"
fi

if [[ ! -d "${eccodes_source_dir}" ]]; then
    echo "Cannot find ecCodes source directory: ${eccodes_source_dir}" >&2
    exit 2
fi

if [[ "${clean_build}" == "1" ]]; then
    rm -rf "${eccodes_build_dir}"
fi

mkdir -p "${eccodes_build_dir}"

echo "[build_eccodes] Checking out ${eccodes_ref} in ${eccodes_source_dir}"
git -C "${eccodes_source_dir}" checkout "${eccodes_ref}"

echo "[build_eccodes] Configuring ecCodes in ${eccodes_build_dir}"
cmake -S "${eccodes_source_dir}" -B "${eccodes_build_dir}" \
    -DCMAKE_BUILD_TYPE="${build_type}" \
    -DCMAKE_INSTALL_PREFIX="${eccodes_install_prefix}" \
    -DCMAKE_MODULE_PATH="${repo_root}/cmake" \
    -DENABLE_NETCDF=OFF \
    -DENABLE_JPG=OFF \
    -DENABLE_PNG=OFF \
    -DENABLE_LARGE_FILE_SUPPORT=OFF \
    -DENABLE_GRIB_THREADS=OFF \
    -DENABLE_ECCODES_THREADS=OFF \
    -DENABLE_FORTRAN=OFF \
    -DENABLE_PYTHON=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_AEC=ON \
    -DENABLE_USE_SHARED_LIB_AEC=OFF

echo "[build_eccodes] Building ecCodes"
cmake --build "${eccodes_build_dir}" -j "${jobs}"

echo "[build_eccodes] Installing ecCodes to ${eccodes_install_prefix}"
cmake --install "${eccodes_build_dir}"
