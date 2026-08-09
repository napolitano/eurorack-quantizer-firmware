# Installing or Updating the Firmware

This guide explains how to install a prebuilt firmware release on the **Free Modular Quantizer** without changing the hardware. The module uses an **Arduino Nano / ATmega328P** and can be flashed over the Nano's USB connection while the Nano remains installed on the Quantizer PCB.

> [!IMPORTANT]
> This is the **end-user firmware installation guide**. Developers building or uploading directly from source should also read the [development environment guide](../development/README.md).

## Contents

- [Safety first](#safety-first)
- [What you need](#what-you-need)
- [Choose the correct firmware file](#choose-the-correct-firmware-file)
- [Remove the module from the rack](#remove-the-module-from-the-rack)
- [Connect the Nano by USB](#connect-the-nano-by-usb)
- [Windows 11: update with AVRDUDESS](#windows-11-update-with-avrdudess)
- [After the update](#after-the-update)
- [macOS and Linux](#macos-and-linux)
- [Troubleshooting](#troubleshooting)
- [Bootloader recovery](#bootloader-recovery)
- [References](#references)

## Safety first

> [!CAUTION]
> **POWER THE EURORACK CASE OFF BEFORE REMOVING THE MODULE.** Do not connect USB while the Quantizer is still connected to the Eurorack power bus. Disconnect the module's ribbon cable from the power bus before attaching USB.

> [!CAUTION]
> **DO NOT TOUCH THE EXPOSED PCB, ARDUINO NANO, HEADERS, OR COMPONENTS.** Handle the module by the edges of the front panel. While the module is powered from USB, keep fingers, tools, conductive surfaces, and loose hardware away from the electronics.

> [!WARNING]
> Use normal ESD precautions and work on a clean, dry, nonconductive surface. If your case uses an external power brick or detachable mains lead, disconnect it before removing or reinstalling the module.

The Nano may remain mounted on the Quantizer PCB during the update. For the procedure described here, the **USB connection powers the Nano**; the Eurorack power ribbon remains disconnected until the USB cable has been removed and the module is ready to go back into the rack.

## What you need

- the Quantizer module;
- a **data-capable USB cable** that fits the installed Nano;
- the firmware HEX file from the GitHub release;
- on Windows, [AVRDUDESS](https://github.com/ZakKemble/AVRDUDESS/releases);
- enough space to place and hold the removed module without touching the PCB.

> [!TIP]
> A charging-only USB cable can power the Nano but cannot upload firmware. If no serial port appears, try a known data cable before changing software settings.

## Choose the correct firmware file

Every tagged release publishes two Nano HEX files:

| Nano bootloader | Release asset | AVRDUDESS baud rate |
|---|---|---:|
| New bootloader / Optiboot | `fm-quantizer-nano-new-bootloader.hex` | `115200` |
| Old Nano bootloader | `fm-quantizer-nano-old-bootloader.hex` | `57600` |

Arduino documents the newer ATmega328P Nano bootloader as the normal choice for boards sold from 2018 onward; older boards and some third-party Nano boards may require the old-bootloader option. If you do not know which bootloader is installed, trying the wrong upload speed should simply fail to establish communication; try the other variant next.

> [!NOTE]
> Both targets use the same **ATmega328P at 16 MHz** hardware. The release keeps separate artifacts so the supported upload path is explicit and reproducible.

## Remove the module from the rack

1. **Switch the Eurorack case OFF.**
2. If applicable, disconnect the case's external power adapter or mains lead.
3. Wait until the case and module LEDs are dark.
4. Remove the module mounting screws.
5. Pull the module forward **by the front-panel edges only**.
6. Disconnect the Eurorack ribbon cable from the module or bus board by its connector housing.
7. Place the module so that the PCB cannot contact metal, tools, screws, rails, or other conductive objects.

> [!CAUTION]
> **THE EURORACK POWER RIBBON MUST BE DISCONNECTED BEFORE USB IS ATTACHED.** Do not leave the module connected to the case bus "just for a moment" during flashing.

## Connect the Nano by USB

With the Eurorack ribbon cable disconnected, plug the USB cable into the Nano and then into the computer. The Nano can remain mounted on the module.

At this point the Nano is powered from USB. Do not handle the PCB or Arduino while it is powered.

## Windows 11: update with AVRDUDESS

For the graphical Windows workflow, use the dedicated **[AVRDUDESS firmware update guide](avrdudess/README.md)**. It contains the numbered AVRDUDESS screenshot, exact control-by-control settings, EEPROM-preserving update procedure, USB/startup sequencing, and Windows-specific troubleshooting.

> [!IMPORTANT]
> The dedicated AVRDUDESS guide is the authoritative Windows end-user procedure. In particular, routine firmware updates preserve EEPROM data rather than erasing user presets and calibration.

## After the update

Only after AVRDUDESS has completed successfully **and** the module has finished its startup sequence:

1. unplug the USB cable from the Nano;
2. make sure the Eurorack case is still OFF;
3. reconnect the Eurorack ribbon cable in its correct orientation;
4. reinstall the module without trapping or straining the ribbon cable;
5. tighten the front-panel screws normally;
6. power the case on;
7. verify normal startup and basic Quantizer operation.

> [!WARNING]
> Before powering the case, verify the power connector orientation against the module and bus-board markings. Do not rely on cable color alone if the cable or connector history is unknown.

## macOS and Linux

AVRDUDESS remains a convenient Windows tool. Its upstream documentation notes that the WinForms/Mono GUI is not supported on current macOS releases. On current macOS and Linux, use the repository's PlatformIO workflow or AVRDUDE directly instead.

The [development guide](../development/README.md) documents the complete PlatformIO setup and upload procedure for Windows, macOS, and Linux.

For users who already have AVRDUDE installed, the equivalent EEPROM-preserving command is:

```text
avrdude -c arduino -P <SERIAL_PORT> -b <BAUD> -p atmega328p -D -U flash:w:<FIRMWARE.hex>:i
```

Use `115200` plus the new-bootloader HEX, or `57600` plus the old-bootloader HEX. Replace `<SERIAL_PORT>` with the actual device, for example `COM7`, `/dev/cu.usbserial-*`, or `/dev/ttyUSB0` as appropriate.

## Troubleshooting

### No serial port appears

- Try a known data-capable USB cable and a different USB port.
- Close PlatformIO serial monitors, Arduino IDE, terminal programs, or any other process that may already own the port.
- Some third-party Nano boards use a **WCH CH340/CH341 USB-to-serial bridge**. WCH publishes a Windows driver that supports Windows 11.

Official WCH driver page:

<https://www.wch-ic.com/downloads/CH341SER_ZIP.html>

### AVRDUDE cannot communicate with the Nano

- Confirm `ATmega328P` is selected.
- Confirm the `arduino` programmer is selected.
- Confirm the correct COM/serial port.
- Try the other supported bootloader/baud combination: new `115200`, old `57600`.
- Disconnect and reconnect USB, then retry.

### Upload starts but fails

> [!CAUTION]
> Do not use `Force (-F)` to push past a device-signature or communication error. Fix the actual port, bootloader, cable, driver, or MCU selection instead.

If the USB serial connection works but uploads continue to fail, the Nano bootloader may need recovery.

### Saved settings disappeared

If **Erase flash and EEPROM (`-e`)** was enabled, EEPROM contents may have been erased. User presets, live state, and calibration values then need to be recreated. This is why routine updates use `-D` and leave `-e` disabled.

## Bootloader recovery

A damaged or missing bootloader is a separate recovery operation and is **not** part of a normal firmware update. Arduino documents how to burn the bootloader on a classic Nano using a second AVR-based Arduino as an ISP programmer.

Official procedure:

<https://support.arduino.cc/hc/en-us/articles/4841602539164-Burn-the-bootloader-on-UNO-Mega-and-classic-Nano-using-another-Arduino>

> [!IMPORTANT]
> Reburning a bootloader changes a lower-level part of the Nano than uploading this firmware. Use that procedure only when normal serial uploads cannot be restored by correcting the port, driver, cable, MCU, or bootloader-speed selection.

## References

- AVRDUDESS repository and documentation: <https://github.com/ZakKemble/AVRDUDESS>
- AVRDUDESS releases: <https://github.com/ZakKemble/AVRDUDESS/releases>
- AVRDUDE documentation: <https://avrdudes.github.io/avrdude/>
- Arduino: select the correct Nano processor/bootloader: <https://support.arduino.cc/hc/en-us/articles/4401874304274-Select-the-right-processor-for-Arduino-Nano>
- Arduino: burn the bootloader on a classic Nano: <https://support.arduino.cc/hc/en-us/articles/4841602539164-Burn-the-bootloader-on-UNO-Mega-and-classic-Nano-using-another-Arduino>
- WCH CH340/CH341 Windows driver: <https://www.wch-ic.com/downloads/CH341SER_ZIP.html>

<p align="center">From Munich With <img src="../assets/blue-heart.svg" width="16" alt="blue heart"></p>
