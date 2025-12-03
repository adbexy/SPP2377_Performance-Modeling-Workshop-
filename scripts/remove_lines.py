#!/usr/bin/env python3
"""Remove specific line numbers from a file and write it back.

This script removes lines 587 and 588 (1-based) from the given file and writes
the result back. It can optionally create a backup of the original file.

Usage:
  python3 scripts/remove_lines.py path/to/sort_core_gather.hpp [--no-backup]

The script is robust if the file has fewer lines than the requested indices;
it will remove any of the requested lines that exist and write back the file.
"""

import argparse
import re
import shutil
import sys
from pathlib import Path


def remove_lines(path: Path, line_numbers, backup=True):
    if not path.exists():
        raise FileNotFoundError(f"File not found: {path}")

    # read all lines
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)

    # convert 1-based line_numbers to 0-based indices and sort descending to remove safely
    indices = sorted({n - 1 for n in line_numbers if n >= 1}, reverse=True)

    # optional backup
    if backup:
        bak = path.with_suffix(path.suffix + ".bak")
        shutil.copy2(path, bak)
        print(f"Backup written to: {bak}")

    removed = []
    for idx in indices:
        if 0 <= idx < len(lines):
            removed.append((idx + 1, lines[idx]))
            del lines[idx]
        else:
            print(f"Note: file has no line {idx + 1}; skipping")

    path.write_text(''.join(lines), encoding="utf-8")

    return removed


def main():
    p = argparse.ArgumentParser(description="Remove specific lines from a file (1-based indexing)")
    p.add_argument("file", type=Path, help="Path to the input file")
    p.add_argument("--no-backup", action="store_true", help="Do not create a .bak backup file")
    p.add_argument("--lines", type=str, default="[587,588]",
                   help="Comma-separated list or bracketed list of 1-based line numbers to remove, e.g. '[45,200]' or '45,200'")
    args = p.parse_args()

    target = args.file

    # parse lines argument: accept formats like "[45,200]", "45,200", "45 200"
    lines_arg = args.lines
    nums = re.findall(r"\d+", lines_arg)
    if not nums:
        print(f"No valid line numbers found in --lines '{lines_arg}'", file=sys.stderr)
        sys.exit(2)
    line_numbers = [int(n) for n in nums]

    try:
        removed = remove_lines(target, line_numbers, backup=not args.no_backup)
    except FileNotFoundError as e:
        print(e, file=sys.stderr)
        sys.exit(2)

    if removed:
        print(f"Removed {len(removed)} line(s): {', '.join(str(r[0]) for r in removed)}")
    else:
        print("No lines were removed.")


if __name__ == '__main__':
    main()
