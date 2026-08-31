#!/usr/bin/env python3
"""Generate a short social announcement for a tagged firmware release.

The message uses the same curated ``### Release summary`` paragraph that feeds
GitHub Release notes. It is normalised to plain text and truncated on a word
boundary when required so the repository URL and firmware version always fit.

SPDX-License-Identifier: GPL-3.0-or-later
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from generate_release_notes import extract_version_section, split_summary


MARKDOWN_LINK_RE = re.compile(r"\[([^\]]+)\]\([^\)]+\)")
MARKDOWN_DECORATION_RE = re.compile(r"[*_`]+")
WHITESPACE_RE = re.compile(r"\s+")
SENTENCE_BOUNDARY_RE = re.compile(r"(?<=[.!?])\s+(?=[A-Z0-9])")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--changelog", default="CHANGELOG.md")
    parser.add_argument("--version", required=True, help="Release version, e.g. 0.2.2")
    parser.add_argument("--repository", required=True, help="OWNER/REPO")
    parser.add_argument("--server-url", default="https://github.com")
    parser.add_argument("--max-length", type=int, default=280)
    return parser.parse_args()


def plain_text(markdown: str) -> str:
    """Collapse the curated release summary into compact plain text."""

    text = MARKDOWN_LINK_RE.sub(r"\1", markdown)
    text = MARKDOWN_DECORATION_RE.sub("", text)
    return WHITESPACE_RE.sub(" ", text).strip()


def feature_summary(summary: str) -> str:
    """Prefer feature-bearing sentences over a boilerplate version opener."""

    compact = plain_text(summary)
    sentences = [part.strip() for part in SENTENCE_BOUNDARY_RE.split(compact) if part.strip()]
    if len(sentences) > 1 and sentences[0].lower().startswith("version "):
        return " ".join(sentences[1:])
    return compact


def truncate_words(text: str, limit: int) -> tuple[str, bool]:
    """Return text within ``limit`` characters, adding ``...`` when clipped."""

    if len(text) <= limit:
        return text, False
    if limit <= 3:
        return "." * max(0, limit), True

    candidate = text[: limit - 3].rstrip()
    if " " in candidate:
        candidate = candidate.rsplit(" ", 1)[0].rstrip(" ,;:-")
    return candidate + "...", True


def build_message(
    *, version: str, summary: str, repository: str, server_url: str, max_length: int
) -> str:
    """Build a deterministic release announcement capped at ``max_length``."""

    repository_url = f"{server_url.rstrip('/')}/{repository}"
    prefix = f"FM Quantizer v{version} released: "
    suffix = f" {repository_url}"
    available = max_length - len(prefix) - len(suffix)
    if available < 4:
        raise ValueError("max length is too small for version and repository URL")

    compact_summary, _ = truncate_words(feature_summary(summary), available)
    message = prefix + compact_summary + suffix
    if len(message) > max_length:
        raise AssertionError("social announcement exceeds requested maximum length")
    return message


def main() -> int:
    args = parse_args()
    if args.max_length < 1:
        print("social-post error: --max-length must be positive", file=sys.stderr)
        return 2

    text = Path(args.changelog).read_text(encoding="utf-8")
    try:
        section = extract_version_section(text, args.version)
        summary, _ = split_summary(section)
        message = build_message(
            version=args.version,
            summary=summary,
            repository=args.repository,
            server_url=args.server_url,
            max_length=args.max_length,
        )
    except (ValueError, AssertionError) as exc:
        print(f"social-post error: {exc}", file=sys.stderr)
        return 2

    print(message)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
