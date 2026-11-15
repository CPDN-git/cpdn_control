#!/usr/bin/env bash
#
# Display or update the controller release version in CMakeLists.txt and
# src/cpdn_main.cpp. Run without arguments to check/display the current version,
# or pass MAJOR.MINOR.PATCH to update both declarations.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
cmake_file="${repo_root}/CMakeLists.txt"
source_file="${repo_root}/src/cpdn_main.cpp"

usage() {
    echo "Usage: ${0##*/} [MAJOR.MINOR.PATCH]" >&2
}

read_versions() {
    local file="$1"
    local pattern="$2"
    mapfile -t versions < <(sed -nE "${pattern}" "${file}")
    if [[ ${#versions[@]} -ne 1 ]]; then
        echo "Error: expected exactly one version declaration in ${file}" >&2
        exit 1
    fi
}

if [[ $# -gt 1 ]]; then
    usage
    exit 2
fi

read_versions "${cmake_file}" 's/^[[:space:]]*project\(cpdn_control[[:space:]]+VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+)([[:space:]]|$).*/\1/p'
cmake_version="${versions[0]}"
read_versions "${source_file}" 's/^[[:space:]]*#define[[:space:]]+CODE_VERSION[[:space:]]+"([0-9]+\.[0-9]+\.[0-9]+)".*/\1/p'
code_version="${versions[0]}"

if [[ $# -eq 0 ]]; then
    printf '%s\n' "${code_version}"
    if [[ "${cmake_version}" != "${code_version}" ]]; then
        echo "Version mismatch: CMakeLists.txt has ${cmake_version}, src/cpdn_main.cpp has ${code_version}" >&2
        exit 1
    fi
    exit 0
fi

new_version="$1"
if [[ ! "${new_version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: version must be in MAJOR.MINOR.PATCH format" >&2
    usage
    exit 2
fi

sed -i -E "s/(project\\(cpdn_control[[:space:]]+VERSION[[:space:]]+)[0-9]+\\.[0-9]+\\.[0-9]+/\\1${new_version}/" "${cmake_file}"
sed -i -E "s/(#define[[:space:]]+CODE_VERSION[[:space:]]+\")[0-9]+\\.[0-9]+\\.[0-9]+\"/\\1${new_version}\"/" "${source_file}"

echo "Updated version to ${new_version}"
