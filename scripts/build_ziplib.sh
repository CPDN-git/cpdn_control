#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat << 'EOF'
Build ZipLib prerequisite library

Usage: $0
No arguments
EOF
}

build_type="Release"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

echo "Building cpdn_zip"
cmake -S "${repo_root}/zip" -B "${repo_root}/zip/build" \
    -DCMAKE_BUILD_TYPE="${build_type}" \
    -DCMAKE_INSTALL_PREFIX="${repo_root}/zip/install"
cmake --build "${repo_root}/zip/build" --target install

echo "Done."
