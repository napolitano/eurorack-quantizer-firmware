# Analog calibration and maintenance

This document describes the hardware-calibration workflow for the Free Modular Quantizer firmware development tree after release 0.2.1. The split-image calibration architecture is currently **Unreleased** and is intended for the next release; it must not be described as a published 0.2.2 release until that release is explicitly declared.

The key design rule is simple: the normal production firmware **uses** calibration, while a separate calibration/maintenance firmware **creates and verifies** it. Both images share the same versioned EEPROM calibration format.

## Contents

- [When calibration is required](#when-calibration-is-required)
- [Power and connection safety](#power-and-connection-safety)
- [Architecture](#architecture)
- [Why calibration survives firmware uploads](#why-calibration-survives-firmware-uploads)
- [EEPROM isolation](#eeprom-isolation)
- [Build and upload targets](#build-and-upload-targets)
- [Maintenance console](#maintenance-console)
- [Recommended calibration workflow](#recommended-calibration-workflow)
- [ADC calibration](#adc-calibration)
- [DAC loopback self-calibration](#dac-loopback-self-calibration)
- [Diagnostics](#diagnostics)
- [Manual affine calibration](#manual-affine-calibration)
- [Failure and recovery behavior](#failure-and-recovery-behavior)
- [EEPROM erase warning](#eeprom-erase-warning)
- [LED and button-ladder calibration](#led-and-button-ladder-calibration)
- [Calibration checklist](#calibration-checklist)

## When calibration is required

Analog calibration is **not periodic maintenance** and is not required simply because the firmware has been updated. Use the calibration/maintenance image when at least one of the following applies:

- the module has never been individually ADC/DAC calibrated;
- the persistent calibration journal has been erased, corrupted, or intentionally reset;
- components in the analog CV input/output path or voltage-reference path have been replaced or materially changed;
- measurement shows a meaningful pitch or 1 V/oct error that cannot be explained by the external test setup.

If a module already has a valid, verified calibration profile and normal EEPROM-preserving firmware uploads are used, **do not recalibrate it merely because a new firmware image was installed**. The production image continues to load the existing calibration journal.

## Power and connection safety

> [!CAUTION]
> **Use USB power only during firmware update and calibration/maintenance.** Switch the Eurorack case off, remove the module, disconnect the Eurorack power ribbon, and leave that ribbon disconnected for the entire maintenance session. Do not power the module from the Eurorack bus and USB at the same time.

The Arduino Nano may remain installed on the Quantizer PCB. Connect the Nano to the computer with a data-capable USB cable; that USB connection powers the module while the maintenance image and serial console are in use.

When an external precision voltage source, DMM, or loopback cable is used, connect only the measurement connections required by the procedure. Do not introduce a second power supply to the module. Keep the exposed PCB on a clean, dry, nonconductive surface and avoid touching components while USB power is present.

## Architecture

The production image contains only the runtime pieces required to apply a previously stored analog profile:

```text
Production firmware
    |
    +-- CalibrationReader          read only
    +-- CalibrationProfile         ADC A/B + DAC A/B affine correction
    +-- runtime ADC/DAC conversion
    |
    `-- EEPROM calibration partition 967..1022
```

The maintenance image contains the writer and diagnostic tools:

```text
Calibration/maintenance firmware
    |
    +-- CalibrationReader
    +-- CalibrationStore           read/write
    +-- guided ADC calibration
    +-- DAC loopback self-calibration
    +-- independent verification
    +-- noise / sweep diagnostics
    +-- EEPROM status / read-only backup
    |
    `-- EepromPartition 967..1022
```

The serial command parser, help text, self-calibration solver and verbose diagnostics are therefore not linked into the normal musical firmware. This keeps scarce ATmega328P application flash available for actual product behavior.

`AnalogConfig.h` remains the **factory/fallback** profile. A valid EEPROM profile takes precedence in both images.

> [!NOTE]
> Per-channel ADC/DAC affine calibration, the maintenance image and loopback self-calibration are extensions of this C++ firmware. Quinn Freedman's original Rust firmware does not provide an equivalent software calibration layer.

## Why calibration survives firmware uploads

The ATmega328P stores program code and EEPROM data in separate memories. The normal serial bootloader upload targets used by this project program **flash only**; they do not intentionally rewrite EEPROM.

The intended workflow is therefore:

```text
production firmware
        |
        | flash calibration image
        v
calibration/maintenance firmware
        |
        | measure + save profile
        v
EEPROM calibration journal
        |
        | flash production image
        v
production firmware loads same profile
```

Changing between the production and maintenance images does not require recalibration as long as the upload process preserves EEPROM.

This guarantee does **not** apply to an explicit chip erase, an ISP workflow configured to erase EEPROM, or a tool invocation that deliberately programs/clears EEPROM. See [EEPROM erase warning](#eeprom-erase-warning).

## EEPROM isolation

The complete 1 KiB EEPROM is partitioned by responsibility:

| Physical EEPROM bytes | Purpose | Writer |
|---:|---|---|
| `0..71` | 12 scale presets | production preset store |
| `72..503` | 12 full-configuration presets | production preset store |
| `504..965` | 11 × 42-byte wear-levelled live-state records | production live-state store |
| `966` | legacy <=0.2.1 twelfth live-slot marker / migration detection | migration path only |
| `967..1022` | 2 × 28-byte analog-calibration records | maintenance calibration store only |
| `1023` | reserved, intentionally unwritten | none |

The maintenance `CalibrationStore` is not given unrestricted access to the parent EEPROM. It receives an `EepromPartition` with a local address range of only `0..55`, mapped physically to `967..1022`. Any attempted access outside that local range is rejected by the partition abstraction.

This makes collision prevention structural: a calibration bug cannot address scale presets or normal live-state bytes through the calibration writer API.

### Upgrade from <=0.2.1

Older firmware used twelve 42-byte live-state slots, so the former twelfth slot starts at physical byte `966` and overlaps the new calibration area.

Both production and maintenance startup paths therefore inspect/migrate the legacy slot **before** the calibration tail is treated as writable. If the old twelfth slot contains the newest valid live state, it is committed into the new 11-slot ring and the asynchronous migration write is completed before calibration proceeds.

This specifically protects the direct upgrade path:

```text
0.2.1 production -> calibration image -> new production image
```

## Build and upload targets

PlatformIO provides separate environments for both Nano bootloader generations.

### Production firmware

```text
pio run -e nanoatmega328new -t upload
pio run -e nanoatmega328 -t upload
```

### Calibration/maintenance firmware

```text
pio run -e nanoatmega328new_calibration -t upload
pio run -e nanoatmega328_calibration -t upload
```

Use the environment matching the Nano bootloader already installed on the module. These normal upload targets are intended to program application flash while preserving EEPROM.

The tagged-release workflow is prepared to publish separate HEX/ELF artifacts for production and calibration images once the next release is actually declared.

## Maintenance console

Connect a serial terminal at **115200 baud** after flashing the calibration image. The image boots directly into the maintenance console; no SHIFT-at-power-on gesture is required.

The banner identifies the firmware as an **Unreleased** development target and reports the active calibration state.

Main commands:

| Command | Purpose |
|---|---|
| `help` | Show command summary |
| `version` | Show maintenance firmware/version information |
| `cal show` | Show active ADC/DAC calibration and journal state |
| `cal load` | Reload the newest valid persistent profile |
| `cal save` | Persist the current in-RAM profile atomically |
| `cal defaults` | Restore `AnalogConfig.h` fallback values in RAM only |
| `cal set adc a|b OFFSET NUM DEN` | Set one ADC affine correction in RAM |
| `cal set dac a|b OFFSET NUM DEN` | Set one DAC affine correction in RAM |
| `cal adc a|b` | Guided two-point ADC calibration |
| `cal self a|b` | Fit, verify and persist DAC loopback calibration |
| `cal verify a|b` | Verify the active DAC correction through loopback |
| `cal report` | Re-verify both DAC loopbacks and print an archival calibration report plus compact label text |
| `measure adc a|b` | Show RAW and calibrated ADC statistics |
| `diag noise a|b` | Capture ADC stability/noise statistics |
| `diag sweep a|b` | Emit a raw DAC-loopback CSV sweep |
| `eeprom status` | Show partition boundaries, journal records and legacy/reserved bytes |
| `eeprom backup` | Print a read-only 1024-byte EEPROM hex dump |

There is intentionally **no generic EEPROM restore/erase command** in the maintenance console. That would defeat the write-isolation design.

## Recommended calibration workflow

Power the module from **USB only**, with the Eurorack ribbon cable disconnected. Allow the USB-powered module, external voltage reference/source, and DMM to thermally settle before precision measurements.

1. Confirm that calibration is actually required according to [When calibration is required](#when-calibration-is-required).
2. Switch the Eurorack case off, remove the module, disconnect the Eurorack power ribbon, and leave it disconnected for the complete maintenance session.
3. Connect the Nano to the computer with a data-capable USB cable; USB powers the module.
4. Flash the calibration/maintenance image without erasing EEPROM.
5. Open a serial terminal at 115200 baud.
6. Run `eeprom status` and, when appropriate, capture `eeprom backup` before changing anything.
7. Establish ADC calibration against a trustworthy external voltage reference/source:
   - `cal adc a`
   - `cal adc b`
8. Inspect the result with `cal show`, `measure adc a|b` and optionally `diag noise a|b`.
9. Persist the verified ADC profile with `cal save`.
10. Patch `CV OUT A -> CV IN A`; run `cal self a`, then `cal verify a`.
11. Patch `CV OUT B -> CV IN B`; run `cal self b`, then `cal verify b`.
12. For a final manufacturing/service record, patch **both** `CV OUT A -> CV IN A` and `CV OUT B -> CV IN B`, then run `cal report`. Save the complete terminal output. The command re-verifies both DAC loops and emits compact label text only when both channels pass.
13. Use `diag sweep a|b` when characterizing residual output non-linearity is useful.
14. Flash the matching production firmware without erasing EEPROM, while the module remains USB-powered and the Eurorack ribbon remains disconnected.
15. After the production image has booted successfully, disconnect USB. With the case still off, reconnect the Eurorack ribbon and reinstall the module.
16. Power the case on and perform a musical 1 V/oct verification on the normal production image.

## Calibration report and module label

`cal report` is a **read-only verification/reporting operation with respect to EEPROM**. It does not alter the active calibration, journal generation, presets, live state, or any other persistent data. During the measurement it drives the two DAC outputs through the existing independent verification points and then returns both outputs to zero.

The command requires:

```text
CV OUT A -> CV IN A
CV OUT B -> CV IN B
```

and a valid stored calibration journal record. A factory fallback profile is deliberately **not** eligible for a calibration label. The active in-RAM profile must also match the newest stored record; if there are unsaved manual changes, `cal report` aborts and asks the operator to use `cal save` or `cal load` first. This prevents a label from certifying coefficients that would disappear on the next reboot.

The full report includes:

- development target and calibration-format version;
- active calibration journal record and generation;
- the stored ADC/DAC affine coefficients for both channels;
- fresh DAC-A and DAC-B loopback verification rows;
- maximum residual for each DAC channel;
- an overall `PASS` or `FAIL`;
- on full PASS only, a compact `LABEL` block suitable for copying into a label-generation workflow.

The label block intentionally says `ADC A/B: STORED PROFILE` rather than claiming a new absolute ADC verification. Loopback can verify the combined input/output path against the already established ADC calibration, but it cannot independently prove the ADC voltage scale. Absolute ADC calibration still requires the external reference procedure described above.


A successful report ends with a block in this form:

```text
----- LABEL -----
CALIBRATION VERIFIED
ADC A/B: STORED PROFILE
DAC A: PASS / max 2 codes
DAC B: PASS / max 1 codes
CAL fmt 1 / gen 7
-----------------
```

The numeric residuals above are DAC-code residuals from the fresh loopback verification. They are intentionally kept as raw verification units rather than presented as an invented absolute-voltage accuracy claim.

The Nano has no real-time clock, so the firmware does **not** invent a calibration date. Add date/time, serial number, builder/tester identity, or other manufacturing metadata on the host side when the captured report is archived or converted into a physical label.

If either DAC verification fails, `cal report` prints `OVERALL: FAIL` and explicitly instructs the operator not to apply a calibration label.

## ADC calibration

The ADC path uses independent integer affine corrections for channels A and B. Runtime order is:

```text
corrected_adc = (raw_adc + offset) * numerator / denominator
```

with safe integer rounding/clamping in the shared conversion code.

The guided `cal adc a` / `cal adc b` command performs a two-point fit using externally known **0.000 V** and **10.000 V** references. For each endpoint it samples the real ADC repeatedly, reports min/mean/max/span, rejects an implausibly unstable or compressed measurement, and computes an affine candidate in RAM.

The ADC calibration is the measurement reference for later DAC loopback self-calibration. It therefore needs an external reference first; DAC self-calibration cannot independently fix a wrong ADC scale.

> [!IMPORTANT]
> The mathematical midpoint of a 10-bit `0..1023` ADC range is 511.5 codes. Do not force a physical midpoint measurement to exactly 512 merely because the runtime representation is integer.

After the wizard, inspect the candidate before `cal save`. `cal defaults` can restore factory/fallback constants in RAM without destroying the last valid EEPROM record.

## DAC loopback self-calibration

The intended patch is:

```text
CV OUT A -> CV IN A
```

or the corresponding B channel.

`cal self a` / `cal self b` deliberately modifies **only the selected DAC/output affine correction**. The active ADC calibration remains fixed and acts as the measurement reference.

The routine:

1. verifies that a plausible loop exists;
2. drives several RAW DAC support points;
3. rejects inadequate span, non-monotonic response and unstable/invalid measurements;
4. fits a new integer affine DAC correction in RAM;
5. tests that candidate at independent verification points;
6. restores the old profile if verification fails;
7. atomically persists the new profile only after PASS.

The DAC runtime order is:

```text
corrected_dac = nominal_dac * numerator / denominator + offset
```

and the result is clipped to the physical 12-bit range `0..4095`.

### Physical limitation of loopback

Loopback observes the composition of output and input paths:

```text
DAC -> output stage -> patch cable -> input stage -> ADC
```

It does not create an independent voltage standard. An ADC gain error can be transferred into the fitted DAC correction or can mask an opposing output error. For that reason ADC calibration must be established independently first.

The current development implementation intentionally stores only affine offset/gain calibration. A per-semitone residual table is not justified until repeatable hardware sweeps demonstrate material residual non-linearity that the affine model cannot meet.

## Diagnostics

### `measure adc a|b`

Captures repeated samples and reports RAW and calibrated statistics. Use it to check endpoint placement and verify the effect of the current affine correction.

### `diag noise a|b`

Captures 1024 ADC samples and reports integer min/mean/max/peak-to-peak values. It deliberately avoids floating-point statistics on the ATmega328P.

### `diag sweep a|b`

Drives the selected DAC over a broad RAW-code sweep and prints CSV-compatible rows:

```text
raw_dac,adc_raw,adc_cal,equivalent_dac
```

This is intended for host-side plotting/analysis of repeatable INL, output-stage curvature and residual error. The sweep itself does not alter persistent calibration.

### `eeprom status`

Reports the known EEPROM regions, calibration journal validity/generation, legacy byte 966 and reserved byte 1023. It is a diagnostic read; it does not modify preset storage.

### `eeprom backup`

Prints the complete EEPROM as a read-only hex dump before risky service operations. The console intentionally does not provide the inverse restore operation.

## Manual affine calibration

For controlled development work the profile can be edited directly:

```text
cal set adc a OFFSET NUM DEN
cal set adc b OFFSET NUM DEN
cal set dac a OFFSET NUM DEN
cal set dac b OFFSET NUM DEN
```

Rules:

- denominator must be non-zero;
- values must fit the persistent calibration format;
- edits affect RAM first;
- use `cal show` to inspect the result;
- use `cal save` only after verification.

Production firmware has no command path that can save or mutate these values.

## Failure and recovery behavior

The persistence journal uses two alternating 28-byte records with format metadata, a generation number and CRC16. The destination validity marker is invalidated before data is replaced and committed last. Until the replacement is complete, the previous valid record remains recoverable.

On load:

- the newest valid generation wins;
- a corrupt/newer record falls back to the other valid record;
- if neither record is valid, `AnalogConfig.h` factory values are used;
- invalid persistent data never becomes a partially accepted profile.

During `cal self` a failed loop, failed fit, failed independent verification or failed persistence operation restores the previous active profile and leaves the previous valid EEPROM journal entry usable.

## EEPROM erase warning

> [!WARNING]
> Normal production/maintenance bootloader uploads are intended to preserve EEPROM. An explicit chip erase, an ISP workflow that erases EEPROM, `avrdude -e`, AVRDUDESS **Erase flash and EEPROM**, or an equivalent programming operation can remove **presets, live state, LED brightness calibration and the ADC/DAC calibration journal**.

Before such service work, capture an EEPROM backup and understand the programmer/fuse settings being used. Reburning a bootloader is not equivalent to a routine firmware upload.

## LED and button-ladder calibration

The separate maintenance image is for analog CV/DAC service. Existing user-facing LED brightness calibration remains part of the production UI because the stored red/green PWM pair is ordinary front-panel configuration and is needed during normal use.

The twelve-note resistor ladder remains characterized by the original-board values in `AnalogConfig.h`. Startup rest/ladder handling is part of normal production initialization; maintenance diagnostics can be used to investigate analog instability, but changing the ladder model still requires a deliberate firmware configuration change.

## Calibration checklist

Before considering a module electrically calibrated:

- [ ] Calibration is actually required (uncalibrated hardware, lost/erased calibration, relevant analog-path hardware change, or verified pitch/1 V/oct problem).
- [ ] Eurorack power ribbon is disconnected for the complete maintenance session; the module is powered from USB only.
- [ ] Latest public release versus `Unreleased` development status is understood.
- [ ] Correct Nano bootloader environment selected.
- [ ] Maintenance image flashed without EEPROM erase.
- [ ] `eeprom status` shows sane partition/journal state.
- [ ] Optional pre-service `eeprom backup` captured.
- [ ] ADC A calibrated against external 0 V / 10 V reference and verified.
- [ ] ADC B calibrated against external 0 V / 10 V reference and verified.
- [ ] ADC stability/noise is acceptable on both channels.
- [ ] Verified ADC profile persisted with `cal save`.
- [ ] `CV OUT A -> CV IN A` self-calibration passes and `cal verify a` passes.
- [ ] `CV OUT B -> CV IN B` self-calibration passes and `cal verify b` passes.
- [ ] Production image reflashed without EEPROM erase.
- [ ] Persistent profile survives the image swap and reboot.
- [ ] Presets/live state remain intact.
- [ ] Final 1 V/oct musical/output verification passes on the production image.
