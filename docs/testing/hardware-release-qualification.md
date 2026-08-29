# Hardware release qualification

This checklist is the target-level counterpart to the native test suite. It is intended for release candidates and other changes that can affect timing, persistence, I/O behaviour, calibration, or the user-visible control path.

> [!IMPORTANT]
> Native tests cannot prove electrical behaviour or the ATmega328P's real-time deadline on physical hardware. A release is not considered fully hardware-qualified until the applicable checks below have been run on an actual module.

## Contents

- [Scope](#scope)
- [Preconditions](#preconditions)
- [Core functional qualification](#core-functional-qualification)
- [Persistence and update qualification](#persistence-and-update-qualification)
- [Timing and clock qualification](#timing-and-clock-qualification)
- [Analog and output qualification](#analog-and-output-qualification)
- [Result recording](#result-recording)

## Scope

Use this checklist for every minor release and for any patch release that changes runtime code, persistence, timing, ADC/DAC handling, clocking, or front-panel state machines. Pure documentation-only releases do not require a new hardware run.

The hardware platform is fixed for this firmware project. Qualification never assumes PCB, component, or wiring changes.

## Preconditions

- [ ] Candidate commit/tag is identified.
- [ ] CI is green for production and calibration/maintenance images on both Nano bootloader targets.
- [ ] AVR flash/SRAM resource gates pass.
- [ ] Native coverage and requirements traceability pass.
- [ ] ASan/UBSan native run passes.
- [ ] Firmware was flashed using the documented installation procedure.
- [ ] Test module is powered from a known-good Eurorack supply.
- [ ] Oscilloscope/DMM setup is referenced to module ground correctly.

> [!CAUTION]
> Do not connect or disconnect the Eurorack power ribbon while the rack is powered. Follow the installation safety procedure in [`docs/installation/README.md`](../installation/README.md).

## Core functional qualification

- [ ] Cold boot completes the fixed <=750 ms startup fade without spurious outputs and restores the expected UI frame.
- [ ] Warm reset/reboot restores the expected live state.
- [ ] Channel A quantizes independently across representative CV values.
- [ ] Channel B quantizes independently in Absolute mode.
- [ ] Relative B correctly responds to the sum of A + B.
- [ ] Linked mode keeps both channel configurations coherent.
- [ ] Track-and-Hold follows while Gate is HIGH and holds while LOW.
- [ ] Sample-and-Hold updates only on rising edges.
- [ ] Delay values 0 and 11 are checked in both sample modes.
- [ ] Pre-, Scale-, and Post-shift are checked in both positive and negative directions.
- [ ] Glide 0 is immediate; a high Glide value moves continuously without retriggering.
- [ ] Output trigger pulse occurs on discrete target-note changes only.
- [ ] SHIFT double-click enters the Arpeggiator layer.
- [ ] The same SHIFT double-click exits the Arpeggiator layer on the first attempt.
- [ ] `SHIFT + note`, SAVE, and LOAD gestures do not accidentally toggle the UI layer.
- [ ] Arpeggiator FREE, RESET, and CLOCK modes are exercised.
- [ ] Arpeggiator Step Trigger OFF preserves pitch-only stepping.
- [ ] Arpeggiator Step Trigger ON produces the expected output pulses.

## Persistence and update qualification

- [ ] Save/load works for a representative Scale slot.
- [ ] Save/load works for a representative Full Configuration slot.
- [ ] Full Configuration restores selected channel and UI layer.
- [ ] Live state survives a normal power cycle.
- [ ] Arpeggiator-layer + ARP-OFF survives a normal power cycle.
- [ ] User presets survive a normal firmware update.
- [ ] LED calibration survives a normal firmware update.
- [ ] Persistent ADC/DAC calibration survives production -> maintenance -> production flash-only image swaps.
- [ ] Upgrade from a <=0.2.1 EEPROM image with the newest live state in the former twelfth slot restores and migrates that state.
- [ ] Reserved EEPROM byte 1023 remains untouched; a stale historical startup-selector value is ignored.
- [ ] SAVE+LOAD erase clears user slots without being described or treated as a factory reset.
- [ ] New-bootloader Nano artifact flashes and boots.
- [ ] Old-bootloader Nano artifact flashes and boots when such a board is available for qualification.

## Timing and clock qualification

The detailed oscilloscope procedure is in [`timing-qualification.md`](timing-qualification.md).

- [ ] Maximum observed 1 kHz control-cycle pulse width remains below 1.000 ms.
- [ ] No missed-deadline accumulation is observed during the stress scenario.
- [ ] Quantizer trigger pulse is nominally 5 ms.
- [ ] CLOCK input is checked at slow, typical, and fast Eurorack rates.
- [ ] CLOCK multiplication is checked at a high multiplier.
- [ ] CLOCK division is checked at a high divider.
- [ ] Swing is checked while externally clocked.
- [ ] Narrow/short but valid trigger pulses are detected consistently.

## Analog and output qualification

- [ ] CV A zero point and octave scaling are measured.
- [ ] CV B zero point and octave scaling are measured.
- [ ] DAC/output A reaches the expected calibrated range monotonically.
- [ ] DAC/output B reaches the expected calibrated range monotonically.
- [ ] The maintenance image boots with exactly three amber full-ring flashes and then holds a steady dim-amber idle ring.
- [ ] `SAVE`, `LOAD`, `SHIFT + SAVE`, and `SHIFT + LOAD` select the documented full-calibration, verify-only, ADC-only, and DAC-only workflows without any serial input.
- [ ] Channel A/B ADC fit prompts identify C# as 1.000 V and A as 9.000 V; the subsequent independent verification prompts identify C as 0.000 V and F as 5.000 V; the correct channel Input LED is on.
- [ ] During ADC acquisition the note ring is completely off; Channel A/B behavior is symmetrical.
- [ ] Matching channel Input + Output LEDs blink to request a single-channel OUT->IN loopback; both pairs blink for final dual-channel verification.
- [ ] During loopback acquisition the note ring is completely off and only the documented status LEDs remain active.
- [ ] A normal maintenance PASS produces exactly two green full-ring flashes; a failure produces two red flashes followed by the documented 1..6 red-note error code.
- [ ] Complete qualification PASS produces exactly three green full-ring flashes; qualification FAIL produces three red flashes where applicable.
- [ ] No changing TLC5947 ring PWM activity occurs during ADC/DAC measurement windows.
- [ ] Full calibration fits ADC A/B from 1.000 V and 9.000 V, verifies each input independently at 0.000 V and 5.000 V, then self-calibrates DAC A/B and persists only after final two-channel verification.
- [ ] ADC-only calibration changes the calibration generation and thereby invalidates any older qualification metadata.
- [ ] DAC-only calibration refuses to run without a valid stored calibration profile.
- [ ] Self-calibration with the loop absent or deliberately mispatched fails with the documented code and does not commit the in-progress RAM profile.
- [ ] LOAD during any wizard prompt cancels safely and restores the previously active profile.
- [ ] Final verification stores qualification residuals bound to the active/new calibration generation.
- [ ] With Eurorack power disconnected and USB connected, the maintenance image prints the stored calibration report at 115200 baud without accepting command input or performing analog verification.
- [ ] The stored report emits a compact label block only when qualification metadata matches the newest valid calibration generation.
- [ ] After restoring production firmware, perform an external DMM cross-check with C as the only enabled pitch class at representative whole-volt inputs (at minimum 0 V, 1 V, 5 V and 9 V); record input, expected output, measured output and error for both channels.
- [ ] Calibration-journal writes are confined to bytes 967…1022; qualification writes are confined to bytes 966 and 1023; preset/current-live-state regions remain unchanged.
- [ ] A sweep on CV A does not create measurable digital-path movement on B in Absolute mode beyond analog noise/crosstalk expectations.
- [ ] A sweep on CV B does not create movement on A beyond analog noise/crosstalk expectations.
- [ ] Trigger outputs reach valid Eurorack logic levels.
- [ ] Red/green/amber LED balance is visually acceptable after calibration.

## Result recording

Copy [`qualification/TEMPLATE.md`](qualification/TEMPLATE.md) to a release-specific result file, for example:

```text
qualification/v0.3.0.md
```

Record the exact firmware commit/tag, Nano/bootloader used, instruments, measured timing maxima, and any deviations. Do not mark a measurement as passed when it was not actually performed.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
