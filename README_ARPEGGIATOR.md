# Arpeggiator Layer

The Arpeggiator is a complete second performance layer of the firmware. It starts from the fast scale-aware pitch cycling used for classic 8-bit-computer-style chord textures, but it can also run as a clocked, triggered and swung arpeggiator for more contemporary rhythmic patches.

It is deliberately implemented as a control-rate state machine. No audio is buffered or analysed, no dynamic memory is used, and the Quantizer remains the source of the base pitch and scale.

## Contents

- [Mental model](#mental-model)
- [Shared interaction grammar](#shared-interaction-grammar)
- [Channel model](#channel-model)
- [Arpeggiator-layer SHIFT map](#arpeggiator-layer-shift-map)
- [Enable — SHIFT + C](#enable--shift--c)
- [Rate / clock ratio — SHIFT + C#](#rate--clock-ratio--shift--c)
- [Pattern — SHIFT + D](#pattern--shift--d)
- [Shape — SHIFT + D#](#shape--shift--d)
- [Length — SHIFT + E](#length--shift--e)
- [Range — SHIFT + F](#range--shift--f)
- [Step Trigger — SHIFT + F#](#step-trigger--shift--f)
- [Sync mode — SHIFT + G](#sync-mode--shift--g)
- [Swing — SHIFT + G#](#swing--shift--g)
- [Typical starting points](#typical-starting-points)
- [Relationship to the Quantizer](#relationship-to-the-quantizer)
- [Persistence](#persistence)
- [Memory and CPU design](#memory-and-cpu-design)
- [Timing and failure behaviour](#timing-and-failure-behaviour)
- [Test coverage](#test-coverage)

## Mental model

There are two front-panel UI layers:

- **Quantizer layer** — normal scale editing plus the established Quantizer SHIFT menu.
- **Arpeggiator layer** — the **same scale editing**, but a different SHIFT menu for Arpeggiator parameters.

The layer changes the meaning of **SHIFT + note**, not the basic meaning of the twelve note buttons. This is deliberate: the user should not have to learn two unrelated interaction models.

> [!IMPORTANT]
> Layer switching is deliberately symmetric: **double-click SHIFT to enter the Arpeggiator layer, and double-click SHIFT again to disable Arpeggiator playback and return to the Quantizer layer.** Normal `SHIFT + note` shortcuts remain modifiers and do not count as the layer gesture.

Double-click **SHIFT by itself** to switch between the two layers. The gesture is deliberately symmetric: the same SHIFT double-click enters the Arpeggiator layer and the same SHIFT double-click leaves it again. Both presses must be short and the second press must begin within 350 ms of the first release. Entering the Arpeggiator layer enables the selected channel's Arpeggiator; with linked channels, both are enabled together. The factory/default Arpeggiator is FREE-running at 24 ms, so entering the layer is immediately audible without an external clock. Returning to the Quantizer layer disables Arpeggiator playback on both channels.

The layer transition uses the same enable/bypass feedback as `SHIFT+C`:

- entering / enabling: complete ring flashes **green twice**;
- leaving / disabling: complete ring flashes **red twice**;
- each flash consists of 150 ms on + 150 ms dark;
- after the second flash, the normal scale/quantized-note display is restored.

There is deliberately no separate amber/green one-shot layer flash. The selected scale remains visible in both layers whenever no temporary menu feedback is active.

Any physical note-button, SAVE or LOAD activity during a pending SHIFT double-click cancels the sequence. Note-button cancellation is evaluated from the raw ladder state before the normal 64 ms key debounce, so an ordinary `SHIFT + note` shortcut cannot accidentally complete the layer gesture. The double-click recogniser debounces SHIFT independently while the menu still sees SHIFT immediately as a modifier. Layer switching is also blocked while a parameter, memory or calibration page is active.

The active UI layer is part of live state. After a normal autosave and reboot, the firmware restores the **same Quantizer or Arpeggiator layer** that was active before power-off. Layer restore itself has no musical side effects and does not replay the green/red toggle feedback. Consequently, if the unit was powered down in the Arpeggiator layer, the first SHIFT double-click after reboot exits that layer and turns the Arpeggiator off as expected.

Live-state format v6 stores this layer explicitly without increasing the EEPROM record size. Existing v5 records are accepted during firmware update; because v5 had no layer bit, the migration infers Arpeggiator layer when either saved Arpeggiator was enabled. After the next autosave, the layer is explicit and exact.

## Shared interaction grammar

The two layers intentionally behave alike:

1. An unmodified note-button press edits the selected scale.
2. **SHIFT + note** selects a menu function from the active layer.
3. If that function has a scalar/multi-choice parameter, release SHIFT and press one of the twelve note buttons to select the value.
4. Boolean commands such as Enable, Step Trigger and Link take effect immediately after the SHIFT shortcut and show short feedback before returning to the scale display.

The only thing that changes between layers is the SHIFT menu map.

## Channel model

Channel A and Channel B have independent Arpeggiator configurations and independent runtime phase.

The three channel-management positions deliberately keep exactly the same gesture in both layers:

| Position | Quantizer layer | Arpeggiator layer |
|---|---|---|
| A | SHIFT + A: Link / unlink | SHIFT + A: Link / unlink |
| A# | SHIFT + A#: Select Channel A | SHIFT + A#: Select Channel A |
| B | SHIFT + B: Select Channel B | SHIFT + B: Select Channel B |

When channels are linked, enabling the link copies the complete Quantizer configuration **and** the complete Arpeggiator configuration from A to B. Subsequent Arpeggiator edits apply to both channels. Unlinking leaves the copied settings in place and allows the channels to diverge again.

Linking does not silently reroute clock inputs. Channel A still receives clock/reset edges from Sample/Gate A and Channel B from Sample/Gate B.

## Arpeggiator-layer SHIFT map

While the Arpeggiator layer is active, the twelve note buttons continue to edit and display the scale when pressed normally. Hold SHIFT to access the layer-specific menu:

| Shortcut | Function |
|---|---|
| SHIFT + C | Enable |
| SHIFT + C# | Rate / clock ratio |
| SHIFT + D | Pattern |
| SHIFT + D# | Shape |
| SHIFT + E | Length |
| SHIFT + F | Range |
| SHIFT + F# | Step Trigger |
| SHIFT + G | Sync mode |
| SHIFT + G# | Swing |
| SHIFT + A | Link |
| SHIFT + A# | Channel A |
| SHIFT + B | Channel B |

For Rate, Pattern, Shape, Length, Range, Sync and Swing, the shortcut opens a parameter page. After SHIFT is released, the next unmodified note-button selects the value. A, A# and B are therefore ordinary value positions 9, 10 and 11 while a scalar parameter page is active, exactly as in the Quantizer parameter menus.

### LED feedback in the Arpeggiator layer

On the main page the ring remains the normal scale/quantized-note display. There is no separate steady Arpeggiator dashboard. This preserves the front-panel visual model already learned from the Quantizer layer.

Inside a scalar parameter page, the currently selected value is shown at its note position in the selected channel colour: green for A, red for B. Step Trigger and the other boolean controls receive short status feedback and then return to the scale display.

Arpeggiator Enable/Bypass uses deliberately stronger feedback across the **entire twelve-LED ring**:

- enabling the selected Arpeggiator: **two green flashes**;
- disabling the selected Arpeggiator: **two red flashes**.

Each flash is followed by a dark interval. After the second flash the normal scale/quantized-note display is restored automatically; the feedback does not replace or alter the scale indication.

SAVE and LOAD retain their normal memory meaning in either UI layer; switching to the Arpeggiator layer does not create a second incompatible preset system.

## Enable — SHIFT + C

Enable is channel-specific:

- selected A: toggle Arpeggiator A;
- selected B: toggle Arpeggiator B;
- linked: toggle both together.

Enabling or changing timing resets that channel's Arpeggiator phase to a defined start. Linked changes reset both channels to the same phase origin.

The persisted default is **OFF**. Entering the Arpeggiator layer enables the selected channel automatically when it is off; **SHIFT + C** remains the explicit bypass/on-off control while working in the layer.

## Rate / clock ratio — SHIFT + C#

The same twelve positions mean different but related things depending on Sync mode.

### FREE and RESET

| Value button | Step time |
|---|---:|
| C | 12 ms |
| C# | 16 ms |
| D | 20 ms |
| D# | **24 ms** |
| E | 32 ms |
| F | 48 ms |
| F# | 64 ms |
| G | 96 ms |
| G# | 125 ms |
| A | 167 ms |
| A# | 250 ms |
| B | 500 ms |

The default is **D# = 24 ms**. Together with UP, 1-3-5, length 3, one octave, no step trigger, FREE and no swing, this is the classic fast 8-bit-style pitch-cycling sound.

### CLOCK

| Value button | External-clock ratio |
|---|---:|
| C | ÷8 |
| C# | ÷6 |
| D | ÷4 |
| D# | ÷3 |
| E | ÷2 |
| F | ×1 |
| F# | ×2 |
| G | ×3 |
| G# | ×4 |
| A | ×6 |
| A# | ×8 |
| B | ×12 |

For divisions, the Arpeggiator advances only after the required number of real rising clock edges. For multiplications, it measures the external period and generates the intermediate steps internally. Every real edge re-anchors the timing so long-term drift is not accumulated.

The measured external period is smoothed with a small integer filter. The implementation deliberately does not replay missed historical steps if the control loop is late; it advances from current time instead.

## Pattern — SHIFT + D

Pattern changes the order in which positions of the current figure are visited.

| Value button | Pattern |
|---|---|
| C | Up |
| C# | Down |
| D | Up / Down |
| D# | Down / Up |
| E | Rotate |
| F | Outside In |
| F# | Inside Out |
| G | Random |
| G#–B | unused |

`Up / Down` and `Down / Up` do not duplicate the end points. `Random` uses a very small deterministic PRNG; it is intended as a musical ordering mode, not as a source of cryptographic randomness. The PRNG advances only when a musical Arpeggiator step advances, so the pitch remains stable between control-loop ticks. A phase reset also resets the PRNG seed, so the Random sequence restarts reproducibly after enable/reset rather than resuming from an arbitrary previous runtime point.

The default is **Up**.

## Shape — SHIFT + D#

Shape selects scale-degree relationships. The numbers below are **degrees of the active Quantizer scale**, not fixed chromatic intervals.

| Value button | Shape |
|---|---|
| C | 1-3-5 |
| C# | 1-2-5 |
| D | 1-4-5 |
| D# | 1-3-6 |
| E | 1-5-8 |
| F | 1-2-3-4 |
| F# | 1-3-5-7 |
| G | Stacked thirds |
| G#–B | unused |

This is why the same shape follows the selected scale naturally. For example, 1-3-5 over C major produces C-E-G, while the same shape over a minor scale produces the corresponding minor third.

`Stacked thirds` continues upward by every second active scale degree rather than repeating a fixed three- or four-value table.

The default is **1-3-5**.

## Length — SHIFT + E

All twelve note buttons are valid:

- C = 1 step;
- C# = 2 steps;
- ...
- B = 12 steps.

Length determines the number of positions over which Pattern operates. It can therefore deliberately differ from the natural size of Shape. Longer lengths cause the shape to continue/repeat through the configured octave range.

The default is **3 steps**.

Odd lengths can be useful against an even metrical grid because the emphasis moves across successive clock cycles.

## Range — SHIFT + F

| Value button | Range |
|---|---:|
| C | 1 octave |
| C# | 2 octaves |
| D | 3 octaves |
| D# | 4 octaves |
| E–B | unused |

The generated pitch is always clamped to the Quantizer's 0…120-semitone output range.

The default is **1 octave**.

## Step Trigger — SHIFT + F#

Step Trigger determines whether Arpeggiator steps are only pitch changes or also become trigger events.

**OFF** — Arpeggiator steps change the final pitch CV but do not create normal Quantizer trigger events. This is the default and preserves the classic fast 8-bit-computer-style effect, where a monophonic oscillator rapidly cycles through chord tones without retriggering an envelope on every step.

**ON** — every Arpeggiator step also starts the normal 5 ms trigger pulse and approximately 65 ms output-activity LED indication. This turns the same engine into a more conventionally articulated arpeggiator for envelopes, percussion or other downstream events.

Quantizer trigger events and Arpeggiator step triggers are logically ORed at the physical trigger output.

## Sync mode — SHIFT + G

Only the first three value buttons are used:

| Value button | Mode | Behaviour |
|---|---|---|
| C | FREE | internal step timing |
| C# | RESET | internal timing; rising Sample/Gate edge resets to the first position |
| D | CLOCK | Sample/Gate rising edges provide the external clock reference |

### FREE

The Arpeggiator runs from the selected millisecond Rate independently of the Sample/Gate input.

This is the simplest mode and the **factory/default mode**. It requires no external clock. At fast rates it produces the characteristic 8-bit-style pitch-cycling sound.

Automatic live-state restore can of course restore RESET or CLOCK if the user explicitly selected and previously saved that state; this does not change FREE as the factory/default mode.

### RESET

The selected internal Rate remains active. A rising edge at that channel's Sample/Gate input resets the pattern to its first position and restarts its timing from the edge.

RESET is useful when the fast pitch-cycling effect should remain internally timed but start at a repeatable point for each phrase or gate event.

### CLOCK

CLOCK is an **explicit optional mode**, not the normal/default operating mode. The Sample/Gate jack becomes that channel's external Arpeggiator clock input. Rate changes meaning from milliseconds to the clock-ratio table above.

While CLOCK is active for a channel, its CV input is processed continuously for quantization regardless of the stored Track-and-Hold / Sample-and-Hold mode. The stored Sample mode itself is not changed; leaving CLOCK restores its normal meaning.

The hardware normalises an unpatched Sample/Gate input HIGH. A static HIGH level is not a stream of clocks: Arpeggiator clocking requires genuine LOW-to-HIGH edges. Short rising edges are captured by the existing external-interrupt path before the 1 kHz control loop consumes them.

## Swing — SHIFT + G#

Swing has twelve levels:

- C = straight, 50:50;
- C#…B progressively increase the long/short contrast;
- B is approximately 66:34.

The duration of a pair of steps remains constant. Swing therefore changes placement rather than simply speeding up the sequence.

Swing applies to FREE/RESET internal timing and to internally generated CLOCK subdivisions. Real external clock edges remain the phase anchors.

The default is **straight**.

## Typical starting points

### Classic fast 8-bit-computer texture

- Enable: ON
- Rate: 24 ms
- Pattern: Up
- Shape: 1-3-5
- Length: 3
- Range: 1 octave
- Step Trigger: OFF
- Sync: FREE
- Swing: 0

This is intentionally close to the original Arpeggiator behaviour: very fast pitch cycling through scale-aware chord degrees.

### Resettable 8-bit-style texture

Use the same settings, but set Sync to RESET. A gate can then restart the rapid figure without converting the effect into a clock-driven sequencer.

### Clocked groove

One useful starting point is:

- Rate / ratio: ×4
- Pattern: Up / Down or Outside In
- Shape: 1-3-5-7 or 1-2-5
- Length: 5
- Range: 2 octaves
- Step Trigger: ON
- Sync: CLOCK
- Swing: moderate

The exact result depends on the active scale, incoming CV, external clock and downstream patch; these are starting points rather than factory promises about a particular musical result.

## Relationship to the Quantizer

The Arpeggiator works **after** normal Quantizer pitch selection. The processing idea is:

```text
CV input
  -> Quantizer scale / shifts / sample behaviour
  -> Quantized base pitch
  -> Arpeggiator scale-degree offset
  -> final pitch
  -> DAC
```

The Arpeggiator therefore uses the active scale of its own channel. It does not maintain a second independent note mask.

In CLOCK mode only, quantization is forced continuously for that channel so the clock jack can be dedicated to timing rather than Sample-and-Hold. This runtime override does not alter the stored Quantizer Sample mode.

## Persistence

Arpeggiator configuration is part of the musical working state.

The following values are persisted per channel:

- enabled;
- rate / clock-ratio index;
- pattern;
- shape;
- length;
- range;
- step-trigger setting;
- sync mode;
- swing.

A **Full Configuration** save contains the Quantizer state, both Arpeggiator configurations, the selected channel and the active Quantizer/Arpeggiator UI layer. Scale saves remain scale-only.

If a Full Configuration user slot has no valid user record, the existing factory fallback for that slot is loaded with the corresponding factory Quantizer configuration and both Arpeggiators reset to their defined defaults (OFF, 24 ms, Up, 1-3-5, length 3, one octave, Step Trigger OFF, FREE, straight swing). Factory data itself is compiled into firmware rather than written into EEPROM.

Automatic live-state persistence is enabled. After a configuration change has been quiet for the configured autosave interval, the complete musical state is queued to the wear-levelled EEPROM live-state ring. On the next boot it is restored.

Persisted live state includes:

- both current Quantizer scales and all Quantizer parameters;
- channel linking and Channel-B Absolute/Relative mode;
- both complete Arpeggiator configurations, including ON/OFF;
- selected channel;
- active Quantizer/Arpeggiator UI layer;
- LED brightness calibration.

The following are intentionally **not** persisted:

- current Arpeggiator step;
- random-generator progress;
- clock period/phase;
- current trigger pulse;
- current Glide intermediate value;
- instantaneous input state.

Those values are runtime state, not musical configuration.

EEPROM writes are asynchronous and records are versioned and CRC-protected. The live-state area uses a ring of records rather than rewriting one physical slot repeatedly. A record is committed by writing its validity marker last.

The current EEPROM layout uses 6 bytes per scale slot, 36 bytes per Full Configuration slot and 42 bytes per live-state record. Twelve wear-levelled live-state records fit alongside the 24 preset records and startup metadata. Compile-time assertions guard the layout against overlap.

## Memory and CPU design

The feature is intentionally small enough for the ATmega328P:

- no dynamic allocation;
- no audio buffer;
- no FFT or audio-rate DSP;
- compact enum/integer configuration;
- small fixed lookup tables;
- per-channel timing state only;
- fixed-point pitch arithmetic;
- external clock edges are counted and timestamped independently of the 1 kHz control loop.

The CI AVR build additionally checks an explicit engineering budget rather than relying only on the absolute MCU limit: target application flash usage is at most 92.5% of a 30,720-byte project budget and static SRAM usage at most 70% of 2 KB.

## Timing and failure behaviour

The Arpeggiator follows the same real-time rule as the rest of the firmware: a late control loop must not replay a backlog using current input data as if it represented historical samples.

For CLOCK multiplication, each D2/D3 edge is timestamped in the external-interrupt ISR with Arduino `micros()` (4 µs Timer0 granularity on the 16 MHz Nano) rather than being assigned the next 1 ms control-loop time. The firmware also counts multiple edges that arrive before the next control tick. Generated substeps use the measured microsecond period, remain phase-anchored to the latest real edge, and advance from their ideal scheduled instant so 1 ms observation jitter does not accumulate. DAC updates still occur on the 1 kHz control loop. If no valid period has yet been measured, no speculative multiplied steps are generated.

All decoded persisted parameter values are validated or clamped before use. Invalid enum values fall back to defined defaults rather than being trusted as runtime state.

## Test coverage

The Arpeggiator is covered at several levels:

- core unit tests for scale-aware degree generation, timing boundaries, patterns, sync modes and wraparound;
- matrix tests over all free rates, patterns, shapes, lengths, ranges, factory scales and clock ratios;
- channel-isolation tests for A-only, B-only and linked state;
- UI-layer integration tests for every front-panel Arpeggiator parameter and Link/A/B behaviour;
- end-to-end clock tests through the production Quantizer/Arpeggiator/DAC path;
- exact step-trigger timing and output-LED timing checks;
- persistence round-trip, validation and byte-corruption tests including Arpeggiator data;
- Full Configuration save/load restoration of Quantizer state, selected channel and complete Arpeggiator state;
- deterministic Random restart and exact two-step swing-pair duration checks.

The repository currently runs **29 default suites with 254 default test cases**. The former Hysteresis-first-sample and ladder-plausibility findings are now regular passing regression tests. See [README_TESTING.md](README_TESTING.md) for the complete test structure.

---

<p align="center">From Munich With <img src="docs/assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
