/**
 * @file ButtonLadder.h
 * Declares resistor-ladder note-button decoding and debounce logic.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_BUTTON_LADDER_H
#define FM_QUANTIZER_CORE_BUTTON_LADDER_H

#include <stdint.h>
#include "fmq/config/AnalogConfig.h"
#include "fmq/config/UiConfig.h"


namespace fmq {

enum class ButtonEventType : uint8_t {
  None,
  Held,
  JustPressed,
  JustReleased,
};

struct ButtonEvent {
  ButtonEventType type;
  uint8_t index;

  static ButtonEvent none() { return ButtonEvent{ButtonEventType::None, 0}; }
  static ButtonEvent held(uint8_t i) { return ButtonEvent{ButtonEventType::Held, i}; }
  static ButtonEvent pressed(uint8_t i) {
    return ButtonEvent{ButtonEventType::JustPressed, i};
  }
  static ButtonEvent released() {
    return ButtonEvent{ButtonEventType::JustReleased, 0};
  }
};

constexpr uint8_t kButtonLadderNoButton = config::kLadderNoButton;
constexpr uint16_t kButtonLadderNominalRest = config::kLadderNominalRest;

/** Decode a value already normalised to the nominal 558-count rest level. */
uint8_t closestButtonIndex(uint16_t adcValue);

/** Decode using an explicitly measured unpressed/rest ADC value. */
uint8_t buttonIndexForAdc(uint16_t adcValue, uint16_t restAdc);

class ButtonLadder {
 public:
  ButtonLadder()
      : candidate_(kButtonLadderNoButton),
        debounced_(kButtonLadderNoButton),
        lastChangeTime_(0),
        restAdc_(kButtonLadderNominalRest),
        debouncing_(false) {}

  /** Set the measured no-button value. Values outside a plausible ADC range
   * are ignored, retaining the safe nominal calibration. */
  void calibrateRest(uint16_t adcValue);

  uint16_t restAdc() const { return restAdc_; }
  ButtonEvent sample(uint32_t currentTimeMs, uint16_t adcValue);

 private:
  static constexpr uint32_t kDebounceTimeMs = config::kLadderDebounceMs;

  uint8_t candidate_;
  uint8_t debounced_;
  uint32_t lastChangeTime_;
  uint16_t restAdc_;
  bool debouncing_;
};

}  // namespace fmq

#endif
