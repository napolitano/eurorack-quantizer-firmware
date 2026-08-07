/**
 * @file AvrDigitalInput.h
 * Implements an Arduino/AVR digital-input port.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_AVR_DIGITAL_INPUT_H
#define FM_QUANTIZER_HAL_AVR_DIGITAL_INPUT_H

#include <Arduino.h>
#include "fmq/ports/DigitalInput.h"

namespace fmq {
class AvrDigitalInput : public IDigitalInput {
 public:
  AvrDigitalInput(uint8_t pin, bool pullUp) : pin_(pin), pullUp_(pullUp), begun_(false) {}
  void begin() {
    pinMode(pin_, pullUp_ ? INPUT_PULLUP : INPUT);
    begun_ = true;
  }
  bool isHigh() const override { return begun_ && digitalRead(pin_) == HIGH; }
 private:
  uint8_t pin_;
  bool pullUp_;
  bool begun_;
};
}
#endif
