#!/usr/bin/env python3
"""Validate frozen EEPROM fixture byte arrays against their binary sources."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "test" / "fixtures" / "persistence"
EXPECTED_SIZE = 1024


def parse_inc(path: Path) -> bytes:
    values = re.findall(r"0x([0-9A-Fa-f]{2})", path.read_text(encoding="utf-8"))
    return bytes(int(value, 16) for value in values)


def main() -> int:
    binaries = sorted(FIXTURES.glob("*.bin"))
    if not binaries:
        print(
            "persistence-fixtures: no binary fixtures found; "
            "test/fixtures/persistence/*.bin must be tracked in Git",
            file=sys.stderr,
        )
        return 1
    for binary in binaries:
        data = binary.read_bytes()
        include = binary.with_suffix(".inc")
        if len(data) != EXPECTED_SIZE:
            print(f"persistence-fixtures: {binary.name} is {len(data)} bytes, expected {EXPECTED_SIZE}", file=sys.stderr)
            return 1
        if not include.exists():
            print(f"persistence-fixtures: missing {include.name}", file=sys.stderr)
            return 1
        if parse_inc(include) != data:
            print(f"persistence-fixtures: {include.name} does not match {binary.name}", file=sys.stderr)
            return 1
        print(f"persistence-fixtures: {binary.name} OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
