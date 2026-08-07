/**
 * @file LedColor.h
 * Defines logical note-ring LED colours.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_LED_COLOR_H
#define FM_QUANTIZER_CORE_LED_COLOR_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"

/**
 * Colours available on the twelve bi-colour note LEDs.
 *
 * Each note LED contains a red and a green emitter driven by two PWM channels of
 * the TLC5947 driver. The four logical colours below combine those two emitters.
 * The actual brightness of red and green is controlled separately by the
 * @ref BrightnessCalibration and applied when the frame is encoded for the
 * driver.
 */
namespace fmq {

/// Logical colour of a single bi-colour note LED.
enum class LedColor : uint8_t {
  Off = 0,    ///< Both emitters dark.
  Green = 1,  ///< Green emitter only.
  Red = 2,    ///< Red emitter only.
  Amber = 3,  ///< Red and green together.
};

/// @return true if the red emitter is lit for this colour.
constexpr bool ledUsesRed(LedColor c) {
  return c == LedColor::Red || c == LedColor::Amber;
}

/// @return true if the green emitter is lit for this colour.
constexpr bool ledUsesGreen(LedColor c) {
  return c == LedColor::Green || c == LedColor::Amber;
}

/// A full frame of the twelve note LEDs.
struct LedFrame {
  LedColor leds[kNoteCount];

  /// Construct a frame with every LED off.
  LedFrame() {
    for (uint8_t i = 0; i < kNoteCount; ++i) {
      leds[i] = LedColor::Off;
    }
  }

  /// Element access for convenience.
  LedColor &operator[](uint8_t i) { return leds[i]; }
  LedColor operator[](uint8_t i) const { return leds[i]; }

  /// Value equality (used to skip redundant SPI writes).
  bool operator==(const LedFrame &other) const {
    for (uint8_t i = 0; i < kNoteCount; ++i) {
      if (leds[i] != other.leds[i]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const LedFrame &other) const { return !(*this == other); }
};

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_LED_COLOR_H
