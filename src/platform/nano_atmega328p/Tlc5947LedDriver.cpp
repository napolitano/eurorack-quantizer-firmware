/**
 * @file Tlc5947LedDriver.cpp
 * Implements TLC5947 frame transmission and output control.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/nano_atmega328p/Tlc5947LedDriver.h"

namespace fmq {

void Tlc5947LedDriver::begin() {
  // Blank the outputs while we shift in an all-zero frame.
  blank_.set(true);
  uint8_t zeros[kTlc5947FrameBytes];
  for (uint8_t i = 0; i < kTlc5947FrameBytes; ++i) {
    zeros[i] = 0;
  }
  chipSelect_.set(false);
  spi_.transfer(zeros, kTlc5947FrameBytes);
  chipSelect_.set(true);
  // Enable the outputs.
  blank_.set(false);
}

void Tlc5947LedDriver::writeFrame(const uint8_t bytes[kTlc5947FrameBytes]) {
  // Copy into a scratch buffer because the SPI transfer overwrites its input
  // with the (unused) bytes read back from the driver.
  uint8_t scratch[kTlc5947FrameBytes];
  for (uint8_t i = 0; i < kTlc5947FrameBytes; ++i) {
    scratch[i] = bytes[i];
  }
  chipSelect_.set(false);
  spi_.transfer(scratch, kTlc5947FrameBytes);
  chipSelect_.set(true);  // rising edge latches the shifted data
}

}  // namespace fmq
