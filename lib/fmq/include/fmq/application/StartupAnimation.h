/**
 * @file StartupAnimation.h
 * Declares the compact fixed note-ring power-on fade.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_APPLICATION_STARTUP_ANIMATION_H
#define FMQ_APPLICATION_STARTUP_ANIMATION_H

#include <stdint.h>

namespace fmq {

/** Normalized red/green intensity for the whole twelve-LED note ring. */
struct StartupAnimationSample {
  uint16_t redQ12 = 0u;
  uint16_t greenQ12 = 0u;
};

/**
 * Fixed startup fade: off -> green -> amber -> red -> off.
 *
 * The animation is intentionally uniform across the complete ring and contains
 * no selectable/random sequence state. This minimizes flash and EEPROM usage.
 */
class StartupAnimation {
 public:
  static constexpr uint16_t kGreenFadeInMs = 150u;
  static constexpr uint16_t kGreenToAmberMs = 200u;
  static constexpr uint16_t kAmberToRedMs = 200u;
  static constexpr uint16_t kRedFadeOutMs = 150u;
  static constexpr uint16_t kDurationMs =
      kGreenFadeInMs + kGreenToAmberMs + kAmberToRedMs + kRedFadeOutMs;

  bool isDone(uint32_t elapsedMs) const { return elapsedMs >= kDurationMs; }
  StartupAnimationSample sampleAt(uint32_t elapsedMs) const;
};

static_assert(StartupAnimation::kDurationMs <= 750u,
              "startup animation must finish within 750 ms");

}  // namespace fmq

#endif
