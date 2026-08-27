/**
 * @file LedConfig.h
 * Central configuration for LED brightness and startup/error indication.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_LED_CONFIG_H
#define FMQ_CONFIG_LED_CONFIG_H

#include <stdint.h>

namespace fmq::config {

constexpr uint16_t kLedPwmMaximum = 4095;

// Reference values from the Rust original. They are useful for comparison, but
// not universal calibration values. LED efficiency, forward voltage and fitted
// series resistors can make the required red/green PWM values differ radically.
constexpr uint16_t kOriginalRedPwm = 0x0FFF;
constexpr uint16_t kOriginalGreenPwm = 0x004F;

// Empirically tuned defaults for the currently tested hardware. These are
// starting values only and must be adjusted to the actual LEDs and resistors.
constexpr uint16_t kDefaultRedPwm = 0x0480;
constexpr uint16_t kDefaultGreenPwm = 0x0D00;

// Startup note-ring fade. The complete visible ring animation is implemented by
// StartupAnimation and is statically constrained to <=750 ms. There is no
// rotating sequence selection and therefore no startup-animation EEPROM state.
constexpr bool kStartupAnimationEnabled = true;

// Startup error indication used when the resistor-ladder rest calibration is
// implausible.
constexpr uint8_t kStartupErrorFlashCount = 3;
constexpr uint16_t kStartupErrorOnMs = 90;
constexpr uint16_t kStartupErrorOffMs = 70;

static_assert(kDefaultRedPwm <= kLedPwmMaximum &&
                  kDefaultGreenPwm <= kLedPwmMaximum,
              "default PWM out of range");

}  // namespace fmq::config

#endif
