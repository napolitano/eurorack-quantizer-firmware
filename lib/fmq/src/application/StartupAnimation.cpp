/**
 * @file StartupAnimation.cpp
 * Implements the selectable note-ring startup animations.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/StartupAnimation.h"

namespace fmq {
namespace {

constexpr uint8_t kGreenPhase = 0;
constexpr uint8_t kRedPhase = 1;
constexpr uint8_t kAmberPhase = 2;
constexpr uint8_t kStartupColorCount = 3;

constexpr uint32_t kColorFadePhaseDurationMs =
    config::kStartupFadeInMs + config::kStartupFadeOutMs +
    config::kStartupInterColorPauseMs;
constexpr uint32_t kColorFadeDurationMs =
    kColorFadePhaseDurationMs * kStartupColorCount;
constexpr uint32_t kGlowwormDurationMs =
    static_cast<uint32_t>(config::kGlowwormStepMs) * kNoteCount *
    kStartupColorCount;
constexpr uint32_t kCogDurationMs =
    static_cast<uint32_t>(config::kCogStepMs) * kNoteCount *
    config::kCogRotations;

static_assert(kColorFadeDurationMs <= config::kStartupRingMaximumDurationMs,
              "ColorFade exceeds startup note-ring duration limit");
static_assert(kGlowwormDurationMs <= config::kStartupRingMaximumDurationMs,
              "Glowworm exceeds startup note-ring duration limit");
static_assert(kCogDurationMs <= config::kStartupRingMaximumDurationMs,
              "Cog exceeds startup note-ring duration limit");
static_assert(config::kSparklesDurationMs <=
                  config::kStartupRingMaximumDurationMs,
              "Sparkles exceeds startup note-ring duration limit");

uint32_t mixBits(uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7FEB352Du;
  value ^= value >> 15u;
  value *= 0x846CA68Bu;
  value ^= value >> 16u;
  return value;
}

void setUniformIntensity(StartupAnimationSample &sample, uint16_t intensity) {
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    sample.intensityQ12[i] = intensity;
  }
}

}  // namespace

uint32_t StartupAnimation::durationMs() const {
  switch (sequence_) {
    case StartupSequence::ColorFade:
      return kColorFadeDurationMs;
    case StartupSequence::Glowworm:
      return kGlowwormDurationMs;
    case StartupSequence::Cog:
      return kCogDurationMs;
    case StartupSequence::Sparkles:
      return config::kSparklesDurationMs;
  }
  return kColorFadeDurationMs;
}

uint16_t StartupAnimation::fadeIntensityAt(uint32_t phaseElapsedMs) {
  if (phaseElapsedMs < config::kStartupFadeInMs) {
    return static_cast<uint16_t>(
        (phaseElapsedMs * config::kLedPwmMaximum) / config::kStartupFadeInMs);
  }

  const uint32_t fadeOutElapsed = phaseElapsedMs - config::kStartupFadeInMs;
  if (fadeOutElapsed < config::kStartupFadeOutMs) {
    const uint32_t remaining = config::kStartupFadeOutMs - fadeOutElapsed;
    return static_cast<uint16_t>(
        (remaining * config::kLedPwmMaximum) / config::kStartupFadeOutMs);
  }

  return 0;
}

LedColor StartupAnimation::fadeColorForPhase(uint8_t phase) {
  switch (phase) {
    case kGreenPhase:
      return LedColor::Green;
    case kRedPhase:
      return LedColor::Red;
    default:
      return LedColor::Amber;
  }
}

LedColor StartupAnimation::cogColorForIndex(uint8_t index) {
  const uint8_t pair = static_cast<uint8_t>((index / 2u) % 3u);
  switch (pair) {
    case 0:
      return LedColor::Red;
    case 1:
      return LedColor::Green;
    default:
      return LedColor::Amber;
  }
}

StartupAnimationSample StartupAnimation::sampleColorFade(
    uint32_t elapsedMs) const {
  StartupAnimationSample sample;
  const uint8_t phase =
      static_cast<uint8_t>(elapsedMs / kColorFadePhaseDurationMs);
  const uint32_t phaseElapsed = elapsedMs % kColorFadePhaseDurationMs;
  const uint16_t intensity = fadeIntensityAt(phaseElapsed);
  if (phase >= kStartupColorCount || intensity == 0) {
    return sample;
  }

  const LedColor color = fadeColorForPhase(phase);
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    sample.frame[i] = color;
  }
  setUniformIntensity(sample, intensity);
  return sample;
}

StartupAnimationSample StartupAnimation::sampleGlowworm(
    uint32_t elapsedMs) const {
  StartupAnimationSample sample;
  const uint32_t stepsPerColor = kNoteCount;
  const uint32_t step = elapsedMs / config::kGlowwormStepMs;
  const uint8_t colorPhase = static_cast<uint8_t>(step / stepsPerColor);
  if (colorPhase >= kStartupColorCount) {
    return sample;
  }

  const uint8_t head = static_cast<uint8_t>(step % stepsPerColor);
  const LedColor color = fadeColorForPhase(colorPhase);
  for (uint8_t tail = 0; tail < config::kGlowwormTailLength; ++tail) {
    const uint8_t index = static_cast<uint8_t>(
        (head + kNoteCount - tail) % kNoteCount);
    sample.frame[index] = color;
    sample.intensityQ12[index] = config::kGlowwormTailIntensityQ12[tail];
  }
  return sample;
}

StartupAnimationSample StartupAnimation::sampleCog(uint32_t elapsedMs) const {
  StartupAnimationSample sample;
  const uint8_t offset = static_cast<uint8_t>(
      (elapsedMs / config::kCogStepMs) % kNoteCount);

  for (uint8_t i = 0; i < kNoteCount; ++i) {
    const uint8_t source = static_cast<uint8_t>(
        (i + kNoteCount - offset) % kNoteCount);
    sample.frame[i] = cogColorForIndex(source);
    sample.intensityQ12[i] = config::kLedPwmMaximum;
  }
  return sample;
}

StartupAnimationSample StartupAnimation::sampleSparkles(
    uint32_t elapsedMs) const {
  StartupAnimationSample sample;
  const uint32_t frameNumber = elapsedMs / config::kSparklesFrameMs;

  for (uint8_t i = 0; i < kNoteCount; ++i) {
    const uint32_t random = mixBits(
        config::kSparklesSeed + frameNumber * 0x9E3779B9u +
        static_cast<uint32_t>(i) * 0x85EBCA6Bu);
    const uint8_t gate = static_cast<uint8_t>(random & 0xFFu);
    if (gate >= config::kSparklesLitThreshold) {
      continue;
    }

    switch (static_cast<uint8_t>((random >> 8u) % 3u)) {
      case 0:
        sample.frame[i] = LedColor::Green;
        break;
      case 1:
        sample.frame[i] = LedColor::Red;
        break;
      default:
        sample.frame[i] = LedColor::Amber;
        break;
    }

    const uint16_t span = static_cast<uint16_t>(
        config::kLedPwmMaximum - config::kSparklesMinimumIntensityQ12);
    const uint16_t randomLevel =
        static_cast<uint16_t>((random >> 16u) & 0x0FFFu);
    sample.intensityQ12[i] = static_cast<uint16_t>(
        config::kSparklesMinimumIntensityQ12 +
        (static_cast<uint32_t>(randomLevel) * span) /
            config::kLedPwmMaximum);
  }
  return sample;
}

StartupAnimationSample StartupAnimation::sampleAt(uint32_t elapsedMs) const {
  if (isDone(elapsedMs)) {
    return StartupAnimationSample();
  }

  switch (sequence_) {
    case StartupSequence::ColorFade:
      return sampleColorFade(elapsedMs);
    case StartupSequence::Glowworm:
      return sampleGlowworm(elapsedMs);
    case StartupSequence::Cog:
      return sampleCog(elapsedMs);
    case StartupSequence::Sparkles:
      return sampleSparkles(elapsedMs);
  }
  return sampleColorFade(elapsedMs);
}

}  // namespace fmq
