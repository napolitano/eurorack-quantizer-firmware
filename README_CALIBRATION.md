# Analog calibration and maintenance firmware

The Quantizer has a dedicated maintenance firmware image for calibrating the two CV inputs and two CV outputs without consuming production-firmware flash. Since the current `Unreleased` development tree, the calibration workflow is **front-panel driven**: no command shell and no typed serial input are required.

Calibration is **not periodic maintenance**. Run it only when:

- a newly built module has never been individually calibrated;
- the calibration EEPROM data were erased or lost;
- components in the analog CV input/output path were replaced or materially changed; or
- measured pitch / 1 V/oct behavior indicates a calibration problem.

A normal EEPROM-preserving firmware update does **not** require recalibration.

## Front-panel reference

<p align="center"><img src="docs/manual/assets/front-panel-reference.png" width="620" alt="Quantizer front panel showing the note ring, SHIFT, LOAD, SAVE, Channel A and Channel B CV inputs, outputs, and status LEDs"></p>

The maintenance image reuses the existing controls only:

| Control | Maintenance function |
|---|---|
| **SAVE** | Start the complete ADC + DAC calibration wizard |
| **LOAD** | Verify the currently stored calibration |
| **SHIFT + SAVE** | Calibrate the two ADC/input channels only |
| **SHIFT + LOAD** | Calibrate the two DAC/output channels only |
| **SAVE inside a wizard** | Confirm the displayed wiring/voltage step and continue |
| **LOAD inside a wizard** | Cancel safely and return to maintenance idle |

The twelve note LEDs are used as voltage instructions. The maintenance wizard deliberately distinguishes **fit points** from **verification points**:

