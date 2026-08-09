# Testing and verification

The test suite is intended to be a safety net for firmware changes, not a collection of smoke tests. Native tests execute the production domain/application code against deterministic simulated inputs and verify externally observable outputs and millisecond control-loop timing wherever the hardware boundary permits it; dedicated Arpeggiator tests additionally exercise ISR-style external-clock timestamps in microsecond units.

The current default suite contains **29 independently runnable PlatformIO test suites and 231 default test cases**. Several of those test cases execute exhaustive or matrix checks internally, so the number of assertions is substantially higher than the test-case count.

Examples of exhaustive work performed by the suite include:

- all 1024 legal ADC input codes;
- the complete Q8.8 output pitch range for DAC monotonicity;
- all 4095 non-empty 12-bit scale masks against boundary/probe pitches;
- all twelve supported trigger-delay values in both Track-and-Hold and Sample-and-Hold;
- all twelve Glide settings;
- every supported pre-, scale- and post-shift value;
- all twelve factory Arpeggiator scales;
- all twelve root pitch classes for Arpeggiator interval generation;
- exact boundaries for all twelve internal Arpeggiator rates and the complete clock-ratio table;
- all Arpeggiator patterns, shapes, supported lengths/ranges and swing limits;
- per-channel Arpeggiator enable/config isolation plus linked-copy behaviour;
- exact two-flash full-ring Arpeggiator enable/disable feedback for both `SHIFT+C` and the 3-second layer transition, including green-on-enable, red-on-disable, dark intervals and restoration of the normal scale display;
- byte-by-byte corruption injection into complete scale and full-configuration EEPROM records;
- all twelve logical note LEDs and all four logical LED colours;
- the complete 0..4095 LED intensity range for monotonic PWM scaling.

## Test levels

### Unit tests

Unit tests verify deterministic components in isolation and intentionally push their boundaries:

| Suite | Main responsibility |
|---|---|
| `unit/test_scale` | scale navigation and empty-scale safety |
| `unit/test_quantizer_boundaries` | pitch boundaries, first-sample tie handling, hysteresis and all scale masks |
| `unit/test_transposition_matrix` | full signed pre/scale/post shift ranges and ordering |
| `unit/test_pitch` | representative ADC/DAC conversion vectors |
| `unit/test_pitch_exhaustive` | exhaustive monotonic ADC/DAC transfer functions |
| `unit/test_buttons` | button and long-press state machines |
| `unit/test_ladder_boundaries` | exact ladder values, plausibility windows, invalid gaps, calibration and 64 ms debounce boundary |
| `unit/test_crc` | CRC reference vectors and bit-error detection |
| `unit/test_brightness` | brightness-step mapping |
| `unit/test_led_frame_matrix` | TLC5947 logical/physical order, colours and full PWM intensity range |
| `unit/test_arpeggiator` | core Arpeggiator examples, microsecond external-clock timing, multi-edge counting and wraparound |
| `unit/test_arpeggiator_matrix` | all factory scales, all roots, rates, patterns, shapes, lengths/ranges and clock ratios |
| `unit/test_ui_layer_gesture` | exact 3-second SHIFT-only Quantizer/Arpeggiator layer switch, cancellation and wraparound |
| `unit/test_arpeggiator_channels` | independent A/B enable state plus deterministic linked-mode toggling |
| `unit/test_startup_sequence_store` | startup-sequence persistence |

### Integration tests

Integration tests verify collaborating production components rather than reimplementing their logic in the test:

| Suite | Main responsibility |
|---|---|
| `integration/test_arpeggiator_layer` | full 3-second entry → enable → two green flashes → audible FREE-running step path, 3-second exit → disable all → two red flashes, unchanged scale display/editing in the second layer, SHIFT-based Arpeggiator menu grammar, every Arpeggiator parameter, Link/A/B navigation, full-config UI-layer restore, reboot restoration and first-hold OFF regression coverage |
| `integration/test_controls` | raw front-panel input -> debounced menu input, including pre-debounce ladder activity used to cancel the layer-hold race |
| `integration/test_menu` | menu operations, configuration, save/load and calibration flow |
| `integration/test_menu_shortcuts` | one dedicated test for every SHIFT + note command |
| `integration/test_persistence` | save/load, CRC, full-config/live UI-layer round-trips including ARP-off layer state, wear levelling and v5→v6 live-state migration |
| `integration/test_persistence_faults` | per-byte corruption, incomplete records, busy writer and validation |
| `integration/test_startup` | all startup-animation sequences and timing |


