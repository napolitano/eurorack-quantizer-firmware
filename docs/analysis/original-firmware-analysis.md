# Notes on the original Rust firmware

These are working notes from reading Quinn Freedman's original Quantizer firmware and the relevant shared `fm-lib` code before and during the C++ reimplementation. They are meant to capture what appears worth preserving, what looks risky, and where the new implementation should be deliberately different.

This is not intended as a verdict on the original project. It is also possible that I have misunderstood a code path or missed context elsewhere in the repository. Where a point is based on a direct code path I say so; where it is an engineering interpretation I try to keep the wording correspondingly cautious.

Files reviewed for these notes:

- `modules/Quantizer/Firmware/src/main.rs`
- `modules/Quantizer/Firmware/src/quantizer.rs`
- `modules/Quantizer/Firmware/src/menu.rs`
- `modules/Quantizer/Firmware/src/persistence.rs`
- `modules/Quantizer/Firmware/src/bitvec.rs`
- `modules/Quantizer/Firmware/src/resistor_ladder_buttons.rs`
- `fm-lib/src/async_adc.rs`
- `fm-lib/src/button_debouncer.rs`
- `fm-lib/src/eeprom.rs`
- `fm-lib/src/mcp4922.rs`
- `fm-lib/src/system_clock.rs`

## What is good about it

The first thing worth keeping in mind is the target. This is a feature-rich two-channel quantizer running on an ATmega328P with 32 KB flash, 2 KB SRAM, a minimal panel and a deliberately affordable BOM. Within that context, the original firmware does several things very well.

### It is built around the limits of the AVR instead of pretending they do not exist

The signal path uses fixed-point arithmetic (`I8F8`, `I8F24`, `I1F15`, `U16F16`) rather than floating point. That keeps runtime predictable and avoids expensive software floating-point on an 8-bit MCU.

The code also stays close to static/stack-based data and `no_std`, which is exactly the right instinct for this class of firmware.

### Persistent configuration and runtime state are conceptually separated

`ChannelConfig` represents user/musical settings while `ChannelState` contains transient state such as trigger history, hysteresis state and timers. That separation is useful and maps naturally onto persistence and testing.

### The two-channel model is easy to understand

The intended routing is explicit: Channel A uses A; Channel B uses B in Absolute mode and A+B in Relative mode, with saturation. That simplicity matters because it makes the musical behaviour reviewable.

### The quantizer itself has good musical semantics

The core logic looks for the nearest enabled pitch class, resolves exact ties upward, handles an empty scale without looping forever, clamps to the pitch range and applies hysteresis.

The hysteresis is particularly interesting because it is based on the distance to the neighbouring enabled notes and adds roughly 0.4 semitone beyond the midpoint. Sparse scales therefore get a sensible wider switching region instead of using a fixed semitone assumption.

### Glide is cheap and thoughtfully implemented

The glide recurrence uses a power-of-two coefficient, which is a good match for AVR. The code also contains a small but important guard against fixed-point rounding stalling before the target: if the recurrence produces no movement, it advances by one LSB in the correct direction.

That is the kind of detail worth preserving.

### The ADC helper shows real embedded awareness

The asynchronous ADC implementation is one of the stronger parts of the shared library. It uses free-running conversions, rotates through channels, captures results in the ADC ISR and protects 16-bit reads with critical sections.

For the Quantizer's three-sample window, the helper effectively returns the median value after sorting. Despite the "averaging" name, the behaviour is therefore median-of-three, which is a useful cheap outlier filter.

The code also explicitly accounts for AVR ADC register/channel-selection behaviour rather than treating the ADC as an abstract desktop API.

### It uses the available I/O economically

The TLC5947 allows a rich bi-colour ring without consuming 24 MCU pins. DAC and LED traffic share SPI. LED frames are only sent when necessary, and the LED driver is blanked at startup until a complete zero frame has been shifted.

### The feature density is impressive

On this hardware the firmware provides two channels, independent scales, linking, Relative B, Track/Sample operation, delay, glide, three transposition mechanisms, scale/config persistence and extensive LED feedback.

That is the context in which the weaknesses below should be read. The project is not a failed attempt at a simple quantizer; it is an ambitious implementation that gets a lot right and has a handful of edge cases that are easy to expose once we start specifying and testing the behaviour more rigorously.

## Behaviour that appears to need correction or hardening

### Track-and-Hold with non-zero delay

This one appears to be a real functional bug in the original control flow.

In Track-and-Hold, `received_trigger` remains true for every processing step while the gate is HIGH. The same path resets the input delay timer whenever `received_trigger` is true. With a configured delay greater than zero, a sustained HIGH therefore appears to restart the delay repeatedly instead of starting it once and then tracking continuously.

The intended model for the reimplementation is simpler: detect the beginning of HIGH, run the delay once, then continue processing for the rest of that HIGH interval.

