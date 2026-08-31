#!/usr/bin/env python3
"""Regression tests for tagged-release social announcement generation.

SPDX-License-Identifier: GPL-3.0-or-later
"""

from __future__ import annotations

import unittest

from generate_release_social import (
    build_message,
    feature_summary,
    plain_text,
    truncate_words,
)


class GenerateReleaseSocialTests(unittest.TestCase):
    def test_plain_text_removes_markdown_without_losing_labels(self) -> None:
        self.assertEqual(
            plain_text("Adds **RAW mode**, `CAL`, and [docs](https://example.test)."),
            "Adds RAW mode, CAL, and docs.",
        )

    def test_feature_summary_skips_version_boilerplate_when_more_follows(self) -> None:
        summary = (
            "Version 0.2.2 is a maintenance release with no hardware changes. "
            "Adds persistent calibration, RAW output testing, and calibration erase."
        )
        self.assertEqual(
            feature_summary(summary),
            "Adds persistent calibration, RAW output testing, and calibration erase.",
        )

    def test_short_message_contains_version_summary_and_repository(self) -> None:
        message = build_message(
            version="0.2.2",
            summary="Persistent calibration and a front-panel maintenance workflow.",
            repository="napolitano/eurorack-quantizer-firmware",
            server_url="https://github.com",
            max_length=280,
        )
        self.assertIn("FM Quantizer v0.2.2 released:", message)
        self.assertIn("Persistent calibration", message)
        self.assertTrue(
            message.endswith("https://github.com/napolitano/eurorack-quantizer-firmware")
        )
        self.assertLessEqual(len(message), 280)

    def test_long_summary_is_truncated_with_ellipsis(self) -> None:
        message = build_message(
            version="0.2.2",
            summary=" ".join(["calibration"] * 80),
            repository="napolitano/eurorack-quantizer-firmware",
            server_url="https://github.com",
            max_length=180,
        )
        body = message.removesuffix(
            " https://github.com/napolitano/eurorack-quantizer-firmware"
        )
        self.assertTrue(body.endswith("..."))
        self.assertLessEqual(len(message), 180)

    def test_truncate_words_keeps_short_text_unchanged(self) -> None:
        self.assertEqual(truncate_words("short text", 20), ("short text", False))


if __name__ == "__main__":
    unittest.main()
