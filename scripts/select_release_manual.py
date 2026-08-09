#!/usr/bin/env python3
"""Select the newest user manual compatible with a firmware release.

Manual source files must use the naming convention
``quantizer-user-manual.X.Y.Z.odt``.  The newest manual whose semantic version
is less than or equal to the firmware release is selected.  Not finding a
compatible manual is not an error; tagged firmware releases may legitimately
reuse an older manual or ship without one.

SPDX-License-Identifier: GPL-3.0-or-later
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


MANUAL_RE = re.compile(
    r"^quantizer-user-manual\.(?P<major>0|[1-9]\d*)\."
    r"(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)\.odt$"
)
VERSION_RE = re.compile(r"^(?:v)?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


@dataclass(frozen=True, order=True)
class Version:
    major: int
    minor: int
    patch: int

    @classmethod
    def parse(cls, value: str) -> "Version":
        match = VERSION_RE.fullmatch(value.strip())
        if not match:
            raise ValueError(f"unsupported release version: {value!r}")
        return cls(*(int(part) for part in match.groups()))

    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}"


@dataclass(frozen=True)
class ManualSelection:
    path: Path
    version: Version


def select_manual(directory: Path, firmware_version: Version) -> ManualSelection | None:
    compatible: list[ManualSelection] = []
    if not directory.is_dir():
        return None

    for path in directory.iterdir():
        if not path.is_file():
            continue
        match = MANUAL_RE.fullmatch(path.name)
        if not match:
            continue
        version = Version(
            int(match.group("major")),
            int(match.group("minor")),
            int(match.group("patch")),
        )
        if version <= firmware_version:
            compatible.append(ManualSelection(path=path, version=version))

    return max(compatible, key=lambda item: item.version, default=None)


def write_github_output(output_file: Path, selection: ManualSelection | None) -> None:
    with output_file.open("a", encoding="utf-8") as handle:
        if selection is None:
            handle.write("found=false\n")
            handle.write("version=\n")
            handle.write("path=\n")
            handle.write("pdf_name=\n")
            return

        handle.write("found=true\n")
        handle.write(f"version={selection.version}\n")
        handle.write(f"path={selection.path.as_posix()}\n")
        handle.write(f"pdf_name=quantizer-user-manual.{selection.version}.pdf\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", default="docs/manual", type=Path)
    parser.add_argument("--firmware-version", required=True)
    parser.add_argument("--github-output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    firmware_version = Version.parse(args.firmware_version)
    selection = select_manual(args.directory, firmware_version)

    if args.github_output is not None:
        write_github_output(args.github_output, selection)

    if selection is None:
        print(
            "manual-selection: no compatible user manual found; "
            "release continues without manual assets"
        )
        return 0

    print(
        f"manual-selection: firmware {firmware_version} -> "
        f"{selection.path.as_posix()} (manual {selection.version})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
