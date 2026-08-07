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

## Product defaults

`kFactoryScaleMask` defaults to `0x0FFF`, a chromatic scale, so a blank module is immediately usable.

`kFactorySampleMode` defaults to **Track-and-Hold** (`0`), matching the Rust original. SHIFT+4 normally toggles only Track-and-Hold and Sample-and-Hold. `kEnableContinuousSampleModeInUi` is `false` by default; setting it to `true` exposes the C++ Continuous extension in the front-panel cycle.

Glide and trigger-delay limits live in `ProductConfig.h` as `kMaxGlideAmount` and `kMaxTriggerDelayAmount` rather than being repeated as literals in processing and persistence code.

## ADC reference and original PCB

The original Rust `main.rs` initially constructs an ADC with an AREF setting, but `fm-lib::async_adc::init_async_adc()` subsequently writes `ADMUX.REFS=AVCC`. The effective reference of the original firmware is therefore **AVCC**, and the original board profile follows that behaviour.

Any future PCB revision using a true external reference should get its own verified board profile rather than silently changing the original profile.

## Resistor ladder and ADC

The original PCB uses the proven Rust ladder values
`[0, 93, 171, 236, 292, 341, 384, 421, 455, 485, 512, 536, 558]` and 64 ms debounce. The original-board profile keeps the nearest-value decoder and disables rest normalization.

The AVR ADC scanner runs interrupt-driven at 125 kHz (prescaler 128), explicitly starts conversions, discards the first conversion after a MUX change and uses a median-of-three filter to match the effective Rust `fm-lib` behaviour.

Per-channel analogue calibration is configured with integer affine corrections:

- ADC: offset, then gain numerator/denominator.
- DAC: gain numerator/denominator, then offset.

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

### Important: LED PWM values are empirical

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
`kSparklesFrameMs`, and the discrete status-LED timings. All note-ring effects
use the normal calibrated red/green brightness as their full-scale reference.
Both ring animation and discrete-LED test can be disabled independently.

## Runtime and diagnostics

`RuntimeConfig.h` centralises UI/LED task dividers, diagnostics period, calibration-console timing, serial baud, startup input settle time and ladder-calibration timeout. Automatic live-state restore/autosave remains disabled by default to match the original startup baseline.


### LED calibration scale

Brightness calibration deliberately spans the complete TLC5947 12-bit range.
The twelve clockwise note positions correspond to 0..4095 inclusive, with
eleven equal intervals. A configured/default PWM value that lies between two
positions is displayed at the nearest position. The calibration UI fills the
ring clockwise from position 0 through that current position, so the selected
level reads as a bar rather than a single marker. Position 0 therefore represents
true off and position 11 represents the actual hardware maximum.


## Factory full-config fallbacks

`lib/fmq/include/fmq/config/FactoryPresets.h` contains the twelve firmware
fallback scales used by `SHIFT+LOAD` when a full-config EEPROM slot is empty or
invalid. Each preset is stored as a 12-bit pitch-class mask (`bit 0 = C`,
`bit 11 = B`). A valid user-saved full configuration always takes precedence.
Changing these masks changes firmware defaults only; it does not rewrite EEPROM.


## Retro Arpeggiator

The Retro Arpeggiator is configured with:

- `kRetroArpToggleHoldMs` in `UiConfig.h`: SHIFT-alone hold duration.
- `kRetroArpFeedbackMs` in `UiConfig.h`: short visual toggle feedback.
- `kRetroArpStepMs` in `ProductConfig.h`: time per arpeggio step.

The arpeggio uses scale-degree offsets root/third/fifth (`0, 2, 4`) against each
channel's currently active scale. It is intentionally a volatile performance mode
and is not persisted.