### Negative scale-degree shifts

`step_in_scale()` calculates a direction for negative movement, but the loop uses `0..num_steps`. With a negative step count that range performs no iterations, so the negative direction never gets a chance to do useful work.

The reimplementation should iterate over the absolute number of scale steps and apply the selected direction.

### Linked-channel invariants are spread across UI branches

The original code often handles linked channels explicitly, but not every operation appears to go through one central "apply to both" invariant. Sample-mode handling is one example where the code path can operate through the currently active channel.

That makes linking fragile because every new UI path has to remember the rule. The C++ version should keep the invariant in one place so a linked configuration cannot silently diverge.

### Timer2 compare value appears to be one count off for exactly 1 kHz

The original code uses Timer2 CTC, prescaler 128 and `OCR2A = 125` while referring to a 1 kHz processing tick.

With the AVR CTC formula, `OCR2A = 124` is the value that gives 1000 Hz at 16 MHz / 128. A value of 125 produces roughly 992.06 Hz.

This is a small timing error rather than a catastrophic bug, but there is no reason to reproduce it in a fresh implementation.

### Some trigger/LED decisions are made before the final post-shift result

From the original signal path, some activity/trigger decisions appear to be based on an intermediate quantized value rather than the final note after all transformations.

For the new implementation, the clean rule is that the physical trigger and "current output note" indication should follow the actual discrete note sent toward the DAC after all configured shifts.

### Scale changes do not always force an immediate held-value recomputation

The original code can leave a previously held note in place after changing the active scale, depending on the Sample/Track state and whether a new sample event occurs.

For a performance quantizer this is surprising. In the reimplementation, a scale or transposition change should invalidate/recompute the discrete target immediately, without requiring another gate edge.

### EEPROM commit order is vulnerable to interrupted writes

The original persistence format uses a sentinel to mark a record as valid. As I read the code, the sentinel is written before the rest of the payload.

That means a loss of power after the sentinel but before the payload is complete can leave a record that looks valid even though its contents are only partially updated.

For the new implementation, validity metadata should be committed last, after the payload has been written successfully.

### EEPROM values are trusted more than I would like

The deserialization path reconstructs values directly from stored bytes and does not appear to validate every constrained field before applying it. Signed shift fields also use a byte reinterpretation via `transmute`.

Nothing about an 8-bit signed reinterpretation is inherently exotic, but explicit decoding plus range checks is easier to audit. The new implementation should validate every enum/range/reserved bit and reject the record as a whole if something is wrong.

### ADC reference setup is internally contradictory

`main.rs` initially creates the ADC using `ReferenceVoltage::Aref`, while `fm-lib::init_async_adc()` later writes ADMUX to AVCC.

The latter appears to win, so the effective ADC reference is AVCC. This is less a mysterious runtime bug than a configuration contradiction that makes the code harder to reason about.

The reimplementation should choose the reference once, at board/profile level, and document that choice.

### Exactly simultaneous SAVE+LOAD long presses can fall through the state-machine gap

The erase gesture is recognized using combinations such as one button being "held long" while the other is "just clicked long". If both debouncers change state in exactly the same processing step, there is a plausible path where the intended combination is missed.

Tracking the duration for which both buttons are simultaneously down is simpler and less state-order dependent.

### The resistor-ladder decoder is permissive

The original ladder logic maps the measured ADC value to the nearest expected key value. The requirement analysis found no sufficiently defensive acceptance window around those expected values.

That means a substantially wrong ADC reading can still be interpreted as some key. For the reimplementation, nearest-match decoding should be combined with a plausibility window/open-state boundary.

### The internal bit-vector clear helper appears wrong

The previously reviewed bit-vector helper contains a clear operation whose mask logic does not appear to clear the requested bit correctly.

This is a good example of why tiny utility functions benefit from exhaustive tests across every bit position. The C++ version should keep these operations intentionally boring and fully covered.

### Compiler/assembly workarounds are implementation history, not product behaviour

The original AVR Rust code uses unstable features and contains assembly/compiler-related workarounds. Given the state of AVR Rust support, that is understandable.

They should not be copied into the C++ implementation unless there is a current, independently demonstrated need. We are reproducing product behaviour, not historical toolchain constraints.

## Additional things I would keep an eye on

### Hysteresis starts with `last_output = 0`

`HysteresisState` appears to initialize `last_output` to zero and does not carry an explicit "no previous output yet" flag.

That means the first quantization can potentially be treated as if C had already been the previous note. If C is active and the input is near the first boundary, the initial sample can therefore be influenced by hysteresis before any real note has been established.

This is one of those cases where a tiny state-model improvement (`Option<note>` / validity flag) makes the intended behaviour much clearer. We have a regression/known-failure test around this boundary in the C++ work.

