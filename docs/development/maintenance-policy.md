# Firmware maintenance policy

The firmware has reached a stage where stability, measurable timing, persistence compatibility and release reproducibility are more valuable than adding features simply because there is still some flash available. This policy defines the default engineering posture for work after firmware 0.2.0.

## Contents

- [Fixed hardware boundary](#fixed-hardware-boundary)
- [Feature and resource policy](#feature-and-resource-policy)
- [Required verification](#required-verification)
- [Persistence compatibility](#persistence-compatibility)
- [Toolchain changes](#toolchain-changes)
- [When a larger change is justified](#when-a-larger-change-is-justified)

## Fixed hardware boundary

> [!IMPORTANT]
> The Arduino Nano R3 / ATmega328P module hardware, PCB, components, pin assignments and wiring are fixed constraints of this firmware project. A change that requires different hardware belongs in a separate project.

Normal maintenance therefore works within the existing 16 MHz ATmega328P, 30,720-byte application-flash budget, 2 KiB SRAM, 1 KiB EEPROM and the existing peripherals.

## Feature and resource policy

The normal AVR builds are subject to the repository engineering gates:

| Resource | Engineering limit |
|---|---:|
| Application flash | **92.5% of 30,720 bytes** |
| Static SRAM | **70% of 2,048 bytes** |

The gate is a ceiling, not a target. Remaining headroom is primarily reserved for defect corrections, persistence migrations, compiler/toolchain changes and genuinely useful refinements.

> [!WARNING]
> Large new runtime features are effectively under a feature freeze. A feature that materially increases flash use needs a clear user-facing justification and an explicit before/after resource comparison. Passing the 92.5% gate alone is not sufficient justification.

Refactoring is held to the same standard. Avoid broad cleanups when they increase code size, timing risk or regression surface without a concrete maintenance benefit.

## Required verification

Changes should use the strongest applicable layer rather than relying on one headline metric:

1. first-party warning-clean compilation;
2. native unit/integration/regression/system tests;
3. deterministic property-style invariant tests;
4. AddressSanitizer and UndefinedBehaviorSanitizer host run;
5. requirement traceability and source-coverage gates;
6. both normal AVR builds and flash/SRAM gates;
7. timing-probe build compilation;
8. real-hardware qualification for changes that can affect runtime timing, I/O, persistence or user-visible behavior.

Native source coverage is intentionally not driven toward 100% for its own sake. Tests should prove meaningful behavior, error handling and invariants.

## Persistence compatibility

EEPROM compatibility is part of the product behavior because normal firmware updates preserve EEPROM contents. Existing golden persistence-format fixtures must never be regenerated merely to match new serializer output. A new persistent format requires a new fixture and an explicit migration path where compatibility is intended.

Historical release fixtures may be added only from a byte-verifiable tagged implementation or a real hardware EEPROM dump. Do not fabricate a release-labelled EEPROM image from assumptions.

The current EEPROM wear audit is documented in [`../testing/eeprom-wear-audit.md`](../testing/eeprom-wear-audit.md). Any change that increases automatic save frequency, reduces the live-state ring or adds frequently rewritten fixed-address data must update the audit and tests.

## Toolchain changes

Build inputs are deliberately pinned. Toolchain or CI dependency updates are maintenance changes, not background drift. Dependabot may propose updates, but they are reviewed and merged only after the normal verification path.

Tagged releases include `BUILD-INFO.txt` with resolved Python packages, PlatformIO package versions and the AVR compiler identity so published artifacts retain their build provenance. See [`reproducible-builds.md`](reproducible-builds.md).

## When a larger change is justified

A larger change remains appropriate when it fixes a demonstrated defect, closes a meaningful usability gap, improves safety/reliability, or provides a clearly valuable musical capability that fits the existing hardware and resource envelope. The expected benefit should be stated before implementation, together with likely flash, SRAM, timing, persistence and test impact.

The default state of a mature firmware is therefore **maintain, measure and simplify**, not continuous feature accumulation.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
