# Hardware notes on the original Quantizer

These are working notes from reviewing the supplied KiCad design set before making further firmware and hardware decisions. They are not intended as a formal design review or as a definitive fault list.

The hardware review is based on the complete KiCad material supplied for the module, rather than screenshots or a simplified redraw, together with the published assembly/BOM information and behaviour already verified in firmware. I have not independently measured PCB copper resistance, fitted component tolerances, Nano clone internals or analogue behaviour on a calibrated bench setup. Where the design data leaves room for doubt, I have tried to keep the wording provisional rather than fill in the gaps.

## What I like about the design

Before listing concerns, it is worth stating why this circuit is interesting in the first place. It gets a surprisingly capable two-channel quantizer out of a small number of inexpensive, readily available parts.

A few choices are particularly sensible:

- The Arduino Nano keeps the controller side accessible to DIY builders and makes replacement and flashing easy.
- A single MCP4922 provides both CV outputs with enough resolution for the intended semitone quantizer use case.
- The DAC has its own LM4040-based 5 V reference instead of blindly using the digital 5 V rail. That is one of the stronger analogue decisions in the design.
- TL072 output stages provide the required scaling to Eurorack CV levels.
- The 1 kΩ output resistors add useful current limiting and isolation at the jacks.
- The TLC5947 gives the front panel a lot of LED functionality without consuming a large number of MCU pins.
- Sharing SPI between DAC and LED driver saves pins and parts. It does require disciplined software, but the original firmware does use complete LED frames before latching them.
- The Sample/Gate inputs end on D2/D3, which happen to be interrupt-capable pins and leave useful options open for firmware work.

In other words, this is not a careless circuit. It is a cost-conscious DIY design with several good ideas. Most of the points below are about margin, protection and precision rather than whether the circuit can work.

## Power path

### The module appears to use the Eurorack +5 V rail directly

From the supplied rear-board schematic, the Eurorack `+5V` rail is brought into the module and used as the 5 V supply domain. I do not see a local 5 V regulator in the submitted schematics.

That is economical, but it means the module inherits the quality of the case's +5 V rail. Rail tolerance, noise and disturbances from other modules therefore matter more than they would with a locally generated supply.

This becomes more important because the ADC side ultimately uses AVCC as its reference in the original firmware. The quality of the 5 V rail can therefore affect input conversion as well as the digital circuitry.

I would not call the choice wrong. For an inexpensive DIY module it saves parts, heat and board area. For a revision aimed more strongly at precision and robustness, I would at least reconsider whether the 5 V domain should be generated locally from +12 V or isolated/filtered more deliberately.

### I could not find ferrite-bead isolation on the incoming rails

I did not find ferrite beads in the supplied rear-board schematic. There *is* local decoupling, including 100 nF capacitors and 10 µF bulk capacitance, so describing the board as "unfiltered" would be inaccurate.

The distinction is that I do not see the common Eurorack arrangement of series/ferrite isolation followed by local decoupling for the individual supply domains. That leaves a more direct HF path between the bus and the module, including the digital/LED load.

This is worth revisiting in a hardware revision, especially because the board mixes precision CV work with an MCU and a fairly active LED driver.

### Power-input protection looks minimal in the schematic

I could not identify a dedicated reverse-polarity diode/ideal-diode stage, fuse/polyfuse or TVS network at the power inlet in the supplied drawings.

A shrouded/keyed connector already reduces one common failure mode, so this is not equivalent to having no protection at all. It is simply mechanical prevention rather than electronic fault containment.

For a DIY board this may have been a deliberate BOM choice. If the board is revised, I would consider whether a small amount of extra input protection is worth the cost and space.

## Reference and ADC path

### The DAC reference is better thought through than the ADC reference

The separate `LM4040LP-5` / `5VREF` path is a positive point. It gives the MCP4922 a controlled reference instead of using whatever happens to be present on the Nano's 5 V domain.

The ADC side is less isolated. The original firmware's asynchronous ADC setup ultimately selects AVCC. That means there is an asymmetry:

- DAC reference: dedicated LM4040-based 5 V reference
- ADC reference: AVCC / module 5 V domain

Static gain error can be calibrated in software. Fast supply/reference noise cannot be calibrated away in the same way. For a quantizer this does not automatically become audible or problematic, but it is one of the first areas I would measure if chasing pitch stability.

### A/B ADC isolation is still an open measurement question

