#!/usr/bin/env python3
"""Validation for functional tests."""

import sys
from pathlib import Path


def read_progress_value(progress_path: Path, key: str) -> str | None:
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
    progress_path = workdir / "slots" / "0" / "cpdn_progressfile.txt"
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
        print(f"[validate] prior_acc_cpu_time is present and non-negative for config: {config_path}")
    else:
        print("[validate] prior_acc_cpu_time is present and non-negative")
    return 0


if __name__ == "__main__":
    sys.exit(main())
