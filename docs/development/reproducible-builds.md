# Reproducible build inputs

The project pins the tool versions that materially affect firmware and native verification so an unchanged source tree does not silently move to a different compiler/platform generation during CI.

## Contents

- [Pinned inputs](#pinned-inputs)
- [Resolved AVR packages](#resolved-avr-packages)
- [Release build metadata](#release-build-metadata)
- [Local reproduction](#local-reproduction)
- [Update policy](#update-policy)

## Pinned inputs

| Input | Pinned value | Location |
|---|---:|---|
| Python in GitHub Actions | 3.11.15 | workflow `setup-python` steps |
| PlatformIO Core | 6.1.19 | `scripts/requirements-ci.txt` |
| gcovr | 8.6 | `scripts/requirements-ci.txt` |
| PlatformIO Atmel AVR platform | 5.3.0 | `platformio.ini` |
| PlatformIO Native platform | 1.2.1 | `platformio.ini` |
| Firmware language mode | GNU C++17 | `platformio.ini` |
| GitHub runner image | Ubuntu 24.04 | workflow `runs-on` values |
| `actions/checkout` | 6.0.2 | workflow action reference |
| `actions/setup-python` | 6.2.0 | workflow action reference |
| `actions/cache` | 4.2.0 | workflow action reference |
| `actions/upload-artifact` | 7.0.1 | workflow action reference |

> [!IMPORTANT]
> Do not replace exact tool versions with an unbounded `pip install --upgrade ...` in CI or release workflows. Toolchain updates are deliberate maintenance changes and must pass the full native, sanitizer, coverage, AVR-build, resource, and hardware-qualification paths as applicable.

## Resolved AVR packages

PlatformIO's pinned Atmel AVR platform resolves the Arduino AVR framework, AVR-GCC toolchain, AVRDUDE, and related packages. The release workflow records the exact resolved package list and the AVR compiler version in `BUILD-INFO.txt` so every published binary carries its build provenance alongside it.

This is more useful than relying on a README that can become stale after a later toolchain update.

## Release build metadata

Tagged releases include `BUILD-INFO.txt` in `dist/`. The file records:

- runner/host platform;
- Python version;
- PlatformIO Core version;
- exact Git commit;
- resolved Python package set (`pip freeze`);
- resolved PlatformIO package versions for the Nano environment;
- `avr-g++ --version` output.

`BUILD-INFO.txt` is included in SHA-256 and MD5 checksum manifests with the firmware and manual artifacts.

## Local reproduction

Install the same Python tooling used by CI:

```sh
python -m pip install -r scripts/requirements-ci.txt
```

Then build the release target:

```sh
pio run -e nanoatmega328new
pio run -e nanoatmega328
```

Inspect resolved packages with:

```sh
pio pkg list -e nanoatmega328new
```

Host compiler versions still differ by operating system for `native` tests. Release AVR binaries, however, are built through the pinned PlatformIO AVR platform/toolchain rather than the host C++ compiler.

## Update policy

1. Update one toolchain layer deliberately.
2. Run all native tests and ASan/UBSan.
3. Re-measure native coverage.
4. Build both AVR environments.
5. Compare flash/SRAM usage with the previous baseline.
6. Perform hardware qualification when generated AVR code or runtime behavior may have changed.
7. Record release-relevant toolchain changes under `Unreleased`.

GitHub-hosted CI uses a fixed Ubuntu runner generation and exact released action tags rather than floating major tags. Dependabot checks both GitHub Actions and the pinned Python CI requirements weekly; updates remain reviewable pull requests and do not bypass the normal verification policy.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
