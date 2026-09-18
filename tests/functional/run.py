#!/usr/bin/env python3
"""Run functional test using parameters from a JSON config."""

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path


def detect_platform() -> str:
    env_platform = os.environ.get("CPDN_PLATFORM")
    if env_platform:
        return env_platform

    system = platform.system()
    machine = platform.machine().lower()

    if system == "Darwin":
        arch = "arm64" if "arm" in machine else "x86_64"
        return f"{arch}-apple-darwin"
    if system == "Windows":
        return "x86_64-pc-windows-msvc"
    # Default to Linux-style naming
    arch = "aarch64" if "aarch64" in machine or "arm64" in machine else "x86_64"
    return f"{arch}-pc-linux-gnu"


def valgrind_available() -> bool:
    return shutil.which("valgrind") is not None


def use_valgrind() -> bool:
    return bool(os.environ.get("CPDN_FUNCTIONAL_USE_VALGRIND"))


def parse_args():
    parser = argparse.ArgumentParser(description="Run functional test workunit.")

    parser.add_argument("--config", dest="config_flag", type=Path, help="Path to test config JSON")
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "build",
        help="Path to the build directory containing binaries",
    )
    parser.add_argument(
        "--controller-binary",
        default=None,
        help="Controller executable name in the build directory (platform defaults applied if omitted)",
    )
    parser.add_argument(
        "--model-binary",
        default=None,
        help="Test model executable name in the build directory (platform defaults applied if omitted)",
    )
    parser.add_argument(
        "--boinc-lib-dir",
        type=Path,
        default=None,
        help="Path to BOINC lib directory to prepend to LD_LIBRARY_PATH",
    )
    return parser.parse_args()


def load_config(config_path: Path) -> dict:
    with config_path.open() as f:
        return json.load(f)


