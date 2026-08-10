# Contributing

Contributions are welcome when they preserve the behavior, constraints, and maintainability goals of the project.

> [!IMPORTANT]
> This firmware targets the **existing Free Modular Quantizer hardware**. PCB, component, pin-assignment, or wiring changes are out of scope and belong in a separate hardware project.

Please read the [Code of Conduct](CODE_OF_CONDUCT.md) before participating. Security vulnerabilities must be reported according to [.github/SECURITY.md](.github/SECURITY.md), not through a public issue.

## Contents

- [Before opening an issue](#before-opening-an-issue)
- [Development environment](#development-environment)
- [Coding requirements](#coding-requirements)
- [Documentation style](#documentation-style)
- [Pull requests](#pull-requests)
- [Changelog policy](#changelog-policy)
- [Releases](#releases)
- [Licensing](#licensing)

## Before opening an issue

Use the repository issue forms where possible. A useful bug report identifies the firmware version or commit, the affected mode or channel, the exact steps required to reproduce the behavior, the expected result, and the observed result. Include relevant build output, serial output, measurements, or photographs when they materially help diagnosis.

For feature requests, describe the user-facing problem first and the proposed implementation second. New firmware behavior must remain compatible with the existing Arduino Nano R3 / ATmega328P hardware and its current wiring.

## Development environment

For complete Windows 11 x64, macOS, Linux and VSCodium setup instructions, including the native host compiler, Python/`PATH` requirements, local tests and firmware upload, see [docs/development/README.md](docs/development/README.md).

The project uses PlatformIO and C++17. The supported target environments are:

- `nanoatmega328new` — Arduino Nano / ATmega328P with the newer bootloader;
- `nanoatmega328` — Arduino Nano / ATmega328P with the old bootloader;
- `native` — host-side Unity test suite;
- `native_coverage` — host-side coverage build.

The normal local verification commands are:

```sh
pio test -e native
pio run -e nanoatmega328new
pio run -e nanoatmega328
```

Validate requirement-to-test traceability with:

```sh
python scripts/check_requirement_traceability.py
```

Generate and enforce native coverage with:

```sh
pio test -e native_coverage
mkdir -p coverage
gcovr --root . --filter lib/fmq/src --exclude test --xml-pretty --output coverage/coverage.xml
python scripts/check_native_coverage.py coverage/coverage.xml
```

Coverage regressions are gated at **92.0% lines / 70.0% branches** for portable production sources. Do not lower the policy floor merely to make a pull request pass.

After an AVR build, the same engineering resource gate used by CI can be checked with:

```sh
python scripts/check_avr_resource_budget.py .pio/build/nanoatmega328new/firmware.elf
python scripts/check_avr_resource_budget.py .pio/build/nanoatmega328/firmware.elf
```

The current engineering limits are 92.5% of the 30,720-byte application-flash budget and 70% of the 2 KB static-SRAM budget.

## Coding requirements

Keep changes small enough to review and test. Production code must remain suitable for the ATmega328P resource envelope and the deterministic 1 kHz control path.

- Use C++17 as configured by PlatformIO.
- Keep code readable, explicit, and maintainable; avoid cleverness that obscures timing or state transitions.
- Do not introduce dynamic allocation into the real-time firmware path.
- Document public interfaces and non-obvious behavior with Doxygen-compatible comments.
- Preserve channel isolation and existing hardware pin assignments.
- Do not require PCB, component, or wiring modifications.
- Add or update native tests for behavior changes and regressions.
- Keep warnings clean under the repository's strict host-side warning configuration.

## Documentation style

Documentation is written for GitHub rendering first and should use [GitHub Flavored Markdown](https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax) deliberately rather than treating Markdown as plain text. Prefer:

- a clear heading hierarchy so GitHub's document outline remains useful;
- relative links for files and images inside the repository;
- fenced code blocks with the appropriate language where syntax highlighting helps;
- tables for compact comparisons and fixed option matrices;
- task lists only for actual checkable work;
- GitHub alerts (`[!NOTE]`, `[!TIP]`, `[!IMPORTANT]`, `[!WARNING]`, `[!CAUTION]`) for information whose significance would otherwise be easy to miss.
- the shared `docs/assets/blue-heart.svg` asset for the centered `From Munich With` documentation footer; do not replace it with a platform-dependent emoji.

> [!TIP]
> Keep alerts scarce and specific. GitHub recommends using them only when they materially improve user success rather than turning every paragraph into a callout. One or two well-placed alerts are usually more effective than a page full of them.

Long Markdown documents with more than three major sections should include a **Contents** index near the top so readers can jump directly to the relevant section. Keep the index focused on useful navigation rather than listing every small subsection.

Use the repository's documentation formats according to the information being explained:

| Format | Preferred use |
|---|---|
| Markdown tables | Parameter maps, command/reference matrices, compact comparisons |
| GitHub alerts | Important notes, recommendations, warnings and cautions |
| Mermaid | Technical flows, state transitions and software architecture that benefit from source-controlled diagrams |
| GitHub math | Equations and algorithm definitions where mathematical notation is clearer than prose or code |
| Task lists | Actionable verification, calibration, PR and release checklists |
| SVG under `docs/assets/` | Reusable user-facing concepts and diagrams that also belong in the LibreOffice manual |

Reusable Quantizer concept artwork lives under `docs/assets/concepts/`. Keep its functional palette consistent: `#1B9DD9` blue for the primary signal/action, orange for timing or transformation parameters, green/red only for genuine channel or state semantics, and neutral gray/black for references and structure.

Purely editorial changes do not belong in the changelog unless they represent a substantial documentation release, such as a new user-manual version.

## Pull requests

Create a focused branch and keep each pull request centered on one coherent change. The pull request template describes the information expected for review.

Before submitting a pull request:

1. run the native test suite;
2. build both Nano environments;
3. run the AVR resource-budget checks when the AVR toolchain is available;
4. update tests for changed behavior;
5. update user or technical documentation when behavior actually changes;
6. verify that no generated build artifacts, Python bytecode, editor files, or local paths are included.

CI is expected to pass before a change is merged.


When a pull request is intended to close a tracked issue, use GitHub's supported closing keywords in the PR description, for example `Fixes #123`, `Closes #123`, or `Resolves #123`. Use them only when merging the PR should actually close that issue.

## Changelog policy

`CHANGELOG.md` records release-relevant changes, not routine editorial activity.

Add an entry when a change affects firmware behavior, compatibility, persistence, timing, tests or CI in a meaningful way, release engineering, security, or a substantial user-facing document such as a new manual version. Do not add entries for wording cleanup, README reordering, formatting changes, or similar editorial work with no material release impact.

Do not rewrite historical release sections. New work belongs under `Unreleased` until a release is prepared.

## Releases

Release preparation is maintainer-controlled. Version metadata, changelog release sections, tags, generated release notes, checksums, firmware binaries, and compatible manual assets are handled by the repository release workflow.

Do not create or move release tags as part of a normal contribution unless explicitly coordinated with the maintainer.

## Licensing

By contributing, you agree that your contribution may be distributed under the licenses applicable to the files and components you modify. The firmware repository license and any separately licensed documentation or third-party assets remain distinct; check the relevant license file before modifying those materials.

---

<p align="center">From Munich With <img src="docs/assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
