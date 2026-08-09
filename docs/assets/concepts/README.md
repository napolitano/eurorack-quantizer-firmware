# Quantizer concept artwork

Reusable SVG diagrams for user-facing and technical documentation. The files are deliberately kept independent from the front-panel pictograms in `docs/assets/` so they can be reused in GitHub Markdown and the LibreOffice user manual.

## Functional palette

| Role | Color |
|---|---|
| Primary signal / action | `#1B9DD9` |
| Timing / transformation parameter | `#F28C28` |
| Positive / Channel A state where semantically appropriate | `#2FA45A` |
| Negative / Channel B state where semantically appropriate | `#D94B3D` |
| Reference / inactive information | neutral gray |
| Structure / axes / text | near-black |

The SVGs use a white canvas deliberately so they remain readable in GitHub dark mode and when imported into the white-page LibreOffice manual. Text uses `Ubuntu, Arial, sans-serif` fallbacks.

## Assets

| File | Concept |
|---|---|
| `quantization.svg` | Continuous input CV mapped to discrete enabled notes |
| `track-vs-sample.svg` | Track-and-Hold versus Sample-and-Hold |
| `scale-rotation.svg` | Rotation of enabled pitch classes |
| `glide.svg` | Smoothed transition toward a discrete target |
| `sample-delay.svg` | Delayed sampling after a rising gate edge |
| `pitch-shifts.svg` | Pre-shift, quantization, scale-shift and post-shift order |
| `relative-channel-b.svg` | Absolute versus Relative Channel B |
| `linked-channels.svg` | Shared configuration with independent CV paths |

## Editing rules

Keep explanatory prose outside the SVG whenever the meaning can live in Markdown or the manual body. Preserve `<title>` and `<desc>` metadata for accessibility, use consistent stroke weights and rounded geometry, and do not encode firmware behavior that is not verified by the implementation or specification.

---

<p align="center">From Munich With <img src="../blue-heart.svg" alt="blue heart" width="14" height="14"></p>