A5, A6 and A7 share the ATmega328P's multiplexed ADC. The firmware cycles through the channels; they are not sampled by independent ADCs.

We have already added native tests showing that the C++ quantizer logic does not digitally route A into B when B is in Absolute mode. Because a real unit was observed producing activity on B while A was patched, the analogue path and ADC multiplexing deserve a closer look.

I do **not** have enough evidence to call this a hardware defect. The useful bench checks are straightforward:

1. Ground CV B explicitly and sweep CV A.
2. Log the raw A6/A7 ADC values while doing so.
3. Repeat with B unpatched.
4. Measure the B analogue-stage output before the Nano while A is swept.
5. Repeat with different source impedances if the behaviour changes.

That should tell us whether we are looking at ADC settling, analogue crosstalk, a floating/unpatched input effect, or something else entirely.

## CV inputs

### I do not see a dedicated external clamp/TVS network

The CV inputs are not simply wired to the MCU; there are resistor networks and MCP6004 analogue processing in front of A6/A7. That is important and limits fault current.

What I could not find in the supplied schematic is an explicit external protection network such as dedicated Schottky clamps or TVS parts at the CV inputs.

That means out-of-range voltages and patch transients appear to rely more heavily on the resistor network and the input protection already present inside the active devices. This may be entirely adequate for the intended module and normal patching, but it provides less deliberately specified fault behaviour than a more defensive Eurorack input stage.

For a revision I would inspect the expected fault currents against the actual MCP6004 input limits before deciding whether extra clamps are necessary.

## Sample/Gate inputs

### The basic approach is simple and useful

The Sample/Gate signals are conditioned with the MCP6004 and resistor networks before reaching D2/D3. For ordinary gate/sample operation this is a cheap and understandable solution.

The choice of D2/D3 is also convenient: both are external-interrupt-capable on the ATmega328P.

### It is not a dedicated comparator/Schmitt-trigger front end

I do not see a separate comparator or a deliberately defined Schmitt-trigger network in the Sample/Gate path. That makes the exact switching behaviour more dependent on the analogue stage and the AVR digital-input thresholds than it would be with a comparator designed specifically for gate detection.

That does not prove there is a practical problem with normal gates. The original module evidently uses this path successfully. The concern is mainly that threshold, hysteresis and noisy/slow edges are not as explicitly controlled as they could be.

### This matters for the proposed Tuner feature

Firmware-side frequency measurement is not the difficult part. D2/D3 edges can be timed accurately enough on the ATmega328P without sampling audio through the ADC or performing audio DSP.

The uncertainty is electrical. A normal Eurorack oscillator output is often bipolar. In the supplied Sample/Gate schematic I could not find a dedicated bipolar-audio protection/clamping stage before the MCP6004 path.

For that reason I would not yet describe the current Sample/Gate input as a supported general-purpose oscillator/audio input. Before enabling a Tuner feature on the existing board, I would want either:

- bench evidence that representative bipolar oscillator signals are safe and produce reliable edges, or
- a small front-end revision with explicit current limiting, clamping and a defined threshold/hysteresis stage.

The latter would make the feature much easier to defend technically.

## DAC and CV outputs

### MCP4922 + reference + TL072 is a good fit for this module

For the original cost target, the output architecture is coherent. One dual 12-bit DAC serves both channels, the separate reference improves repeatability, and the TL072 stages provide the level scaling needed for roughly 0–10 V output.

I do not currently see a reason to replace this part of the design merely for the sake of changing it. A higher-resolution DAC could be interesting for a new design, but that would need a concrete accuracy target and a real benefit at the output rather than a larger number on a datasheet.

### The 1 kΩ output resistors are a good defensive detail

The output paths include 1 kΩ series resistors. That is useful for current limiting and gives the op-amp some isolation from whatever is attached to the jack. It is a small but worthwhile detail I would preserve.

## LED driver and shared SPI

The MCP4922 and TLC5947 share clock/data lines. That is a sensible way to save MCU pins, but it creates a software rule that must never be broken: the TLC5947 must only be latched after a complete intended LED frame has been shifted.

DAC traffic can alter the TLC5947 shift register without changing its visible output latch. That is harmless as long as the firmware always sends a complete 288-bit LED frame before the next latch operation.

The original firmware follows this full-frame approach. Our reimplementation should continue treating this as a hardware invariant and keep it covered by tests where possible.

### PowerPAD soldering on the TLC5947

