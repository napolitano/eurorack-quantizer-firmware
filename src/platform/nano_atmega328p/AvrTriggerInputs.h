/**
 * @file AvrTriggerInputs.h
 * Captures trigger levels and short rising edges on AVR interrupt pins.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_AVR_TRIGGER_INPUTS_H
#define FMQ_PLATFORM_NANO_AVR_TRIGGER_INPUTS_H

#include <Arduino.h>
#include <util/atomic.h>

namespace fmq {

/** Latches the electrically active edge on the two external-interrupt inputs. */
class AvrTriggerInputs {
 public:
  AvrTriggerInputs(uint8_t pinA, uint8_t pinB, bool activeHigh, bool pullUp)
      : pinA_(pinA), pinB_(pinB), activeHigh_(activeHigh), pullUp_(pullUp) {}

  void begin() {
    pinMode(pinA_, pullUp_ ? INPUT_PULLUP : INPUT);
    pinMode(pinB_, pullUp_ ? INPUT_PULLUP : INPUT);
    instance_ = this;
    const int mode = activeHigh_ ? RISING : FALLING;
    attachInterrupt(digitalPinToInterrupt(pinA_), activationA, mode);
    attachInterrupt(digitalPinToInterrupt(pinB_), activationB, mode);
  }

  bool levelA() const { return (digitalRead(pinA_) == HIGH) == activeHigh_; }
  bool levelB() const { return (digitalRead(pinB_) == HIGH) == activeHigh_; }

  bool consumeActivationA() {
    bool value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      value = activationA_;
      activationA_ = false;
    }
    return value;
  }

  bool consumeActivationB() {
    bool value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      value = activationB_;
      activationB_ = false;
    }
    return value;
  }

 private:
  static void activationA() { if (instance_) instance_->activationA_ = true; }
  static void activationB() { if (instance_) instance_->activationB_ = true; }

  uint8_t pinA_;
  uint8_t pinB_;
  bool activeHigh_;
  bool pullUp_;
  volatile bool activationA_ = false;
  volatile bool activationB_ = false;
  static AvrTriggerInputs *instance_;
};

inline AvrTriggerInputs *AvrTriggerInputs::instance_ = nullptr;
}  // namespace fmq
#endif
