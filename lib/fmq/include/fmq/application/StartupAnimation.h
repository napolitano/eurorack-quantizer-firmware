/**
 * @file StartupAnimation.h
 * Declares the rotating set of configurable note-ring power-on animations.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_STARTUP_ANIMATION_H
#define FM_QUANTIZER_CORE_STARTUP_ANIMATION_H

#include <stdint.h>

#include "fmq/config/LedConfig.h"
#include "fmq/domain/LedColor.h"

namespace fmq {

/** Available startup animations. Stored values are deliberately stable. */
enum class StartupSequence : uint8_t {
  ColorFade = 0,  ///< Full ring fades green, red, then amber.
  Glowworm = 1,  ///< Moving head with a dimming tail, once per colour.
  Cog = 2,       ///< Repeating colour pairs rotate like a cog wheel.
  Sparkles = 3,  ///< Deterministic multi-colour twinkles.
};

/** One instant of a startup animation.
 *
 * Each LED has a logical colour and an independent normalized 0..4095
 * intensity. The board controller combines this with the empirically calibrated
 * red/green PWM levels, so every animation respects the user's LED balance.
 */
struct StartupAnimationSample {
  LedFrame frame;
  uint16_t intensityQ12[kNoteCount];

  StartupAnimationSample() : frame(), intensityQ12{} {}
};

/** Generates one of the selectable startup animations. */
class StartupAnimation {
 public:
  static constexpr uint8_t kSequenceCount = 4;

  explicit StartupAnimation(StartupSequence sequence) : sequence_(sequence) {}

  StartupSequence sequence() const { return sequence_; }
  uint32_t durationMs() const;
  bool isDone(uint32_t elapsedMs) const { return elapsedMs >= durationMs(); }
  StartupAnimationSample sampleAt(uint32_t elapsedMs) const;

 private:
  StartupSequence sequence_;

  static uint16_t fadeIntensityAt(uint32_t phaseElapsedMs);
  static LedColor fadeColorForPhase(uint8_t phase);
  static LedColor cogColorForIndex(uint8_t index);

  StartupAnimationSample sampleColorFade(uint32_t elapsedMs) const;
  StartupAnimationSample sampleGlowworm(uint32_t elapsedMs) const;
  StartupAnimationSample sampleCog(uint32_t elapsedMs) const;
  StartupAnimationSample sampleSparkles(uint32_t elapsedMs) const;
};

}  // namespace fmq

#endif
