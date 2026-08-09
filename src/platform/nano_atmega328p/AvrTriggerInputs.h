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

/** A batch of external-interrupt activations captured since last consume. */
struct TriggerActivations {
  uint8_t count;
  uint32_t latestTimestampUs;

  bool any() const { return count != 0u; }
};

/**
 * Latches electrically active edges on the two external-interrupt inputs.
 *
 * The ISR stores a microsecond timestamp immediately at the physical edge and
 * counts every activation until the 1 kHz control loop consumes the batch. This
 * removes the former +/-1 ms period quantisation and prevents multiple clock
 * edges inside one control tick from collapsing into a single boolean event.
 */
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

  TriggerActivations consumeActivationsA() {
    TriggerActivations value{};
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      value.count = activationCountA_;
      value.latestTimestampUs = latestActivationUsA_;
      activationCountA_ = 0u;
    }
    return value;
  }

  TriggerActivations consumeActivationsB() {
    TriggerActivations value{};
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      value.count = activationCountB_;
      value.latestTimestampUs = latestActivationUsB_;
      activationCountB_ = 0u;
    }
    return value;
  }

 private:
  static void activationA() {
    if (!instance_) return;
    if (instance_->activationCountA_ != static_cast<uint8_t>(0xFFu)) {
      ++instance_->activationCountA_;
    }
    instance_->latestActivationUsA_ = ::micros();
  }

  static void activationB() {
    if (!instance_) return;
    if (instance_->activationCountB_ != static_cast<uint8_t>(0xFFu)) {
      ++instance_->activationCountB_;
    }
    instance_->latestActivationUsB_ = ::micros();
  }

  uint8_t pinA_;
  uint8_t pinB_;
  bool activeHigh_;
  bool pullUp_;
  volatile uint8_t activationCountA_ = 0u;
  volatile uint8_t activationCountB_ = 0u;
  volatile uint32_t latestActivationUsA_ = 0u;
  volatile uint32_t latestActivationUsB_ = 0u;
  static AvrTriggerInputs *instance_;
};

inline AvrTriggerInputs *AvrTriggerInputs::instance_ = nullptr;
}  // namespace fmq
#endif
