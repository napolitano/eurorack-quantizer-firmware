# Interactive calibration guide

This directory contains the self-contained browser-based assistant for the FM Quantizer calibration and maintenance workflow.

The normative procedure remains [`../../../README_CALIBRATION.md`](../../../README_CALIBRATION.md). The interactive guide is a user-interface representation of that procedure and must not introduce undocumented calibration requirements or acceptance limits.

## Run locally

Open `index.html` directly in a modern browser. No package manager, framework, backend, network access, or build step is required.

For development through a local HTTP server, from the repository root:

```text
python -m http.server 8000
```

Then open:

```text
http://localhost:8000/docs/calibration/interactive/
```

## Design constraints

- Static HTML, CSS and vanilla JavaScript only.
- No remote dependencies and no telemetry.
- No access to serial ports or hardware from the browser.
- No claim that the guide can detect USB/Eurorack power state.
- `README_CALIBRATION.md` remains normative.
- Wiring illustrations are shared from `docs/manual/assets/calibration/`.
- External DMM errors are calculated but are not automatically declared PASS/FAIL unless a normative external acceptance limit is added to the project specification.
