# Calibration guide

This guide describes the practical calibration workflow for the Free Modular Quantizer C++/PlatformIO firmware. Calibration is intentionally split into **LED**, **CV input**, **DAC output**, and **button-ladder** checks because these depend on the actual assembled hardware rather than firmware alone.

The firmware does not invent analogue correction values. Measurements should be taken on the real module and then transferred into the corresponding configuration constants.

## What you need

For basic calibration:

- the assembled module and a stable Eurorack power supply;
- a reliable digital multimeter;
- a known CV source or precision voltage source for input calibration;
- a computer with a serial terminal at **115200 baud** for the boot-time calibration console.

For higher-confidence verification, an oscilloscope and a precision voltage reference are useful but not mandatory.

## 1. LED brightness calibration

### Why this must be calibrated empirically

The red and green channels of the bi-colour LEDs can differ dramatically in perceived brightness. The result depends on the fitted LED type, forward voltage, optical efficiency, viewing angle, diffuser/button cap and series resistors.

The current hardware, for example, uses very different resistor values for the two colour channels. Therefore equal numerical PWM values do **not** imply equal visible brightness. There is no reliable formula that will produce a final red/green/amber balance for every build.

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

The display is a filled clockwise bar: every LED from 12 o'clock through the selected position is illuminated in the colour currently being calibrated.

### Practical procedure

1. Enter calibration mode.
2. Start with Channel A / green.
3. Select progressively higher steps until green has the desired normal operating brightness.
4. Switch to Channel B / red with `SHIFT + B`.
5. Adjust red until its perceived brightness is balanced with green.
6. Switch between both colours several times to compare them directly.
7. Check an amber indication in normal operation or during the startup colour fade. Amber should read visually as a deliberate red/green mixture rather than almost pure red or green.
8. Repeat the red/green adjustments if necessary. This normally takes several iterations.
9. Hold **SHIFT alone for 5 seconds** to save and leave calibration.

Do not optimise only for maximum brightness. The target is a readable, comfortable normal level with a convincing amber mixture and sufficient headroom.

## 2. Boot-time hardware calibration console

Hold **SHIFT while powering on**. After the startup LED self-test, the firmware enters the serial calibration console instead of normal operation.

Serial settings:

- **115200 baud**

Commands:

| Command | Function |
|---|---|
| `r` | Print current button-ladder, CV-A and CV-B ADC codes |
| `s` | Toggle a 10 Hz ADC stream |
| `z` | Set both DACs to raw code 0 |
| `m` | Set both DACs to raw code 2048 |
| `f` | Set both DACs to raw code 4095 |
| `[` / `]` | Decrement / increment DAC A by one raw code |
| `{` / `}` | Decrement / increment DAC B by one raw code |
| `?` | Print abbreviated help |

The console is enabled by `kCalibrationConsoleEnabled` in `RuntimeConfig.h`.

## 3. Resistor-ladder verification

The original PCB uses the proven Rust-firmware ladder reference values:

```text
0, 93, 171, 236, 292, 341, 384, 421, 455, 485, 512, 536, 558
```

The final value represents the released/no-button state. The original-board profile uses these absolute values with a nearest-value decoder and 64 ms debounce.

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

Calibration is represented as an integer affine correction:

1. input offset;
2. gain numerator / denominator.

### Minimum two-point procedure

1. Connect a precise **0 V** source to CV input A.
2. Record the ADC value using the calibration console.
3. Apply a second accurate reference, preferably **5 V**.
4. Record the second ADC value.
5. Derive the required offset and gain correction.
6. Enter the values for Channel A in `AnalogConfig.h`.
7. Repeat independently for Channel B.

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

### Procedure

1. Enter the boot-time calibration console.
2. Use `z` to command raw DAC code **0**.
3. Measure both CV outputs accurately.
4. Use `f` to command raw DAC code **4095** and measure again.
5. `m` provides code **2048** as a useful midpoint check.
6. Adjust the DAC gain and offset values in `AnalogConfig.h` independently for A and B.
7. Rebuild and flash the firmware.
8. Repeat the measurements until the error is acceptably small.

The `[` / `]` and `{` / `}` commands allow single-code adjustment when investigating the analogue stage or confirming DAC monotonicity.

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
- [ ] CV input A tracks accurately across several volts;
- [ ] CV input B tracks accurately across several volts;
- [ ] DAC output A is calibrated at low, midpoint and high values;
- [ ] DAC output B is calibrated at low, midpoint and high values;
- [ ] several octave intervals have been checked musically;
- [ ] Track-and-Hold and Sample-and-Hold have both been verified with real gates.
