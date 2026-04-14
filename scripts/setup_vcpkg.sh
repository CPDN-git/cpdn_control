#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: setup_vcpkg.sh [options]

Clone/bootstrap the pinned vcpkg checkout used by this repo and optionally
install the repo manifest dependencies for a chosen triplet.

Options:
  --vcpkg-root DIR   vcpkg checkout path. Default: \$HOME/github/vcpkg
  --triplet NAME     Install manifest dependencies for this triplet after bootstrap
  --skip-install     Bootstrap vcpkg only; do not run `vcpkg install`
  --force-bootstrap  Run the bootstrap step even if the vcpkg executable already exists
  -h, --help         Show this help text
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
manifest_root="${repo_root}/vcpkg"

vcpkg_root="${HOME}/github/vcpkg"
# pinned commit to ensure consistency
vcpkg_commit="197fa8bf282e537136e4cf196af167e7f79be07b"
triplet=""
skip_install="0"
force_bootstrap="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --vcpkg-root)
            vcpkg_root="$2"
            shift 2
            ;;
        --triplet)
            triplet="$2"
            shift 2
            ;;
        --skip-install)
            skip_install="1"
            shift
            ;;
        --force-bootstrap)
            force_bootstrap="1"
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

if ! command -v git >/dev/null 2>&1; then
    echo "git is required but was not found in PATH." >&2
    exit 2
fi

mkdir -p "$(dirname "${vcpkg_root}")"

if [[ ! -d "${vcpkg_root}/.git" ]]; then
    echo "[setup_vcpkg] Cloning vcpkg into ${vcpkg_root}"
    git clone https://github.com/microsoft/vcpkg.git "${vcpkg_root}"
fi

echo "[setup_vcpkg] Checking out pinned vcpkg commit ${vcpkg_commit}"
git -C "${vcpkg_root}" fetch --depth 1 origin "${vcpkg_commit}"
git -C "${vcpkg_root}" checkout "${vcpkg_commit}"

if [[ "${force_bootstrap}" == "1" || ! -x "${vcpkg_root}/vcpkg" ]]; then
    echo "[setup_vcpkg] Bootstrapping vcpkg"
    "${vcpkg_root}/bootstrap-vcpkg.sh"
fi

if [[ "${skip_install}" == "1" ]]; then
    exit 0
fi

if [[ -z "${triplet}" ]]; then
    echo "[setup_vcpkg] No triplet provided; bootstrap complete, manifest install skipped."
    echo "[setup_vcpkg] Pass --triplet x64-linux-cpdn-static or --triplet x64-windows-cpdn-static to install dependencies."
    exit 0
fi

echo "[setup_vcpkg] Installing repo manifest dependencies for triplet ${triplet}"
"${vcpkg_root}/vcpkg" install --x-manifest-root="${manifest_root}" --triplet="${triplet}"