One assembly detail is worth recording separately because it is easy to overstate in either direction. The `DAP` package of the TLC5947 has an exposed PowerPAD. Texas Instruments characterises the part both with the PowerPAD soldered to a PCB copper area and with the PowerPAD not soldered; the thermal ratings are substantially better in the soldered case. In other words, leaving the pad unsoldered is not equivalent to an immediate functional failure, but it gives away thermal margin.

As I read the supplied design/build approach, the LED driver is not mounted with the exposed pad used in the way TI would normally prefer for maximum thermal performance. I would therefore call this a deviation from the preferred package implementation rather than a dramatic defect. The practical risk in this particular module appears fairly low: the BOM uses a relatively large IREF resistor and the application is nowhere near the TLC5947's 30 mA-per-channel headline capability. That judgement should still be treated as an engineering estimate until temperature has been checked on a built unit under a deliberately bright worst-case LED pattern.

For a board revision I would simply give the PowerPAD a proper copper landing/thermal connection and remove the question. For an already-built unit that behaves normally, I would not regard this point alone as a reason to rework the board.

## Arduino Nano as part of the hardware architecture

The Nano is one of the reasons this project is so approachable. It is inexpensive, socketable, easy to flash and avoids fine-pitch MCU assembly.

The trade-off is that some electrical details are delegated to the particular Nano or Nano-compatible board that gets fitted: decoupling, regulator implementation, USB interface, oscillator and miscellaneous onboard loads can vary between clones.

For a production-oriented mixed-signal design I would probably integrate the MCU. For this project's DIY context, the Nano is a reasonable compromise rather than an obvious mistake.

## BOM and sourcing notes

The published parts information is good enough to make the module buildable, and that deserves credit. There is a useful assembly table, an interactive BOM and a Tayda quick-order list. For an experienced DIY builder who is prepared to cross-check the board, this is a workable starting point.

It is not, however, a completely unambiguous manufacturing BOM. The project repository itself labels the Tayda quick-order CSV as incomplete, which matches what the assembly table shows: several of the more specific parts have to be sourced elsewhere. I also could not find a complete distributor-neutral Excel/XLSX workbook in the Quantizer project tree. The public project material I found consists of the assembly table, interactive BOMs and the incomplete Tayda CSV.

That is not a reason the module cannot be built. It means the builder still has to make a number of engineering and sourcing decisions that a production-style BOM would normally settle explicitly.

### Some entries are more descriptive than procurement-grade

A few examples stood out while cross-checking the published table against the KiCad design:

- The LM4040 is described as a **voltage regulator** in the assembly table. In this circuit it is being used as a **5 V shunt voltage reference**. The intended part is still identifiable, so this is mainly a terminology problem, but it matters when somebody looks for substitutes.
- The resistor notes around the four lower I/O LEDs appear to contain a reference-designator inconsistency. The text says that `R14-R17` control those LEDs, while the KiCad design assigns different values across that range and also uses additional 1 kΩ resistors in the same front-panel/status area. I would not order those resistors from the prose alone; I would use the KiCad values as the source of truth.
- The 10 kΩ analogue resistor groups are listed generically, while the text separately says that some pairs should ideally be matched or lower tolerance. That requirement should really live in the BOM itself. For a precision CV path, `10 kΩ` and `10 kΩ, 0.1 %, matched pair preferred` are meaningfully different purchasing instructions.
- The 100 kΩ input pairs are described as less critical because they act before quantization. That is broadly plausible, but their ratio still influences the analogue scaling seen by the ADC. A complete BOM should state the intended tolerance rather than leave the importance of matching to prose.
- The decoupling capacitors are marked **optional**. The board may run without every one of them, but I would not describe IC-local 100 nF decoupling as optional in a revised build guide. For a mixed digital/analogue module, those parts are cheap enough that the safer instruction is to fit them unless there is a specific reason not to.
- The 100 pF amplifier-stability capacitors are also marked optional. I would want to confirm their exact role against the output-stage compensation before encouraging builders to omit them. If they were added to control stability or edge behaviour, they should be specified as part of the intended circuit, not as a convenience part.
- `TL082 is probably fine too` is a reasonable workshop note, but not a particularly strong substitution rule. It would be better to either specify the tested alternatives or state the electrical requirements a substitute has to meet.
- The Nano entry points builders at a compatible clone rather than one tightly specified implementation. That fits the DIY intent, but Nano-compatible boards can differ in USB interface, regulator, mechanical height and onboard circuitry. A procurement sheet should at least call out the mechanical and electrical assumptions that matter here.
- The 3.5 mm jack entry groups several Thonkiconn/PJ-style names together. These families are often treated as interchangeable in DIY Eurorack, but a complete BOM should still name the tested footprint/mechanical variant explicitly.
- The LED entry is actually one of the better ones: it calls out 3-pin red/green common-anode construction and KAK orientation. The warning that red/green lead assignment can vary between suppliers is useful and should be retained.