- **C#** means **1.000 V** - lower affine fit point;
- **A** means **9.000 V** - upper affine fit point;
- **C** (12 o'clock) means **0.000 V** - independent zero sanity check;
- **F** means **5.000 V** - independent mid-scale verification point.

The small Channel A/B status LEDs identify which CV path is being measured. When a loopback is requested, the matching **IN and OUT status LEDs blink together**.

## Power safety - read before calibration

> [!CAUTION]
> **Never connect normal Eurorack bus power and the Nano USB connection at the same time on this module.** The Eurorack +5 V rail and USB-derived +5 V must not be allowed to drive the module simultaneously.

The safe workflow deliberately separates flashing/reporting from actual analog calibration:

```text
1. Eurorack case OFF
2. Disconnect the module power ribbon
3. Connect USB
4. Flash the maintenance firmware
5. Disconnect USB completely
6. Reconnect the normal Eurorack ribbon
7. Power the case ON
8. Perform calibration from the front panel only
9. Power the case OFF
10. Disconnect the Eurorack ribbon
11. Connect USB again
12. Read the stored report and/or flash the production firmware
```

During the **actual analog measurements**, the module must be powered exactly as it is in normal use: from the regular Eurorack `+12 V / -12 V / +5 V / GND` bus connection, with **USB physically disconnected**. This ensures that the DAC reference and analog input/output stages operate under their normal supply conditions.

During **USB flashing or report reading**, the Eurorack ribbon must be disconnected.

## Flashing the maintenance image

For a current Nano bootloader:

```text
pio run -e nanoatmega328new_calibration -t upload
```

For an older Nano bootloader:

```text
pio run -e nanoatmega328_calibration -t upload
```

The normal bootloader upload writes program flash only. It must not be replaced with a chip-erase or EEPROM-upload operation. The calibration profile lives in EEPROM and is intended to survive normal firmware-image swaps.

After the maintenance image has been flashed, **disconnect USB before reconnecting Eurorack power**.

## Maintenance-mode visual language

A successful maintenance boot is intentionally unmistakable:

| Indication | Meaning |
|---|---|
| Full note ring flashes **amber three times** | Maintenance firmware booted |
| Full ring remains **dim amber** | Maintenance idle / ready |
| One note pulses **green** | Apply the voltage represented by that note and press SAVE |
| Matching channel **Input LED** on | ADC/input channel being measured |
| Matching **Input + Output LEDs blink together** | Patch that channel's `CV OUT -> CV IN` loopback |
| Both channels' **Input + Output LEDs blink** | Patch both loopbacks for final verification |
| Note ring **off** during acquisition | Analog measurement is in progress |
| **Two green** full-ring flashes | Non-qualification operation passed |
| **Two red** full-ring flashes | Operation failed; an error code follows |
| **Three green** full-ring flashes | Complete verification/qualification passed |
| **Three red** full-ring flashes | Complete verification/qualification failed |

The ring is deliberately dark while analog samples are captured so changing TLC5947 PWM load does not add avoidable switching activity during the measurement window.

## Preparing the ADC reference voltages

### 0.000 V

Zero volts is used only as an **independent sanity check**, not as an affine fit point. That distinction matters: a small negative analogue offset can already be clipped to ADC code 0, so fitting against the lower rail can falsely appear to have zero offset.

The best 0 V reference is the module's **own local ground**. Do not rely on a bench supply set to `0.000 V` when a direct ground reference is available.

Use a TS jack adapter or shorting plug so that the CV input sees:

```text
TIP ----+
        +---- local 0 V
SLEEVE -+
```

A 3.5 mm TS breakout with screw terminals is useful: short TIP to SLEEVE for the 0 V verification step, and use the same adapter for the positive reference voltages.

### 1.000 V, 5.000 V and 9.000 V

The affine ADC correction is fitted from **1.000 V and 9.000 V**. Both points are intentionally inside the converter's linear range:

- using **1.000 V instead of 0 V** exposes input-path offset that could otherwise be hidden by clipping at ADC code 0;
- using **9.000 V instead of 10.000 V** preserves useful headroom below full scale and avoids hiding an over-range condition once the ADC has saturated at code 1023.

After the fit, **0.000 V** is checked as a lower-rail sanity point and **5.000 V** is measured as an independent mid-scale verification point. The 5 V measurement is not used to compute the fit, so it can reveal non-linearity or a poor two-point solution.

At runtime, raw ADC code `0` is explicitly anchored to calibrated code `0`. This boundary rule is intentional: if a small negative analog offset is clipped by the converter, the 1 V / 9 V fit can correctly infer a positive inverse offset, but applying that offset blindly at raw code 0 would create a false positive CV floor.

A laboratory supply is suitable for the 1 V, 5 V and 9 V references when used carefully:

1. Use one normal positive output channel, not a series/tracking dual-rail configuration.
2. Set the required voltage with the output disabled.
3. Set a conservative current limit, for example **5-10 mA**.
4. Connect supply **COM / -** to the adapter SLEEVE (module ground).
5. Connect supply **+** to the adapter TIP (CV).
6. Verify polarity before enabling the output.
7. Measure the actual voltage at the adapter with a trustworthy DMM and trim the supply to **1.000 V, 5.000 V or 9.000 V based on the DMM**, not merely the supply display.
8. Only then press SAVE to capture the displayed point.

> [!WARNING]
> Apply the reference voltage only to the module's **CV input jack**. Never apply these voltages directly to Arduino A6/A7 or another logic/MCU pin. The module input jack is followed by the intended protection/scaling hardware; the MCU pin is not a 10 V input.

A breakout adapter is strongly recommended because it gives a mechanically secure place for the bench-supply and DMM leads while retaining a normal Eurorack patch cable to the module.

## Complete calibration workflow - SAVE

Press **SAVE** from dim-amber maintenance idle to start the complete wizard. Release SAVE after the wizard begins; SAVE is then used to confirm each displayed step. LOAD cancels without committing the in-progress RAM profile.

### 1. ADC / CV IN A fit at 1 V

- Channel A **Input LED** is on.
- **C#** pulses green.
- Apply exactly **1.000 V** to CV IN A.
- Press **SAVE**.

The ring turns off while the firmware captures repeated ADC samples. An unstable input is rejected.

### 2. ADC / CV IN A fit at 9 V

- Channel A **Input LED** remains the channel indicator.
- **A** pulses green.
- Apply exactly **9.000 V** to CV IN A.
- Press **SAVE**.

The firmware computes the Channel A affine ADC correction from the 1 V and 9 V means.

### 3. ADC / CV IN A zero sanity check

- **C** pulses green.
- Short CV IN A to local 0 V.
- Press **SAVE**.

This point is **not used for fitting**. The firmware checks that the raw ADC reading is still plausibly at the lower rail. This prevents a gross positive input offset from being silently accepted while avoiding the original mistake of deriving offset from a potentially clipped zero measurement.

### 4. ADC / CV IN A independent 5 V verification

- **F** pulses green.
- Apply exactly **5.000 V** to CV IN A.
- Press **SAVE**.

The firmware applies the newly fitted ADC correction and verifies the independent mid-scale point against the expected code. A failure here indicates that the two-point affine fit does not adequately describe the measured input path.

### 5-8. Repeat the four ADC steps for CV IN B

The same **C# / 1 V -> A / 9 V -> C / 0 V -> F / 5 V** sequence is repeated with the Channel B Input LED.

After the B-channel 5 V verification, disable and disconnect the external voltage source before moving to loopback calibration.

### 9. DAC / CV OUT A self-calibration

The Channel A **Input and Output status LEDs blink together**.

Patch:

```text
CV OUT A -> CV IN A
```

Press **SAVE**. The firmware drives several DAC values, measures them through the already calibrated Channel A ADC path, fits the inverse output correction, and independently verifies the result. The ADC calibration remains the measurement reference; loopback self-calibration does not create an independent laboratory voltage standard.

### 10. DAC / CV OUT B self-calibration

Move or add the loopback so that:

```text
CV OUT B -> CV IN B
```

Press **SAVE** when the B IN/OUT indicators blink.

### 11. Final dual-channel verification

Both channels' **IN and OUT status LEDs blink**. Patch both loopbacks simultaneously:

```text
CV OUT A -> CV IN A
CV OUT B -> CV IN B
```

Press **SAVE**. Both channels are verified at independent DAC points.

Only after this final verification passes are the new calibration coefficients committed to the redundant EEPROM calibration journal. The associated qualification residuals are stored only for that exact journal generation.

A complete PASS is indicated by **three green full-ring flashes**. The firmware then returns to dim-amber idle.

## ADC-only calibration - SHIFT + SAVE

Use this after work on the CV input/ADC path when the DAC side does not need to be recalibrated immediately.

The wizard performs eight ADC steps:

1. A fit at 1.000 V
2. A fit at 9.000 V
3. A sanity check at 0.000 V
4. A independent verification at 5.000 V
5. B fit at 1.000 V
6. B fit at 9.000 V
7. B sanity check at 0.000 V
8. B independent verification at 5.000 V

The resulting profile is saved after both channels pass. Because changing the ADC reference invalidates any earlier DAC qualification, the previous qualification certificate automatically becomes invalid through the calibration-generation mismatch. Run DAC-only calibration or a full calibration before applying a new calibration label.

A successful ADC-only operation ends with **two green flashes**.

## DAC/self-calibration only - SHIFT + LOAD

This mode requires an already valid stored ADC calibration profile. It performs:

1. Channel A `OUT -> IN` fit and verification
2. Channel B `OUT -> IN` fit and verification
3. final dual-channel verification with both loopbacks patched
4. atomic persistence of the new DAC profile and its generation-bound qualification metadata

A complete PASS ends with **three green flashes**.

If no valid stored calibration profile exists, the operation fails with error code 6.

## Verify an existing calibration - LOAD

Press **LOAD** from idle to verify the currently stored profile without changing its ADC/DAC coefficients.

Both IN/OUT pairs blink. Patch both loopbacks and press SAVE. The firmware re-runs the independent DAC verification points for A and B. On PASS, only the compact qualification metadata for the **current** calibration generation is refreshed; the calibration journal itself is not rewritten.

A successful verification ends with **three green flashes**.

## Canceling safely

Press **LOAD** during any wizard prompt to cancel. Until a workflow has passed its required final verification, newly fitted parameters live only in RAM. Canceling or a measurement failure restores the previously active profile and leaves the last valid persistent calibration record usable.

## Error codes

A failure is shown as **two red full-ring flashes**, followed by a static group of red note LEDs. Count the illuminated notes:

| Red LEDs | Meaning |
|---:|---|
| **1** | Input unstable or ADC acquisition timeout |
| **2** | ADC 1 V/9 V fit, 0 V sanity check, or independent 5 V verification is invalid |
| **3** | Loopback missing, wrong, or non-monotonic |
| **4** | Loopback range/gain/fit is implausible |
| **5** | Independent DAC verification residual is too large |
| **6** | Required persistent profile unavailable or persistence operation failed |

Press **LOAD** to acknowledge the error and return to dim-amber idle.

## Stored calibration report and module label

The maintenance firmware has **no serial command console**. Serial is read-only and used only to print the most recently stored calibration/qualification record.

After a successful calibration:

1. Power the Eurorack case OFF.
2. Disconnect the module's Eurorack ribbon.
3. Connect the Nano by USB.
4. Open a serial monitor at **115200 baud**. Opening the port normally resets the Nano, so the maintenance image prints the stored report during startup.

Example:

```text
FM QUANTIZER STORED CALIBRATION REPORT
Firmware target: 0.2.2 (Unreleased)
Calibration format: 1
Generation: 7
ADC A: offset=-3 gain=.../...
ADC B: offset=-4 gain=.../...
DAC A: offset=... gain=.../...
DAC B: offset=... gain=.../...
DAC A verification: PASS / max residual 2 codes
DAC B verification: PASS / max residual 1 codes
OVERALL: PASS
----- LABEL -----
CALIBRATION VERIFIED
ADC A/B: STORED PROFILE
DAC A: PASS / max 2 codes
DAC B: PASS / max 1 codes
CAL fmt 1 / gen 7
-----------------
```

The USB-only report does **not** perform a fresh analog measurement: the +/-12 V analog stages are intentionally not powered in this state. It reports the residuals captured by the last successful panel verification and accepts them only when their stored generation matches the newest valid calibration record.

If no matching verified qualification exists, the firmware explicitly prints that no calibration label may be applied.

Date, module serial number, and tester identity are not generated by the Nano because there is no real-time clock. Add those as host-side or handwritten label metadata.

## EEPROM layout and isolation

The ATmega328P provides 1024 bytes of EEPROM. The current `Unreleased` layout is intentionally partitioned:

| EEPROM bytes | Purpose |
|---:|---|
| `0..503` | User scale/full-configuration presets |
| `504..965` | 11-slot wear-leveled live-state ring |
| `966` | Qualification commit marker / legacy <=0.2.1 live-slot marker during migration |
| `967..1022` | Two redundant 28-byte calibration records |
| `1023` | Packed A/B qualification residuals |

The writable `CalibrationStore` receives an `EepromPartition` that exposes only bytes `967..1022`. It cannot address presets or live state. Qualification metadata is handled by a separate fixed-address component that can touch only bytes 966 and 1023.

A direct upgrade from <=0.2.1 can still contain the newest live-state record in the former twelfth slot beginning at byte 966. The maintenance image completes that migration into the current 11-slot ring before byte 966 is ever reused as qualification metadata.

Calibration records are redundant, CRC-protected, versioned, and committed by writing their marker last. Qualification metadata uses the same principle: its marker is invalidated first, residuals are written, and the generation-bound marker is committed last. A qualification written for a future calibration generation remains invalid until the matching calibration journal record itself commits.

## What loopback self-calibration can and cannot correct

The loopback path is:

```text
DAC -> output amplifier -> CV OUT -> patch cable -> CV IN -> input stage -> ADC
```

The active ADC calibration is treated as the measurement reference. Self-calibration therefore corrects the **DAC/output affine error** against that reference. It cannot mathematically separate simultaneous absolute errors in the input and output paths without an independent voltage reference.

This is why the ADC side is calibrated first from external 1.000 V / 9.000 V fit references and then checked independently at 0.000 V and 5.000 V.

The current implementation deliberately stores affine offset/gain correction only. It does not reserve EEPROM for a 121-note residual table. A non-linear residual correction should be introduced only if real hardware measurements demonstrate that it is justified.

## Recommended independent post-calibration check

The stored PASS report proves that the panel workflow completed and that the DAC loopback verification agreed with the calibrated ADC reference. It is **not an independent laboratory measurement**, because the loopback uses the module's own ADC as its measuring instrument.

For a newly built module, or after changing the analog path, perform one external DMM check after restoring the production firmware:

1. Use neutral Quantizer settings: Track-and-Hold, Glide 0, Delay 0, all shifts 0, Channel B absolute, channels unlinked.
2. Enable **C only** in the scale.
3. Apply accurately measured input references such as `0.000 V`, `1.000 V`, `3.000 V`, `5.000 V`, `7.000 V`, and `9.000 V`.
4. Measure the corresponding CV output with a second breakout/test adapter and the same trustworthy DMM.
5. Record input, expected output, measured output, and error.

With C as the only enabled pitch class, whole-volt inputs should quantize to the same whole-volt output. A nearly constant error suggests residual offset; an error that grows with voltage suggests residual gain error; irregular point-to-point error suggests non-linearity or measurement noise.

This external check is recommended for hardware qualification and module labeling, but it is not required after every EEPROM-preserving firmware update.

## Returning to production firmware

After calibration/reporting:

1. Keep the Eurorack ribbon disconnected.
2. Flash the production image via normal USB:

```text
pio run -e nanoatmega328new -t upload
```

or, for an old bootloader:

```text
pio run -e nanoatmega328 -t upload
```

3. Disconnect USB.
4. Reconnect the normal Eurorack ribbon.
5. Power the case ON.

The production firmware has no calibration writer, calibration state machine, or serial shell. It only reads the newest valid calibration journal record during boot and applies that profile to the real-time ADC/DAC conversion path.

## Checklist for a newly built module

- [ ] Maintenance image flashed with the Eurorack ribbon disconnected.
- [ ] USB physically disconnected before normal Eurorack power is restored.
- [ ] 1.000 V, 5.000 V and 9.000 V references measured at the jack adapter with a DMM.
- [ ] 0 V sanity reference made by shorting CV TIP to local GND/SLEEVE.
- [ ] ADC A and B complete without error.
- [ ] DAC A and B loopback self-calibration complete without error.
- [ ] Both loopbacks are connected for final verification.
- [ ] Three green flashes confirm full qualification PASS.
- [ ] Case powered OFF and ribbon disconnected before USB is connected again.
- [ ] Stored USB report says `OVERALL: PASS` for the active generation.
- [ ] Production image flashed without EEPROM erase.
