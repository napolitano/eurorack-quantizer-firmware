# Calibration guide

This guide describes the practical calibration workflow for the Free Modular Quantizer C++/PlatformIO firmware. Calibration is intentionally split into **LED**, **CV input**, **DAC output**, and **button-ladder** checks because these depend on the actual assembled hardware rather than firmware alone.

> [!IMPORTANT]
> Calibration values are hardware-specific. Do not copy another build's LED, CV, DAC, or ladder values blindly; measure the actual module and verify the result electrically and musically.

The firmware does not invent analogue correction values. Measurements should be taken on the real module and then transferred into the corresponding configuration constants.

## Contents

- [What you need](#what-you-need)
- [Upstream context: calibration is an extension](#upstream-context-calibration-is-an-extension)
- [1. LED brightness calibration](#1-led-brightness-calibration)
- [2. Boot-time hardware calibration console](#2-boot-time-hardware-calibration-console)
- [3. Resistor-ladder verification](#3-resistor-ladder-verification)
- [4. CV input calibration](#4-cv-input-calibration)
- [5. DAC output calibration](#5-dac-output-calibration)
- [6. Musical verification](#6-musical-verification)
- [7. Automatic ladder-rest check](#7-automatic-ladder-rest-check)
- [8. Runtime diagnostics](#8-runtime-diagnostics)
- [Calibration checklist](#calibration-checklist)

## What you need

For basic calibration:

- the assembled module and a stable Eurorack power supply;
- a reliable digital multimeter;
- a known CV source or precision voltage source for input calibration;
- a computer with a serial terminal at **115200 baud** for the boot-time calibration console.

For higher-confidence verification, an oscilloscope and a precision voltage reference are useful but not mandatory.

## Upstream context: calibration is an extension

Quinn Freedman's original Rust firmware is the behavioural and hardware reference for this project, but it does **not** implement per-channel software offset/gain correction for the CV ADC inputs or MCP4922 DAC outputs. The upstream signal path converts the 10-bit ADC reading directly into the 0…120-semitone domain and converts the resulting semitone value directly into a 12-bit DAC code. The shared upstream `fm-lib` MCP4922 driver then writes the supplied DAC code without another calibration layer.

The original firmware also does not contain the boot-time serial calibration console described in this document. `AnalogConfig.h`, the per-channel ADC/DAC affine corrections, and the RAW/CAL verification console are extensions of this C++/PlatformIO firmware.

> [!NOTE]
> Do not try to derive `AnalogConfig.h` semantics from the upstream firmware: there are no equivalent offset/numerator/denominator constants there. Use the RAW measurements from this firmware to characterize the assembled hardware, then use CAL output to verify the correction applied by this firmware.

Upstream reference sources:

- Quantizer firmware: <https://github.com/QuinnFreedman/modular/tree/main/modules/Quantizer/Firmware>
- Quantizer `main.rs`: <https://github.com/QuinnFreedman/modular/blob/main/modules/Quantizer/Firmware/src/main.rs>
- shared `fm-lib`: <https://github.com/QuinnFreedman/modular/tree/main/fm-lib>
- MCP4922 driver: <https://github.com/QuinnFreedman/modular/blob/main/fm-lib/src/mcp4922.rs>

## 1. LED brightness calibration

### Why this must be calibrated empirically

The red and green channels of the bi-colour LEDs can differ dramatically in perceived brightness. The result depends on the fitted LED type, forward voltage, optical efficiency, viewing angle, diffuser/button cap and series resistors.

The current hardware, for example, uses very different resistor values for the two colour channels. Therefore equal numerical PWM values do **not** imply equal visible brightness. There is no reliable formula that will produce a final red/green/amber balance for every build.

> [!NOTE]
> LED PWM values are starting points, not universal calibration constants. Perceived red/green balance depends strongly on the fitted LEDs, resistors, optics, and viewing conditions.

**LED brightness must therefore be adjusted empirically and iteratively on the actual module.** The values shipped in the firmware are starting values only.

### Entering LED calibration

Hold:

**SHIFT + LOAD + SAVE for 5 seconds**

The calibration interface uses the twelve note positions as a linear representation of the complete 12-bit TLC5947 PWM range.

- Channel A represents **green**.
- Channel B represents **red**.
- Channel A / green is selected initially.
- `SHIFT + A` selects green.
- `SHIFT + B` selects red.

### Calibration scale

The ring spans the real hardware range **0…4095 inclusive**:

- 12 o'clock / first position = **0**, true off;
- final clockwise position = **4095**, the TLC5947 maximum;
- the ten intermediate positions divide the range into eleven equal intervals.

If the current configured PWM value falls between two displayed steps, the firmware selects the **nearest** step.

During active calibration the ring deliberately stops behaving like a numeric bar. Instead it shows a repeating **green / red / amber** comparison pattern around all twelve positions. The currently stored red and green PWM levels are applied live, and amber uses both at the same time. This makes the actual balance visible while either emitter is adjusted instead of forcing the builder to remember how the other colour looked.

After a note button selects a new step, that ring position is shown briefly as a dark gap. The gap is only a position marker; the full green/red/amber comparison pattern returns automatically. The two discrete status LEDs for Channel A are lit while **green** is being edited, and the Channel B pair is lit while **red** is being edited.

### Practical procedure

1. Enter calibration mode.
2. Start with Channel A / green. The Channel A status-LED pair identifies the active editor.
3. Choose a note position for the green PWM step while watching the green, red and amber samples together.
4. Switch to Channel B / red with `SHIFT + B`; the Channel B status-LED pair now identifies the active editor.
5. Adjust red while judging both the direct red/green balance and the amber mixture.
6. Move back to green with `SHIFT + A` if another correction is useful. Because all three colours stay visible, repeated A/B switching is no longer required merely to compare brightness.
7. Hold **SHIFT alone for 5 seconds** to save and leave calibration.

Do not optimise only for maximum brightness. The target is a readable, comfortable normal level with a convincing amber mixture and sufficient headroom.

## 2. Boot-time hardware calibration console

Hold **SHIFT while powering on**. After the startup LED self-test, the firmware enters the serial calibration console instead of normal operation.

Serial settings:

- **115200 baud**

The console prints the active ADC/DAC calibration constants when it starts and deliberately distinguishes two signal paths:

- **RAW** values are the uncorrected hardware ADC/DAC codes. Use them to derive offset and gain corrections.
- **CAL** values pass through the exact same calibration helpers used by normal quantizer operation. Use them to verify the values compiled into `AnalogConfig.h`.

> [!IMPORTANT]
> Changing `AnalogConfig.h` does not change the **RAW** readings. That is intentional: RAW is the measurement of the hardware itself. Use the **CAL** view to confirm that the firmware correction is actually being applied.

Commands:

| Command | Function |
|---|---|
| `r` | Print current RAW ADC readings plus calibrated CV-A/CV-B codes |
| `s` | Toggle a 10 Hz RAW+CAL ADC stream |
| `c` | Print the active ADC and DAC offset/gain constants |
| `z` | Set both DACs to **RAW** code 0 |
| `m` | Set both DACs to **RAW** code 2048 |
| `f` | Set both DACs to **RAW** code 4095 |
| `Z` | Send nominal code 0 through the normal runtime DAC calibration and output the corrected codes |
| `M` | Send nominal code 2048 through the normal runtime DAC calibration and output the corrected codes |
| `F` | Send nominal code 4095 through the normal runtime DAC calibration and output the corrected codes |
| `[` / `]` | Decrement / increment DAC A by one **RAW** hardware code |
| `{` / `}` | Decrement / increment DAC B by one **RAW** hardware code |
| `?` | Print abbreviated help |

Example ADC output:

```text
ADC RAW ladder=558 A=510 B=508
ADC CAL A=512 B=511
```

Example calibrated DAC verification:

```text
DAC CAL A=2048->2057 B=2048->2046
```

The value before `->` is the nominal runtime DAC code. The value after `->` is the corrected code actually written to the MCP4922.

The console is enabled by `kCalibrationConsoleEnabled` in `RuntimeConfig.h`.

## 3. Resistor-ladder verification

The original PCB uses the proven Rust-firmware ladder reference values:

```text
0, 93, 171, 236, 292, 341, 384, 421, 455, 485, 512, 536, 558
```

The final value represents the released/no-button state. The original-board profile uses these absolute values with 64 ms debounce. Decoding first finds the nearest nominal key, then accepts it only when the reading is within ±10 ADC counts; implausible gap values are treated as no button. The released/high region begins at ADC 548.

### Verification procedure

1. Enter the boot-time calibration console.
2. Use `s` to enable the ADC stream.
3. Record the released ladder value.
4. Press each of the twelve note buttons individually and record its stable ADC value.
5. Repeat the complete sequence at least twice.
6. Verify that every button forms a clearly distinguishable cluster around its expected value.

If a custom PCB, different resistor network or different ADC reference is used, adjust the ladder configuration in:

```text
lib/fmq/include/fmq/config/AnalogConfig.h
```

Do not change the original-board values merely to hide unstable hardware. Large or inconsistent deviations should first be investigated electrically.

## 4. CV input calibration

The ADC path supports independent correction for Channel A and Channel B.

The normal runtime applies ADC calibration in this order:

1. add the input offset;
2. clamp to the 10-bit ADC range;
3. apply the integer gain ratio;
4. round to the nearest code and clamp again.

In compact form:

```text
CAL = clamp(round((RAW + offset) * numerator / denominator), 0, 1023)
```

> [!NOTE]
> The full 0–10 V range maps to ADC codes 0…1023. The mathematical 5 V midpoint is therefore **511.5**, not exactly 512. Integer calibration ratios can represent that half-code relationship without pretending that the ADC itself has half-code resolution.

### Minimum two-point procedure

1. Connect a precise **0 V** source to CV input A.
2. Record the **RAW** ADC value using the calibration console.
3. Apply a second accurate reference, preferably **5 V**.
4. Record the second **RAW** ADC value.
5. Derive the required offset and gain correction.
6. Enter the values for Channel A in `AnalogConfig.h`.
7. Rebuild and flash the firmware.
8. Return to the console and use `r` or `s` to compare **RAW** and **CAL** values directly.
9. Repeat independently for Channel B.

For example, if Channel A measures:

```text
0 V -> RAW 7
5 V -> RAW 510
```

the zero-point correction is:

```text
offset = -7
```

After removing the offset, the measured 0–5 V span is 503 codes. A precise integer representation of the required midpoint correction is:

```text
gainNumerator   = 1023
gainDenominator = 1006
```

because `1023 / 1006` represents `511.5 / 503`. With those constants compiled in, a RAW value near 510 should appear as a CAL value near 512.

### Recommended verification points

After applying the correction, check several points across the intended range, for example:

```text
0 V
1 V
2 V
3 V
4 V
5 V
```

For 1 V/octave use, errors should not accumulate significantly across octaves. If the middle of the range is correct but the ends drift, the gain still needs adjustment. If the error is nearly constant everywhere, the offset is the more likely cause.

## 5. DAC output calibration

DAC A and DAC B also have independent gain and offset correction.

The DAC correction intentionally uses the opposite affine order from the ADC path:

1. apply the integer gain ratio to the nominal DAC code;
2. add the output offset;
3. round and clamp to the 12-bit DAC range.

```text
CORRECTED = clamp(round(NOMINAL * numerator / denominator) + offset, 0, 4095)
```

> [!IMPORTANT]
> Lowercase `z` / `m` / `f` are **RAW hardware tests** and bypass the configured DAC correction. Uppercase `Z` / `M` / `F` use the same calibrated-code function as normal runtime output. Use RAW commands to characterize the analogue path and CAL commands to verify the compiled correction.

### Procedure

1. Enter the boot-time calibration console.
2. Use `z` to command **RAW** DAC code 0 and measure both CV outputs accurately.
3. Use `f` to command **RAW** DAC code 4095 and measure again.
4. Use `m` as a midpoint linearity check at RAW code 2048.
5. Derive the per-channel DAC gain and offset values and enter them in `AnalogConfig.h`.
6. Rebuild and flash the firmware.
7. Use `Z`, `M` and `F` to verify the corrected runtime-equivalent output points.
8. Repeat the measurements until the error is acceptably small.

The `[` / `]` and `{` / `}` commands always operate on **RAW** codes and remain useful when investigating the analogue stage or confirming DAC monotonicity one hardware code at a time.

> [!CAUTION]
> Software calibration cannot recover headroom beyond the physical 12-bit range. If the required corrected code would exceed 4095, it clips at 4095. The same applies below code 0.

## 6. Musical verification

After electrical calibration, perform a musical check rather than relying only on raw voltages.

1. Use a chromatic scale.
2. Feed a known 1 V/octave source into Channel A.
3. Check octave intervals across the usable voltage range with a tuner or frequency counter.
4. Repeat for Channel B.
5. Select a sparse scale and sweep slowly across note boundaries to verify stable quantization and hysteresis.
6. Verify Track-and-Hold with an unpatched sample input; the original hardware normalises it HIGH and should therefore quantize continuously.
7. Patch a gate and verify Track-and-Hold and Sample-and-Hold independently.

## 7. Automatic ladder-rest check

During normal startup the firmware evaluates fresh asynchronous ADC snapshots. A rest calibration is accepted only when the measured value is stable and lies inside the configured rest interval.

If this check fails, the complete note ring flashes red three times. The firmware then continues with the nominal ladder value instead of accepting a likely pressed button or unstable input as a reference.

## 8. Runtime diagnostics

Set:

```cpp
kDiagnosticsEnabled = true;
```

in `RuntimeConfig.h` to enable periodic serial diagnostics.

The report includes:

- processed 1 kHz control ticks;
- probable missed deadlines;
- maximum pending-tick queue depth;
- ticks that reused an older ADC snapshot;
- UI-state-machine executions;
- attempted LED refreshes.

Diagnostics are disabled by default because serial traffic can itself affect real-time behaviour.

## Calibration checklist

Before considering a module calibrated:

- [ ] all twelve note buttons decode reliably;
- [ ] green brightness is comfortable and clearly visible;
- [ ] red brightness is balanced against green;
- [ ] amber is visually distinct from both red and green;
- [ ] RAW and CAL console readings are clearly understood and the compiled ADC correction has been verified;
- [ ] CV input A tracks accurately across several volts;
- [ ] CV input B tracks accurately across several volts;
- [ ] RAW DAC measurements and CAL runtime-equivalent verification agree with the intended correction;
- [ ] DAC output A is calibrated at low, midpoint and high values;
- [ ] DAC output B is calibrated at low, midpoint and high values;
- [ ] several octave intervals have been checked musically;
- [ ] Track-and-Hold and Sample-and-Hold have both been verified with real gates.

---

<p align="center">From Munich With <img src="docs/assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
