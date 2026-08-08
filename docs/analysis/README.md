# Engineering notes

This directory is where I keep the working notes made while getting familiar with the original Quantizer before changing anything substantial. They are deliberately separate from the user documentation: the point here is to record what looks good, what deserves another look, and what should be measured or verified before a redesign.

- [`hardware-findings.md`](hardware-findings.md) — notes from the supplied KiCad design set and published build/BOM material, covering power, analogue I/O, Sample/Gate conditioning, DAC/output stage, shared SPI, sourcing and Tuner feasibility.
- [`original-firmware-analysis.md`](original-firmware-analysis.md) — notes from reading the original Rust firmware, including the ideas worth preserving, edge cases and robustness issues, and implications for the C++ reimplementation.

The wording is intentionally cautious. Some observations are obvious from the design or source; others are only plausible explanations until they have been checked on real hardware. If bench measurements or a later source review contradict something here, the notes should change with the evidence.

Nothing in this directory is an automatic change request. It is preparation for engineering decisions.
