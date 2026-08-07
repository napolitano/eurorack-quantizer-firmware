/**
 * @file BrightnessCalibration.h
 * Defines independent red/green LED brightness calibration.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_BRIGHTNESS_CALIBRATION_H
#define FM_QUANTIZER_CORE_BRIGHTNESS_CALIBRATION_H

#include <stdint.h>
#include "fmq/config/LedConfig.h"


/**
 * User-adjustable brightness of the red and green LED emitters.
 *
 * The twelve note LEDs are bi-colour. Because red and green emitters usually
 * have very different efficiencies, the firmware drives each colour at an
 * independent PWM level. This structure stores the user's chosen level for each
 * colour as one of twelve discrete steps, set via the LED calibration mode.
 *
 * Until the user calibrates, the empirically chosen defaults from LedConfig are
 * used. Red and green deliberately have independent ranges because the actual
 * LEDs and their series resistors can require very different PWM values.
 */
namespace fmq {

struct BrightnessCalibration {
  /// Sentinel meaning "not yet calibrated – use the configured factory level".
  static constexpr uint8_t kUseLegacyDefault = 0xFF;
  /// Number of discrete brightness steps offered in calibration mode.
  static constexpr uint8_t kStepCount = 12;
  /// Configured factory PWM level for red.
  static constexpr uint16_t kLegacyRedLevel = config::kDefaultRedPwm;
  /// Configured factory PWM level for green.
  static constexpr uint16_t kLegacyGreenLevel = config::kDefaultGreenPwm;

  /// Chosen red step 0..11, or @ref kUseLegacyDefault.
  uint8_t redStep;
  /// Chosen green step 0..11, or @ref kUseLegacyDefault.
  uint8_t greenStep;

  /// @return A calibration using the configured factory levels for both colours.
  static BrightnessCalibration makeDefault() {
    return BrightnessCalibration{kUseLegacyDefault, kUseLegacyDefault};
  }

  /**
   * @brief Map one of the twelve clockwise calibration positions to PWM.
   *
   * Position 0 is true off (0). Position 11 is the TLC5947 hardware maximum
   * (4095). The ten positions between them divide that complete electrical
   * range linearly. This makes calibration predictable and guarantees that the
   * full usable range of the installed hardware is available.
   *
   * @param step Calibration position 0..11; larger values clamp to 11.
   * @return 12-bit PWM level in the inclusive range 0..4095.
   */
  static uint16_t stepToLevel(uint8_t step) {
    if (step >= kStepCount) {
      step = static_cast<uint8_t>(kStepCount - 1u);
    }

    constexpr uint32_t kIntervals = static_cast<uint32_t>(kStepCount - 1u);
    const uint32_t numerator =
        static_cast<uint32_t>(step) * config::kLedPwmMaximum + kIntervals / 2u;
    return static_cast<uint16_t>(numerator / kIntervals);
  }

  /**
   * @brief Find the calibration position nearest to an arbitrary PWM level.
   *
   * Used when calibration is entered with a factory/default value that is not
   * itself one of the twelve discrete calibration levels.
   */
  static uint8_t nearestStepForLevel(uint16_t level) {
    if (level >= config::kLedPwmMaximum) {
      return static_cast<uint8_t>(kStepCount - 1u);
    }

    constexpr uint32_t kIntervals = static_cast<uint32_t>(kStepCount - 1u);
    const uint32_t numerator =
        static_cast<uint32_t>(level) * kIntervals + config::kLedPwmMaximum / 2u;
    const uint32_t step = numerator / config::kLedPwmMaximum;
    return static_cast<uint8_t>(step);
  }

  /// @return The calibration position currently represented by red.
  uint8_t redDisplayStep() const {
    return redStep == kUseLegacyDefault ? nearestStepForLevel(kLegacyRedLevel)
                                        : redStep;
  }

  /// @return The calibration position currently represented by green.
  uint8_t greenDisplayStep() const {
    return greenStep == kUseLegacyDefault
               ? nearestStepForLevel(kLegacyGreenLevel)
               : greenStep;
  }

  /// @return The effective 12-bit PWM level for the red emitter.
  uint16_t redLevel() const {
    return redStep == kUseLegacyDefault
               ? kLegacyRedLevel
               : stepToLevel(redStep);
  }

  /// @return The effective 12-bit PWM level for the green emitter.
  uint16_t greenLevel() const {
    return greenStep == kUseLegacyDefault
               ? kLegacyGreenLevel
               : stepToLevel(greenStep);
  }

  bool operator==(const BrightnessCalibration &o) const {
    return redStep == o.redStep && greenStep == o.greenStep;
  }
  bool operator!=(const BrightnessCalibration &o) const { return !(*this == o); }
};

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_BRIGHTNESS_CALIBRATION_H
