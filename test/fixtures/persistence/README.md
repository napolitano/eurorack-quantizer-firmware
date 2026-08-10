# Persistence golden fixtures

These frozen 1024-byte EEPROM images protect the binary persistence contract independently of the serializer that happens to be compiled today.

## Contents

- [Current format fixture](#current-format-fixture)
- [Supported legacy fixture](#supported-legacy-fixture)
- [Why these are format fixtures](#why-these-are-format-fixtures)
- [Maintenance rule](#maintenance-rule)

## Current format fixture

`current-v5-save-v6-live.bin` contains representative user data using save-record format v5 and live-state format v6: a scale slot, a full-configuration slot, live state including the UI layer and LED calibration, plus startup-sequence metadata.

## Supported legacy fixture

`legacy-v4-save-v5-live.bin` carries the same musical state encoded using the two legacy formats the current reader explicitly supports: save-record v4 and live-state v5. The UI-layer bit is intentionally absent so the production migration path must infer the Arpeggiator layer from the enabled Arpeggiator state.

## Why these are format fixtures

> [!IMPORTANT]
> These files are intentionally named by **persistence format**, not by firmware release. The current branch proves compatibility with the formats its production readers explicitly accept. It does not invent an EEPROM image and label it `0.1.0` or `0.1.1` without a byte-for-byte source from those tagged releases or a real device dump.

An exact historical release image may be added later when it is obtained directly from the corresponding tag/build or captured from hardware. Until then, the format fixtures are the stronger and more honest compatibility contract.

## Maintenance rule

The `.bin` file is the canonical frozen artifact. The matching `.inc` file exists only so PlatformIO/Unity can compile the same bytes into a native test. `scripts/check_persistence_fixtures.py` verifies that every `.inc` still matches its binary source exactly.

Do not regenerate an existing fixture merely because serialization code changes. A fixture changes only when correcting a demonstrably wrong fixture; a new persistence format gets a new fixture.

---

<p align="center">From Munich With <img src="../../../docs/assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