### The scheduler collapses missed ticks into one pending flag

Timer2 sets a boolean `SAMPLE_READY`. If another timer event occurs while it is already true, the firmware cannot tell that more than one deadline was missed.

There is actually a positive side to this: it does not replay a backlog of fake historical samples using current input values. I would preserve that no-replay behaviour.

What is missing is observability. A missed-deadline counter is useful during testing and diagnostics without changing the runtime result.

### The ADC helper's naming can mislead maintainers

For the three-sample Quantizer configuration, the "averaging" helper returns a median. The behaviour is good; the name is the problem.

Naming it in terms of the actual filter semantics would remove an easy source of future mistakes.

### The original Rust toolchain raises the maintenance barrier

The firmware relies on several unstable/nightly AVR Rust features. That is not a criticism of the original author so much as a consequence of the ecosystem available for AVR Rust.

For someone wanting to build, debug and maintain the firmware years later, it creates more toolchain friction than a conventional PlatformIO AVR C/C++ project. That maintenance concern is one of the practical reasons for this reimplementation.

### The persistence format is intentionally tiny, but has very little self-description

The original format is compact and easy on EEPROM: a sentinel plus fixed-size payloads. That is a real advantage on a 1 KB EEPROM.

It has no version and no CRC/checksum, though, so schema changes and silent corruption are difficult to distinguish from valid data. Combined with sentinel-first writes, this is an area where a small amount of extra metadata buys a lot of robustness.

### Hardware and product logic meet in fairly central code paths

The original project has useful module separation, but the application still brings GPIO, EEPROM, SPI, menu decisions and scheduling together quite tightly.

That is manageable on a small firmware project, but it makes host-native testing harder. One of the goals of the C++ architecture is therefore not merely stylistic: explicit ports/interfaces let us test the musical state machine without AVR hardware attached.

## Things that look odd but are intentional

A few behaviours are easy to misread if the hardware context is missing.

### Unpatched Sample/Gate being HIGH is intentional

The hardware normalizes the Sample/Gate inputs to HIGH, while D2/D3 are used as floating digital inputs without the MCU's internal pull-ups. In Track-and-Hold this intentionally means an unpatched channel tracks continuously.

That behaviour should remain compatible with the original board.

### Channel B is independent of A in Absolute mode

The original quantizer core uses B alone in Absolute mode and A+B only in Relative mode. If real hardware shows B following A while Absolute mode is active, the mathematical routing is not the first place I would blame.

### Median-of-three is useful despite the helper name

The mismatch is naming, not behaviour. A three-sample median is a sensible little filter here.

### The LED channel order is board-specific, not random

The logical ring order maps through the physical TLC5947 wiring. The `6..11` then `0..5` order therefore should not be "cleaned up" without checking the board.

## Notes for the C++ reimplementation

The main lesson I take from the original code is not "replace everything". Quite the opposite: preserve the parts that fit the hardware and musical intent, while making the invariants harder to violate.

For the reimplementation I would continue to favour:

- fixed-point real-time calculations;
- median-of-three ADC filtering;
- no dynamic allocation in the real-time path;
- no backlog replay after a missed deadline;
- clear separation of persistent configuration and runtime state;
- full-frame TLC5947 updates before latching;
- the existing musical quantization and scale-aware hysteresis model;
- the compact two-channel A / B-absolute / B-relative model.

The places worth deliberately hardening are:

- initialize hysteresis only after the first real quantization result;
- treat Track delay as a one-shot event at the beginning of HIGH;
- centralize linked-channel invariants;
- recompute targets immediately after relevant configuration changes;
- generate triggers from the final discrete target;
- validate all loaded persistence data;
- write persistence commit metadata last and include format version/integrity checking;
- give the resistor ladder an explicit validity window;
- make ADC reference selection unambiguous;
- count timing overruns for diagnostics;
- keep hardware access behind testable ports wherever practical.

## Current take

I have a fairly positive view of the original firmware after going through it in detail. It is compact, musically coherent and often quite clever about the limitations of an ATmega328P. The asynchronous ADC, fixed-point signal path, glide implementation and amount of UI functionality extracted from a small front panel are all worth crediting.

The weak points are mostly the kind that appear when an ambitious embedded project grows without a large automated regression net: a few state-machine edge cases, invariants duplicated across branches, persistence that trusts its data too much, and some toolchain-era compromises.

That makes the original code a good behavioural and architectural reference, but not something I would translate line-for-line. The sensible approach is to keep the musical model and the pragmatic low-level ideas, then put stronger boundaries, validation and tests around them.

I would also keep these notes provisional. If later hardware measurements or another pass through the original repository contradict any of the observations above, the document should be corrected rather than defended. Its purpose is to help the next engineering decision, not to prove that the reviewer was right the first time.
