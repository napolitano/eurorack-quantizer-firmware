# Requirements and test traceability

This document connects the original Quantizer firmware acceptance criteria and requirement groups to executable verification in this repository. It complements [README_TESTING.md](../../README_TESTING.md): that document explains the test architecture; this one answers **which requirement is proven where**.

> [!IMPORTANT]
> The original requirement specification predates the expanded firmware 0.2.x Arpeggiator layer. Its `FA-*`, `TA-*`, and `SM-*` identifiers therefore describe the Quantizer baseline, not every later extension. Arpeggiator requirements are verified by the dedicated unit, integration, and system suites documented in [README_ARPEGGIATOR.md](../../README_ARPEGGIATOR.md).

## Contents

- [Traceability policy](#traceability-policy)
- [Acceptance criteria](#acceptance-criteria)
- [Requirement-group coverage](#requirement-group-coverage)
- [What remains target- or hardware-dependent](#what-remains-target--or-hardware-dependent)
- [CI validation](#ci-validation)

## Traceability policy

The machine-readable source for the acceptance table is [`test/requirements-traceability.json`](../../test/requirements-traceability.json). CI validates that:

- all twenty acceptance criteria `AC-01` through `AC-20` are present exactly once;
- every criterion references at least one executable test;
- every referenced test file exists;
- every referenced test case is actually registered with `RUN_TEST(...)`.

This deliberately avoids using source-line coverage as a substitute for behavioral verification.

## Acceptance criteria

| ID | Acceptance criterion | Primary verification | Status |
|---|---|---|---|
| AC-01 | Both channels quantize in parallel and independently | `system/test_channel_matrix` | Native |
| AC-02 | All twelve pitch classes can be enabled and disabled | `integration/test_menu` — all-note-button matrix | Native |
| AC-03 | Empty scales produce the defined zero result | `unit/test_quantizer_boundaries` | Native |
| AC-04 | Nearest enabled note is selected | quantizer regression + all-scale-mask boundary matrix | Native |
| AC-05 | Exact ties resolve upward | first-sample half-semitone regression | Native |
| AC-06 | Hysteresis prevents boundary chatter | explicit upper/lower threshold tests | Native |
| AC-07 | Track-and-Hold and Sample-and-Hold behave as specified | `system/test_signal_path` | Native |
| AC-08 | Delay 0…11 works in both sample modes | `system/test_sample_timing_matrix` | Native |
| AC-09 | Positive/negative pre-, scale-, and post-shifts work | `unit/test_transposition_matrix` | Native |
| AC-10 | Relative Channel B uses CV A + CV B | signal-path + channel-matrix tests | Native |
| AC-11 | Channel linking stays coherent across edits and loads | shortcut, Arpeggiator, and scale-load integration tests | Native |
| AC-12 | Glide transition time increases monotonically | `system/test_glide_trigger_matrix` | Native |
| AC-13 | Triggers follow discrete musical target changes only | Glide trigger matrix + Arpeggiator trigger-off test | Native |
| AC-14 | DAC output continues to move during Glide | full signal-path Glide test | Native |
| AC-15 | Logical LED colors/slot indications match UI state | menu color semantics + TLC5947 frame matrix | Native + hardware |
| AC-16 | All twelve scale slots save/load correctly | twelve-slot persistence matrix | Native |
| AC-17 | All twelve full-config slots save/load correctly | twelve-slot full-configuration matrix | Native |
| AC-18 | Exact simultaneous Save+Load long hold erases user slots | menu integration regression | Native |
| AC-19 | Corrupt EEPROM data cannot become undefined state | byte-corruption + decoder-validation tests | Native |
| AC-20 | 1 kHz processing remains stable during UI/persistence activity | asynchronous-writer/state-machine tests plus target verification | Native + target |

The exact case-level references are kept in the machine-readable mapping so renames cannot silently make this table stale.

## Requirement-group coverage

| Requirement group | Verification focus |
|---|---|
| `FA-001..008` | dual-channel processing, pitch/output range, clamping |
| `FA-009..024` | note selection, empty scales, nearest-note search, tie handling, hysteresis |
| `FA-025..035` | absolute/relative Channel B, channel independence, linked configuration |
| `FA-036..048` | normalized-HIGH Track-and-Hold, Sample-and-Hold, complete delay matrix |
| `FA-049..063` | pre-/scale-/post-shift ranges, processing order, scale rotation |
| `FA-064..075` | all Glide levels, DAC progression, 5 ms trigger and output/input LED timing |
| `FA-076..082` | unlinked A/B colors, linked amber scale, A/B current-note colors, A priority on collisions |
| `FA-083..098` | all twelve SHIFT shortcuts and complete unsigned/signed selector mappings |
| `FA-099..111` | all scale/config slots, linked scale load, occupancy, atomic/corrupt records, exact erase gesture |
| `TA-025..028` | exhaustive 10-bit ADC transfer function and safe clamping |
| `TA-035..042` | quantizer termination/boundaries and Glide algorithm behavior |
| `TA-050..055` | exhaustive pitch-to-DAC monotonicity and endpoints |
| `TA-056..065` | button state machines, resistor-ladder nominal values, plausibility window, 64 ms debounce |
| `TA-066..081` | logical TLC5947 mapping, colors, order, and PWM scaling |
| `SM-001..007` | commit-last persistence semantics, CRC validation, invalid data handling, non-blocking writer |

> [!NOTE]
> Several technical requirements are properties of the AVR target configuration or physical circuit rather than portable C++ behavior. They are intentionally not forced into host unit tests merely to claim a higher requirement count.

## What remains target- or hardware-dependent

Native verification cannot prove:

- the physical 1 kHz deadline under the ATmega328P instruction budget;
- ADC reference accuracy, acquisition settling, INL/DNL, or analogue crosstalk;
- MCP4922 and output-amplifier gain/offset accuracy;
- real gate-edge quality and protection behavior;
- physical SPI integrity;
- actual red/green LED brightness balance.

Those remain part of AVR build/resource checks and hardware validation. See [README_TESTING.md](../../README_TESTING.md#what-native-tests-cannot-prove).

## CI validation

Run the traceability consistency check locally with:

```sh
python scripts/check_requirement_traceability.py
```

A stale or broken test reference fails CI. The traceability mapping should be changed only when the executable verification genuinely changes.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
