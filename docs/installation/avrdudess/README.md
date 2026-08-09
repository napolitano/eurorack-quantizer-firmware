# Updating the Quantizer Firmware with AVRDUDESS on Windows 11

This guide covers the **Windows 11 x64 end-user update path** for a prebuilt Quantizer firmware release. It uses [AVRDUDESS](https://github.com/ZakKemble/AVRDUDESS), a graphical front end for AVRDUDE, so no development environment is required.

> [!IMPORTANT]
> This document assumes that you are installing a prebuilt `.hex` file from a tagged firmware release. Developers building or deploying from source should use the [development environment guide](../../development/README.md) instead.

## Contents

- [Before you start](#before-you-start)
- [Remove and isolate the module](#remove-and-isolate-the-module)
- [Connect the Arduino Nano by USB](#connect-the-arduino-nano-by-usb)
- [Install AVRDUDESS](#install-avrdudess)
- [Select the correct firmware file](#select-the-correct-firmware-file)
- [Configure AVRDUDESS](#configure-avrdudess)
- [Program the firmware](#program-the-firmware)
- [Wait for the module to restart](#wait-for-the-module-to-restart)
- [Reinstall the module](#reinstall-the-module)
- [Troubleshooting](#troubleshooting)
- [Bootloader recovery](#bootloader-recovery)
- [References](#references)

## Before you start

You need:

- the Quantizer module;
- a **data-capable USB cable** for the installed Arduino Nano;
- the correct Quantizer `.hex` file from the GitHub release;
- [AVRDUDESS](https://github.com/ZakKemble/AVRDUDESS/releases) on Windows 11;
- a clean, dry, nonconductive work surface.

> [!CAUTION]
> **TURN THE EURORACK CASE OFF BEFORE REMOVING THE MODULE.** The module must not remain connected to the Eurorack power bus while USB is attached.

> [!CAUTION]
> **DISCONNECT THE EURORACK RIBBON CABLE BEFORE CONNECTING USB.** This is mandatory. Do not power the module from the Eurorack bus and USB at the same time during this procedure.

> [!CAUTION]
> **DO NOT TOUCH THE PCB, THE ARDUINO NANO, HEADERS, OR COMPONENTS.** Handle the removed module by the **edges of the front panel only**. Keep tools, screws, rails, conductive surfaces, and loose hardware away from the electronics while USB power is present.

## Remove and isolate the module

1. Switch the Eurorack case **OFF**.
2. If applicable, disconnect the case's external power adapter or mains lead.
3. Wait until the module and case LEDs are dark.
4. Remove the module mounting screws.
5. Pull the module forward by the **front-panel edges only**.
6. Disconnect the Eurorack ribbon cable by its connector housing.
7. Put the module on a clean, dry, nonconductive surface where the PCB cannot touch metal.

> [!WARNING]
> Use normal ESD precautions. Do not pull on the ribbon cable itself, and do not use the Arduino Nano or PCB as a handle.

## Connect the Arduino Nano by USB

The Nano may remain installed on the Quantizer PCB. With the Eurorack ribbon cable disconnected, connect the USB cable to the Nano and then to the computer.

The module can be powered from USB for the update. Once USB is connected, do not touch the PCB or Arduino.

> [!TIP]
> A charging-only USB cable can power the Nano but cannot transfer firmware. If Windows does not show a serial port, try a known data-capable cable first.

## Install AVRDUDESS

Download AVRDUDESS from its official GitHub Releases page:

<https://github.com/ZakKemble/AVRDUDESS/releases>

The screenshot below shows **AVRDUDESS 2.20 / AVRDUDE 8.1**. It is used only to identify the relevant controls. Follow the settings in this guide rather than copying every visible checkbox state from the screenshot.

## Select the correct firmware file

Tagged releases provide two Nano firmware images:

| Nano bootloader | Firmware asset | Baud rate |
|---|---|---:|
| New bootloader / Optiboot | `fm-quantizer-nano-new-bootloader.hex` | `115200` |
| Old Nano bootloader | `fm-quantizer-nano-old-bootloader.hex` | `57600` |

Both targets use the same ATmega328P/16 MHz hardware. The difference is the bootloader/upload protocol timing.

> [!NOTE]
> If you do not know which bootloader is installed, start with the new-bootloader image at `115200`. If AVRDUDESS cannot establish communication, retry with the old-bootloader image at `57600`.

## Configure AVRDUDESS

<p align="center">
  <img src="../../assets/installation/avrdudess-2.20-windows-numbered.png" width="900" alt="AVRDUDESS 2.20 on Windows with six numbered controls used for a Quantizer firmware update">
</p>

The blue numbers correspond to these controls:

| No. | Area | What to do |
|---:|---|---|
| **1** | Presets | A preset is not required. Keep the normal/default configuration unless you deliberately maintain your own known-good preset. |
| **2** | Port | Select the COM port that appears when the Arduino Nano is connected. |
| **3** | Baud rate | Use `115200` for the new bootloader or `57600` for the old bootloader. |
| **4** | Flash file | Click `...`, select the matching release `.hex` file, and leave **Write** selected. |
| **5** | Erase flash and EEPROM | For a **normal firmware update, this must be OFF** so saved presets, live state, and calibration data are preserved. |
| **6** | Program! | Starts the upload. Watch the log window below until AVRDUDE reports successful completion. |

Before clicking **Program!**, also verify:

| Control | Required value |
|---|---|
| Programmer | `arduino ... (Arduino bootloader using STK500 v1 protocol)` |
| MCU | `ATmega328P` |
| Disable verify (`-V`) | **OFF** |
| Disable flash erase (`-D`) | **ON** |
| Do not write (`-n`) | **OFF** |
| Force (`-F`) | **OFF** |
| EEPROM file | Leave empty |
| Fuses / lock bits | Do not change |
| Additional command-line arguments | Leave empty |

> [!CAUTION]
> **DO NOT ENABLE `Erase flash and EEPROM (-e)` FOR A NORMAL UPDATE.** The Quantizer stores user scales/configurations, live state, startup metadata, and LED calibration in EEPROM. Erasing EEPROM removes those saved values.

> [!CAUTION]
> **DO NOT CHANGE FUSES OR LOCK BITS.** They are not part of a normal firmware update and incorrect values can make the Nano stop booting normally.

> [!WARNING]
> Do not use **Force (`-F`)** to suppress a signature or communication error. Correct the selected MCU, serial port, cable, driver, or bootloader setting instead.

## Program the firmware

1. Confirm that the Eurorack ribbon cable is **disconnected**.
2. Confirm the correct COM port.
3. Confirm the correct bootloader baud rate.
4. Confirm the matching Quantizer `.hex` file.
5. Confirm that **Erase flash and EEPROM (`-e`) is OFF**.
6. Confirm that **Disable flash erase (`-D`) is ON** and verification remains enabled.
7. Click **Program!**.
8. Do not move the module, touch its electronics, close AVRDUDESS, or disconnect USB while AVRDUDE is writing or verifying.
9. Wait until the log reports successful completion.

A normal successful AVRDUDE run ends with a completion message such as `avrdude done. Thank you.`

## Wait for the module to restart

After programming, the Nano resets and starts the newly installed firmware.

> [!CAUTION]
> **DO NOT UNPLUG THE USB CABLE YET.** The update is not considered complete until AVRDUDE has finished **and the Quantizer has completed its startup LED sequence**.

Wait until the complete startup sequence has run. Only then disconnect the USB cable.

This final boot check confirms that the newly written firmware actually starts before the module is returned to the rack.

## Reinstall the module

After the startup sequence has completed:

1. Disconnect the USB cable.
2. Make sure the Eurorack case is still **OFF**.
3. Reconnect the Eurorack ribbon cable in the correct orientation.
4. Reinstall the module without trapping or straining the cable.
5. Tighten the panel screws normally.
6. Power the Eurorack case on.
7. Verify the normal startup sequence and basic Quantizer operation.

> [!CAUTION]
> **NEVER RECONNECT THE EURORACK POWER BUS WHILE USB IS STILL ATTACHED FOR THIS UPDATE PROCEDURE.** Disconnect USB first, then reconnect the rack power cable while the case remains off.

## Troubleshooting

### No COM port appears

- Try another USB port.
- Try a known data-capable USB cable.
- Close serial monitors, Arduino IDE, PlatformIO, terminal programs, or other applications that may own the port.
- Some Nano-compatible boards use a CH340/CH341 USB-to-serial bridge and may require the appropriate driver.

### AVRDUDE cannot communicate with the Nano

- Confirm `ATmega328P` as the MCU.
- Confirm the `arduino` programmer.
- Confirm the correct COM port.
- Try the other supported bootloader/baud combination.
- Disconnect and reconnect USB, then retry.

### Saved settings disappeared after an update

If **Erase flash and EEPROM (`-e`)** was enabled, the saved EEPROM data may have been erased. User presets, live state, and calibration values then need to be recreated.

## Bootloader recovery

A missing or damaged bootloader is a separate recovery procedure. It is not required for normal firmware updates.

Arduino documents how to burn the bootloader on a classic Nano using another AVR-based Arduino as an ISP programmer:

<https://support.arduino.cc/hc/en-us/articles/4841602539164-Burn-the-bootloader-on-UNO-Mega-and-classic-Nano-using-another-Arduino>

> [!IMPORTANT]
> Reburning the bootloader is a lower-level operation than installing Quantizer firmware. Use it only when normal serial uploading cannot be restored by correcting the USB cable, driver, COM port, MCU, or bootloader-speed selection.

## References

- AVRDUDESS repository: <https://github.com/ZakKemble/AVRDUDESS>
- AVRDUDESS releases: <https://github.com/ZakKemble/AVRDUDESS/releases>
- AVRDUDE documentation: <https://avrdudes.github.io/avrdude/>
- Arduino Nano processor/bootloader selection: <https://support.arduino.cc/hc/en-us/articles/4401874304274-Select-the-right-processor-for-Arduino-Nano>
- Arduino classic Nano bootloader recovery: <https://support.arduino.cc/hc/en-us/articles/4841602539164-Burn-the-bootloader-on-UNO-Mega-and-classic-Nano-using-another-Arduino>

---

<p align="center">From Munich With <img src="../../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
