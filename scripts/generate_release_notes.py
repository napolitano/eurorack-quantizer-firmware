#!/usr/bin/env python3
"""Generate deterministic GitHub Release notes from CHANGELOG.md.

The matching release section must contain a ``### Release summary`` heading
followed by one prose paragraph. The public notes are emitted in this order:
summary, detailed changelog excerpt, checksum notice, compare link.

SPDX-License-Identifier: GPL-3.0-or-later
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


VERSION_HEADING_RE = re.compile(r"^##\s+([^\s]+)\s+—\s+.+$")
SUBHEADING_RE = re.compile(r"^###\s+(.+?)\s*$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--changelog", default="CHANGELOG.md")
    parser.add_argument("--tag", required=True, help="Release tag, e.g. v0.1.2")
    parser.add_argument("--repository", required=True, help="OWNER/REPO")
    parser.add_argument("--server-url", default="https://github.com")
    parser.add_argument("--previous-tag", default="")
    return parser.parse_args()


def extract_version_section(text: str, version: str) -> list[str]:
    lines = text.splitlines()
    start = None
    for i, line in enumerate(lines):
        match = VERSION_HEADING_RE.match(line)
        if match and match.group(1) == version:
            start = i + 1
            break
    if start is None:
        raise ValueError(f"CHANGELOG.md has no release section for {version}")

    end = len(lines)
    for i in range(start, len(lines)):
        if lines[i].startswith("## "):
            end = i
            break
    return lines[start:end]


def split_summary(section: list[str]) -> tuple[str, list[str]]:
    summary_heading = None
    for i, line in enumerate(section):
        match = SUBHEADING_RE.match(line)
        if match and match.group(1).strip().lower() == "release summary":
            summary_heading = i
            break
    if summary_heading is None:
        raise ValueError("release section is missing '### Release summary'")

    summary_lines: list[str] = []
    i = summary_heading + 1
    while i < len(section) and not section[i].strip():
        i += 1
    while i < len(section) and not section[i].startswith("### "):
        if section[i].strip():
            summary_lines.append(section[i].strip())
        elif summary_lines:
            break
        i += 1

    summary = " ".join(summary_lines).strip()
    if not summary:
        raise ValueError("'### Release summary' must contain one prose paragraph")
    if len(summary_lines) > 7:
        raise ValueError(
            "'### Release summary' must not exceed 7 non-empty source lines"
        )

    # Detailed excerpt = everything before the summary heading plus every
    # subsection after the summary paragraph. Trim surrounding empty lines.
    detail = section[:summary_heading] + section[i:]
    while detail and not detail[0].strip():
        detail.pop(0)
    while detail and not detail[-1].strip():
        detail.pop()
    return summary, detail


def main() -> int:
    args = parse_args()
    version = args.tag[1:] if args.tag.startswith("v") else args.tag
    text = Path(args.changelog).read_text(encoding="utf-8")

    try:
        section = extract_version_section(text, version)
        summary, detail = split_summary(section)
    except ValueError as exc:
        print(f"release-notes error: {exc}", file=sys.stderr)
        return 2

    print(summary)
    print()
    print("## Changelog")
    print()
    if detail:
        print("\n".join(detail))
        print()
    print("## Artifact integrity")
    print()
    print(
        "Release assets include `SHA256SUMS.txt` and `MD5SUMS.txt`. "
        "Use the SHA-256 manifest for normal integrity verification; the MD5 "
        "manifest is provided as an additional compatibility checksum."
    )
    print()
    print("## Diff")
    print()
    if args.previous_tag:
        base = args.server_url.rstrip("/")
        print(
            f"[{args.previous_tag}...{args.tag}]"
            f"({base}/{args.repository}/compare/{args.previous_tag}...{args.tag})"
        )
    else:
        base = args.server_url.rstrip("/")
        print(f"[{args.tag}]({base}/{args.repository}/commits/{args.tag})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
