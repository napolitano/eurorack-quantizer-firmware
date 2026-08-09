# Free Modular Quantizer — Alternative C++/PlatformIO Firmware

[![CI](https://github.com/napolitano/eurorack-quantizer-firmware/actions/workflows/ci.yml/badge.svg)](https://github.com/napolitano/eurorack-quantizer-firmware/actions/workflows/ci.yml)
[![Release](https://github.com/napolitano/eurorack-quantizer-firmware/actions/workflows/release.yml/badge.svg)](https://github.com/napolitano/eurorack-quantizer-firmware/actions/workflows/release.yml)
[![Tests](https://img.shields.io/badge/tests-native-2ea44f)](.github/workflows/ci.yml)
[![AVR builds](https://img.shields.io/badge/builds-Nano%20%2F%20ATmega328P-00979D?logo=arduino&logoColor=white)](.github/workflows/ci.yml)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-project-F5822A?logo=platformio&logoColor=white)](https://platformio.org/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](platformio.ini)
[![Target](https://img.shields.io/badge/target-ATmega328P-00979D?logo=arduino&logoColor=white)](platformio.ini)
[![License](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)](LICENSE)

An independent alternative firmware for the **Free Modular Quantizer** Eurorack module, written in modern C++17 for **PlatformIO** and the original **Arduino Nano / ATmega328P at 16 MHz** hardware.

It keeps the module recognisably the same instrument: the original pin mapping, panel controls and core quantizer workflow remain intact. The implementation focuses on easier builds, stronger testability and persistence, hardware calibration, well-defined edge-case behaviour and an additional performance-oriented Arpeggiator layer.

> **Status:** active, hardware-tested firmware. The portable core is covered by host-side tests; analogue calibration and final LED brightness values remain specific to the actual module and fitted components.

## Acknowledgement — Quinn Freedman

This project exists because of **Quinn Freedman's original Quantizer design and Rust firmware**.

The original design makes a particularly pragmatic set of choices: it is cost-conscious, approachable, uses readily understandable building blocks and avoids unnecessary hardware complexity. For DIY use, that balance is unusually strong. The module is comparatively straightforward to build, easy to study and well suited to modification without requiring an expensive or highly specialised platform.

That makes the original Quantizer more than just a reference implementation for this repository: it is a very good example of hardware that invites experimentation. This C++ reimplementation is intended to respect that character rather than replace or diminish it.

Original project and firmware: <https://github.com/QuinnFreedman/modular>

This repository is an independent reimplementation and is **not an official Free Modular repository**.

## Why this alternative firmware exists

The original Rust firmware is compact and closely tied to the module it was written for. That is part of its appeal. For continued maintenance and experimentation, however, its AVR Rust setup also comes with a relatively specialised toolchain: it relies on a nightly compiler and unstable features, and parts of the original source contain a compiler-specific inline-assembly workaround. Reproducing and maintaining that environment is less convenient today than using the conventional Arduino/AVR tooling available through PlatformIO.

This project therefore takes a different engineering route while retaining the original hardware and interaction concept. The goals are practical:

- straightforward builds and uploads on Windows, Linux and macOS;
- a familiar Arduino-compatible C++17 toolchain for the ATmega328P;
- host-side unit, integration, regression and system tests without requiring AVR hardware for every change;
- native AVR builds and resource-budget checks in GitHub Actions;
- clear separation between quantizer logic, application/UI code, persistence and board support;
- easier serial diagnostics and instrumentation during development and calibration;
- centralised configuration of timing, analogue assumptions, LED behaviour and calibration;
- a lower maintenance barrier for contributors already familiar with Arduino and PlatformIO;
- robust, validated persistence instead of treating arbitrary or interrupted EEPROM data as valid state;
- explicit fixes for a small number of edge cases verified in the original implementation;
- room for additional features that fit the existing front panel without requiring hardware changes.

The original firmware remains the behavioural and hardware reference wherever that behaviour is intentional and useful. This is deliberately **not a line-by-line translation**: compatible behaviour is preserved where appropriate, known defects are not intentionally reproduced, and extensions are documented explicitly.

## What this firmware adds

The original firmware already provides the essential instrument: **two independent quantizer channels**, twelve-note scale editing, Track-and-Hold and Sample-and-Hold, Glide and delay, pre-/scale-/post-shift, Channel-B Relative/Absolute operation, channel linking, and twelve scale plus twelve full-configuration memory slots. Those are core Free Modular Quantizer features and are retained here rather than presented as additions.

This alternative firmware adds or deliberately changes the following:

- **Arpeggiator performance layer:** a complete second operating layer with internal FREE timing, external CLOCK/RESET operation, clock ratios, patterns, scale-aware shapes, adjustable length and octave range, swing and optional per-step triggers.
- **Fast, symmetrical layer switching:** double-click SHIFT to enter the Arpeggiator layer and double-click SHIFT again to disable it and return to the Quantizer. No hardware changes or additional controls are required.
- **Built-in factory preset bank:** empty full-configuration slots expose twelve useful factory scales; saving a user configuration to a slot transparently overrides its factory fallback.
- **Live-state restore:** the working state can be restored after power cycling, including both channel configurations, Arpeggiator settings, selected channel and active UI layer.
- **More robust EEPROM persistence:** records are versioned, CRC-checked and range-validated, and writes are handled asynchronously rather than accepting a slot solely because a sentinel byte is present.
- **On-device LED colour calibration:** red and green brightness can be adjusted independently over the TLC5947 range while green, red and amber are shown together for direct visual balancing.
- **Improved power-on behaviour:** the default scale is chromatic, the final enabled scale note is protected against accidental removal, four short startup animations rotate between boots, and the four discrete status LEDs receive a separate self-test.
- **Corrected edge cases from the reference implementation:** verified issues such as Track-and-Hold with delay, negative scale shifts, full-scale ADC mapping and interrupted/invalid persistence are handled explicitly; the detailed comparison is documented below.

The implementation itself also differs substantially: it uses a conventional **C++17/PlatformIO AVR toolchain**, portable host-side tests, AVR CI builds and explicit flash/SRAM resource gates. These engineering changes are primarily about maintainability and reproducibility rather than changing the front-panel instrument.

The module still uses the original hardware. **No PCB, component or wiring modification is required or supported by this firmware project.**

# Using the firmware

## User manual

The dedicated end-user manual workspace is prepared under [`docs/manual/`](docs/manual/README.md). The editable manual source is intended to be maintained as a LibreOffice Writer document and uses the **Ubuntu Font Family** as a required editing dependency. Manual-specific licensing and font setup information are kept with the manual sources so the firmware licence and documentation licence remain clearly separated.

The sections below provide the essential front-panel reference directly in the repository README.

## Normal display

The twelve note buttons are indexed **0–11 clockwise**, with index 0 at the **12 o'clock / C** position.

- **Channel A:** green
- **Channel B:** red
- **Current quantized note:** amber overlay
- **Linked channels:** shared scale amber; current A/B notes green/red

The factory scale is chromatic, so a freshly flashed module is immediately usable. The firmware prevents the final active note of a scale from being disabled accidentally.

## Main controls

| Control | Function |
|---|---|
| Note 0–11 | Toggle the corresponding pitch class in the selected scale |
| SAVE | Select a scale-save slot |
| LOAD | Select a scale-load slot |
| SHIFT + SAVE | Select a full-configuration save slot |
| SHIFT + LOAD | Select a full-configuration load slot; user data takes priority, otherwise factory preset |
| LOAD + SAVE, hold 2 s | Erase all user save slots |
| SHIFT + LOAD + SAVE, hold 5 s | Enter LED brightness calibration |
| SHIFT, double-click | Switch Quantizer ↔ Arpeggiator layer; entering enables the selected Arpeggiator and flashes the ring green twice, returning disables all Arpeggiators and flashes red twice |

A SHIFT double-click is deliberately symmetrical: **double-click to enter, double-click again to leave**. Both clicks must be short; the second press must begin within 350 ms of the first release. Ordinary `SHIFT + note`, `SHIFT + SAVE` and `SHIFT + LOAD` gestures remain normal modifiers and do not trigger a layer change.

## Quantizer SHIFT shortcuts

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

## Discrete channel LEDs

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

Normal SAVE/LOAD stores or recalls only the active scale. `SHIFT+SAVE` and `SHIFT+LOAD` use the twelve full-configuration slots. A full configuration means the complete musical setup: both Quantizer channel configurations, channel linking, Channel-B Absolute/Relative mode, both complete Arpeggiator configurations including enable state, the selected channel, and the active Quantizer/Arpeggiator UI layer.

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

The factory presets are rooted on C and otherwise use the normal factory Quantizer configuration for both channels. Their Arpeggiator state uses the documented Arpeggiator defaults and starts disabled.

## Arpeggiator layer

The **Arpeggiator is an official second performance layer** rather than a hidden shortcut. Its default produces the rapid scale-aware pitch cycling associated with classic 8-bit home computers and game music, but the same engine can also run with external clock ratios, alternate patterns and shapes, 1–12 step lengths, up to four octaves, per-step triggers, reset/clock sync and swing.

Double-click **SHIFT by itself** to switch from the Quantizer to the Arpeggiator UI layer. Both clicks must be short; the second press must begin within 350 ms of the first release. Entering the Arpeggiator layer enables the selected channel automatically (both channels when linked), so the default FREE arpeggio becomes audible immediately. The complete ring flashes **green twice** to confirm activation.

Double-click **SHIFT again** to return to the Quantizer layer. This disables Arpeggiator playback on both channels and flashes the complete ring **red twice**. After either confirmation the normal scale/quantized-note display is restored. Channel A and Channel B keep independent Arpeggiator settings and phase while the layer is active unless explicitly linked.

The Arpeggiator layer deliberately keeps the **same interaction grammar** as the Quantizer layer. The normal note buttons still edit the selected scale and the ring still shows that scale. Only the `SHIFT + note` menu changes. Scalar Arpeggiator parameters are selected exactly like Quantizer parameters: choose the function with `SHIFT + note`, release SHIFT, then select the value with one of the twelve note buttons.

The Arpeggiator-layer SHIFT functions are:

| Shortcut | Function |
|---|---|
| SHIFT+C | Enable |
| SHIFT+C# | Rate / clock ratio |
| SHIFT+D | Pattern |
| SHIFT+D# | Shape |
| SHIFT+E | Length |
| SHIFT+F | Range |
| SHIFT+F# | Step Trigger |
| SHIFT+G | FREE / RESET / CLOCK |
| SHIFT+G# | Swing |
| SHIFT+A | Link |
| SHIFT+A# | Channel A |
| SHIFT+B | Channel B |

`SHIFT+C` toggles the selected Arpeggiator immediately. Enable is confirmed by **two full-ring green flashes**; disable/bypass is confirmed by **two full-ring red flashes**. After the second flash the ring automatically returns to the normal scale/quantized-note display.

The factory/default Arpeggiator mode is **FREE**, so it runs from its internal rate without any external clock. **CLOCK** is an explicitly selected additional mode; in CLOCK mode the channel's Sample/Gate jack becomes its external clock input and the CV path is processed continuously without changing the stored Track/Sample setting. Automatic live-state restore may restore CLOCK only when the user previously selected it. Step Trigger can remain OFF for the classic pitch-only effect or be enabled to generate the normal 5 ms trigger pulse on every Arpeggiator step.

The complete parameter tables, pattern/shape definitions, sync semantics, persistence rules and examples are documented in [README_ARPEGGIATOR.md](README_ARPEGGIATOR.md).

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
- active calibration shows a repeating green / red / amber comparison pattern so both emitter levels and the resulting amber balance remain visible together;
- a newly selected step is marked briefly by a dark gap at that ring position;
- the Channel A status-LED pair identifies green editing and the Channel B pair identifies red editing;
- entering the mode or switching colour maps the current PWM value to the nearest of the twelve available steps;
- hold SHIFT alone for 5 seconds to save and leave calibration.

See [README_CALIBRATION.md](README_CALIBRATION.md) for the detailed procedure.

## Startup sequences

Startup animation can be disabled in configuration. When sequence rotation is enabled, the firmware cycles through four animations on successive power-ups and stores the **next sequence index** in a dedicated EEPROM byte.

1. **Color Fade** — all twelve LEDs fade in/out together: green, red, then amber.
2. **Glowworm** — clockwise green, red and amber revolutions with a dimming tail.
3. **Cog** — repeating pairs of red, green and amber rotate for three revolutions.
4. **Sparkles** — pseudo-random multi-colour twinkles.

The twelve-note ring portion of **every** sequence is capped at **1500 ms**; current timings are 840 ms for Color Fade, 1440 ms for Glowworm, 1440 ms for Cog and 1450 ms for Sparkles. The separate four-status-LED self-test follows afterwards and is not part of this ring-animation limit. After the ring animation, the four discrete status LEDs flash once each in physical order before normal operation begins. After sequence 4 the selector wraps back to sequence 1; invalid or erased sequence-state data also falls back to sequence 1.

# Technical reference

## Hardware compatibility

The default board profile follows the original Free Modular Quantizer hardware.

**Hardware is a fixed constraint of this firmware project.** Firmware changes must remain compatible with the existing module; PCB, component or wiring changes are explicitly out of scope and belong to a separate hardware project.

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

## Why behaviour differs from the original firmware in a few places

The original Rust firmware remains the behavioural reference, and its compact design is one of the reasons the module is so attractive for DIY. During the reimplementation, however, a small number of concrete limitations and defects could be verified directly in the original source. The items below are deliberately limited to those cases. They do **not** include bugs introduced and fixed during development of this C++ implementation.

| Area | Original firmware behaviour | This implementation |
|---|---|---|
| Negative scale-degree shift | `step_in_scale()` correctly selects a downward direction for negative values, but then iterates over `0..num_steps`. For a negative `num_steps` that range is empty, so negative scale shifts perform no steps at all. | The magnitude and direction are handled separately. Negative values now step downward through the enabled notes exactly as the UI indicates. |
| Trigger delay in Track-and-Hold | In Track-and-Hold, a HIGH trigger resets `input_trigger_timer` to zero on every processing step. With a non-zero trigger delay, the timer therefore cannot reach the configured delay while the gate remains HIGH; the delayed update can occur only after the input goes LOW and the timer starts advancing. | Trigger detection, delay timing and UI indication use separate state. A HIGH gate starts one delay interval; once it expires, Track-and-Hold follows the CV continuously for as long as the gate remains HIGH. |
| Empty power-on scale | `QuantizerChannel::new()` initialises all twelve scale notes to `false`. With no saved scale loaded, the quantizer therefore starts with no enabled pitch classes and the quantization fallback is 0 semitones until the user creates or loads a scale. | The factory state is a chromatic scale, so a freshly flashed or erased module is immediately usable. The UI also prevents the final enabled note from being disabled accidentally. |
| ADC full-scale conversion | The original `adc_to_semitones()` converts a 10-bit reading by shifting it into an `I1F15` value and interpolating as though the input could reach `0x7FFF`. The source itself notes that the actual maximum after the shift is `0x7FE0`. Consequently a raw ADC value of 1023 does not map exactly to the configured 120-semitone endpoint. | ADC conversion is performed directly from the real 10-bit range `0…1023`, with rounded integer arithmetic, so 0 maps exactly to 0 semitones and 1023 maps exactly to 120 semitones. Per-channel gain and offset calibration are available on top of that mapping. |
| EEPROM record integrity | A saved scale or configuration is considered valid solely when a one-byte sentinel is present. The sentinel is written **before** the payload, and there is no checksum, format version or payload validation. A reset or interrupted write can therefore leave a slot marked as occupied even when its payload is incomplete or stale; arbitrary EEPROM contents can also be accepted as configuration values. | Persistent records are versioned and CRC-checked and their decoded values are validated before use. Invalid, incomplete or erased records are rejected. Full-configuration slots without valid user data expose known factory presets instead. |
| EEPROM writes in the UI path | Save and erase operations are issued synchronously from the menu/persistence path. This keeps the implementation simple, but EEPROM programming latency is part of the foreground interaction path. | A single shared asynchronous EEPROM writer queues writes and lets normal processing continue. Persistence operations report whether a write was accepted instead of assuming immediate completion. |
| LED colour balance | Red and green brightness are fixed at compile time (`0xFFF` red and `0x04F` green). There is no user-accessible calibration mechanism, so balancing depends entirely on those constants matching the particular LEDs and resistor population of the built module. | Red and green levels are independently configurable and can be calibrated on the finished hardware over the complete TLC5947 `0…4095` range. Calibration keeps green, red and amber visible together for direct visual balancing, and the same calibrated levels are used by normal rendering and startup effects. |
| Toolchain fragility | The firmware depends on an AVR Rust nightly toolchain and several unstable language/compiler features. In the scalar submenu code the original source also contains an inline-assembly workaround because, without it, an optimisation issue could cause `button_idx` to become zero; the accompanying comment explicitly describes this as either a compiler bug or possible memory corruption/optimisation behaviour. | The reimplementation uses a conventional PlatformIO/C++17 AVR toolchain and expresses the menu calculation directly without the compiler-specific assembly workaround. Native tests and AVR builds are part of CI. |

These differences are not intended as criticism of the original project. Quinn Freedman's implementation is deliberately compact and pragmatic, and that simplicity is a major strength of the design. This section exists to make the behavioural differences explicit and to document why the replacement firmware intentionally diverges at these specific points.

## Persistence

User records are versioned and CRC checked. Invalid or incomplete records are rejected rather than silently interpreted as valid data.

EEPROM writes use a non-blocking writer. Automatic live-state restore/autosave is enabled: after a short quiescent interval the complete musical working state is written to a versioned, CRC-protected wear-levelled live-state ring. This includes both Quantizer configurations, both Arpeggiator configurations including ON/OFF, selected channel, the active Quantizer/Arpeggiator UI layer and LED brightness calibration. Runtime phase and the current Arpeggiator step remain transient. A reboot therefore returns to the same front-panel layer that was active before power-off; restoring that layer is side-effect free and does not replay activation feedback.

## Configuration

User-adjustable constants are grouped under:

```text
lib/fmq/include/fmq/config/
```

Important files:

- `ProductConfig.h` — factory mode, scale, quantizer limits and Arpeggiator limits/defaults
- `AnalogConfig.h` — ADC, DAC and resistor-ladder assumptions/calibration
- `LedConfig.h` — LED brightness and startup animations
- `UiConfig.h` — debounce, menu timings, Quantizer/Arpeggiator mappings and layer-switch gesture
- `RuntimeConfig.h` — scheduler, diagnostics and serial settings
- `PersistenceConfig.h` — persistence behaviour
- `FactoryPresets.h` — built-in full-config fallback scales

See [README_CONFIGURATION.md](README_CONFIGURATION.md).

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

The native suite currently contains **29 independently runnable suites and 237 default test cases**, including exhaustive/matrix checks for ADC/DAC conversion, all scale masks, Track/Sample delays, Glide values, transposition ranges, EEPROM corruption, the complete Arpeggiator UI layer, external-clock behaviour including ISR-captured microsecond edge timing, and per-channel Arpeggiator isolation. System tests drive simulated CV/gate inputs through the production quantizer path and verify DAC codes, triggers and status LEDs at 1 ms resolution. See [README_TESTING.md](README_TESTING.md) for the test strategy and resolved specification findings. AVR-specific behaviour is additionally compiled in CI for both supported Nano bootloader variants; analogue behaviour still requires real-hardware validation.

The AVR CI also enforces an engineering resource budget. The application-flash limit is **92.5% of the 30,720-byte Nano application space** and the static-SRAM limit is **70% of 2 KB**.

Release history is maintained in [CHANGELOG.md](CHANGELOG.md).

## GitHub Actions

Two workflows are included under `.github/workflows/`.

### `ci.yml`

Runs on pushes, pull requests and manual dispatches. It:

1. installs PlatformIO Core;
2. runs all 29 native test suites as independent matrix jobs (`fail-fast: false`);
3. runs a separate aggregate coverage job and uploads HTML/XML/text reports;
4. builds both Nano environments;
5. checks the AVR flash/SRAM engineering budget;
6. uploads the resulting `.hex` and `.elf` files as workflow artifacts.

### `release.yml`

Runs for version tags matching `v*` and can also be started manually. It:

1. resolves the firmware version and selects the newest compatible `docs/manual/quantizer-user-manual.X.Y.Z.odt` whose manual version is not newer than the firmware;
2. runs the native tests;
3. builds both Nano bootloader variants and enforces the AVR flash/SRAM resource budgets;
4. collects the firmware, README and licence;
5. publishes the selected ODT unchanged and, when a compatible manual exists, attempts a headless LibreOffice PDF export using the required Ubuntu fonts;
6. creates `SHA256SUMS.txt` and `MD5SUMS.txt` over every generated release artifact, including manual files when present;
7. builds human-readable release notes from the matching `CHANGELOG.md` section, including its short release summary, detailed changelog excerpt and compare link;
8. uploads a workflow artifact;
9. when triggered by a version tag, verifies that the tag already exists and creates the corresponding GitHub Release with the generated notes and build products.

A missing compatible manual does **not** block a firmware release; the workflow emits a warning and continues without manual assets. A PDF export failure likewise leaves the versioned ODT available and does not fail the firmware release. The release workflow deliberately does not rely on GitHub's generic `--generate-notes` output. A release section must contain a `### Release summary` paragraph in `CHANGELOG.md`; this keeps the public release description concise while preserving the detailed change history below it. Release assets include both SHA-256 and MD5 checksum manifests for integrity verification.

The workflow structure follows PlatformIO's documented GitHub Actions approach: install PlatformIO Core in CI and use `pio run` / `pio test` from the project root.

## Documentation

- [README.md](README.md) — user-oriented project overview and front-panel quick reference
- [`docs/manual/`](docs/manual/README.md) — end-user manual workspace and manual editing/licensing information
- [README_CONFIGURATION.md](README_CONFIGURATION.md) — firmware configuration reference
- [README_CALIBRATION.md](README_CALIBRATION.md) — detailed hardware and LED calibration workflow
- [README_ARPEGGIATOR.md](README_ARPEGGIATOR.md) — complete second-layer Arpeggiator operation, timing, sync and persistence reference
- [README_TESTING.md](README_TESTING.md) — native unit, integration and system signal-path test strategy
- [CHANGELOG.md](CHANGELOG.md) — public release history and queued unreleased changes
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

**From Munich With ♥**
