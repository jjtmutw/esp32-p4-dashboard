"""Apply local component patch files into ESP-IDF managed_components.

The project relies on ESP-IDF component manager for third-party components.
Only a few board-specific fixes are kept in this repository under
component_patches/ so the full managed_components tree does not need to be
versioned.
"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def apply_patches(quiet: bool = False) -> int:
    root = repo_root()
    patch_root = root / "component_patches" / "managed_components"
    target_root = root / "managed_components"

    if not patch_root.exists():
        if not quiet:
            print("No component patches found.")
        return 0

    if not target_root.exists():
        if not quiet:
            print("managed_components does not exist yet; run idf.py reconfigure first.")
        return 0

    copied = 0
    for patch_file in patch_root.rglob("*"):
        if not patch_file.is_file():
            continue
        relative = patch_file.relative_to(patch_root)
        target_file = target_root / relative
        target_file.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(patch_file, target_file)
        copied += 1
        if not quiet:
            print(f"patched {relative.as_posix()}")

    if not quiet:
        print(f"Applied {copied} component patch file(s).")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Apply local ESP-IDF component patches.")
    parser.add_argument("--quiet", action="store_true", help="suppress per-file output")
    args = parser.parse_args()
    return apply_patches(quiet=args.quiet)


if __name__ == "__main__":
    raise SystemExit(main())
