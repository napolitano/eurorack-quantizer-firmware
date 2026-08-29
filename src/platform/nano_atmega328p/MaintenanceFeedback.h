/**
 * @file MaintenanceFeedback.h
 * Visual status language for panel-driven calibration/maintenance firmware.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_MAINTENANCE_FEEDBACK_H
#define FMQ_PLATFORM_NANO_MAINTENANCE_FEEDBACK_H

#include <stdint.h>

#include "fmq/ports/DigitalOutput.h"
#include "platform/nano_atmega328p/Tlc5947LedDriver.h"

namespace fmq::platform::nano {

/** Drives all visual guidance used by the panel calibration state machine. */
class MaintenanceFeedback {
 public:
  MaintenanceFeedback(Tlc5947LedDriver &ring, IDigitalOutput &inputLedA,
                      IDigitalOutput &outputLedA, IDigitalOutput &inputLedB,
                      IDigitalOutput &outputLedB, bool statusLedsActiveHigh)
      : ring_(ring), inputLedA_(inputLedA), outputLedA_(outputLedA),
        inputLedB_(inputLedB), outputLedB_(outputLedB),
        statusLedsActiveHigh_(statusLedsActiveHigh) {}

  void begin();
  void playBootIndication();
  void showIdle();

  /** Show C#/A fit points or C/F verification points as pulsing green targets. */
  void showVoltagePrompt(uint8_t channel, uint8_t noteIndex, bool lit);

  /** Blink the matching IN/OUT pair to request one loopback patch. */
  void showLoopPrompt(uint8_t channel, bool lit);

  /** Blink both IN/OUT pairs to request both loopbacks for final verification. */
  void showDualLoopPrompt(bool lit);

  /** Ring off during ADC capture; selected input LED remains on. */
  void beginAdcMeasurement(uint8_t channel);

  /** Ring off during loopback capture; matching IN/OUT LEDs remain on. */
  void beginLoopMeasurement(uint8_t channel);

  /** Ring off during dual-channel final verification. */
  void beginDualLoopMeasurement();

  void showPass();
  void showFail();
  void showQualificationPass();
  void showQualificationFail();

  /** Show two red FAIL flashes followed by @p code red note LEDs. */
  void showErrorCode(uint8_t code);

 private:
  void setStatus(bool inputA, bool outputA, bool inputB, bool outputB);
  void writeUniform(uint16_t redLevel, uint16_t greenLevel);
  void writeSingle(uint8_t noteIndex, uint16_t redLevel, uint16_t greenLevel);
  void writeErrorCode(uint8_t code);
  void showRingOff();
  void blink(uint16_t redLevel, uint16_t greenLevel, uint8_t count);

  Tlc5947LedDriver &ring_;
  IDigitalOutput &inputLedA_;
  IDigitalOutput &outputLedA_;
  IDigitalOutput &inputLedB_;
  IDigitalOutput &outputLedB_;
  bool statusLedsActiveHigh_;
};

}  // namespace fmq::platform::nano

#endif
