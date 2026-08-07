# Changelog

This file records public repository releases. Development notes and intermediate experiments are intentionally not tracked here.

Version numbers follow semantic-versioning conventions where practical:

- patch (`0.1.x`) — compatible fixes and small refinements;
- minor (`0.x.0`) — new features or meaningful behavioural additions;
- major (`x.0.0`) — intentionally incompatible or substantially redefined releases.

A new section should be added when a release version is explicitly declared.

## 0.1.0 — 2026-08-07

Initial public release of the PlatformIO/C++ firmware reimplementation.

### Added

- PlatformIO/C++ firmware for Arduino Nano / ATmega328P at 16 MHz.
- Support for both current and legacy Nano bootloader environments.
- Two independent quantizer channels, A and B.
- Twelve-note programmable scales.
- Track-and-Hold and Sample-and-Hold operation compatible with the original hardware normalisation.
- Optional internal Continuous mode extension, excluded from the standard UI by default.
- Pre-shift, scale-degree shift and post-shift per channel.
- Glide and trigger delay.
- Relative / Absolute Channel-B operation.
- Optional Channel A/B configuration linking.
- Twelve scale-save slots.
- Twelve full-configuration save/load slots.
- Factory fallback bank behind empty full-configuration slots:
  - Chromatic
  - Major / Ionian
  - Natural Minor / Aeolian
  - Harmonic Minor
  - Melodic Minor
  - Dorian
  - Phrygian
  - Lydian
  - Mixolydian
  - Major Pentatonic
  - Minor Pentatonic
  - Blues
- User-saved full configurations transparently override factory fallbacks; erasing user data reveals the factory bank again.
- Versioned, CRC-checked EEPROM records.
- Shared non-blocking EEPROM writer.
- Independent red and green TLC5947 brightness calibration over the complete 0…4095 range.
- Clockwise twelve-step calibration display with nearest-step mapping and filled-bar indication.
- Four rotating startup sequences:
  - Color Fade
  - Glowworm
  - Cog
  - Sparkles
- EEPROM rotation of the next startup sequence.
- Startup self-test of the four discrete activity LEDs.
- Scale-aware Retro Arpeggiator performance mode, toggled by holding SHIFT alone.
- Retro Arpeggiator intervals derived from each channel's active scale.
- Boot-time serial hardware calibration console.
- Optional runtime diagnostics.
- Central configuration split by product, UI, analogue, LED, runtime, persistence and board concerns.
- Native unit, integration and regression tests.
- GitHub Actions for native tests, AVR builds and tagged releases.
- Consistent source headers with purpose, authorship, original-project reference and SPDX licence identifier.

### Changed

- Reimplemented the original Rust firmware architecture in a maintainable PlatformIO/C++ structure rather than performing a line-by-line translation.
- Separated portable domain/application/UI/persistence code from AVR board support.
- Reduced `main.cpp` to Arduino entry-point delegation through `FirmwareController`.
- Centralised hardware pin mapping and electrical assumptions in a board profile.
- Matched the effective original ADC reference behaviour: AVCC.
- Matched the original resistor-ladder values and 64 ms debounce behaviour on the original PCB profile.
- Matched the original ADC filter semantics with median-of-three sampling.
- Converted ADC acquisition to an interrupt-driven scanner with deterministic MUX settling.
- Captured short trigger edges through external-interrupt latches.
- Restored the original default Track-and-Hold mode and standard SHIFT+4 Track/Sample toggle behaviour.
- Preserved the original hardware behaviour where an unpatched trigger/sample input is normalised HIGH, making Track-and-Hold operate continuously.
- Made SHIFT an immediate modifier so simultaneous SHIFT+note gestures behave like the original firmware.
- Prevented the final active scale note from being disabled accidentally.
- Made the factory scale chromatic so a freshly flashed module is immediately usable.
- Normalised A/B selection behaviour when channels are linked.
- Made menu/configuration edits preserve runtime trigger state instead of generating artificial activity indications.
- Rebalanced normal red/green LED defaults based on real hardware testing while keeping the values explicitly configurable.
- Made startup effects use the calibrated normal red/green levels as their full-scale reference.
- Reworked startup behaviour to use full-ring colour fades and additional animation variants rather than the earlier travelling test sequence.
- Reworked LED calibration so Channel A consistently represents green and Channel B red.
- Changed calibration from a single-position marker to a filled clockwise bar.
- Disabled automatic live-state restore/autosave by default to keep deterministic startup behaviour aligned with the original interaction model.
- Added SPI transactions and final-DAC-code caching to reduce unnecessary transfers.
- Replaced queued tick replay with missed-deadline accounting instead of reprocessing current input values as historical samples.
- Improved naming, comments, configuration ownership and removal of implementation magic numbers.

### Fixed

- Track-and-Hold plus trigger-delay behaviour that could prevent updates while a gate remained HIGH.
- Input-trigger LED behaviour in Track-and-Hold: HIGH now continuously refreshes the activity indication as on the original hardware.
- Separation of trigger-delay timing from input-activity LED timing.
- SHIFT modifier ordering that previously caused note events to arrive before SHIFT was recognised.
- A/B channel-selection and SHIFT-menu interaction failures caused by the modifier timing issue.
- ADC reference mismatch introduced during early reimplementation work.
- Resistor-ladder decoding incompatibilities caused by assumptions that did not match the original PCB.
- False A/B activity flashes caused by resetting channel runtime state during front-panel scale edits.
- Track/Sample feedback timing and documentation ambiguity.
- LED calibration A/B colour mapping.
- LED calibration range so the final step now represents the actual TLC5947 maximum of 4095.
- Loading of empty or invalid scales.
- Scale-shift behaviour at pitch-range boundaries.
- EEPROM validation of persisted enum/range values.
- EEPROM slot versioning and invalid-record handling.
- Competing EEPROM writers and blocking runtime EEPROM waits.
- Save/erase UI confirmation occurring before a persistence operation was actually accepted.
- Trigger-edge latch consumption after sustained HIGH gates.
- ADC free-running/MUX timing ambiguity.
- ADC sequence-counter width and atomic snapshot handling.
- GPIO initialisation occurring in global constructors.
- Trigger/status output polarity configuration not being applied consistently.
- DAC/output-trigger events caused purely by front-panel configuration edits.
- AVR-incompatible `<limits>` dependency.
- Duplicate local declaration in `FirmwareController::begin()` found during AVR-oriented review.
- Multiple stale or contradictory README/configuration statements accumulated during development.

### Documentation

- Added a repository-focused `README.md` covering features, hardware assumptions, build/flash, complete menu logic, Track/Sample behaviour, discrete LED semantics, factory presets, startup sequences, Retro Arpeggiator and licensing/credits.
- Added `README_CONFIGURATION.md` as the firmware configuration reference.
- Added `README_CALIBRATION.md` with step-by-step LED, ladder, CV-input and DAC-output calibration procedures.
- Added this release-oriented `CHANGELOG.md` in place of internal fix notes.
- Added a prominent acknowledgement of Quinn Freedman's original Quantizer design and Rust firmware, including its practical value as an approachable, cost-conscious and highly adaptable DIY design.
