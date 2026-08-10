#!/usr/bin/env python3
# Purpose: Enforce native line/branch coverage regression floors.
# Author: Axel Napolitano
# Original FM Quantizer concept and Rust firmware: Quinn Freedman
# Copyright (C) 2026 Axel Napolitano
# SPDX-License-Identifier: GPL-3.0-or-later

"""Enforce line/branch coverage floors for the portable FMQ production core."""

from __future__ import annotations

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def load_policy(path: Path) -> tuple[float, float]:
    data = json.loads(path.read_text(encoding="utf-8"))
    return float(data["line_min_percent"]), float(data["branch_min_percent"])


def read_cobertura(path: Path) -> tuple[float, float]:
    root = ET.parse(path).getroot()
    try:
        line_rate = float(root.attrib["line-rate"])
        branch_rate = float(root.attrib["branch-rate"])
    except (KeyError, ValueError) as exc:
        raise ValueError("coverage XML does not contain valid line-rate/branch-rate attributes") from exc
    return line_rate * 100.0, branch_rate * 100.0


def evaluate(line: float, branch: float, line_min: float, branch_min: float) -> list[str]:
    failures: list[str] = []
    if line + 1e-9 < line_min:
        failures.append(f"line coverage {line:.2f}% is below {line_min:.2f}%")
    if branch + 1e-9 < branch_min:
        failures.append(f"branch coverage {branch:.2f}% is below {branch_min:.2f}%")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("coverage_xml", type=Path)
    parser.add_argument(
        "--policy",
        type=Path,
        default=Path(__file__).with_name("native_coverage_policy.json"),
    )
    args = parser.parse_args()

    line_min, branch_min = load_policy(args.policy)
    line, branch = read_cobertura(args.coverage_xml)
    failures = evaluate(line, branch, line_min, branch_min)

    print(f"native-coverage: lines   {line:.2f}% (floor {line_min:.2f}%)")
    print(f"native-coverage: branches {branch:.2f}% (floor {branch_min:.2f}%)")
    if failures:
        for failure in failures:
            print(f"native-coverage: {failure}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
