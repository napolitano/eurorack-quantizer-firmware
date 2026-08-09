# Changelog

This file records public repository releases. Development notes and intermediate experiments are intentionally not tracked here.

Version numbers follow semantic-versioning conventions where practical:

- patch (`0.1.x`) — compatible fixes and small refinements;
- minor (`0.x.0`) — new features or meaningful behavioural additions;
- major (`x.0.0`) — intentionally incompatible or substantially redefined releases.

A new section should be added when a release version is explicitly declared.

## 0.1.1 — 2026-08-07

Current 0.1.1 release line. The entries below are accumulated changes for this version; a new version is declared only when explicitly chosen.

### Added

- Expanded the native test net with fine-grained two-channel isolation checks: in Absolute mode Channel B is verified at quantized-pitch and final DAC-code level while Channel A sweeps the complete 10-bit ADC range; the complementary Relative-mode behaviour is verified explicitly.
- Added Sample-and-Hold and reverse-direction A/B independence regression coverage so activity on either CV input cannot silently leak into the other channel's digital signal path.
- Expanded Arpeggiator documentation to describe its intended fast, scale-aware 8-bit-computer-style arpeggio character.
- Added a complete second Arpeggiator UI layer with dedicated controls for enable, rate/clock ratio, pattern, scale-degree shape, length, octave range, step trigger, FREE/RESET/CLOCK sync and swing while retaining Link/A/B navigation.
- Expanded the Arpeggiator engine from the fixed 24 ms root/third/fifth behaviour to twelve internal rates, clock division/multiplication, eight patterns, eight scale-aware shapes, 1–12 step length and 1–4 octave range; the former 24 ms 1-3-5 behaviour remains the default.
- Added external Sample/Gate clock integration per channel, including reset-only operation, phase re-anchoring on real clock edges, generated subdivisions and independent physical clock inputs for A/B.
- Added optional 5 ms output triggers for Arpeggiator steps while retaining pitch-only operation as the default.
- Added complete Arpeggiator-layer, clock/sync and persistence regression coverage, including full-config Arpeggiator restoration, stable/deterministic Random stepping and exact swing-pair timing; the regular native suite now contains 29 independently runnable suites and 231 test cases.
- Added `README_ARPEGGIATOR.md` as the complete second-layer operation, parameter, sync, persistence and testing reference.
- Added an AVR resource-budget CI gate to preserve engineering headroom for flash and static SRAM.
- Added dedicated per-channel Arpeggiator state and regression coverage for A-only, B-only and linked operation.
- Added live green/red/amber comparison feedback to LED brightness calibration so colour balance and the resulting amber mix can be judged while either emitter is adjusted.
- Added regression coverage for the calibration comparison view, calibration colour selection and the 1500 ms startup note-ring duration ceiling.
- Extended full-configuration and live-state persistence to include both complete per-channel Arpeggiator configurations and the selected channel; automatic live restore/autosave is enabled with versioned CRC-protected wear-levelled records.

### Fixed

