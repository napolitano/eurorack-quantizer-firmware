# Contributing

Contributions are welcome when they preserve the behavior, constraints, and maintainability goals of the project. This firmware targets the existing Free Modular Quantizer hardware; hardware changes are not part of this repository.

Please read the [Code of Conduct](CODE_OF_CONDUCT.md) before participating. Security vulnerabilities must be reported according to [.github/SECURITY.md](.github/SECURITY.md), not through a public issue.

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

Coverage can be generated with:

```sh
pio test -e native_coverage
```

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

## Changelog policy

`CHANGELOG.md` records release-relevant changes, not routine editorial activity.

Add an entry when a change affects firmware behavior, compatibility, persistence, timing, tests or CI in a meaningful way, release engineering, security, or a substantial user-facing document such as a new manual version. Do not add entries for wording cleanup, README reordering, formatting changes, or similar editorial work with no material release impact.

Do not rewrite historical release sections. New work belongs under `Unreleased` until a release is prepared.

## Releases

Release preparation is maintainer-controlled. Version metadata, changelog release sections, tags, generated release notes, checksums, firmware binaries, and compatible manual assets are handled by the repository release workflow.

Do not create or move release tags as part of a normal contribution unless explicitly coordinated with the maintainer.

## Licensing

By contributing, you agree that your contribution may be distributed under the licenses applicable to the files and components you modify. The firmware repository license and any separately licensed documentation or third-party assets remain distinct; check the relevant license file before modifying those materials.
