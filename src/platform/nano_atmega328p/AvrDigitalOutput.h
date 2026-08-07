/**
 * @file AvrDigitalOutput.h
 * Implements an Arduino/AVR digital-output port.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_AVR_DIGITAL_OUTPUT_H
#define FM_QUANTIZER_HAL_AVR_DIGITAL_OUTPUT_H

#include <Arduino.h>
#include "fmq/ports/DigitalOutput.h"

namespace fmq {
class AvrDigitalOutput : public IDigitalOutput {
 public:
  explicit AvrDigitalOutput(uint8_t pin, bool initialHigh = false)
      : pin_(pin), initialHigh_(initialHigh), begun_(false) {}
  void begin() {
    digitalWrite(pin_, initialHigh_ ? HIGH : LOW);  // preload output latch
    pinMode(pin_, OUTPUT);
    begun_ = true;
  }
  void set(bool high) override {
    if (!begun_) return;
    digitalWrite(pin_, high ? HIGH : LOW);
  }
 private:
  uint8_t pin_;
  bool initialHigh_;
  bool begun_;
};
}
#endif