def copy_binary(src: Path, dst: Path, label: str):
    if not src.exists():
        raise FileNotFoundError(f"{label} binary not found: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    print(f"[run] Copied {label} binary to {dst}")


def find_latest_controller_binary(build_dir: Path, platform_triplet: str) -> Path:
    """Find the newest release controller build for the current platform.

    The executable name includes the CMake project version, so functional tests
    must not duplicate a particular version here.  Debug builds intentionally
    do not match this pattern: the functional test runs the release controller.
    """
    extension = r"\.exe" if platform_triplet.endswith("windows-msvc") else ""
    pattern = re.compile(
        rf"^cpdn_control_(?P<version>\d+(?:\.\d+)*)_{re.escape(platform_triplet)}{extension}$"
    )
    candidates = []
    for candidate in build_dir.iterdir() if build_dir.is_dir() else []:
        match = pattern.match(candidate.name)
        if match and candidate.is_file():
            version = tuple(int(part) for part in match.group("version").split("."))
            candidates.append((version, candidate))

    if not candidates:
        raise FileNotFoundError(
            "release controller binary not found in "
            f"{build_dir} for platform {platform_triplet}"
        )

    return max(candidates, key=lambda item: item[0])[1]


def ensure_app_bundle_zip(slot0_dir: Path, member_id: str, batch_id: str, filename_label: str):
    base_zip = slot0_dir / f"test_model_{member_id}_yyyymmddhh_1_{batch_id}_0.zip"
    target_zip = slot0_dir / f"test_model_{member_id}_{filename_label}_{batch_id}_0.zip"
    if target_zip.exists():
        print(f"[run] Forecast zip already present: {target_zip.name}")
        return
    if not base_zip.exists():
        raise FileNotFoundError(f"Base namelist zip not found: {base_zip}")
    shutil.copy2(base_zip, target_zip)
    print(f"[run] Created app-bundle zip: {target_zip.name}")


def maybe_symlink(target: Path, link_path: Path):
    if link_path.exists() or link_path.is_symlink():
        link_path.unlink()
    if link_path.name != target.name:
        link_path.symlink_to(target.name)
        print(f"[run] Linked {link_path.name} -> {target.name}")


def dump_slot_log(log_path: Path) -> None:
    print(f"[run] --- Begin {log_path} ---")
    if not log_path.exists():
        print(f"[run] ({log_path.name} not found)")
        print(f"[run] --- End {log_path} ---")
        return
    try:
        content = log_path.read_text(encoding="utf-8", errors="replace")
        sys.stdout.write(content)
        if content and not content.endswith("\n"):
            sys.stdout.write("\n")
    except OSError as exc:
        print(f"[run] Failed to read {log_path}: {exc}", file=sys.stderr)
    print(f"[run] --- End {log_path} ---")


def ensure_not_repo_root(workdir: Path) -> None:
    repo_root = Path(__file__).resolve().parents[2]
    if workdir.resolve() == repo_root:
        raise SystemExit(
            f"[run] Refusing to run the functional harness from the repository root: {repo_root}\n"
            "[run] Use a dedicated test work directory instead."
        )


def main():
    args = parse_args()
    platform_triplet = detect_platform()
    config_path = args.config_flag or args.config
    if not config_path:
        raise SystemExit("Error: config path is required (pass --config)")
    config = load_config(config_path)

    workdir = Path.cwd()
    ensure_not_repo_root(workdir)
    project_dir = workdir / "projects" / "climateprediction.net"
    slot0_dir = workdir / "slots" / "0"
    print(f"[run] Working directory: {workdir}")

    default_model = "test_model"

    controller_name = args.controller_binary
    model_name = args.model_binary or default_model

    # Append .exe on Windows if no extension was provided and the file is missing
    def with_exe(name: str) -> str:
        if platform_triplet.endswith("windows-msvc") and not name.lower().endswith(".exe"):
            return name + ".exe"
        return name

    model_name = with_exe(model_name)

    if controller_name:
        controller_src = args.build_dir / with_exe(controller_name)
    else:
        controller_src = find_latest_controller_binary(args.build_dir, platform_triplet)
    controller_name = controller_src.name
    controller_dst = project_dir / controller_name
    copy_binary(controller_src, controller_dst, "controller")

    model_src = args.build_dir / model_name
    model_dst = slot0_dir / model_name
    copy_binary(model_src, model_dst, "model")

    #maybe_symlink(model_dst, slot0_dir / "test_model")

    batch_id = config["batch_id"]
    workunit = config["wu_name"]
    member_id = config["member_id"]
    forecast_length = int(config["forecast_length"])
    filename_label = config.get("filename_label", f"yyyymmddhh_{forecast_length}")
    upload_interval = int(config["upload_interval"])

    # The bundle label is opaque controller metadata, not a runtime forecast length.
    ensure_app_bundle_zip(slot0_dir, member_id, batch_id, filename_label)

    # set the library path environment
    env = os.environ.copy()
    env["CPDN_PLATFORM"] = platform_triplet
    if args.boinc_lib_dir:
        lib_dir = str(args.boinc_lib_dir)
        if platform_triplet.endswith("windows-msvc"):
            env["PATH"] = f"{lib_dir}{os.pathsep}{env.get('PATH', '')}".rstrip(os.pathsep)
        else:
            env["LD_LIBRARY_PATH"] = f"{lib_dir}:{env.get('LD_LIBRARY_PATH', '')}".rstrip(":")
            if platform_triplet.endswith("apple-darwin"):
                env["DYLD_LIBRARY_PATH"] = f"{lib_dir}:{env.get('DYLD_LIBRARY_PATH', '')}".rstrip(":")

    # Either use <arg>=<val> syntax or split the arg & val into separate tokens.
    controller_cmd = [
        str(controller_dst),
        f"--filename_label={filename_label}",
        f"--batch={batch_id}",
        f"--workunit={workunit}",
        f"--memberid={member_id}",
        f"--upload_interval={upload_interval}",
    ]

    if use_valgrind() and valgrind_available():
        controller_cmd = ["valgrind", "--leak-check=full", *controller_cmd]
    elif use_valgrind():
        print("[run] CPDN_FUNCTIONAL_USE_VALGRIND is set, but valgrind was not found; running controller directly")
    else:
        print("[run] CPDN_FUNCTIONAL_USE_VALGRIND is not set; running controller without valgrind")

    print(f"[run] Launching controller: {' '.join(controller_cmd)}")
    try:
        result = subprocess.run(controller_cmd, cwd=slot0_dir, env=env)
    except OSError as exc:
        dump_slot_log(slot0_dir / "stderr.txt")
        dump_slot_log(slot0_dir / "stdout.txt")
        print(f"[run] Failed to launch controller: {exc}", file=sys.stderr)
        raise SystemExit(127)
    else:
        dump_slot_log(slot0_dir / "stderr.txt")
        dump_slot_log(slot0_dir / "stdout.txt")

    if result.returncode != 0:
        print(f"[run] Controller failed with exit code {result.returncode}", file=sys.stderr)
        raise SystemExit(result.returncode)
    print("[run] Controller run completed")


if __name__ == "__main__":
    main()