There may be further small discrepancies hidden in the published prose, so for any future revision I would generate the BOM from the KiCad source first and then annotate it, rather than maintain the component table independently by hand.

### Builder notes: precision resistors, LEDs and parts quality

A few choices from our own build are useful to record here, but they should not be confused with requirements of the original design. They are practical decisions made while trying to build the module reproducibly rather than cheaply at any cost.

For the resistor pairs that materially affect CV scaling, we used **0.1% precision resistors**. That is probably more accuracy than the basic DIY design strictly requires, especially because trim and firmware calibration can remove part of the static error. I still think it is a sensible place to spend a small amount of money. Good ratio accuracy reduces the amount of error that has to be corrected later and, unlike many cosmetic upgrades, directly supports the function of a quantizer. If a future BOM keeps this choice, it should identify exactly which positions require the tighter tolerance instead of silently turning every resistor into a precision part.

The LED resistors deserve much stronger wording than a normal fixed-value BOM entry. The values published with the original assembly notes were evidently appropriate for the LEDs used by Quinn Freedman when that BOM was written, but they should not be assumed to transfer unchanged to another batch. Forward voltage, luminous efficiency and the red/green brightness ratio can differ substantially between parts that are all sold under a generic description such as "3 mm red/green common-anode LED". This is particularly difficult with inexpensive parts whose manufacturer and optical bin are not traceable.

In our current batch, the values that produced the desired balance were **150 Ω for green and 1.5 kΩ for red**. Those numbers are empirical values for that batch, not recommended universal replacements. A builder should determine the resistor values for the actual LEDs before committing to the full assembly, ideally with one sample on a breadboard or other current-limited test setup. The polarity/color assignment should be checked at the same time.

This is also a good example of why "cheap component" and "equivalent component" are not synonyms. For non-critical passives, anonymous stock may be perfectly adequate. In the precision CV path, voltage reference, semiconductors, switches with mechanical constraints, and LEDs where brightness balance matters, a traceable manufacturer part number has real value: tolerance, temperature behaviour, package dimensions, absolute ratings and optical/electrical characteristics are at least specified and repeatable. With untraceable marketplace parts, the issue is not that they are automatically bad; it is that the builder often has no reliable specification against which a failure or variation can be judged.

My practical rule for this module would therefore be selective rather than dogmatic: commodity parts can remain inexpensive where the circuit is insensitive to them, while the few components that set CV accuracy, reference stability, mechanical fit or visible LED behaviour deserve known specifications.

### Cost target versus a reproducible build

The very low build-cost figure associated with this project is useful as an illustration of how inexpensive the architecture can be, but I would be cautious about treating roughly **20 EUR** as a reproducible end-to-end build cost. I have not found enough source material to reconstruct exactly what was included in that figure, so this note should not be read as a correction of a precise published accounting model.

In practice, a builder has to buy the parts at some point even if some of them later come from a parts drawer. Small-quantity pricing, minimum order quantities, separate switch caps, the DAC, LED driver, reference, Nano, connectors, PCB/front panel, and shipping all count economically. For an EU builder, splitting purchases across Tayda and one or more mainstream distributors can add enough postage and import/VAT handling that the real cost moves noticeably above the headline parts-only figure.

That does not make the design expensive. Quite the opposite: the circuit remains impressively economical for what it does. The distinction I would make in future documentation is between a **theoretical BOM cost using stocked/optimally sourced parts** and a **realistic one-off build cost for somebody starting without the required inventory**. Publishing both would make the cost claim much more useful.

### Tayda is convenient, but not equally convenient everywhere

The Tayda quick-order approach is practical for the original project's audience: it keeps many commodity parts in one basket and makes a one-off DIY order easy. It is less attractive for every builder outside that context.

