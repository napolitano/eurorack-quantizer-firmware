/**
 * @file StartupAnimation.cpp
 * Implements the compact fixed note-ring startup fade.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/StartupAnimation.h"

namespace fmq {
namespace {

constexpr uint16_t kFull = 4095u;

uint16_t rampUp(uint32_t elapsed, uint16_t duration) {
  return static_cast<uint16_t>((elapsed * kFull) / duration);
}

uint16_t rampDown(uint32_t elapsed, uint16_t duration) {
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(duration) - elapsed) * kFull / duration);
}

}  // namespace

StartupAnimationSample StartupAnimation::sampleAt(uint32_t elapsedMs) const {
  StartupAnimationSample sample;
  if (isDone(elapsedMs)) return sample;

  if (elapsedMs < kGreenFadeInMs) {
    sample.greenQ12 = rampUp(elapsedMs, kGreenFadeInMs);
    return sample;
  }
  elapsedMs -= kGreenFadeInMs;

  if (elapsedMs < kGreenToAmberMs) {
    sample.greenQ12 = kFull;
    sample.redQ12 = rampUp(elapsedMs, kGreenToAmberMs);
    return sample;
  }
  elapsedMs -= kGreenToAmberMs;

  if (elapsedMs < kAmberToRedMs) {
    sample.redQ12 = kFull;
    sample.greenQ12 = rampDown(elapsedMs, kAmberToRedMs);
    return sample;
  }
  elapsedMs -= kAmberToRedMs;

  sample.redQ12 = rampDown(elapsedMs, kRedFadeOutMs);
  return sample;
}

}  // namespace fmq
