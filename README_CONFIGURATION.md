# Firmware configuration

Configuration is split by responsibility under `lib/fmq/include/fmq/config/`:

- `ProductConfig.h` — factory scale, sample mode, quantizer limits and optional product behaviour.
- `UiConfig.h` — debounce, long-press and menu timing.
- `AnalogConfig.h` — resistor-ladder model, ADC behaviour and per-channel ADC/DAC calibration.
- `LedConfig.h` — normal LED brightness and startup/self-test timing.
- `PersistenceConfig.h` — EEPROM/autosave timing.
- `RuntimeConfig.h` — task dividers, diagnostics and calibration-console timing.
- `FirmwareConfig.h` — convenience umbrella include for the configuration above.
- `src/platform/nano_atmega328p/BoardConfig.h` — selected board profile, pin map, ADC-reference choice and electrical polarities.

Major ranges are guarded by compile-time assertions where practical.

## Contents

- [Product defaults](#product-defaults)
- [Signal-processing model](#signal-processing-model)
- [ADC reference and original PCB](#adc-reference-and-original-pcb)
- [Resistor ladder and ADC](#resistor-ladder-and-adc)
- [UI timing](#ui-timing)
- [Normal LED brightness](#normal-led-brightness)
- [Startup sequences](#startup-sequences)
- [Runtime and diagnostics](#runtime-and-diagnostics)
- [Factory full-config fallbacks](#factory-full-config-fallbacks)
- [Arpeggiator layer](#arpeggiator-layer)
- [EEPROM layout and live state](#eeprom-layout-and-live-state)
- [AVR resource budget](#avr-resource-budget)


## Product defaults

`kFactoryScaleMask` defaults to `0x0FFF`, a chromatic scale, so a blank module is immediately usable.

`kFactorySampleMode` defaults to **Track-and-Hold** (`0`), matching the Rust original. SHIFT+4 normally toggles only Track-and-Hold and Sample-and-Hold. `kEnableContinuousSampleModeInUi` is `false` by default; setting it to `true` exposes the C++ Continuous extension in the front-panel cycle.

Glide and trigger-delay limits live in `ProductConfig.h` as `kMaxGlideAmount` and `kMaxTriggerDelayAmount` rather than being repeated as literals in processing and persistence code.

## Signal-processing model

The core per-channel pitch path is intentionally ordered. Pre-shift affects which note is selected, Scale-shift moves within the enabled scale after quantization, Post-shift applies a chromatic offset to that result, and Glide affects only the transition to the final CV target.

```mermaid
flowchart LR
    IN[CV input] --> SAMPLE[Track / Sample stage]
    SAMPLE --> PRE[Pre-shift]
    PRE --> Q[Quantize + hysteresis]
    Q --> SCALE[Scale-shift]
    SCALE --> POST[Post-shift]
    POST --> GLIDE[Glide]
    GLIDE --> DAC[DAC output]
```

### Hysteresis thresholds

After a real previous note $L$ exists, the hold band is defined from the next enabled lower and upper notes $D$ and $U$. With the configured hysteresis amount $H = 0.4$ semitone, the implementation follows:

```math
T_{upper} = L + \frac{U-L}{2} + H
```

```math
T_{lower} = L - \frac{L-D}{2} - H
```

A fresh channel deliberately skips this hold band until its first actual note has been emitted, so an exact half-semitone tie keeps the normal upward-rounding rule.

### Glide recurrence

For Glide amount $g$, the Q8.24 integrator uses the discrete update below on every processing tick, with integer fixed-point arithmetic and a one-ULP progress guarantee when rounding would otherwise produce zero movement:

```math
y_{n+1} = y_n + 2^{-g}(t-y_n)
```

At $g=0$, the multiplier is 1 and the output jumps directly to the target. Increasing $g$ produces progressively slower convergence.

## ADC reference and original PCB

The original Rust `main.rs` initially constructs an ADC with an AREF setting, but `fm-lib::async_adc::init_async_adc()` subsequently writes `ADMUX.REFS=AVCC`. The effective reference of the original firmware is therefore **AVCC**, and the original board profile follows that behaviour.

Any future PCB revision using a true external reference should get its own verified board profile rather than silently changing the original profile.

## Resistor ladder and ADC

The original PCB uses the proven Rust ladder values
`[0, 93, 171, 236, 292, 341, 384, 421, 455, 485, 512, 536, 558]` and 64 ms debounce. The original-board profile disables rest normalization. A nearest-value candidate is accepted only within ±10 ADC counts of its nominal key value; readings in the gaps are treated as no button. Values from the documented rest floor upward are also no button.

The AVR ADC scanner runs interrupt-driven at 125 kHz (prescaler 128), explicitly starts conversions, discards the first conversion after a MUX change and uses a median-of-three filter to match the effective Rust `fm-lib` behaviour.

Per-channel analogue calibration is configured with integer affine corrections:

- ADC: offset, then gain numerator/denominator.
- DAC: gain numerator/denominator, then offset.

> [!NOTE]
> These software gain/offset corrections are an extension of this firmware. Quinn Freedman's original Rust Quantizer maps the ADC value directly into the semitone domain and the resulting pitch directly into a DAC code; the shared upstream MCP4922 driver writes that code without a per-channel calibration transform. The detailed measurement and verification workflow is documented in [README_CALIBRATION.md](README_CALIBRATION.md).

## UI timing

`UiConfig.h` contains all normal control timing, including:

- `kDigitalDebounceMs`
- `kLadderDebounceMs`
- `kLongPressMs`
- calibration entry/save hold durations
- blink timing
- save/erase confirmation timing

SHIFT remains an immediate modifier, matching the Rust input semantics.

## Normal LED brightness

The normal ring defaults are `kDefaultRedPwm` and `kDefaultGreenPwm` in `LedConfig.h`. Current empirically tuned starting values are:

```cpp
kDefaultRedPwm   = 0x0480;
kDefaultGreenPwm = 0x0D00;
```

### LED PWM values are empirical

> [!IMPORTANT]
> The default PWM values are **not universal brightness values**. They must be verified on the actual assembled module and adjusted when the fitted LEDs or resistor values differ.

These are **not universal brightness values**. Red and green must be tuned for the actual LEDs and series resistors on the assembled module. LED efficiency, forward voltage and resistor values can differ strongly; in the tested hardware the colour channels use very different series resistances, so numerically similar PWM values would not produce a visually balanced result.

There is no reliable formula that predicts the final perceived red/green/amber balance from the nominal component values alone. Final values therefore need to be determined **empirically and iteratively on the real module**. Adjust red and green independently until the individual colours and their amber combination look correct.

## Startup sequences

`LedConfig.h` contains all timing and behaviour values for the four cyclic boot
animations: `ColorFade`, `Glowworm`, `Cog` and `Sparkles`. The EEPROM selector
contains the sequence to play on the next boot; after playback it advances with
wraparound to sequence 0. Set `kRotateStartupSequences` to `false` to always use
sequence 0.

Relevant settings include `kStartupFadeInMs`, `kStartupFadeOutMs`,
`kStartupInterColorPauseMs`, `kGlowwormStepMs`, the Glowworm tail intensity
table, `kCogStepMs`, `kCogRotations`, `kSparklesDurationMs`,
`kSparklesFrameMs`, and the discrete status-LED timings.
`kStartupRingMaximumDurationMs` is a hard upper bound for the twelve-note ring
portion of every animation and is currently 1500 ms; compile-time assertions
reject a timing change that would exceed it. The separate four-status-LED
self-test is intentionally outside that ring-animation limit. All note-ring
effects use the normal calibrated red/green brightness as their full-scale
reference. Both ring animation and discrete-LED test can be disabled
independently.

## Runtime and diagnostics

`RuntimeConfig.h` centralises UI/LED task dividers, diagnostics period, calibration-console timing, serial baud, startup input settle time and ladder-calibration timeout. Automatic live-state restore and asynchronous autosave are enabled by default. The musical state and the active Quantizer/Arpeggiator UI layer are restored on boot. Layer restoration is side-effect free: it does not enable/disable an Arpeggiator or replay toggle feedback.


### LED calibration scale

Brightness calibration deliberately spans the complete TLC5947 12-bit range.
The twelve clockwise note positions correspond to 0..4095 inclusive, with
eleven equal intervals. A configured/default PWM value that lies between two
positions maps to the nearest available step. During active editing the ring
shows a repeating green/red/amber comparison pattern using the current live PWM
levels for both emitters. A newly chosen step is marked briefly by a dark gap at
its ring position. Channel A's discrete status-LED pair identifies green editing;
Channel B's pair identifies red editing. Position 0 still represents true off and
position 11 the actual hardware maximum; the comparison view changes only how
those steps are presented, not how they are stored.


## Factory full-config fallbacks

`lib/fmq/include/fmq/config/FactoryPresets.h` contains the twelve firmware
fallback scales used by `SHIFT+LOAD` when a full-config EEPROM slot is empty or
invalid. Each preset is stored as a 12-bit pitch-class mask (`bit 0 = C`,
`bit 11 = B`). A valid user-saved full configuration always takes precedence.
Changing these masks changes firmware defaults only; it does not rewrite EEPROM.


## Arpeggiator layer

The Arpeggiator is a full second UI layer. `UiConfig.h` defines the debounced SHIFT double-click layer-switch gesture and the nine Arpeggiator function positions. Both SHIFT presses must remain within the short-click limit, and the second raw press must begin within 350 ms of the first debounced release. Entering that layer enables the selected Arpeggiator if necessary. The interaction grammar does **not** change between layers: unmodified note buttons continue to edit/show the scale, while `SHIFT + note` selects the active layer's menu function. A, A# and B therefore remain `SHIFT+A` Link, `SHIFT+A#` Channel A and `SHIFT+B` Channel B in both layers. Scalar Arpeggiator parameters use the next unmodified note press as their value selector, matching the established Quantizer menu behaviour.

The factory/default Arpeggiator sync mode is `FREE`; no external clock is required. `RESET` and `CLOCK` are explicit additional modes selected from the Arpeggiator Sync menu.

`ProductConfig.h` contains only product limits/defaults rather than one fixed arpeggio:

- `kArpRateCount = 12`;
- `kArpDefaultRateIndex = 3`, corresponding to 24 ms in FREE/RESET;
- `kArpMaximumLength = 12`;
- `kArpMaximumRange = 4`;
- `kArpMaximumSwingStep = 11`.

The per-channel persistent configuration contains enable, rate/clock ratio, pattern, shape, length, octave range, step-trigger enable, sync mode and swing. FREE and RESET use the internal rate table; CLOCK interprets the same index as a divider/multiplier of the channel's Sample/Gate clock. Full behaviour and all value tables are documented in [README_ARPEGGIATOR.md](README_ARPEGGIATOR.md).

## EEPROM layout and live state

Scale slots remain compact scale-only records. A full-configuration payload is 32 bytes and contains the Quantizer state, both Arpeggiator configurations, the selected channel and the active Quantizer/Arpeggiator UI layer. The layer uses a spare bit in the selected-channel byte, so the payload size does not grow. Full-configuration records are versioned and CRC-protected. New writes use save-format v5; v4 scale/config records remain readable for in-place firmware upgrades.

The live-state record additionally stores the two-byte LED brightness calibration. The current layout leaves a wear-levelled ring of 12 live-state records before the final startup-sequence metadata byte. The validity marker is committed last.

`PersistenceConfig.h::kLiveAutosaveQuiescenceMs` is currently 3000 ms. A front-panel configuration change marks live state dirty; once no further change has occurred for that interval and the shared EEPROM writer is idle, the complete state is queued asynchronously. Runtime phase, current Arpeggiator step and current Glide position are not serialized. The UI layer **is** serialized in both full configurations and live state so the front-panel function map matches the restored Arpeggiator status after preset loads and reboot. A legacy record with an enabled Arpeggiator but no layer bit is migrated to the Arpeggiator layer.

The shared `AsyncEepromWriter` queue is sized for the largest current atomic transaction. Compile-time assertions protect record sizes and prevent the configured EEPROM regions from overlapping.

## AVR resource budget

PlatformIO still enforces the board's absolute program/RAM capacity. CI adds a deliberately more conservative engineering gate through `scripts/check_avr_resource_budget.py`:

- application flash target: at most 95% of 30,720 bytes (29,184 bytes);
- static SRAM target: at most 70% of 2,048 bytes.

The check counts `.data` in both flash and SRAM, because AVR initialised data consumes flash storage and is copied into SRAM at startup. The aim is to preserve room for stack/interrupt activity and future maintenance rather than treating the last byte of the MCU as usable feature budget.

---

<p align="center">From Munich With <img src="docs/assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
