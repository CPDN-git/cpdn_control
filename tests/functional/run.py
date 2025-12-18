#!/usr/bin/env python3
"""Run functional test using parameters from a JSON config."""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


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
        default="cpdn_control_1.0.0_x86_64-pc-linux-gnu-debug",
        help="Controller executable name in the build directory",
    )
    parser.add_argument(
        "--model-binary",
        default="test_model",
        help="Test model executable name in the build directory",
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


def ensure_forecast_zip(slot0_dir: Path, unique_member_id: str, batch_id: str, forecast_length: int):
    base_zip = slot0_dir / f"test_model_{unique_member_id}_yyyymmddhh_1_{batch_id}_0.zip"
    target_zip = slot0_dir / f"test_model_{unique_member_id}_yyyymmddhh_{forecast_length}_{batch_id}_0.zip"
    if target_zip.exists():
        print(f"[run] Forecast zip already present: {target_zip.name}")
        return
    if not base_zip.exists():
        raise FileNotFoundError(f"Base namelist zip not found: {base_zip}")
    shutil.copy2(base_zip, target_zip)
    print(f"[run] Created forecast-length zip: {target_zip.name}")


def maybe_symlink(target: Path, link_path: Path):
    if link_path.exists() or link_path.is_symlink():
        link_path.unlink()
    if link_path.name != target.name:
        link_path.symlink_to(target.name)
        print(f"[run] Linked {link_path.name} -> {target.name}")

def dump_slot_stderr(slot0_dir: Path) -> None:
    stderr_path = slot0_dir / "stderr.txt"
    print(f"[run] --- Begin {stderr_path} ---")
    if not stderr_path.exists():
        print("[run] (stderr.txt not found)")
        print(f"[run] --- End {stderr_path} ---")
        return
    try:
        content = stderr_path.read_text(encoding="utf-8", errors="replace")
        sys.stdout.write(content)
        if content and not content.endswith("\n"):
            sys.stdout.write("\n")
    except OSError as exc:
        print(f"[run] Failed to read {stderr_path}: {exc}", file=sys.stderr)
    print(f"[run] --- End {stderr_path} ---")


def main():
    args = parse_args()
    config_path = args.config_flag or args.config
    if not config_path:
        raise SystemExit("Error: config path is required (pass --config)")
    config = load_config(config_path)

    workdir = Path.cwd()
    projects_dir = workdir / "projects"
    slot0_dir = workdir / "slots" / "0"
    print(f"[run] Working directory: {workdir}")

    controller_src = args.build_dir / args.controller_binary
    controller_dst = projects_dir / args.controller_binary
    copy_binary(controller_src, controller_dst, "controller")

    model_src = args.build_dir / args.model_binary
    model_dst = slot0_dir / args.model_binary
    copy_binary(model_src, model_dst, "model")

    #maybe_symlink(model_dst, slot0_dir / "test_model")

    forecast_length = int(config["forecast_length"])
    experiment_id = config["experiment_id"]
    unique_member_id = config["unique_member_id"]
    batch_id = config["batch_id"]

    # create a copy for the correct forecast length.
    ensure_forecast_zip(slot0_dir, unique_member_id, batch_id, forecast_length)

    env = os.environ.copy()
    if args.boinc_lib_dir:
        env["LD_LIBRARY_PATH"] = f"{args.boinc_lib_dir}:{env.get('LD_LIBRARY_PATH', '')}".rstrip(":")

    controller_cmd = [
        str(controller_dst),
        "yyyymmddhh",
        experiment_id,
        unique_member_id,
        batch_id,
        "0",
        str(forecast_length),
    ]
    print(f"[run] Launching controller: {' '.join(controller_cmd)}")
    try:
        result = subprocess.run(controller_cmd, cwd=slot0_dir, env=env)
    except OSError as exc:
        dump_slot_stderr(slot0_dir)
        print(f"[run] Failed to launch controller: {exc}", file=sys.stderr)
        raise SystemExit(127)
    else:
        dump_slot_stderr(slot0_dir)

    if result.returncode != 0:
        print(f"[run] Controller failed with exit code {result.returncode}", file=sys.stderr)
        raise SystemExit(result.returncode)
    print("[run] Controller run completed")


if __name__ == "__main__":
    main()
