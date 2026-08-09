# Engineering notes

> [!NOTE]
> These files are engineering observations and source-analysis notes, not normative user documentation and not automatic change requests. Claims that depend on hardware behavior remain provisional until verified on the physical module.

This directory is where I keep the working notes made while getting familiar with the original Quantizer before changing anything substantial. They are deliberately separate from the user documentation: the point here is to record what looks good, what deserves another look, and what should be measured or verified before a redesign.

- [`hardware-findings.md`](hardware-findings.md) — notes from the supplied KiCad design set and published build/BOM material, covering power, analogue I/O, Sample/Gate conditioning, DAC/output stage, shared SPI, sourcing and Tuner feasibility.
- [`original-firmware-analysis.md`](original-firmware-analysis.md) — notes from reading the original Rust firmware, including the ideas worth preserving, edge cases and robustness issues, and implications for the C++ reimplementation.

## Primary upstream references

These notes should be read against Quinn Freedman's published original sources:

- Free Modular Quantizer project page: <https://freemodular.org/modules/Quantizer/>
- Original Quantizer module tree, including Rust firmware, documentation, and KiCad sources: <https://github.com/QuinnFreedman/modular/tree/main/modules/Quantizer>
- Shared Rust `fm-lib` used by the original firmware: <https://github.com/QuinnFreedman/modular/tree/main/fm-lib>

The original firmware is not self-contained inside `modules/Quantizer/Firmware`: its `Cargo.toml` references `fm-lib` by local path. Source-level analysis therefore needs both trees. Electrical claims should additionally be checked against the KiCad projects under `modules/Quantizer/PCBs`.

The wording is intentionally cautious. Some observations are obvious from the design or source; others are only plausible explanations until they have been checked on real hardware. If bench measurements or a later source review contradict something here, the notes should change with the evidence.

Nothing in this directory is an automatic change request. It is preparation for engineering decisions.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
