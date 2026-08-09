/**
 * @file LedConfig.h
 * Central configuration for LED brightness, startup animation and status LEDs.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_LED_CONFIG_H
#define FMQ_CONFIG_LED_CONFIG_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"

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

// Startup self-test ---------------------------------------------------------
constexpr bool kStartupAnimationEnabled = true;
constexpr bool kStartupStatusLedTestEnabled = true;
// No note-ring startup animation may keep the twelve note LEDs busy longer
// than this. The separate four-status-LED self-test is not part of this limit.
constexpr uint32_t kStartupRingMaximumDurationMs = 1500;

// Each colour lights all twelve note LEDs at once, fades from off to the
// configured normal brightness, then fades back to off. Order: green, red,
// amber. Amber uses both calibrated colour channels simultaneously.
constexpr uint16_t kStartupFadeInMs = 120;
constexpr uint16_t kStartupFadeOutMs = 140;
constexpr uint16_t kStartupInterColorPauseMs = 20;

// Additional startup sequences. The sequence to play is persisted in EEPROM
// and advances cyclically on each successful power-up animation.
constexpr bool kRotateStartupSequences = true;

// Glowworm: one clockwise revolution in green, red and amber, with a four-LED
// dimming tail behind the moving head.
constexpr uint16_t kGlowwormStepMs = 40;
constexpr uint8_t kGlowwormTailLength = 4;
constexpr uint16_t kGlowwormTailIntensityQ12[kGlowwormTailLength] = {
    4095, 2300, 1100, 450};

// Cog: repeating pairs of red, green and amber rotate three complete turns.
constexpr uint16_t kCogStepMs = 40;
constexpr uint8_t kCogRotations = 3;

// Sparkles: deterministic pseudo-random coloured twinkles, kept deliberately
// short so repeated power cycles do not make the startup effect intrusive.
constexpr uint32_t kSparklesDurationMs = 1450;
constexpr uint16_t kSparklesFrameMs = 70;
constexpr uint8_t kSparklesLitThreshold = 150;
constexpr uint16_t kSparklesMinimumIntensityQ12 = 700;
constexpr uint32_t kSparklesSeed = 0x514E545Au;

// The four discrete channel LEDs are then flashed in physical order.
constexpr uint16_t kStatusLedOnMs = 90;
constexpr uint16_t kStatusLedOffMs = 30;

// Startup error indication used when the resistor-ladder rest calibration is
// implausible.
constexpr uint8_t kStartupErrorFlashCount = 3;
constexpr uint16_t kStartupErrorOnMs = 90;
constexpr uint16_t kStartupErrorOffMs = 70;

static_assert(kDefaultRedPwm <= kLedPwmMaximum &&
                  kDefaultGreenPwm <= kLedPwmMaximum,
              "default PWM out of range");
static_assert(kStartupFadeInMs > 0 && kStartupFadeOutMs > 0,
              "startup fade duration must be nonzero");
static_assert(kGlowwormTailLength > 0 && kGlowwormTailLength <= kNoteCount,
              "invalid Glowworm tail length");
static_assert(kCogRotations > 0, "Cog must rotate at least once");
static_assert(kSparklesDurationMs > 0 && kSparklesFrameMs > 0,
              "Sparkles timing must be nonzero");

}  // namespace fmq::config

#endif
