# Development Environment and Local Workflow

This guide describes the supported local development workflow for the Free Modular Quantizer alternative firmware. It covers **Windows 11 x64**, the current macOS release line, and common Linux distributions, with **VSCodium** as the reference editor and **PlatformIO Core** as the canonical build, test, and upload interface.

The project deliberately does not depend on editor-specific build buttons. If `pio` works in a normal terminal, the same commands work from VSCodium, another editor, or CI.

> [!IMPORTANT]
> **Hardware boundary:** This repository targets the existing Free Modular Quantizer hardware based on the Arduino Nano / ATmega328P. PCB, component, pin-assignment, and wiring changes are outside the scope of this firmware project.

## Contents

- [Original project and upstream reference material](#original-project-and-upstream-reference-material)
- [1. Toolchain overview](#1-toolchain-overview)
- [2. Repository checkout](#2-repository-checkout)
- [3. VSCodium](#3-vscodium)
- [4. Windows 11 x64](#4-windows-11-x64)
  - [4.1 What Windows actually needs](#41-what-windows-actually-needs)
  - [4.3 Install Python 3](#43-install-python-3)
  - [4.4 Install PlatformIO Core](#44-install-platformio-core)
  - [4.5 Install MSYS2 UCRT64 GCC/G++ for native tests](#45-install-msys2-ucrt64-gccg-for-native-tests)
  - [4.6 Recommended Windows Path model](#46-recommended-windows-path-model)
  - [4.7 Verify both Windows compiler paths](#47-verify-both-windows-compiler-paths)
- [5. macOS](#5-macos)
- [6. Linux](#6-linux)
- [7. First project verification](#7-first-project-verification)
- [8. Coverage](#8-coverage)
- [9. Uploading firmware to the Nano](#9-uploading-firmware-to-the-nano)
- [10. Hardware smoke test after deployment](#10-hardware-smoke-test-after-deployment)
- [11. Normal development cycle](#11-normal-development-cycle)
- [12. Project-specific files](#12-project-specific-files)
- [13. Troubleshooting quick checks](#13-troubleshooting-quick-checks)
- [14. Upstream setup references](#14-upstream-setup-references)

## Original project and upstream reference material

This firmware targets Quinn Freedman's existing **Free Modular Quantizer** hardware. When working on compatibility, hardware behavior, or differences from the original implementation, use the upstream project as the primary reference context rather than inferring behavior from this repository alone.

- **Free Modular Quantizer project page:** <https://freemodular.org/modules/Quantizer/>  
  The public project page collects the original user manual, assembly instructions and BOM, interactive BOMs, Gerber files, firmware HEX, schematic, source-code links, and the published module specifications.
- **Original Quantizer source tree:** <https://github.com/QuinnFreedman/modular/tree/main/modules/Quantizer>  
  This is the module-specific source tree. It contains the original Rust firmware, documentation, faceplate sources, and the PCB design. The KiCad sources under `PCBs/quantizer_pcb/` include the front-panel schematic, main PCB schematic and board, and rear-panel schematic; `PCBs/quantizer_faceplate/` contains the KiCad faceplate project.
- **Shared `fm-lib` used by the original firmware:** <https://github.com/QuinnFreedman/modular/tree/main/fm-lib>  
  The original Quantizer firmware depends on this shared Rust library through a local Cargo path dependency (`../../../fm-lib`). Hardware-facing behavior such as ADC handling, EEPROM access, button debouncing, DAC support, and timing helpers therefore may live in `fm-lib` rather than in `modules/Quantizer/Firmware` itself.

For source-level compatibility work, use the references in this order:

1. Check `modules/Quantizer/Firmware` for Quantizer-specific behavior and UI semantics.
2. Follow calls into `fm-lib` for shared AVR and peripheral behavior.
3. Use the KiCad sources under `modules/Quantizer/PCBs` to verify electrical assumptions, pin routing, normalizations, and the analogue signal path.
4. Use the Free Modular project page and its linked manual/build material for the original user-facing behavior and published hardware context.

When an issue or regression test depends on a particular upstream implementation detail, record the relevant upstream file and commit SHA in the engineering note or issue. The upstream repository can evolve independently of this firmware.

## 1. Toolchain overview

The repository uses four distinct tool layers:

| Layer | Purpose | Provided by |
|---|---|---|
| VSCodium | Editing, navigation, integrated terminal | VSCodium |
| Python 3 | PlatformIO Core and repository helper scripts | Host OS / Python installation |
| PlatformIO Core | AVR builds, uploads, packages, toolchains, and test orchestration | PlatformIO |
| Host C/C++ compiler | `native`, `native_coverage`, and `native_sanitized` tests | Host operating system |

**Toolchain split:** The AVR compiler and the native-test compiler are **not the same toolchain**. PlatformIO downloads and manages `avr-g++` for `nanoatmega328new` and `nanoatmega328`. The `native` environments deliberately use the host compiler discovered through `PATH`; PlatformIO does not install that compiler for you.

This distinction matters most on Windows. A machine can build and upload the Nano firmware successfully while `pio test -e native` still fails because no host `gcc`/`g++` is visible. Conversely, installing a desktop C++ compiler does not replace PlatformIO's managed AVR compiler.

The project uses GCC-style warnings and `gcov`/`gcovr` coverage instrumentation. The documented Windows host toolchain is therefore **MSYS2 UCRT64 GCC/G++**. Microsoft Visual C++ Build Tools may be useful for other projects, but they are not the reference compiler for this repository's `native` and coverage environments.

CI is pinned to Python **3.11.15**. Using the same interpreter locally provides the closest match to CI; the pinned PlatformIO/gcovr versions are listed in `scripts/requirements-ci.txt`.

## 2. Repository checkout

Clone the repository and open the project root, the directory containing `platformio.ini`:

```sh
git clone https://github.com/napolitano/eurorack-quantizer-firmware.git
cd eurorack-quantizer-firmware
codium .
```

All commands in this guide are intended to be run from this project root unless stated otherwise.

## 3. VSCodium

VSCodium is the reference editor for this guide. It is a freely licensed build of the VS Code source and is available for Windows, macOS, and Linux.

PlatformIO's official IDE extension is documented for Microsoft Visual Studio Code. VSCodium uses the Open VSX extension registry by default, so extension availability is not identical. For that reason, this project treats the **PlatformIO Core CLI as authoritative**. A compatible editor extension may be used for convenience, but it is not required for building, testing, or flashing the firmware.

The integrated VSCodium terminal is suitable once the same `python`, `pio`, and host compiler commands are visible there as in the operating-system terminal.

## 4. Windows 11 x64

Windows needs the most explicit setup because three independent pieces must resolve correctly from the same terminal session: **Python**, **PlatformIO Core**, and a **host GCC/G++ toolchain** for native tests.

> [!WARNING]
> A successful Nano build does **not** prove that the Windows C++ test toolchain is configured. The AVR environments use PlatformIO's downloaded AVR compiler; `native`, `native_coverage`, and `native_sanitized` use the desktop `gcc`/`g++` found through `PATH`.

### 4.1 What Windows actually needs

| Task | Required executable/toolchain | Where it comes from |
|---|---|---|
| Run PlatformIO and repository Python helpers | Python 3 | Python installation / PlatformIO environment |
| Build or upload the Nano firmware | `avr-g++`, AVR binutils, uploader | Managed automatically by PlatformIO |
| Run `pio test -e native` | desktop `gcc` and `g++` | MSYS2 UCRT64 |
| Run `native_coverage` | desktop GCC/G++ plus `gcov` | MSYS2 UCRT64 |
| Git operations | `git` | Git for Windows |
| Editing | `codium` | VSCodium |

No global `CC` or `CXX` environment variable is required for the normal project setup. The intended configuration lets PlatformIO discover `gcc` and `g++` from `PATH`.

### 4.2 Install VSCodium and Git

Install the current 64-bit VSCodium build and Git for Windows. VSCodium can also be installed through Windows Package Manager if the package is available on the machine:

```powershell
winget install -e --id VSCodium.VSCodium
```

Verify Git:

```powershell
git --version
where.exe git
```

### 4.3 Install Python 3

Install a current **64-bit Python 3** from python.org. Python 3.11 is the recommended baseline because the GitHub Actions workflows use Python 3.11.

During installation, enable **Add Python to PATH**. PlatformIO's Windows guidance explicitly requires a usable Python command when installing Core from a normal terminal.

Open a **new** PowerShell window and verify the actual interpreter rather than trusting the command name:

```powershell
python --version
python -c "import sys; print(sys.executable)"
where.exe python
py -3 --version
```

**Windows Python alias:** If `where.exe python` resolves first to `%LOCALAPPDATA%\Microsoft\WindowsApps\python.exe` and launches the Microsoft Store instead of the installed interpreter, either move the real Python installation ahead of that alias in `Path` or disable the App Installer `python.exe` / `python3.exe` entries under **Manage app execution aliases**. Microsoft documents this Windows behavior explicitly.

Do not continue until Python resolves predictably. Mixing a python.org interpreter, a Microsoft Store alias, and an MSYS2 Python installation without checking resolution order is a common source of confusing PlatformIO behavior. When you need to force the python.org interpreter explicitly, use the Windows launcher with a version selector such as `py -3.11`.

### 4.4 Install PlatformIO Core

Use PlatformIO's official installer script. It creates an isolated PlatformIO Python environment below the user profile, normally under:

```text
%USERPROFILE%\.platformio\penv\
```

PlatformIO's Windows shell-command documentation requires the following directory in the Windows `Path` and recommends placing it near the beginning:

```text
%USERPROFILE%\.platformio\penv\Scripts\
```

Open a new terminal after changing `Path`, then verify:

```powershell
pio --version
where.exe pio
```

The expected `pio.exe` should resolve below `%USERPROFILE%\.platformio\penv\Scripts\`.

Avoid several unrelated PlatformIO Core installations in the same `PATH`. One predictable Core installation is considerably easier to diagnose than a mixture of editor-bundled, `pip`-installed, and standalone instances.

### 4.5 Install MSYS2 UCRT64 GCC/G++ for native tests

The firmware's AVR compiler is already handled by PlatformIO. This step is **only** for the host-side `native`, `native_coverage`, and `native_sanitized` environments.

Install current 64-bit MSYS2 using its official installer. MSYS2 recommends **UCRT64** when unsure; UCRT64 is the current GCC-based 64-bit environment and uses the Windows Universal C Runtime.

Launch the **MSYS2 UCRT64** terminal and update the package database/system first:

```sh
pacman -Syu
```

If MSYS2 requests a terminal restart during the full update, restart UCRT64 and run the update again before installing packages. Then install the GCC toolchain:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

For this project, expose the UCRT64 toolchain to normal Windows terminals and VSCodium through:

```text
C:\msys64\ucrt64\bin
C:\msys64\usr\bin
```

PlatformIO's Native-platform documentation lists the MSYS2 compiler directories as `PATH` requirements. MSYS2 itself recommends UCRT64 for new 64-bit GCC setups; this project therefore standardizes on UCRT64 rather than mixing UCRT64 and the legacy MINGW64 environment.

After changing `Path`, fully close and reopen PowerShell and VSCodium, then verify:

```powershell
gcc --version
g++ --version
where.exe gcc
where.exe g++
```

For the documented setup, the first GCC/G++ paths should resolve below `C:\msys64\ucrt64\bin`.

### 4.6 Recommended Windows `Path` model

Use the **User** `Path` unless there is a specific reason to configure the toolchain for every account on the machine. Exact Python installation paths vary by Python version and installation choice; never copy another machine's version-specific directory blindly.

The following entries must be reachable:

```text
%USERPROFILE%\.platformio\penv\Scripts\
<real Python installation directory>
<real Python Scripts directory>
C:\msys64\ucrt64\bin
C:\msys64\usr\bin
<Git for Windows command directory>
```

The critical point is not a universal magic ordering; it is **unambiguous command resolution**. PlatformIO's shell-command directory should be early enough that the intended `pio` wins, the real Python interpreter must win over the Windows Store alias, and UCRT64 `gcc`/`g++` must win over stale or unrelated GCC installations.

Use this diagnostic block after setup and whenever native tests behave differently between terminals:

```powershell
python --version
python -c "import sys; print(sys.executable)"
pio --version
gcc --version
g++ --version
git --version
where.exe python
where.exe pio
where.exe gcc
where.exe g++
where.exe git
```

PowerShell can also show the executable that wins command resolution:

```powershell
Get-Command python, pio, gcc, g++, git | Format-Table Name, Source
```

If the commands work in an external PowerShell but not in VSCodium, **fully exit and restart VSCodium**. Existing GUI processes do not automatically inherit later changes to Windows environment variables.

### 4.7 Verify both Windows compiler paths

Run these as two separate checks:

```powershell
# Host compiler path used by native tests
pio test -e native

# PlatformIO-managed AVR compiler path used by the hardware target
pio run -e nanoatmega328new
```

Both must succeed for a complete development environment. If only the AVR build succeeds, troubleshoot MSYS2/GCC. If `pio` itself fails before either command starts, troubleshoot Python/PlatformIO first.

## 5. macOS

At the time of the firmware 0.2.0 release, the current macOS release is **macOS Tahoe 26.6.1**. The project does not depend on Tahoe-specific APIs; the relevant requirements are Python 3, PlatformIO Core, Git, and an available host C/C++ compiler.

### 5.1 Install VSCodium

Install VSCodium from the project distribution. With Homebrew already available, the VSCodium project documents:

```sh
brew install --cask vscodium
```

### 5.2 Install the host compiler

Install Apple's Xcode Command Line Tools:

```sh
xcode-select --install
```

PlatformIO recommends this for the native development platform on macOS. Verify the compiler:

```sh
cc --version
c++ --version
```

### 5.3 Python and PlatformIO Core

Verify Python 3 first:

```sh
python3 --version
```

Python 3.11 is the closest match to CI. Install PlatformIO Core using its official installer script, then expose the installed commands through your shell. PlatformIO documents user-local symlinks such as:

```sh
mkdir -p "$HOME/.local/bin"
ln -s ~/.platformio/penv/bin/platformio ~/.local/bin/platformio
ln -s ~/.platformio/penv/bin/pio ~/.local/bin/pio
ln -s ~/.platformio/penv/bin/piodebuggdb ~/.local/bin/piodebuggdb
```

Ensure `~/.local/bin` is appended to `PATH`, for example in `~/.zprofile`:

```sh
export PATH="$PATH:$HOME/.local/bin"
```

Restart the terminal session and verify:

```sh
pio --version
which pio
```

## 6. Linux

The exact package names vary by distribution. Debian and Ubuntu are used here as the reference because the repository CI also runs on Ubuntu.

### 6.1 Base packages

On Debian/Ubuntu:

```sh
sudo apt update
sudo apt install git curl build-essential python3 python3-venv
```

`build-essential` supplies the host GCC/G++ toolchain required by PlatformIO's native platform. Verify:

```sh
python3 --version
gcc --version
g++ --version
git --version
```

### 6.2 Install PlatformIO Core

Use PlatformIO's official installer script and expose `pio` using the same user-local symlink method documented for Unix-like systems:

```sh
mkdir -p "$HOME/.local/bin"
ln -s ~/.platformio/penv/bin/platformio ~/.local/bin/platformio
ln -s ~/.platformio/penv/bin/pio ~/.local/bin/pio
ln -s ~/.platformio/penv/bin/piodebuggdb ~/.local/bin/piodebuggdb
```

Add `~/.local/bin` to the shell `PATH` without replacing the existing value:

```sh
export PATH="$PATH:$HOME/.local/bin"
```

Then verify:

```sh
pio --version
which pio
```

### 6.3 USB/serial permissions

PlatformIO recommends installing its `99-platformio-udev.rules` on Linux for supported USB devices. Alternatively, serial access can be granted through the appropriate system group, commonly `dialout` on Debian/Ubuntu systems.

Use PlatformIO to identify the connected board and its serial device:

```sh
pio device list
```

After changing udev rules or group membership, reconnect the Nano; a login/session restart may also be required for group changes.

## 7. First project verification

After cloning the repository and completing the operating-system setup, verify the environment before changing code.

### 7.1 Native tests

Run all host-side unit, integration, regression, and system tests:

```sh
pio test -e native
```

A single suite can be selected with `-f`, for example:

```sh
pio test -e native -f unit/test_arpeggiator_matrix
pio test -e native -f integration/test_arpeggiator_layer
pio test -e native -f system/test_signal_path
```

The native suite compiles the portable production code on the host and applies the repository's strict warning policy. A failing native compiler setup is therefore usually a host-toolchain problem rather than an AVR-toolchain problem.

Run the aggregate sanitizer environment as an additional pre-merge check:

```sh
pio test -e native_sanitized
```

This executes the portable core under AddressSanitizer and UndefinedBehaviorSanitizer; it does not affect the AVR binary.

### 7.2 Build both supported Nano variants

Newer Nano bootloader:

```sh
pio run -e nanoatmega328new
```

Older Nano bootloader / compatible older clone:

```sh
pio run -e nanoatmega328
```

The resulting firmware files are written below:

```text
.pio/build/nanoatmega328new/
.pio/build/nanoatmega328/
```

### 7.3 Check AVR resource budgets

After both AVR builds, run the same engineering-budget checks used by CI. Use `python3` instead of `python` on Unix systems if that is the local Python 3 command.

```sh
python scripts/check_avr_resource_budget.py .pio/build/nanoatmega328new/firmware.elf
python scripts/check_avr_resource_budget.py .pio/build/nanoatmega328/firmware.elf
```

Current engineering gates:

- application flash: no more than **95% of 30,720 bytes (29,184 bytes)**;
- static SRAM: no more than **70% of 2,048 bytes**.

These are engineering headroom limits, not alternative MCU capacities.

Compile the qualification-only timing image as a separate check when target-level timing is relevant:

```sh
pio run -e nanoatmega328new_timing
```

This image repurposes Nano D1/TX as an oscilloscope probe and is **not** user firmware. See [ATmega328P timing qualification](../testing/timing-qualification.md).

## 8. Coverage

The coverage environment uses the host compiler and GCC coverage instrumentation:

```sh
pio test -e native_coverage
```

To generate the same report formats used by CI, install `gcovr` into the active Python environment and run:

```sh
python -m pip install -r scripts/requirements-ci.txt
mkdir -p coverage
gcovr --root . --filter lib/fmq/src --exclude test --txt --output coverage/coverage.txt
gcovr --root . --filter lib/fmq/src --exclude test --xml-pretty --output coverage/coverage.xml
gcovr --root . --filter lib/fmq/src --exclude test --json-pretty --output coverage/coverage.json
gcovr --root . --filter lib/fmq/src --exclude test --html-details --output coverage/coverage.html
python scripts/check_native_coverage.py coverage/coverage.xml
```

On Windows PowerShell, create the directory with:

```powershell
New-Item -ItemType Directory -Force coverage | Out-Null
```

Coverage is a secondary metric. Behavior, timing boundaries, state transitions, persistence faults, and regressions still require explicit tests. The current CI floors are **92.0% lines** and **70.0% branches** for `lib/fmq/src/`; see [the native coverage policy](../testing/coverage.md). The acceptance-criterion mapping is documented in [the traceability matrix](../testing/requirements-traceability.md).

## 9. Uploading firmware to the Nano

> [!NOTE]
> If you are installing a **prebuilt GitHub Release HEX** rather than developing from source, use the dedicated [end-user firmware installation/update guide](../installation/README.md). It documents safe rack removal, AVRDUDESS, bootloader selection, EEPROM-preserving update settings, and recovery.


Connect the Arduino Nano used by the module and identify its serial port:

```sh
pio device list
```

For a Nano using the newer bootloader:

```sh
pio run -e nanoatmega328new -t upload
```

For a Nano using the old bootloader:

```sh
pio run -e nanoatmega328 -t upload
```

PlatformIO normally auto-detects the upload port. When several serial devices are connected, specify it explicitly:

```sh
pio run -e nanoatmega328new -t upload --upload-port <PORT>
```

Typical port names are `COMx` on Windows, `/dev/cu.*` on macOS, and `/dev/ttyUSB*` or `/dev/ttyACM*` on Linux. Always use the port reported for the actual connected Nano rather than relying on a hard-coded example.

If upload synchronization fails on otherwise working hardware, first verify the selected PlatformIO environment matches the Nano bootloader variant, then verify the serial port and USB/serial driver or permission state. Do not change board wiring or firmware pin assignments as a workaround.

## 10. Hardware smoke test after deployment

**Hardware verification:** Native tests validate firmware logic, not the physical analog path. A change that affects CV conversion, timing, LEDs, persistence, or external clock/gate behavior is not fully verified until it has passed the relevant real-module smoke test.

Native tests cannot validate the analog signal path, actual LED brightness, DAC accuracy, or physical Eurorack timing. After flashing a firmware change, perform a short hardware smoke test before considering the change verified:

1. Power-cycle the module and confirm the expected startup ring animation followed by the four discrete status-LED self-test.
2. Verify normal Quantizer operation on Channel A and Channel B independently.
3. Confirm scale editing and the selected-channel LED behavior.
4. Double-click SHIFT to enter the Arpeggiator layer and confirm the two green enable flashes.
5. Double-click SHIFT again and confirm Arpeggiator playback stops, the Quantizer layer returns, and the ring flashes red twice.
6. When persistence code changed, power-cycle after changing state and verify that the saved live state, selected channel, and active UI layer restore correctly.
7. When Sample/Gate or clock handling changed, verify both physical inputs independently with representative Eurorack signals.
8. When analog conversion or calibration code changed, verify measured CV/DAC behavior on the real module rather than relying only on host tests.

Do not infer or modify Eurorack power wiring from this development guide; use the existing module hardware and its assembly documentation.

## 11. Normal development cycle

Create a focused branch before making a change:

```sh
git switch -c feature/short-description
```

During development, run the narrowest relevant native suite frequently. Before a pull request or release candidate, run the complete local verification sequence:

```sh
pio test -e native
pio test -e native_sanitized
pio run -e nanoatmega328new
pio run -e nanoatmega328
python scripts/check_avr_resource_budget.py .pio/build/nanoatmega328new/firmware.elf
python scripts/check_avr_resource_budget.py .pio/build/nanoatmega328/firmware.elf
```

Then inspect the repository before committing:

```sh
git status
git diff
```

Do not commit `.pio/`, Python bytecode, editor caches, local compilation databases, machine-specific paths, or generated coverage output.

Behavior changes require corresponding tests and Doxygen-compatible documentation where public interfaces or non-obvious behavior are affected. Hardware changes are not accepted in this repository.

## 12. Project-specific files

The most relevant entry points for development are:

```text
platformio.ini                 PlatformIO environments and compiler flags
src/                           Arduino/AVR integration layer
lib/fmq/include/fmq/           public firmware-core interfaces
lib/fmq/src/                   portable firmware-core implementation
test/                          native unit/integration/regression/system tests
scripts/                       CI, release, coverage and resource helpers
README_TESTING.md              detailed test strategy
docs/testing/                   coverage, traceability, timing and hardware qualification
docs/development/maintenance-policy.md  mature-firmware maintenance/resource policy
README_CONFIGURATION.md        firmware configuration reference
README_CALIBRATION.md          calibration workflow
README_ARPEGGIATOR.md          Arpeggiator behavior and controls
CONTRIBUTING.md                contribution and changelog policy
```

## 13. Troubleshooting quick checks

### `pio` is not found

Verify PlatformIO's executable directory is in `PATH`:

- Windows: `%USERPROFILE%\.platformio\penv\Scripts\`
- macOS/Linux: `~/.platformio/penv/bin/` directly, or user-local symlinks in `~/.local/bin`

Then open a new terminal and run `pio --version`.

### Native tests report that GCC/G++ is missing

The AVR build toolchain is unrelated to this error. Verify the host compiler:

```sh
gcc --version
g++ --version
```

On Windows, verify `where.exe gcc` and `where.exe g++` point to the intended MSYS2 UCRT64 toolchain. On Linux, install the distribution's build toolchain. On macOS, install the Xcode Command Line Tools.

### Windows finds the wrong Python, GCC, or PlatformIO

Inspect resolution order:

```powershell
where.exe python
where.exe pio
where.exe gcc
where.exe g++
```

Remove stale or duplicate entries from the Windows user/system `Path`, then restart the terminal and VSCodium. PlatformIO also advises against maintaining multiple independent PlatformIO Core installations.

### The Nano is not found for upload

Run:

```sh
pio device list
```

Verify the USB cable supports data, the expected serial device appears, and the correct bootloader environment is selected. On Linux, verify udev/group permissions. USB/serial drivers depend on the USB interface fitted to the particular Nano or clone and are not changed by this firmware.

## 14. Upstream setup references

- PlatformIO Core installation: <https://docs.platformio.org/en/latest/core/installation/>
- PlatformIO native platform/compiler requirements: <https://docs.platformio.org/en/latest/platforms/native.html>
- PlatformIO shell commands and `PATH`: <https://docs.platformio.org/en/latest/core/installation/shell-commands.html>
- PlatformIO Linux udev rules: <https://docs.platformio.org/en/latest/core/installation/udev-rules.html>
- Microsoft Python-on-Windows guidance and App Execution Aliases: <https://learn.microsoft.com/windows/dev-environment/python>
- VSCodium: <https://vscodium.com/>
- MSYS2 installation: <https://www.msys2.org/>
- MSYS2 environment selection (UCRT64): <https://www.msys2.org/docs/environments/>

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
