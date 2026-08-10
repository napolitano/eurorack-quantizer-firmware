#!/usr/bin/env python3
# Purpose: Self-test the native coverage policy checker.
# Author: Axel Napolitano
# Original FM Quantizer concept and Rust firmware: Quinn Freedman
# Copyright (C) 2026 Axel Napolitano
# SPDX-License-Identifier: GPL-3.0-or-later

"""Self-tests for scripts/check_native_coverage.py."""

from pathlib import Path
import tempfile

from check_native_coverage import evaluate, read_cobertura


def write_xml(path: Path, line: float, branch: float) -> None:
    path.write_text(
        f'<coverage line-rate="{line}" branch-rate="{branch}"/>',
        encoding="utf-8",
    )


def main() -> int:
    assert evaluate(95.0, 80.0, 92.0, 70.0) == []
    assert len(evaluate(91.99, 80.0, 92.0, 70.0)) == 1
    assert len(evaluate(95.0, 69.99, 92.0, 70.0)) == 1
    assert len(evaluate(80.0, 60.0, 92.0, 70.0)) == 2
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "coverage.xml"
        write_xml(path, 0.9569, 0.8028)
        line, branch = read_cobertura(path)
        assert abs(line - 95.69) < 0.001
        assert abs(branch - 80.28) < 0.001
    print("native coverage policy self-test: passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
