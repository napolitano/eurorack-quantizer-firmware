#!/usr/bin/env python3
# Purpose: Validate acceptance-criterion to native-test traceability.
# Author: Axel Napolitano
# Original FM Quantizer concept and Rust firmware: Quinn Freedman
# Copyright (C) 2026 Axel Napolitano
# SPDX-License-Identifier: GPL-3.0-or-later

"""Validate acceptance-criterion -> native-test traceability metadata."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

RUN_TEST_RE = re.compile(r"RUN_TEST\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")


def test_cases(path: Path) -> set[str]:
    return set(RUN_TEST_RE.findall(path.read_text(encoding="utf-8")))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "mapping",
        nargs="?",
        type=Path,
        default=Path("test/requirements-traceability.json"),
    )
    args = parser.parse_args()
    data = json.loads(args.mapping.read_text(encoding="utf-8"))
    entries = data.get("acceptance_criteria", [])
    expected = {f"AC-{n:02d}" for n in range(1, 21)}
    actual = {entry.get("id") for entry in entries}
    failures: list[str] = []

    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing:
        failures.append("missing acceptance criteria: " + ", ".join(missing))
    if extra:
        failures.append("unknown acceptance criteria: " + ", ".join(extra))
    if len(entries) != len(actual):
        failures.append("duplicate acceptance-criterion IDs")

    cache: dict[Path, set[str]] = {}
    for entry in entries:
        if not entry.get("tests"):
            failures.append(f"{entry.get('id')}: no verification tests listed")
            continue
        for ref in entry["tests"]:
            path = Path(ref["file"])
            case = ref["case"]
            if not path.is_file():
                failures.append(f"{entry['id']}: missing test file {path}")
                continue
            cases = cache.setdefault(path, test_cases(path))
            if case not in cases:
                failures.append(f"{entry['id']}: {case} is not RUN_TEST'd by {path}")

    if failures:
        for failure in failures:
            print(f"requirement-traceability: {failure}", file=sys.stderr)
        return 1
    refs = sum(len(entry["tests"]) for entry in entries)
    print(f"requirement-traceability: {len(entries)} acceptance criteria / {refs} test references: passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