- Added the active Quantizer/Arpeggiator UI layer to full-configuration persistence without increasing the 32-byte payload; user save records now write format v5 while v4 records remain readable: a spare bit in the selected-channel byte is used. Full-config loads now restore the layer side-effect free, including the valid Arpeggiator-layer/ARP-OFF state, and legacy configs with ARP enabled are migrated to the Arpeggiator layer instead of recreating a hidden running ARP in Quantizer UI.
- Removed the fresh-channel C bias from Hysteresis by tracking whether a previous output actually exists; the first exact 0.5-semitone tie now rounds upward as specified.
- Added a ±10-count plausibility window to the original resistor-ladder decoder. Nominal values remain unchanged, while implausible gap readings and high/open readings fail safe as no button.
- Replaced millisecond-assigned external-clock edges with ISR-captured microsecond timestamps and per-tick edge counts. CLOCK period measurement is no longer quantized to the 1 kHz control loop, multiple physical edges are not collapsed into one boolean event, and multiplied-step phase no longer accumulates control-loop observation error.
- Reworked the Arpeggiator as a true second control layer: entering it with the 3-second SHIFT-only gesture enables the selected Arpeggiator, while returning to the Quantizer layer disables Arpeggiator playback on both channels.
- Restored one consistent front-panel grammar across both UI layers: unmodified note buttons continue to edit and display the selected scale, while `SHIFT + note` selects Quantizer or Arpeggiator menu functions according to the active layer; scalar parameters then use the note buttons as value selectors.
- Fixed live restore of the active UI layer: reboot now returns to the same Quantizer/Arpeggiator front-panel layer that was active before power-off, so the first 3-second SHIFT hold after an Arpeggiator-layer reboot correctly performs OFF/exit instead of re-entering the layer. Live-state v6 stores the layer explicitly while retaining in-place v5 migration compatibility.
- Removed the separate steady Arpeggiator dashboard so the normal scale/quantized-note display remains visible in both UI layers; temporary parameter and activation feedback is retained.
- Changed Arpeggiator enable/bypass feedback to two full-ring flashes: green when enabling, red when disabling, followed automatically by restoration of the normal scale/quantized-note display.
- Removed the independent one-shot layer-change LED path that produced a single amber flash on entry and a single green flash on exit; the 3-second layer transition now uses the same exact two-green / two-red full-ring Arpeggiator feedback renderer as `SHIFT+C`.
- Made FREE internal timing the explicit factory/default Arpeggiator sync mode; RESET and CLOCK remain user-selectable additional modes.
- Closed the 64 ms layer-switch race by cancelling the SHIFT hold from immediate physical resistor-ladder activity before note-key debounce, and blocking layer switching outside the stable main page.
- Added an end-to-end regression path for 3-second layer entry -> automatic Arpeggiator enable -> actual FREE-running pitch stepping, plus layer-consistent SHIFT-menu, scale-display/editing and raw-ladder cancellation tests.
- Added regression checks for exactly two green enable flashes, exactly two red disable flashes, intervening dark phases and automatic return to the scale display.
- Limited every twelve-note startup animation variant to at most 1500 ms; the separate four-status-LED self-test remains independent of that note-ring limit.
- Added the per-channel Arpeggiator regression suite to the GitHub Actions matrix so the channel-isolation tests run as an explicit CI job.
- Renamed PlatformIO test-suite directories to the required `test_*` convention so `pio test -e native` discovers and builds the native unit, integration and regression tests correctly.
- Aligned stale native tests with the firmware's established hardware behaviour: original absolute resistor-ladder values, 64 ms ladder debounce, normalized-HIGH Track-and-Hold semantics, full-range ADC pitch conversion, last-note protection, and asynchronous EEPROM writes.
- Corrected the startup-sequence-store test include path.
- Added safe defaults to `MenuInput` so a value-initialized input represents no key, no SAVE/LOAD press, and SHIFT released.
- Restored GitHub Actions native-test execution.
- Fixed the native coverage job by explicitly linking the GCC gcov runtime for `--coverage` instrumented test binaries.
- Standardised the root `LICENSE` file for GitHub licence detection and added a GitHub security policy.
- Prevented local `compile_commands.json` files from being tracked because compilation databases may contain machine-specific absolute paths.

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
- Scale-aware Arpeggiator performance mode, toggled by holding SHIFT alone.
- Arpeggiator intervals derived from each channel's active scale.
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

- Added a repository-focused `README.md` covering features, hardware assumptions, build/flash, complete menu logic, Track/Sample behaviour, discrete LED semantics, factory presets, startup sequences, Arpeggiator and licensing/credits.
- Added `README_CONFIGURATION.md` as the firmware configuration reference.
- Added `README_CALIBRATION.md` with step-by-step LED, ladder, CV-input and DAC-output calibration procedures.
- Added this release-oriented `CHANGELOG.md` in place of internal fix notes.
- Added a prominent acknowledgement of Quinn Freedman's original Quantizer design and Rust firmware, including its practical value as an approachable, cost-conscious and highly adaptable DIY design.
