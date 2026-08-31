#!/usr/bin/env python3
"""Static contract checks for the interactive calibration guide."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GUIDE = ROOT / "docs" / "calibration" / "interactive"
STEPS = GUIDE / "steps.js"
FLOWS = GUIDE / "flows.js"
APP = GUIDE / "calibration.js"
INDEX = GUIDE / "index.html"
CSS = GUIDE / "calibration.css"


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def main() -> int:
    required_files = [INDEX, CSS, GUIDE / "calculations.js", STEPS, FLOWS, APP, GUIDE / "README.md"]
    for path in required_files:
        require(path.is_file(), f"missing guide file: {path.relative_to(ROOT)}")

    index = INDEX.read_text(encoding="utf-8")
    steps = STEPS.read_text(encoding="utf-8")
    flows = FLOWS.read_text(encoding="utf-8")
    app = APP.read_text(encoding="utf-8")
    combined = "\n".join(path.read_text(encoding="utf-8") for path in required_files if path.suffix in {".html", ".js", ".css"})

    for forbidden in ("http://", "https://", "fetch(", "XMLHttpRequest", "localStorage", "sessionStorage"):
        require(forbidden not in combined, f"guide must remain self-contained; found forbidden token {forbidden!r}")

    script_order = ["calculations.js", "steps.js", "flows.js", "calibration.js"]
    positions = [index.find(f'src="{name}"') for name in script_order]
    require(all(pos >= 0 for pos in positions), "index.html must load all guide scripts")
    require(positions == sorted(positions), "guide scripts are not loaded in dependency order")

    asset_refs = sorted(set(re.findall(r'"([^"]+\.svg)"', steps)))
    require(len(asset_refs) >= 12, "expected the complete calibration SVG family to be referenced")
    asset_dir = ROOT / "docs" / "manual" / "assets" / "calibration"
    for name in asset_refs:
        require((asset_dir / name).is_file(), f"referenced SVG does not exist: {name}")

    expected_adc = "var positive = [1, 3, 5, 7, 9];\n    var verify = [0, 2, 4, 6, 8];"
    require(expected_adc in steps, "ADC fit/verification sequence drifted from README_CALIBRATION.md")

    for fragment in (
        "pio --version",
        "pio project config",
        "buildCalibration",
        "uploadCalibration",
        "buildProduction",
        "uploadProduction",
        "Never connect normal Eurorack bus power and the Nano USB connection at the same time",
        "Do not generate the 0 V reference by setting the bench PSU to 0.000 V",
        "The PSU is the source; the DMM is the reference",
        "pio device monitor -b 115200",
        "FM QUANTIZER STORED CALIBRATION REPORT",
    ):
        require(fragment in combined, f"required calibration contract text missing: {fragment}")

    full_match = re.search(r"function fullFlow\(\) \{(.*?)\n  \}", flows, re.S)
    require(full_match is not None, "full calibration flow not found")
    full = full_match.group(1)
    required_order = [
        "S.buildCalibration",
        "S.powerOffBeforeUsb",
        "S.connectUsb",
        "S.uploadCalibration",
        "S.transitionToEurorack",
        "S.startFull",
        "adcA",
        "adcB",
        "S.dacA",
        "S.dacB",
        "S.dualLoop",
        "S.outputA",
        "S.outputB",
        "S.powerOffForReport",
        "S.report",
        "S.buildProduction",
        "S.uploadProduction",
        "S.returnPower",
    ]
    cursor = -1
    for token in required_order:
        pos = full.find(token, cursor + 1)
        require(pos >= 0, f"full flow missing required step {token}")
        cursor = pos

    require("raw-adc-prerequisite" in flows and "start-dac-only" in flows, "RAW maintenance flow must enforce ADC prerequisite and subsequent DAC calibration")
    require("No invented threshold" in app, "external DMM guide must explicitly avoid inventing a PASS threshold")
    require("window.print()" in app, "completion page should remain printable")

    print(f"Interactive calibration guide contract: PASS ({len(asset_refs)} SVG assets checked)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