For builders in the EU, ordering a substantial part of the BOM from a non-EU supplier can add relatively high shipping cost and import/VAT/customs handling compared with buying the same commodity parts from an EU-stocked distributor. The exact cost depends on order size, destination and current import rules, so I would not attach a fixed percentage to it here. The broader point is simply that a Tayda-first BOM is not distributor-neutral.

A more useful project package would therefore provide either:

- additional ready-to-import lists for Mouser, DigiKey and Farnell/element14; or
- one complete spreadsheet (`.xlsx` or `.ods`) with manufacturer part number, value, tolerance, package/footprint, quantity, reference designators, approved alternatives and distributor ordering codes.

The second option is probably the most maintainable. Distributor columns can then be updated without changing the electrical definition of the BOM. It would also make the few genuinely important details -- matched resistor pairs, LED polarity, switch/cap pairing, the exact voltage-reference grade and mechanical parts -- much harder to overlook.

### Buildability versus reproducibility

My current read is that the published documentation is **buildable but not fully reproducible from the BOM alone**. An experienced builder can get the module together successfully because the schematic/KiCad data, interactive BOM and assembly notes fill in the gaps. A less experienced builder is more likely to have to infer things that should ideally be explicit.

That distinction matters. The original documentation is clearly written as practical DIY guidance, not as a contract-manufacturing package. Judged on that basis it works reasonably well. If we are using the design as the starting point for a more polished revision, though, the BOM is one of the places where a relatively small documentation effort would improve the experience a lot.

## Notes for a possible hardware revision

These are not change requests. They are the items I would put on the table before drawing a revised board:

- Decide deliberately whether to keep depending on the Eurorack +5 V rail or generate a local 5 V rail from +12 V.
- Add or at least evaluate ferrite/series isolation between the bus and local analogue/digital domains.
- Decide how much reverse-polarity, fuse and transient protection fits the BOM target.
- Check all external CV/Gate inputs against worst-case negative/positive voltages and decide whether explicit clamps are justified.
- If Tuner becomes a real product feature, use a properly specified protected threshold/comparator input rather than relying on the existing Gate path by accident.
- Revisit the ADC reference choice; using a controlled reference would make the input side more symmetrical with the DAC side.
- Preserve the MCP4922 + dedicated reference concept unless measurements show a reason to change it.
- Preserve output current limiting.
- Preserve the complete-frame/latch rule if DAC and TLC5947 continue to share SPI.
- Measure A/B analogue and ADC isolation before redesigning anything around the observed Channel-B symptom.
- Generate the build BOM from KiCad and make tolerance/matching requirements explicit instead of leaving them in prose.
- Treat IC-local decoupling as part of the intended build unless measurements justify an omission.
- Publish a distributor-neutral master BOM, ideally with Mouser, DigiKey and Farnell/element14 ordering columns in addition to any Tayda convenience list.
- Mark the accuracy-sensitive resistor positions explicitly and consider 0.1% parts there rather than relying on generic 1% stock plus trimming.
- Treat the four discrete LED resistor values as batch-dependent setup values; document a short pre-build brightness/polarity check instead of presenting one historical set as universal.
- Give the TLC5947 PowerPAD a proper thermal landing on a revised PCB, even though the present low-current use makes the practical risk of the existing implementation look modest.
- Separate "minimum theoretical BOM" from "realistic one-off builder cost" when quoting the economics of the module.
- Prefer traceable MPNs for precision, reference, semiconductor and mechanically sensitive parts; allow generic commodity parts where the circuit is genuinely insensitive.

## Current take

My overall impression is favourable. The circuit is clearly optimized for a low-cost, buildable DIY module, and within that constraint it achieves a lot with relatively few parts. The DAC/reference/output section in particular is more thoughtful than one might expect from such a small BOM.

The areas I would be least comfortable carrying forward unchanged are the power entry, the direct dependency on bus +5 V, the lack of clearly visible dedicated input protection, and the fairly opportunistic Sample/Gate conditioning. None of those observations proves the current board is unreliable; they simply leave less engineering margin than I would want if we were turning the design into a new revision with stronger precision and robustness goals.

The documentation has a similar character to the hardware: pragmatic and sufficient to get a determined DIY builder to a working module, but not as tightly specified as I would want for repeatable builds across different regions and suppliers. The BOM would benefit more from clarification and normalization than from being reinvented.

The right next step is measurement, not redesign by assumption. In particular, A/B isolation and the Sample/Gate behaviour with real oscillator waveforms should be verified on the bench before those findings are promoted from "worth checking" to "needs changing".
