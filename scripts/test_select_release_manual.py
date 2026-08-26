#!/usr/bin/env python3
"""Regression tests for release-manual selection.

SPDX-License-Identifier: GPL-3.0-or-later
"""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from select_release_manual import Version, select_manual


class SelectReleaseManualTests(unittest.TestCase):
    def test_selects_highest_manual_not_newer_than_firmware(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for version in ("0.1.0", "0.2.0", "0.3.0"):
                (root / f"quantizer-user-manual.{version}.odt").touch()

            selected = select_manual(root, Version.parse("0.2.1"))

            self.assertIsNotNone(selected)
            assert selected is not None
            self.assertEqual(str(selected.version), "0.2.0")
            self.assertEqual(selected.path.name, "quantizer-user-manual.0.2.0.odt")

    def test_exact_version_wins(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "quantizer-user-manual.0.2.0.odt").touch()
            (root / "quantizer-user-manual.0.2.1.odt").touch()

            selected = select_manual(root, Version.parse("v0.2.1"))

            self.assertIsNotNone(selected)
            assert selected is not None
            self.assertEqual(str(selected.version), "0.2.1")

    def test_0_2_2_exact_manual_wins_without_affecting_0_2_1(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "quantizer-user-manual.0.2.0.odt").touch()
            (root / "quantizer-user-manual.0.2.2.odt").touch()

            old = select_manual(root, Version.parse("0.2.1"))
            current = select_manual(root, Version.parse("0.2.2"))

            self.assertIsNotNone(old)
            self.assertIsNotNone(current)
            assert old is not None and current is not None
            self.assertEqual(str(old.version), "0.2.0")
            self.assertEqual(str(current.version), "0.2.2")

    def test_newer_manual_is_never_used_for_older_firmware(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "quantizer-user-manual.0.3.0.odt").touch()

            self.assertIsNone(select_manual(root, Version.parse("0.2.9")))

    def test_nonmatching_files_are_ignored(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "quantizer-user-manual.latest.odt").touch()
            (root / "quantizer-user-manual.0.2.0.pdf").touch()
            (root / "README.md").touch()

            self.assertIsNone(select_manual(root, Version.parse("0.2.0")))


if __name__ == "__main__":
    unittest.main()
