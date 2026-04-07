#!/usr/bin/env python3
"""Validation for functional tests."""

import json
import sys
from pathlib import Path
from typing import Optional


def read_progress_value(progress_path: Path, key: str) -> Optional[str]:
    if not progress_path.exists():
        print(f"[validate] Progress file not found: {progress_path}", file=sys.stderr)
        return None

    for line in progress_path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("!") or stripped in {"&CPDN", "/"}:
            continue
        if "=" not in stripped:
            continue
        field, value = stripped.split("=", 1)
        if field.strip() == key:
            return value.strip()
    return None


def ensure_not_repo_root(workdir: Path) -> None:
    repo_root = Path(__file__).resolve().parents[2]
    if workdir.resolve() == repo_root:
        raise SystemExit(
            f"[validate] Refusing to run the functional harness from the repository root: {repo_root}\n"
            "[validate] Use a dedicated test work directory instead."
        )


def main():
    workdir = Path.cwd()
    ensure_not_repo_root(workdir)
    slot0_dir = workdir / "slots" / "0"
    progress_path = slot0_dir / "cpdn_progressfile.txt"
    config_path = Path(sys.argv[1]) if len(sys.argv) > 1 else None

    cpu_time_text = read_progress_value(progress_path, "prior_acc_cpu_time")
    if cpu_time_text is None:
        print(f"[validate] Missing prior_acc_cpu_time in {progress_path}", file=sys.stderr)
        return 1

    try:
        cpu_time_value = float(cpu_time_text)
    except ValueError:
        print(f"[validate] prior_acc_cpu_time is not a valid float: {cpu_time_text!r}", file=sys.stderr)
        return 1

    if cpu_time_value < 0.0:
        print(f"[validate] prior_acc_cpu_time is negative: {cpu_time_value}", file=sys.stderr)
        return 1

    if config_path:
        config = json.loads(config_path.read_text(encoding="utf-8"))
        logical_files = [
            slot0_dir / f"ic_ancil_{config['wu_name']}.zip",
            slot0_dir / f"ifsdata_{config['wu_name']}.zip",
            slot0_dir / f"clim_data_{config['wu_name']}.zip",
        ]
        for logical_file in logical_files:
            if not logical_file.exists():
                print(f"[validate] Logical BOINC file missing after run: {logical_file}", file=sys.stderr)
                return 1
            content = logical_file.read_text(encoding="utf-8", errors="replace").strip()
            if not content.startswith("<soft_link>"):
                print(f"[validate] Logical BOINC file was overwritten: {logical_file}", file=sys.stderr)
                return 1

        print(f"[validate] prior_acc_cpu_time is present and non-negative for config: {config_path}")
    else:
        print("[validate] prior_acc_cpu_time is present and non-negative")
    return 0


if __name__ == "__main__":
    sys.exit(main())
