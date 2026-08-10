# ATmega328P timing qualification

The firmware scheduler is designed around a 1 kHz control period. Host tests verify state-machine timing, but only the real Nano can demonstrate that the complete board-level control path consistently finishes before the next 1 ms tick.

## Contents

- [Measurement build](#measurement-build)
- [Probe signal](#probe-signal)
- [Flashing and setup](#flashing-and-setup)
- [Stress scenario](#stress-scenario)
- [Measurements](#measurements)
- [Interpretation](#interpretation)

## Measurement build

PlatformIO provides a dedicated non-release environment:

```sh
pio run -e nanoatmega328new_timing
```

or, for a directly connected Nano during the flashing step:

```sh
pio run -e nanoatmega328new_timing -t upload
```

This environment defines `FMQ_TIMING_PROBE=1`. It is deliberately excluded from release packaging.

> [!WARNING]
> The timing build repurposes Arduino Nano **D1/TX (PD1)** as an oscilloscope probe output. Serial diagnostics and the boot-time calibration console are disabled in this build. Do not use this image as normal user firmware.

## Probe signal

D1 goes HIGH in the Timer2 compare ISR that schedules each 1 kHz control tick and goes LOW only after the corresponding control work has completely finished. The HIGH pulse therefore measures scheduling-to-completion latency, not just the body of the main processing function. It includes:

- Timer2 scheduling/dispatch latency before the main control body begins;
- ADC snapshot processing;
- Quantizer A/B processing;
- front-panel/UI processing;
- Arpeggiator processing;
- DAC and status-output updates;
- scheduled LED-frame work;
- one asynchronous EEPROM service step;
- any ISR time that pre-empts the control cycle while the probe is HIGH.

Direct AVR port writes are used so probe overhead is deterministic and minimal. If another 1 kHz timer event occurs before the previous work has finished, the pin is already HIGH and the pulse naturally extends through the missed deadline/backlog.

## Flashing and setup

1. Flash the timing environment while following the normal firmware installation safety procedure.
2. Disconnect USB after flashing is complete.
3. Power the module in the normal Eurorack test setup.
4. Connect oscilloscope ground to module ground.
5. Probe Nano D1/TX.
6. Use a timebase that can resolve individual sub-millisecond pulses and long persistence over many seconds.

> [!CAUTION]
> Connecting an oscilloscope probe is a measurement operation, not a hardware modification. Avoid shorts around the Nano headers and exposed PCB. Never move Eurorack power connections while energized.

## Stress scenario

Exercise combinations likely to maximize control-loop work rather than measuring only an idle module:

- both CV inputs moving;
- both channels active;
- Arpeggiator enabled on both channels;
- external CLOCK edges present;
- high clock multiplier;
- Step Trigger enabled;
- frequent note changes causing DAC writes;
- LED state changes;
- front-panel activity;
- a live-state autosave becoming due so the asynchronous EEPROM writer is active.

Capture enough time to include LED refresh ticks and EEPROM activity.

## Measurements

Record at minimum:

| Measurement | Required result |
|---|---|
| Maximum HIGH pulse | `< 1.000 ms` |
| Typical HIGH pulse | informational |
| Overlapping/back-to-back pulses caused by backlog | none expected |
| Control pulse at or above one full period | **none** (`< 1.000 ms` for every captured pulse) |

Also verify the physical output-trigger HIGH time independently; its nominal firmware duration is 5 ms.

## Interpretation

> [!IMPORTANT]
> A successful AVR compile or flash/SRAM resource check does **not** prove the 1 ms timing deadline. Conversely, the timing-probe build is instrumentation and must not replace the normal release artifact for musical testing.

If the maximum pulse reaches or exceeds 1 ms, treat it as a release blocker for any change that introduced the regression. A long capture should also be inspected for sustained back-to-back control pulses, which indicate the scheduler has no idle margin. Record the exact stimulus and scope capture so the path can be reproduced.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
