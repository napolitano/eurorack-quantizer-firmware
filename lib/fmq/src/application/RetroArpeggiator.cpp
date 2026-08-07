/**
 * @file RetroArpeggiator.cpp
 * Implements the scale-aware Retro Arpeggiator performance feature.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/RetroArpeggiator.h"

#include "fmq/config/ProductConfig.h"

namespace fmq {
namespace {
constexpr uint8_t kArpScaleDegreeOffsets[config::kRetroArpDegreeCount] = {0, 2, 4};
}

uint8_t RetroArpeggiator::semitoneOffsetForScaleDegree(
    const bool notes[kNoteCount], uint8_t rootPitchClass,
    uint8_t scaleDegreeOffset) {
  if (scaleDegreeOffset == 0u) {
    return 0u;
  }

  uint8_t enabledNotesPassed = 0u;
  for (uint8_t semitoneOffset = 1u; semitoneOffset <= 24u; ++semitoneOffset) {
    const uint8_t pitchClass = static_cast<uint8_t>(
        (rootPitchClass + semitoneOffset) % kNoteCount);
    if (!notes[pitchClass]) {
      continue;
    }
    ++enabledNotesPassed;
    if (enabledNotesPassed == scaleDegreeOffset) {
      return semitoneOffset;
    }
  }
  return 0u;
}

SemitoneQ8_8 RetroArpeggiator::process(
    SemitoneQ8_8 basePitch, int8_t nominalSemitones,
    const bool notes[kNoteCount], uint32_t nowMs) const {
  if (!enabled_) {
    return basePitch;
  }

  const uint32_t elapsedMs = nowMs - enabledAtMs_;
  const uint8_t arpStep = static_cast<uint8_t>(
      (elapsedMs / config::kRetroArpStepMs) % config::kRetroArpDegreeCount);
  const uint8_t rootPitchClass = static_cast<uint8_t>(
      static_cast<uint8_t>(nominalSemitones) % kNoteCount);
  const uint8_t semitoneOffset = semitoneOffsetForScaleDegree(
      notes, rootPitchClass, kArpScaleDegreeOffsets[arpStep]);

  const int32_t shifted = static_cast<int32_t>(basePitch) +
      static_cast<int32_t>(semitoneOffset) * kSemitoneOneQ8_8;
  const int32_t maximum =
      static_cast<int32_t>(kMaxSemitone) * kSemitoneOneQ8_8;
  return static_cast<SemitoneQ8_8>(shifted > maximum ? maximum : shifted);
}

}  // namespace fmq
