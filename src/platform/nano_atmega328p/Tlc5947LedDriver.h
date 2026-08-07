/**
 * @file Tlc5947LedDriver.h
 * Declares the TLC5947 note-ring LED driver.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_AVR_TLC5947_LED_DRIVER_H
#define FM_QUANTIZER_HAL_AVR_TLC5947_LED_DRIVER_H

#include <stdint.h>

#include "fmq/ui/LedFrameEncoder.h"
#include "fmq/ports/DigitalOutput.h"
#include "fmq/ports/SpiBus.h"

/**
 * Driver for the TLC5947 24-channel 12-bit PWM LED driver.
 *
 * Drives the twelve bi-colour note LEDs. Like the DAC driver it is bus-agnostic
 * (an @ref ISpiBus plus chip-select and BLANK @ref IDigitalOutput lines) and so
 * host-testable. The frame is already-encoded bytes from @ref encodeLedFrame.
 */
namespace fmq {

class Tlc5947LedDriver {
 public:
  Tlc5947LedDriver(ISpiBus &spi, IDigitalOutput &chipSelect,
                   IDigitalOutput &blank)
      : spi_(spi), chipSelect_(chipSelect), blank_(blank) {}

  /// Clear all channels and enable the outputs (BLANK low).
  void begin();

  /// Shift a full 36-byte frame to the driver and latch it.
  void writeFrame(const uint8_t bytes[kTlc5947FrameBytes]);

 private:
  ISpiBus &spi_;
  IDigitalOutput &chipSelect_;
  IDigitalOutput &blank_;
};

}  // namespace fmq

#endif  // FM_QUANTIZER_HAL_AVR_TLC5947_LED_DRIVER_H
