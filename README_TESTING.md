# Testing and verification

The test suite is intended to be a safety net for firmware changes, not a collection of smoke tests. Native tests execute the production domain/application code against deterministic simulated inputs and verify externally observable outputs and millisecond timing wherever the hardware boundary permits it.

The current default suite contains **26 independently runnable PlatformIO test suites and 168 default test cases**. Several of those test cases execute exhaustive or matrix checks internally, so the number of assertions is substantially higher than the test-case count.

Examples of exhaustive work performed by the suite include:

- all 1024 legal ADC input codes;
- the complete Q8.8 output pitch range for DAC monotonicity;
- all 4095 non-empty 12-bit scale masks against boundary/probe pitches;
- all twelve supported trigger-delay values in both Track-and-Hold and Sample-and-Hold;
- all twelve Glide settings;
- every supported pre-, scale- and post-shift value;
- all twelve factory Arpeggiator scales;
- all twelve root pitch classes for Arpeggiator interval generation;
- hundreds of exact 24 ms Arpeggiator phase boundaries;
- byte-by-byte corruption injection into complete scale and full-configuration EEPROM records;
- all twelve logical note LEDs and all four logical LED colours;
- the complete 0..4095 LED intensity range for monotonic PWM scaling.

## Test levels

### Unit tests

Unit tests verify deterministic components in isolation and intentionally push their boundaries:

| Suite | Main responsibility |
|---|---|
| `unit/test_scale` | scale navigation and empty-scale safety |
| `unit/test_quantizer_boundaries` | pitch boundaries, hysteresis and all scale masks |
| `unit/test_transposition_matrix` | full signed pre/scale/post shift ranges and ordering |
| `unit/test_pitch` | representative ADC/DAC conversion vectors |
| `unit/test_pitch_exhaustive` | exhaustive monotonic ADC/DAC transfer functions |
| `unit/test_buttons` | button and long-press state machines |
| `unit/test_ladder_boundaries` | exact ladder values, midpoints, calibration and 64 ms debounce boundary |
| `unit/test_crc` | CRC reference vectors and bit-error detection |
| `unit/test_brightness` | brightness-step mapping |
| `unit/test_led_frame_matrix` | TLC5947 logical/physical order, colours and full PWM intensity range |
| `unit/test_retro_arpeggiator` | core Arpeggiator examples and clock wraparound |
| `unit/test_retro_arpeggiator_matrix` | all factory scales, all roots, sparse scales and phase boundaries |
| `unit/test_retro_arpeggiator_gesture` | 3-second SHIFT-only activation gesture |
| `unit/test_startup_sequence_store` | startup-sequence persistence |

### Integration tests

Integration tests verify collaborating production components rather than reimplementing their logic in the test:

| Suite | Main responsibility |
|---|---|
| `integration/test_controls` | raw front-panel input -> debounced menu input |
| `integration/test_menu` | menu operations, configuration, save/load and calibration flow |
| `integration/test_menu_shortcuts` | one dedicated test for every SHIFT + note command |
| `integration/test_persistence` | save/load, CRC, live-state ring and wear levelling |
| `integration/test_persistence_faults` | per-byte corruption, incomplete records, busy writer and validation |
| `integration/test_startup` | all startup-animation sequences and timing |

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

The harness deliberately calls the real `QuantizerState`, `RetroArpeggiator`, `adcToSemitones()` and `semitonesToDac()` implementations. It does not contain a second quantizer implementation that could accidentally agree with itself.

| Suite | Main responsibility |
|---|---|
| `system/test_signal_path` | representative complete CV/gate -> DAC/trigger signal paths |
| `system/test_sample_timing_matrix` | exact Track/Sample behaviour, all delay values and input-LED timing |
| `system/test_glide_trigger_matrix` | all Glide levels, monotonicity, 95% timing order, 5 ms trigger and 65 ms LED |
| `system/test_channel_matrix` | simultaneous A/B operation, independent gates/configs, Relative B and per-channel Arpeggiator scales |

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

A separate aggregate coverage job runs the complete instrumented suite and uploads text, XML and detailed HTML coverage reports. AVR builds for both Nano bootloader variants remain separate CI jobs.

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
pio test -e native -f unit/test_retro_arpeggiator_matrix
pio test -e native -f integration/test_persistence_faults
pio test -e native -f system/test_sample_timing_matrix
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

## Known specification findings discovered by the expanded suite

The expanded tests have already found two behaviours that should not be silently normalised into passing tests:

1. **Fresh Hysteresis state is biased toward C.** `Hysteresis` currently starts with `lastOutput_ == 0` and immediately applies a C-centred hold band before any real note has been emitted. A fresh chromatic input at exactly 0.5 semitone therefore resolves to C instead of the specified upward tie. The reproducer is guarded by `FMQ_ENABLE_KNOWN_FAILURE_TESTS` pending approval of the firmware fix.
2. **The resistor-ladder decoder has no explicit plausibility window.** The current implementation intentionally mirrors the original nearest-value decoder, so every ADC value is assigned to some key/rest candidate. TA-064/065 require a validity window so implausible readings can be rejected. A guarded regression test documents that gap pending approval of the behavioural change.

These checks are intentionally **not** rewritten to assert the current defective behaviour. That would turn a known defect into a contract.

To run the known-issue characterisation tests explicitly:

```text
pio test -e native_known_issues -f unit/test_quantizer_boundaries
pio test -e native_known_issues -f unit/test_ladder_boundaries
```

This environment is expected to fail until the corresponding fixes are approved and implemented. It is not part of the normal CI merge gate.

## Coverage reports

Locally:

```text
python -m pip install gcovr
pio test -e native_coverage
gcovr --root . --filter lib/fmq/src --exclude test --html-details coverage.html
gcovr --root . --filter lib/fmq/src --exclude test --txt
```

Coverage is a secondary metric. A high line percentage is not considered sufficient when a timing edge, state transition, corruption case or externally observable requirement is untested.

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
