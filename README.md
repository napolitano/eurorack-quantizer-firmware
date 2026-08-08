# Free Modular Quantizer — PlatformIO/C++ Firmware

[![CI](https://img.shields.io/badge/CI-GitHub%20Actions-2088FF?logo=githubactions&logoColor=white)](.github/workflows/ci.yml)
[![Tests](https://img.shields.io/badge/tests-native-2ea44f)](.github/workflows/ci.yml)
[![AVR builds](https://img.shields.io/badge/builds-Nano%20%2F%20ATmega328P-00979D?logo=arduino&logoColor=white)](.github/workflows/ci.yml)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-project-F5822A?logo=platformio&logoColor=white)](https://platformio.org/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](platformio.ini)
[![Target](https://img.shields.io/badge/target-ATmega328P-00979D?logo=arduino&logoColor=white)](platformio.ini)
[![License](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)](LICENSE)

Independent C++/PlatformIO firmware reimplementation for the **Free Modular Quantizer** Eurorack module, targeting the **Arduino Nano / ATmega328P at 16 MHz**.

The firmware keeps the original hardware mapping and core interaction model while making the project easier to build, test, calibrate, understand and extend. It is deliberately **not a line-by-line translation** of the Rust firmware: compatible behaviour is preserved where appropriate, known defects are not intentionally reproduced, and additional features are documented explicitly.

> **Status:** active, hardware-tested development firmware. The portable core is covered by host-side tests; analogue calibration and final LED brightness values remain specific to the actual module and fitted components.

## Highlights

- Two independent quantizer channels, A and B
- Twelve-note programmable scales
- Track-and-Hold and Sample-and-Hold
- Pre-shift, scale-degree shift and post-shift per channel
- Glide and trigger delay
- Optional A/B configuration linking
- Relative or absolute pitch behaviour for Channel B
- Twelve scale save slots
- Twelve full-configuration slots with factory-preset fallbacks
- Versioned, CRC-checked EEPROM persistence
- Independent red/green LED calibration over the full TLC5947 range
- Four rotating startup animations
- Discrete input/output activity LEDs for both channels
- **Scale-aware Retro Arpeggiator** performance mode
- Native host tests plus AVR builds through GitHub Actions
- Central configuration for hardware, UI, timing and calibration values

## Acknowledgement — Quinn Freedman

This project exists because of **Quinn Freedman's original Quantizer design and Rust firmware**.

The original design makes a particularly pragmatic set of choices: it is cost-conscious, approachable, uses readily understandable building blocks and avoids unnecessary hardware complexity. For DIY use, that balance is unusually strong. The module is comparatively straightforward to build, easy to study and well suited to modification without requiring an expensive or highly specialised platform.

That makes the original Quantizer more than just a reference implementation for this repository: it is a very good example of hardware that invites experimentation. This C++ reimplementation is intended to respect that character while making the firmware easier to build, test and adapt with a conventional PlatformIO toolchain.

Original project and firmware: <https://github.com/QuinnFreedman/modular>

## Why C++ and PlatformIO?

The original Rust firmware remains the behavioural and hardware reference. PlatformIO/C++ is used here for pragmatic reasons:

- straightforward builds and uploads on Windows, Linux and macOS;
- a familiar Arduino-compatible toolchain for the ATmega328P;
- host-side unit, integration and regression tests without AVR hardware;
- clear separation between quantizer logic, application/UI code and board support;
- easier serial diagnostics and instrumentation;
- centralised configuration of board and calibration assumptions;
- a lower maintenance barrier for contributors already familiar with Arduino and PlatformIO.

This repository is an independent reimplementation and is **not an official Free Modular repository**.

## Original firmware limitations addressed

The original Rust firmware is the behavioural reference for this project, and its compact design is one of the reasons the module is so attractive for DIY. During the reimplementation, however, a small number of concrete limitations and defects could be verified directly in the original source. The items below are deliberately limited to those cases. They do **not** include bugs introduced and fixed during development of this C++ implementation.

| Area | Original firmware behaviour | This implementation |
|---|---|---|
| Negative scale-degree shift | `step_in_scale()` correctly selects a downward direction for negative values, but then iterates over `0..num_steps`. For a negative `num_steps` that range is empty, so negative scale shifts perform no steps at all. | The magnitude and direction are handled separately. Negative values now step downward through the enabled notes exactly as the UI indicates. |
| Trigger delay in Track-and-Hold | In Track-and-Hold, a HIGH trigger resets `input_trigger_timer` to zero on every processing step. With a non-zero trigger delay, the timer therefore cannot reach the configured delay while the gate remains HIGH; the delayed update can occur only after the input goes LOW and the timer starts advancing. | Trigger detection, delay timing and UI indication use separate state. A HIGH gate starts one delay interval; once it expires, Track-and-Hold follows the CV continuously for as long as the gate remains HIGH. |
| Empty power-on scale | `QuantizerChannel::new()` initialises all twelve scale notes to `false`. With no saved scale loaded, the quantizer therefore starts with no enabled pitch classes and the quantization fallback is 0 semitones until the user creates or loads a scale. | The factory state is a chromatic scale, so a freshly flashed or erased module is immediately usable. The UI also prevents the final enabled note from being disabled accidentally. |
| ADC full-scale conversion | The original `adc_to_semitones()` converts a 10-bit reading by shifting it into an `I1F15` value and interpolating as though the input could reach `0x7FFF`. The source itself notes that the actual maximum after the shift is `0x7FE0`. Consequently a raw ADC value of 1023 does not map exactly to the configured 120-semitone endpoint. | ADC conversion is performed directly from the real 10-bit range `0…1023`, with rounded integer arithmetic, so 0 maps exactly to 0 semitones and 1023 maps exactly to 120 semitones. Per-channel gain and offset calibration are available on top of that mapping. |
| EEPROM record integrity | A saved scale or configuration is considered valid solely when a one-byte sentinel is present. The sentinel is written **before** the payload, and there is no checksum, format version or payload validation. A reset or interrupted write can therefore leave a slot marked as occupied even when its payload is incomplete or stale; arbitrary EEPROM contents can also be accepted as configuration values. | Persistent records are versioned and CRC-checked and their decoded values are validated before use. Invalid, incomplete or erased records are rejected. Full-configuration slots without valid user data expose known factory presets instead. |
| EEPROM writes in the UI path | Save and erase operations are issued synchronously from the menu/persistence path. This keeps the implementation simple, but EEPROM programming latency is part of the foreground interaction path. | A single shared asynchronous EEPROM writer queues writes and lets normal processing continue. Persistence operations report whether a write was accepted instead of assuming immediate completion. |
| LED colour balance | Red and green brightness are fixed at compile time (`0xFFF` red and `0x04F` green). There is no user-accessible calibration mechanism, so balancing depends entirely on those constants matching the particular LEDs and resistor population of the built module. | Red and green levels are independently configurable and can be calibrated on the finished hardware over the complete TLC5947 `0…4095` range. The same calibrated levels are used by normal rendering and startup effects. |
| Toolchain fragility | The firmware depends on an AVR Rust nightly toolchain and several unstable language/compiler features. In the scalar submenu code the original source also contains an inline-assembly workaround because, without it, an optimisation issue could cause `button_idx` to become zero; the accompanying comment explicitly describes this as either a compiler bug or possible memory corruption/optimisation behaviour. | The reimplementation uses a conventional PlatformIO/C++17 AVR toolchain and expresses the menu calculation directly without the compiler-specific assembly workaround. Native tests and AVR builds are part of CI. |

These are not intended as criticism of the original project. Quinn Freedman's implementation is deliberately compact and pragmatic, and that simplicity is a major strength of the design. This section exists to make the behavioural differences explicit and to document why the replacement firmware intentionally diverges at these specific points.

## Hardware compatibility

The default board profile follows the original Free Modular Quantizer hardware.

| Function | Arduino Nano pin | Notes |
|---|---:|---|
| SPI SCK / MOSI / MISO | D13 / D11 / D12 | shared SPI bus |
| MCP4922 DAC CS | D10 | LDAC tied low on original hardware |
| TLC5947 LED driver CS | D9 | |
| TLC5947 BLANK | A0 | active high |
| Input LED A / Output LED A | A1 / A2 | discrete activity LEDs |
| Input LED B / Output LED B | A3 / A4 | discrete activity LEDs |
| Note-button resistor ladder | A5 | 12 buttons on one ADC channel |
| CV input A / B | A6 / A7 | ADC-only pins |
| SHIFT / SAVE / LOAD | D8 / D7 / D6 | active low, internal pull-ups |
| Trigger input A / B | D2 / D3 | active high |
| Trigger output A / B | D4 / D5 | active high |

The original `fm-lib` configures the ADC to use **AVCC** as its effective reference. The default board profile therefore does the same.

### Trigger-input normalisation

On the original hardware the trigger/sample inputs are normalised to **HIGH** when no cable is inserted. This is fundamental to the normal Track-and-Hold behaviour:

- no cable → trigger input HIGH → CV is followed and quantized continuously;
- cable inserted, gate HIGH → CV is tracked continuously;
- cable inserted, gate LOW → the most recent quantized value is held.

This hardware normalisation is why a separate Continuous mode is not required for normal operation. A Continuous mode exists internally as an optional extension but is excluded from the standard UI by default.

## Build and upload

Requirements:

- PlatformIO Core or the PlatformIO IDE extension
- Arduino Nano / ATmega328P target

Nano with newer bootloader:

```bash
pio run -e nanoatmega328new
pio run -e nanoatmega328new -t upload
```

Nano with old bootloader / compatible older clones:

```bash
pio run -e nanoatmega328
pio run -e nanoatmega328 -t upload
```

Run the native test suite:

```bash
pio test -e native
```

The serial monitor is configured for **115200 baud**.

## GitHub Actions

Two workflows are included under `.github/workflows/`.

### `ci.yml`

Runs on pushes, pull requests and manual dispatches. It:

1. installs PlatformIO Core;
2. runs all 26 native test suites as independent matrix jobs (`fail-fast: false`);
3. runs a separate aggregate coverage job and uploads HTML/XML/text reports;
4. builds both Nano environments;
5. uploads the resulting `.hex` and `.elf` files as workflow artifacts.

### `release.yml`

Runs for version tags matching `v*` and can also be started manually. It:

1. runs the native tests;
2. builds both Nano bootloader variants;
3. collects the firmware, README and licence;
4. uploads a release artifact;
5. when triggered by a version tag, creates the corresponding GitHub Release and attaches the build products.

The workflow structure follows PlatformIO's documented GitHub Actions approach: install PlatformIO Core in CI and use `pio run` / `pio test` from the project root.

## User interface

The twelve note buttons are indexed **0–11 clockwise**, with index 0 at the **12 o'clock / C** position.

### Normal display

- **Channel A:** green
- **Channel B:** red
- **Current quantized note:** amber overlay
- **Linked channels:** shared scale amber; current A/B notes green/red

The factory scale is chromatic, so a freshly flashed module is immediately usable.

### Main controls

| Control | Function |
|---|---|
| Note 0–11 | Toggle the corresponding pitch class in the selected scale |
| SAVE | Select a scale-save slot |
| LOAD | Select a scale-load slot |
| SHIFT + SAVE | Select a full-configuration save slot |
| SHIFT + LOAD | Select a full-configuration load slot; user data takes priority, otherwise factory preset |
| LOAD + SAVE, hold 2 s | Erase all user save slots |
| SHIFT + LOAD + SAVE, hold 5 s | Enter LED brightness calibration |
| SHIFT alone, hold 3 s | Toggle Retro Arpeggiator |

The firmware prevents the final active note of a scale from being disabled accidentally.

### SHIFT shortcuts

The symbols below match the function pictograms used for the module documentation and panel reference.

| Symbol | Combination | Function | Feedback |
|:---:|---|---|---|
| <img src="docs/assets/01_rotate-ccw-transpose-down.svg" width="34" alt="Rotate counterclockwise / transpose down"> | SHIFT + 0 | Rotate scale one step left | updated scale |
| <img src="docs/assets/02_rotate-cw-transpose-up.svg" width="34" alt="Rotate clockwise / transpose up"> | SHIFT + 1 | Rotate scale one step right | updated scale |
| <img src="docs/assets/03_glide-portamento.svg" width="34" alt="Glide / portamento"> | SHIFT + 2 | Glide | scalar display |
| <img src="docs/assets/04_sample-delay.svg" width="34" alt="Sample delay"> | SHIFT + 3 | Trigger delay | scalar display |
| <img src="docs/assets/05_track-sample-toggle.svg" width="34" alt="Track / sample toggle"> | SHIFT + 4 | Toggle Track-and-Hold / Sample-and-Hold | green = Track, red = Sample |
| <img src="docs/assets/06_post-shift.svg" width="34" alt="Post shift"> | SHIFT + 5 | Post-shift | signed scalar display |
| <img src="docs/assets/07_scale-shift.svg" width="34" alt="Scale shift"> | SHIFT + 6 | Scale-degree shift | signed scalar display |
| <img src="docs/assets/08_pre-shift.svg" width="34" alt="Pre shift"> | SHIFT + 7 | Pre-shift | signed scalar display |
| <img src="docs/assets/09_relative-pitch-toggle.svg" width="34" alt="Relative pitch toggle"> | SHIFT + 8 | Channel-B Relative / Absolute mode | green = Relative, red = Absolute |
| <img src="docs/assets/10_channels-linked-toggle.svg" width="34" alt="Channels linked toggle"> | SHIFT + 9 | Link / unlink channels | green = linked, red = unlinked |
| <img src="docs/assets/11_select-channel-a.svg" width="34" alt="Select Channel A"> | SHIFT + 10 | Select Channel A | A / green |
| <img src="docs/assets/12_select-channel-b.svg" width="34" alt="Select Channel B"> | SHIFT + 11 | Select Channel B | B / red |

When channels are linked, edits apply to both channels. Linking copies Channel A's current configuration to Channel B and returns selection to Channel A.

## Track-and-Hold and Sample-and-Hold

The factory mode is **Track-and-Hold**, matching the original firmware.

### Track-and-Hold

While the trigger/sample input is HIGH, the CV input is followed continuously and re-quantized. When the input goes LOW, the last quantized value is held.

Because the original hardware normalises an empty trigger jack to HIGH, this behaves like a normal continuously running quantizer when nothing is patched to the trigger/sample input.

### Sample-and-Hold

Only a LOW→HIGH transition samples the CV input. The resulting quantized value is held until the next rising edge.

### Mode feedback

`SHIFT+4` changes the mode first and then reports the **new** state on ring position 4:

- green = Track-and-Hold
- red = Sample-and-Hold

After a fresh boot, the first `SHIFT+4` changes Track → Sample and therefore shows red. The next press changes Sample → Track and shows green.

## Discrete Channel LEDs

The four separate LEDs are runtime activity indicators rather than persistent Track/Sample status lights.

| LED | Meaning |
|---|---|
| Input A | Trigger/sample activity on Channel A |
| Output A | Quantized-note change on Channel A |
| Input B | Trigger/sample activity on Channel B |
| Output B | Quantized-note change on Channel B |

With the original HIGH-normalised trigger inputs:

- Track-and-Hold + no trigger cable → Input LED remains on;
- Track-and-Hold + external gate HIGH → Input LED remains on while the gate is HIGH;
- Sample-and-Hold → Input LED flashes on the rising edge and then extinguishes even if the gate remains HIGH;
- Output LED lights briefly whenever the channel changes to a different nominal quantized note.

The physical trigger output pulse is intentionally shorter than the visual LED pulse.

## Scalar menus

Glide and Trigger Delay use an unsigned 0–11 representation.

Pre-shift, Scale-shift and Post-shift use a signed representation:

- centre/reference position: amber
- positive values: green clockwise
- negative values: red from the upper end of the ring

Pressing a note button selects the displayed value.

## Save/load and factory presets

Normal SAVE/LOAD stores or recalls only the active scale. `SHIFT+SAVE` and `SHIFT+LOAD` use the twelve full-configuration slots.

Every full-config load position remains useful even before the user has saved anything:

1. a valid user configuration in the selected slot is loaded first;
2. if the slot is empty, erased or invalid, the corresponding built-in factory preset is loaded;
3. saving to that slot transparently overrides the factory fallback;
4. erasing user data reveals the factory preset again;
5. factory presets are firmware data and are never written into EEPROM simply to initialise the module.

### Factory preset bank

| Slot | Preset | Notes |
|---:|---|---|
| 1 | Chromatic | C C# D D# E F F# G G# A A# B |
| 2 | Major / Ionian | C D E F G A B |
| 3 | Natural Minor / Aeolian | C D Eb F G Ab Bb |
| 4 | Harmonic Minor | C D Eb F G Ab B |
| 5 | Melodic Minor | C D Eb F G A B |
| 6 | Dorian | C D Eb F G A Bb |
| 7 | Phrygian | C Db Eb F G Ab Bb |
| 8 | Lydian | C D E F# G A B |
| 9 | Mixolydian | C D E F G A Bb |
| 10 | Major Pentatonic | C D E G A |
| 11 | Minor Pentatonic | C Eb F G Bb |
| 12 | Blues | C Eb F F# G Bb |

The factory presets are rooted on C and otherwise use the normal factory configuration for both channels.

## Retro Arpeggiator

The **Retro Arpeggiator is an official performance feature** of this firmware. Its deliberately fast three-note pattern can bring back the characteristic rapid arpeggio textures associated with **classic 8-bit home computers and game music**, while remaining musically useful because the intervals are derived from the selected scale rather than from a fixed chord table.

Hold **SHIFT by itself for 3 seconds** during normal operation to toggle it. If a note button, LOAD or SAVE is used while SHIFT is held, the pending long-hold is cancelled so normal SHIFT shortcuts cannot accidentally enable the feature. The gesture is disabled during LED calibration.

When active:

- the normal quantizer still determines the base note;
- the output cycles through **root, diatonic third and diatonic fifth**;
- chord tones are derived from the **currently active scale** rather than hard-coded major/minor intervals;
- Major therefore produces a major third where appropriate, Natural Minor a minor third, and modal or custom scales derive their own scale-degree intervals;
- both channels use their own active scale/configuration;
- the default step time is **24 ms**;
- activation is acknowledged by a short amber ring indication;
- deactivation is acknowledged in red.

The mode is intentionally **not persisted**. A reboot always returns to normal quantizer operation.

Configuration values live in `ProductConfig.h` and `UiConfig.h` under the `kRetroArp...` names.

## LED brightness calibration

Red and green brightness must be calibrated independently. Correct values depend strongly on the **actual LEDs, their forward voltages, optical efficiency and fitted series resistors**. There is no universal numerical pair that guarantees a visually balanced red/green/amber result.

> **Important:** LED brightness calibration is inherently empirical and iterative. Firmware defaults are only starting values for the currently tested hardware. A different LED type or resistor value can require dramatically different PWM settings.

Enter calibration by holding **SHIFT + LOAD + SAVE for 5 seconds**.

Calibration behaviour:

- Channel A / green is selected initially;
- `SHIFT+A` selects green;
- `SHIFT+B` selects red;
- the twelve ring positions cover the complete TLC5947 PWM range from **0 to 4095**;
- 12 o'clock represents 0;
- values increase clockwise;
- the final position is the true 12-bit maximum, 4095;
- the display is a filled clockwise bar from 12 o'clock through the selected step;
- the active colour is rendered at the selected PWM value as a live preview;
- entering the mode or switching colour maps the current PWM value to the nearest of the twelve displayed steps;
- hold SHIFT alone for 5 seconds to save and leave calibration.

See [README_CALIBRATION.md](README_CALIBRATION.md) for the detailed procedure.

## Startup sequences

Startup animation can be disabled in configuration. When sequence rotation is enabled, the firmware cycles through four animations on successive power-ups and stores the **next sequence index** in a dedicated EEPROM byte.

1. **Color Fade** — all twelve LEDs fade in/out together: green, red, then amber.
2. **Glowworm** — clockwise green, red and amber revolutions with a dimming tail.
3. **Cog** — repeating pairs of red, green and amber rotate for three revolutions.
4. **Sparkles** — pseudo-random multi-colour twinkles for approximately 2.5 seconds.

After the ring animation, the four discrete status LEDs flash once each in physical order before normal operation begins. After sequence 4 the selector wraps back to sequence 1; invalid or erased sequence-state data also falls back to sequence 1.

## Persistence

User records are versioned and CRC checked. Invalid or incomplete records are rejected rather than silently interpreted as valid data.

EEPROM writes use a non-blocking writer. Live-state restore/autosave exists as an optional extension but is disabled by default to preserve deterministic power-on behaviour and compatibility with the original interaction model.

## Configuration

User-adjustable constants are grouped under:

```text
lib/fmq/include/fmq/config/
```

Important files:

- `ProductConfig.h` — factory mode, scale, quantizer limits and Retro Arpeggiator timing
- `AnalogConfig.h` — ADC, DAC and resistor-ladder assumptions/calibration
- `LedConfig.h` — LED brightness and startup animations
- `UiConfig.h` — debounce, menu timings, button mappings and Retro Arpeggiator gesture
- `RuntimeConfig.h` — scheduler, diagnostics and serial settings
- `PersistenceConfig.h` — persistence behaviour
- `FactoryPresets.h` — built-in full-config fallback scales

See [README_CONFIGURATION.md](README_CONFIGURATION.md).

## Project structure and testing

Portable firmware logic is separated from AVR/Arduino board support:

```text
lib/fmq/
  include/fmq/
    application/
    config/
    domain/
    persistence/
    ports/
    ui/
  src/

src/
  main.cpp
  platform/nano_atmega328p/

test/
  unit/
  integration/
  regression/
  system/
  support/
```

The native suite currently contains **26 independently runnable suites and 168 default test cases**, including exhaustive/matrix checks for ADC/DAC conversion, all scale masks, Track/Sample delays, Glide values, transposition ranges, EEPROM corruption and the Retro Arpeggiator. System tests drive simulated CV/gate inputs through the production quantizer path and verify DAC codes, triggers and status LEDs at 1 ms resolution. See [README_TESTING.md](README_TESTING.md) for the test strategy and known specification findings. AVR-specific behaviour is additionally compiled in CI for both supported Nano bootloader variants; analogue behaviour still requires real-hardware validation.

Release history is maintained in [CHANGELOG.md](CHANGELOG.md).

## Documentation

- [README.md](README.md) — project overview, features and UI reference
- [README_CONFIGURATION.md](README_CONFIGURATION.md) — firmware configuration reference
- [README_CALIBRATION.md](README_CALIBRATION.md) — detailed hardware and LED calibration workflow
- [CHANGELOG.md](CHANGELOG.md) — public release history
- [.github/SECURITY.md](.github/SECURITY.md) — vulnerability reporting and supported-version policy
- [`docs/assets/`](docs/assets/) — panel artwork and UI pictograms used by the documentation

## Security

Please report suspected vulnerabilities privately rather than through public issues or discussions. See the repository [Security Policy](.github/SECURITY.md) for scope, supported versions and reporting instructions.

## Source headers and maintenance metadata

All C/C++ source and header files contain a consistent header with purpose, authorship, original-project reference and SPDX licence identifier. The active maintenance year is **2026**.

## Credits and licence

- **Original Quantizer design and Rust firmware:** Quinn Freedman
- **C++ / PlatformIO reimplementation:** Axel Napolitano

Licensed under **GPL-3.0-or-later**, consistent with the original firmware. See [LICENSE](LICENSE).

This is an independent reimplementation and is not affiliated with or endorsed by Free Modular.

**From Munich With Love ❤️**


## Testing

Native unit, integration and system signal-path tests are documented in [`README_TESTING.md`](README_TESTING.md).