LED calibration regression coverage also verifies that active calibration exposes green, red and amber simultaneously, that a selected step gets a temporary position marker without changing the stored value, and that the A/B editor indication follows green/red selection.

### Regression tests

| Suite | Main responsibility |
|---|---|
| `regression/test_quantizer` | previously corrected quantizer behaviour that must never regress |
| `regression/test_hardware_ui` | original PCB ladder and TLC5947 compatibility assumptions |

### System / virtual-module tests

The system tests model the firmware-visible Eurorack boundary. `QuantizerTestRig` feeds 10-bit CV samples and digital gate levels into the real production quantizer path and captures one result for every 1 ms control tick.

Captured values include:

- Channel A/B nominal quantized pitch;
- Channel A/B post-Glide pitch;
- post-Arpeggiator pitch;
- final MCP4922 DAC code;
- trigger outputs A/B;
- input-status LEDs A/B;
- output-status LEDs A/B.

The harness deliberately calls the real `QuantizerState`, `ArpeggiatorBank`/`Arpeggiator`, `adcToSemitones()` and `semitonesToDac()` implementations. It does not contain a second quantizer implementation that could accidentally agree with itself.

| Suite | Main responsibility |
|---|---|
| `system/test_arpeggiator_clock` | FREE/RESET/CLOCK integration, clock ratios, continuous CV override, channel clock isolation and Arpeggiator step triggers |
| `system/test_signal_path` | representative complete CV/gate -> DAC/trigger signal paths |
| `system/test_sample_timing_matrix` | exact Track/Sample behaviour, all delay values and input-LED timing |
| `system/test_glide_trigger_matrix` | all Glide levels, monotonicity, 95% timing order, 5 ms trigger and 65 ms LED |
| `system/test_channel_matrix` | simultaneous A/B operation, independent gates/configs, Relative B, per-channel Arpeggiator scales and A-only/B-only Arpeggiator output isolation |

## Fine-grained CI

GitHub Actions runs **every native suite as an individual matrix job** with `fail-fast: false`. A failure therefore identifies the affected behaviour directly instead of reporting only that a monolithic `pio test` command failed.

The native build is compiled with:

```text
-Wall
-Wextra
-Werror
-Wconversion
-Wsign-conversion
-Wshadow
-Wpedantic
```

A separate aggregate coverage job runs the complete instrumented suite and uploads text, XML and detailed HTML coverage reports. AVR builds for both Nano bootloader variants remain separate CI jobs. After each AVR build, `scripts/check_avr_resource_budget.py` enforces the current engineering headroom targets: no more than 85% of the conservative 30,720-byte application-flash budget and no more than 70% of the ATmega328P's 2 KB static SRAM.

## Running tests locally

All supported native tests:

```text
pio test -e native
```

One suite only:

```text
pio test -e native -f system/test_glide_trigger_matrix
```

Examples:

```text
pio test -e native -f unit/test_arpeggiator_matrix
pio test -e native -f integration/test_arpeggiator_layer
pio test -e native -f system/test_arpeggiator_clock
```

## Requirement-oriented verification

Tests reference the requirement IDs from the firmware requirements specification in comments. The target is **requirement coverage**, not merely source-line coverage.

Representative traceability:

| Requirement | Verification |
|---|---|
| FA-001..003 | simultaneous two-channel system tests |
| FA-015..019 | empty/sparse/all-scale-mask quantizer tests |
| FA-020..024 | explicit upper/lower hysteresis and scale-change tests |
| FA-026..030 | Absolute/Relative B and channel independence matrix, including full-range A→B isolation at quantized-pitch and DAC-code level |
| FA-036..048 | Track/Sample and full 0..11 delay matrix |
| FA-049..060 | complete signed transposition matrix |
| FA-064..069 | all Glide levels and no-retrigger system tests |
| FA-070..075 | exact trigger/output-LED/input-LED timing |
| TA-025..028 | all 1024 ADC codes |
| TA-041..042 | Glide monotonicity and ordered transition times |
| TA-050..055 | full Q8.8-to-DAC monotonic transfer path |
| TA-060..065 | ladder nominal values, midpoint/debounce/calibration tests |
| TA-066..081 | TLC5947 logical order, colour encoding and PWM scaling |
| SM-001..007 | CRC/fault injection/incomplete-write/validation tests |

