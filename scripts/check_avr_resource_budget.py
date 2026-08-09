#!/usr/bin/env python3
# Purpose: Enforce conservative ATmega328P flash/SRAM engineering budgets.
# Author: Axel Napolitano
# Original FM Quantizer concept and Rust firmware: Quinn Freedman
# Copyright (C) 2026 Axel Napolitano
# SPDX-License-Identifier: GPL-3.0-or-later

"""Enforce conservative ATmega328P flash/SRAM budgets from an AVR ELF file.

The hard MCU/board limits are already checked by PlatformIO. This script adds
an engineering margin so feature growth does not consume the last usable bytes
without making that loss of headroom visible in CI.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

FLASH_CAPACITY = 30_720  # conservative application space used for Nano builds
SRAM_CAPACITY = 2_048
FLASH_BUDGET_PERCENT = 85.0
SRAM_BUDGET_PERCENT = 70.0


def find_avr_size() -> str:
    direct = shutil.which("avr-size")
    if direct:
        return direct
    home = Path.home()
    candidates = [
        home / ".platformio/packages/toolchain-atmelavr/bin/avr-size",
        home / ".platformio/packages/toolchain-atmelavr/bin/avr-size.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    raise FileNotFoundError("avr-size was not found; build an AVR environment first")


def read_sizes(tool: str, elf: Path) -> tuple[int, int, int]:
    result = subprocess.run(
        [tool, "--format=berkeley", str(elf)],
        check=True,
        capture_output=True,
        text=True,
    )
    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if len(lines) < 2:
        raise RuntimeError(f"unexpected avr-size output:\n{result.stdout}")
    fields = lines[-1].split()
    if len(fields) < 3:
        raise RuntimeError(f"unexpected avr-size data row: {lines[-1]}")
    text, data, bss = (int(fields[0]), int(fields[1]), int(fields[2]))
    return text, data, bss


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    args = parser.parse_args()

    if not args.elf.is_file():
        print(f"resource-budget: ELF not found: {args.elf}", file=sys.stderr)
        return 2

    try:
        tool = find_avr_size()
        text, data, bss = read_sizes(tool, args.elf)
    except (FileNotFoundError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"resource-budget: {exc}", file=sys.stderr)
        return 2

    # AVR .data initializers occupy flash and are copied into SRAM at startup.
    flash = text + data
    sram = data + bss
    flash_pct = 100.0 * flash / FLASH_CAPACITY
    sram_pct = 100.0 * sram / SRAM_CAPACITY

    print(
        f"resource-budget: flash {flash}/{FLASH_CAPACITY} bytes "
        f"({flash_pct:.1f}%, target <= {FLASH_BUDGET_PERCENT:.1f}%)"
    )
    print(
        f"resource-budget: static SRAM {sram}/{SRAM_CAPACITY} bytes "
        f"({sram_pct:.1f}%, target <= {SRAM_BUDGET_PERCENT:.1f}%)"
    )
    print(f"resource-budget: sections text={text}, data={data}, bss={bss}")

    failed = False
    if flash_pct > FLASH_BUDGET_PERCENT:
        print("resource-budget: flash engineering budget exceeded", file=sys.stderr)
        failed = True
    if sram_pct > SRAM_BUDGET_PERCENT:
        print("resource-budget: static SRAM engineering budget exceeded", file=sys.stderr)
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
