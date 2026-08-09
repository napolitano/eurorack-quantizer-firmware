# Development Environment and Local Workflow

This guide describes the supported local development workflow for the Free Modular Quantizer alternative firmware. It covers **Windows 11 x64**, the current macOS release line, and common Linux distributions, with **VSCodium** as the reference editor and **PlatformIO Core** as the canonical build, test, and upload interface.

The project deliberately does not depend on editor-specific build buttons. If `pio` works in a normal terminal, the same commands work from VSCodium, another editor, or CI.

> **Hardware boundary:** this repository targets the existing Free Modular Quantizer hardware based on the Arduino Nano / ATmega328P. PCB, component, pin-assignment, and wiring changes are outside the scope of this firmware project.

## 1. Toolchain overview

The repository uses four distinct tool layers:

1. **VSCodium** for editing and source navigation.
2. **Python 3** for PlatformIO Core and repository helper scripts.
3. **PlatformIO Core** for AVR builds, uploads, dependency/toolchain management, and native test orchestration.
4. **A host C/C++ compiler** for the `native` and `native_coverage` environments.

The distinction between the AVR and native compilers is important. PlatformIO downloads and manages the AVR toolchain required by the `nanoatmega328new` and `nanoatmega328` environments. The `native` platform does **not** install a desktop compiler; it uses the GCC/Clang toolchain available on the host operating system.

CI currently runs the project with Python 3.11. Using Python 3.11 locally provides the closest match to CI, although current PlatformIO Core supports newer Python 3 versions as well.

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

Windows requires the most explicit setup because Python and the host GCC toolchain used by native tests are not supplied by PlatformIO itself.

### 4.1 Install VSCodium and Git

Install the current 64-bit VSCodium build and Git for Windows. VSCodium can also be installed through Windows Package Manager:

```powershell
winget install vscodium
```

Verify Git:

```powershell
git --version
```

### 4.2 Install Python 3

Install a current **64-bit Python 3** from python.org. Python 3.11 is the recommended baseline because the GitHub Actions workflows use Python 3.11.

During Python installation, enable **Add Python to PATH**. PlatformIO's Windows installation guidance explicitly relies on this when Python is invoked from a normal terminal.

After installation, open a **new** PowerShell or Command Prompt window and verify:

```powershell
python --version
where.exe python
```

If the Python launcher is installed, this is also useful:

```powershell
py -3 --version
```

Do not continue until `python` resolves to the intended Python 3 installation.

### 4.3 Install PlatformIO Core

Use PlatformIO's official installer script to create its isolated environment under the user profile. After installation, expose the PlatformIO commands to normal terminals by adding the following directory near the beginning of the Windows user `Path` environment variable:

```text
%USERPROFILE%\.platformio\penv\Scripts\
```

Open a new terminal after changing `Path`, then verify:

```powershell
pio --version
where.exe pio
```

The expected executable should resolve below `%USERPROFILE%\.platformio\penv\Scripts\`.

Avoid keeping several unrelated PlatformIO Core installations in `Path`; PlatformIO explicitly recommends using one Core instance to prevent version and package inconsistencies.

### 4.4 Install the native C/C++ compiler

The AVR firmware build does **not** require a manually installed AVR compiler. PlatformIO manages that toolchain.

The host-side tests are different. PlatformIO's `native` platform requires a system GCC toolchain in `PATH`. On Windows, PlatformIO recommends MSYS2. For a current 64-bit setup, install MSYS2 and use its UCRT64 GCC package:

```sh
pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc
```

For the UCRT64 setup, make sure these directories are visible from normal Windows terminals and from VSCodium:

```text
C:\msys64\ucrt64\bin
C:\msys64\usr\bin
```

PlatformIO's general Windows native-toolchain documentation also lists `C:\msys64\mingw64\bin` for MinGW64 installations. Do not add an unused alternative toolchain merely for completeness; the important requirement is that one working GCC/G++ installation resolves unambiguously.

After changing `Path`, start a new terminal and verify:

```powershell
gcc --version
g++ --version
where.exe gcc
where.exe g++
```

For a typical UCRT64 setup, the first compiler path should be below `C:\msys64\ucrt64\bin`.

### 4.5 Windows `Path` checklist

A working Windows 11 x64 setup normally exposes at least:

```text
Python installation directory
Python Scripts directory
%USERPROFILE%\.platformio\penv\Scripts\
C:\msys64\ucrt64\bin
C:\msys64\usr\bin
Git for Windows command directory
```

The Python installer normally manages its own two entries when **Add Python to PATH** is selected. Do not copy a version-specific Python path from another machine; verify the actual installation using `where.exe python`.

Use this diagnostic block after setup:

```powershell
python --version
pio --version
gcc --version
g++ --version
git --version
where.exe python
where.exe pio
where.exe gcc
where.exe g++
```

If these work in PowerShell but not in VSCodium, fully close and restart VSCodium so it inherits the updated environment.

## 5. macOS

The current macOS release line at the time of writing is **macOS Tahoe 26**. The project does not depend on Tahoe-specific APIs; the relevant requirements are Python 3, PlatformIO Core, Git, and an available host C/C++ compiler.

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

- application flash: no more than **92.5% of 30,720 bytes**;
- static SRAM: no more than **70% of 2,048 bytes**.

These are engineering headroom limits, not alternative MCU capacities.

## 8. Coverage

The coverage environment uses the host compiler and GCC coverage instrumentation:

```sh
pio test -e native_coverage
```

To generate the same report formats used by CI, install `gcovr` into the active Python environment and run:

```sh
python -m pip install --upgrade gcovr
mkdir -p coverage
gcovr --root . --filter lib/fmq/src --exclude test --txt --output coverage/coverage.txt
gcovr --root . --filter lib/fmq/src --exclude test --xml-pretty --output coverage/coverage.xml
gcovr --root . --filter lib/fmq/src --exclude test --html-details --output coverage/coverage.html
```

On Windows PowerShell, create the directory with:

```powershell
New-Item -ItemType Directory -Force coverage | Out-Null
```

Coverage is a secondary metric. Behavior, timing boundaries, state transitions, persistence faults, and regressions still require explicit tests.

## 9. Uploading firmware to the Nano

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
- VSCodium: <https://vscodium.com/>
- MSYS2: <https://www.msys2.org/>