The expanded persistence tests also round-trip the complete `StoredConfiguration`: Quantizer state, selected channel, active UI layer and both per-channel Arpeggiator configurations. Byte-by-byte corruption of a full-config record must invalidate the whole record rather than partially accepting Arpeggiator data.

## Specification findings closed by regression tests

Two defects found by the expanded suite are now fixed and permanently covered:

1. **Fresh Hysteresis state no longer assumes C.** A new channel has no previous note until the first real quantization result. An exact chromatic 0.5-semitone tie therefore follows FA-018 and rounds upward before hysteresis becomes active.
2. **The resistor-ladder decoder now has an explicit plausibility window.** After nearest-candidate selection, a key is accepted only within ±10 ADC counts of its nominal value. Gap values and the released/high region are rejected as no button, satisfying TA-064/065 without changing the proven nominal ladder values.

These are regular CI tests, not opt-in expected failures.

## Coverage reports

Locally:

```text
python -m pip install gcovr
pio test -e native_coverage
gcovr --root . --filter lib/fmq/src --exclude test --html-details coverage.html
gcovr --root . --filter lib/fmq/src --exclude test --txt
```

Coverage is a secondary metric. A high line percentage is not considered sufficient when a timing edge, state transition, corruption case or externally observable requirement is untested.

`scripts/platformio_coverage.py` explicitly adds GCC's `--coverage` option to the final native link step. This is required because instrumented objects reference the gcov runtime (`__gcov_init`, `__gcov_exit`, `__gcov_merge_add`); compile-time instrumentation without the matching link flag produces unresolved-symbol linker failures in CI.

## What native tests cannot prove

Native tests cannot validate analogue or physical effects such as:

- CV-input resistor and op-amp tolerances;
- real AVCC/reference voltage;
- ADC noise, acquisition settling and INL/DNL;
- MCP4922 INL/DNL;
- output-amplifier gain and offset;
- gate-edge shape and protection circuitry;
- physical SPI signal integrity;
- actual LED brightness and colour balance;
- power-supply behaviour.

Those require calibrated hardware-in-the-loop measurements. The native suite exists to make the digital behaviour deterministic before hardware uncertainty is introduced.

### Strict compiler warnings and Unity

The native test environments deliberately treat warnings in FMQ production code and
FMQ test code as errors (`-Werror`, `-Wconversion`, `-Wsign-conversion`, `-Wshadow`,
`-Wpedantic`). These flags are intentionally **not** enforced on PlatformIO's generated
Unity adapter or the upstream Unity C sources. Recent host GCC versions report
sign-conversion warnings in those external sources which are outside this project's
control.

`scripts/platformio_strict_warnings.py` implements this boundary with PlatformIO build
middleware. It removes only the project-specific strict diagnostics for generated or
third-party Unity sources; FMQ library sources and all test suites retain the full
warning policy. This keeps third-party diagnostics from masking the actual test result
without weakening compilation checks for code maintained in this repository.


## First-party warning policy

The native test environment keeps baseline compiler warnings (`-Wall -Wextra`) global,
so PlatformIO-generated code and the third-party Unity framework compile with their own
upstream-compatible warning policy. The repository then adds `-Werror`, `-Wconversion`,
`-Wsign-conversion`, `-Wshadow`, and `-Wpedantic` **only** to first-party production
sources under `lib/fmq/src/` and test sources under `test/`. This is implemented by
`scripts/platformio_strict_warnings.py` using PlatformIO build middleware.

This is intentionally additive rather than subtractive: external/generated Unity code
never receives the project-only strict flags, while every translation unit owned by this
repository remains warning-clean under the stricter policy.
