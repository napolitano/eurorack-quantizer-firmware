/**
 * @file Arpeggiator.cpp
 * Implements the scale-aware Arpeggiator engine.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/Arpeggiator.h"

#include "fmq/config/ProductConfig.h"

namespace fmq {
namespace {
constexpr uint16_t kFreeRatesMs[config::kArpRateCount] = {
    12u, 16u, 20u, 24u, 32u, 48u, 64u, 96u, 125u, 167u, 250u, 500u};

// Index 0..11 maps to /8, /6, /4, /3, /2, x1, x2, x3, x4, x6, x8, x12.
constexpr uint8_t kClockDividers[config::kArpRateCount] = {
    8u, 6u, 4u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
constexpr uint8_t kClockMultipliers[config::kArpRateCount] = {
    1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 4u, 6u, 8u, 12u};

constexpr uint8_t kShapeTriad[] = {0u, 2u, 4u};
constexpr uint8_t kShapeOneTwoFive[] = {0u, 1u, 4u};
constexpr uint8_t kShapeOneFourFive[] = {0u, 3u, 4u};
constexpr uint8_t kShapeOneThreeSix[] = {0u, 2u, 5u};
constexpr uint8_t kShapeOneFiveEight[] = {0u, 4u, 7u};
constexpr uint8_t kShapeAdjacent[] = {0u, 1u, 2u, 3u};
constexpr uint8_t kShapeSeventh[] = {0u, 2u, 4u, 6u};
constexpr uint16_t kRandomSeed = 0x514Eu;

uint8_t clampU8(uint8_t value, uint8_t maximum) {
  return value > maximum ? maximum : value;
}

uint8_t wrapPosition(uint8_t value, uint8_t length) {
  if (length == 0u) return static_cast<uint8_t>(0u);
  return static_cast<uint8_t>(value % length);
}

}  // namespace

ArpeggiatorConfig ArpeggiatorConfig::makeDefault() {
  ArpeggiatorConfig config{};
  config.enabled = false;
  config.rateIndex = config::kArpDefaultRateIndex;
  config.pattern = ArpeggiatorPattern::Up;
  config.shape = ArpeggiatorShape::Triad135;
  config.length = 3u;
  config.range = 1u;
  config.stepTrigger = false;
  config.syncMode = ArpeggiatorSyncMode::Free;
  config.swing = 0u;
  return config;
}

Arpeggiator::Arpeggiator()
    : config_(ArpeggiatorConfig::makeDefault()),
      stepCounter_(0u),
      lastStepMs_(0u),
      lastClockEdgeUs_(0u),
      clockPeriodUs_(0u),
      lastClockStepUs_(0u),
      dividerEdgeCount_(0u),
      generatedSubsteps_(0u),
      clockSeen_(false),
      randomState_(kRandomSeed) {}

ArpeggiatorConfig Arpeggiator::sanitize(const ArpeggiatorConfig &input) {
  ArpeggiatorConfig out = input;
  out.rateIndex = clampU8(out.rateIndex, config::kArpRateCount - 1u);
  if (static_cast<uint8_t>(out.pattern) >= patternCount()) {
    out.pattern = ArpeggiatorPattern::Up;
  }
  if (static_cast<uint8_t>(out.shape) >= shapeCount()) {
    out.shape = ArpeggiatorShape::Triad135;
  }
  if (out.length < 1u) out.length = 1u;
  out.length = clampU8(out.length, config::kArpMaximumLength);
  if (out.range < 1u) out.range = 1u;
  out.range = clampU8(out.range, config::kArpMaximumRange);
  if (static_cast<uint8_t>(out.syncMode) >= syncModeCount()) {
    out.syncMode = ArpeggiatorSyncMode::Free;
  }
  out.swing = clampU8(out.swing, config::kArpMaximumSwingStep);
  return out;
}

void Arpeggiator::setConfig(const ArpeggiatorConfig &newConfig,
                           uint32_t nowMs) {
  const ArpeggiatorConfig sanitized = sanitize(newConfig);
  const bool timingChanged =
      sanitized.rateIndex != config_.rateIndex ||
      sanitized.syncMode != config_.syncMode ||
      sanitized.swing != config_.swing;
  const bool enableChanged = sanitized.enabled != config_.enabled;
  config_ = sanitized;
  if (timingChanged || enableChanged) {
    resetPhase(nowMs);
  }
}

void Arpeggiator::setEnabled(bool enabled, uint32_t nowMs) {
  if (config_.enabled == enabled) return;
  config_.enabled = enabled;
  resetPhase(nowMs);
}

void Arpeggiator::resetPhase(uint32_t nowMs) {
  stepCounter_ = 0u;
  lastStepMs_ = nowMs;
  const uint32_t nowUs = nowMs * 1000u;
  lastClockEdgeUs_ = nowUs;
  clockPeriodUs_ = 0u;
  lastClockStepUs_ = nowUs;
  dividerEdgeCount_ = 0u;
  generatedSubsteps_ = 0u;
  clockSeen_ = false;
  randomState_ = kRandomSeed;
}

uint16_t Arpeggiator::freeRateMs(uint8_t rateIndex) {
  const uint8_t index = clampU8(rateIndex, config::kArpRateCount - 1u);
  return kFreeRatesMs[index];
}

uint8_t Arpeggiator::clockMultiplier(uint8_t rateIndex) {
  const uint8_t index = clampU8(rateIndex, config::kArpRateCount - 1u);
  return kClockMultipliers[index];
}

uint8_t Arpeggiator::clockDivider(uint8_t rateIndex) {
  const uint8_t index = clampU8(rateIndex, config::kArpRateCount - 1u);
  return kClockDividers[index];
}

uint8_t Arpeggiator::countEnabledNotes(const bool notes[kNoteCount]) {
  uint8_t count = 0u;
  for (uint8_t i = 0u; i < kNoteCount; ++i) {
    if (notes[i]) ++count;
  }
  return count;
}

uint8_t Arpeggiator::semitoneOffsetForScaleDegree(
    const bool notes[kNoteCount], uint8_t rootPitchClass,
    uint8_t scaleDegreeOffset) {
  if (scaleDegreeOffset == 0u) return 0u;

  uint8_t enabledNotesPassed = 0u;
  for (uint8_t semitoneOffset = 1u; semitoneOffset <= kMaxSemitone;
       ++semitoneOffset) {
    const uint8_t pitchClass = static_cast<uint8_t>(
        (rootPitchClass + semitoneOffset) % kNoteCount);
    if (!notes[pitchClass]) continue;
    ++enabledNotesPassed;
    if (enabledNotesPassed == scaleDegreeOffset) return semitoneOffset;
  }
  return 0u;
}

uint8_t Arpeggiator::patternPosition(ArpeggiatorPattern pattern,
                                    uint8_t stepCounter, uint8_t length,
                                    uint16_t randomState) {
  if (length <= 1u) return 0u;
  const uint8_t s = wrapPosition(stepCounter, length);
  switch (pattern) {
    case ArpeggiatorPattern::Up:
      return s;
    case ArpeggiatorPattern::Down:
      return static_cast<uint8_t>(length - 1u - s);
    case ArpeggiatorPattern::UpDown: {
      const uint8_t period = static_cast<uint8_t>(2u * length - 2u);
      const uint8_t p = static_cast<uint8_t>(stepCounter % period);
      return p < length ? p : static_cast<uint8_t>(period - p);
    }
    case ArpeggiatorPattern::DownUp: {
      const uint8_t period = static_cast<uint8_t>(2u * length - 2u);
      const uint8_t p = static_cast<uint8_t>(stepCounter % period);
      const uint8_t upDown = p < length ? p : static_cast<uint8_t>(period - p);
      return static_cast<uint8_t>(length - 1u - upDown);
    }
    case ArpeggiatorPattern::Rotate:
      return static_cast<uint8_t>((static_cast<uint16_t>(s) * 2u) % length);
    case ArpeggiatorPattern::OutsideIn: {
      const uint8_t half = static_cast<uint8_t>((s + 1u) / 2u);
      return (s & 1u) == 0u ? static_cast<uint8_t>(s / 2u)
                            : static_cast<uint8_t>(length - half);
    }
    case ArpeggiatorPattern::InsideOut: {
      const uint8_t middle = static_cast<uint8_t>((length - 1u) / 2u);
      const uint8_t distance = static_cast<uint8_t>((s + 1u) / 2u);
      int16_t candidate = static_cast<int16_t>(middle);
      if ((s & 1u) == 0u) {
        candidate += static_cast<int16_t>(s / 2u);
      } else {
        candidate -= static_cast<int16_t>(distance);
      }
      while (candidate < 0) candidate += length;
      return static_cast<uint8_t>(candidate % length);
    }
    case ArpeggiatorPattern::Random:
      // Random state advances only when the Arpeggiator advances a musical
      // step. Re-rendering the same control-loop step must be pitch-stable.
      return static_cast<uint8_t>(randomState % length);
    case ArpeggiatorPattern::Count:
      break;
  }
  return s;
}

uint16_t Arpeggiator::nextRandomState(uint16_t state) {
  // xorshift16: tiny deterministic PRNG, sufficient for musical ordering.
  state ^= static_cast<uint16_t>(state << 7u);
  state ^= static_cast<uint16_t>(state >> 9u);
  state ^= static_cast<uint16_t>(state << 8u);
  return state;
}

uint8_t Arpeggiator::shapeDegree(ArpeggiatorShape shape,
                                uint8_t position,
                                uint8_t activeScaleNotes, uint8_t range) {
  if (activeScaleNotes == 0u) return 0u;

  const uint8_t *values = kShapeTriad;
  uint8_t baseLength = 3u;
  switch (shape) {
    case ArpeggiatorShape::Triad135:
      break;
    case ArpeggiatorShape::OneTwoFive:
      values = kShapeOneTwoFive;
      break;
    case ArpeggiatorShape::OneFourFive:
      values = kShapeOneFourFive;
      break;
    case ArpeggiatorShape::OneThreeSix:
      values = kShapeOneThreeSix;
      break;
    case ArpeggiatorShape::OneFiveEight:
      values = kShapeOneFiveEight;
      break;
    case ArpeggiatorShape::Adjacent1234:
      values = kShapeAdjacent;
      baseLength = 4u;
      break;
    case ArpeggiatorShape::Seventh1357:
      values = kShapeSeventh;
      baseLength = 4u;
      break;
    case ArpeggiatorShape::StackedThirds: {
      const uint16_t degree = static_cast<uint16_t>(position) * 2u;
      const uint16_t maximumDegree = static_cast<uint16_t>(activeScaleNotes) * range;
      return static_cast<uint8_t>(degree >= maximumDegree
                                      ? (maximumDegree == 0u ? 0u : maximumDegree - 1u)
                                      : degree);
    }
    case ArpeggiatorShape::Count:
      break;
  }

  const uint8_t cycle = static_cast<uint8_t>(position / baseLength);
  const uint8_t octave = static_cast<uint8_t>(cycle % range);
  const uint16_t baseDegree =
      static_cast<uint16_t>(values[position % baseLength]);
  const uint16_t octaveDegree = static_cast<uint16_t>(
      static_cast<uint16_t>(octave) * static_cast<uint16_t>(activeScaleNotes));
  const uint16_t degree = static_cast<uint16_t>(baseDegree + octaveDegree);
  const uint16_t maximumDegree = static_cast<uint16_t>(activeScaleNotes) * range;
  return static_cast<uint8_t>(degree >= maximumDegree
                                  ? (maximumDegree == 0u ? 0u : maximumDegree - 1u)
                                  : degree);
}

uint16_t Arpeggiator::stepDurationMs() const {
  const uint16_t base = freeRateMs(config_.rateIndex);
  if (config_.swing == 0u) return base;

  // swing 0..11 maps linearly from 50:50 to 66:34. The pair duration remains
  // 2*base; odd/even steps alternate long and short durations.
  const uint16_t longPercent = static_cast<uint16_t>(50u +
      (static_cast<uint16_t>(config_.swing) * 16u + 5u) / 11u);
  const uint16_t pair = static_cast<uint16_t>(2u * base);
  const uint16_t longDuration = static_cast<uint16_t>((pair * longPercent) / 100u);
  const uint16_t shortDuration = static_cast<uint16_t>(pair - longDuration);
  return (stepCounter_ & 1u) == 0u ? shortDuration : longDuration;
}

void Arpeggiator::advanceStep() {
  ++stepCounter_;
  randomState_ = nextRandomState(randomState_);
}

bool Arpeggiator::updateFreeOrReset(bool syncEdge, uint32_t nowMs) {
  if (config_.syncMode == ArpeggiatorSyncMode::Reset && syncEdge) {
    stepCounter_ = 0u;
    randomState_ = kRandomSeed;
    lastStepMs_ = nowMs;
    return true;
  }

  const uint16_t duration = stepDurationMs();
  if (nowMs - lastStepMs_ < duration) return false;
  advanceStep();
  lastStepMs_ = nowMs;  // skip backlog instead of replaying stale steps.
  return true;
}

bool Arpeggiator::updateClock(uint8_t syncEdgeCount,
                               uint32_t latestSyncEdgeUs,
                               uint32_t nowUs) {
  const uint8_t divider = clockDivider(config_.rateIndex);
  const uint8_t multiplier = clockMultiplier(config_.rateIndex);

  if (syncEdgeCount != 0u) {
    bool advanced = false;
    uint8_t edgesToApply = syncEdgeCount;

    if (!clockSeen_) {
      // The first edge establishes phase but no period. Preserve the existing
      // behaviour: it reports a step at the root without incrementing the
      // pattern counter. Any additional edges captured in the same control tick
      // are still counted below instead of being collapsed.
      clockSeen_ = true;
      lastClockEdgeUs_ = latestSyncEdgeUs;
      lastClockStepUs_ = latestSyncEdgeUs;
      dividerEdgeCount_ = 1u;
      generatedSubsteps_ = 0u;
      stepCounter_ = 0u;
      randomState_ = kRandomSeed;
      advanced = true;
      --edgesToApply;
    } else {
      const uint32_t elapsed = latestSyncEdgeUs - lastClockEdgeUs_;
      if (elapsed > 0u) {
        // If more than one physical edge arrived within a 1-ms control tick,
        // use their count to estimate the mean external period. This preserves
        // divider/multiplier phase while avoiding a fictitious 1-ms period.
        uint32_t measured = elapsed / syncEdgeCount;
        if (measured == 0u) measured = 1u;
        clockPeriodUs_ = clockPeriodUs_ == 0u
                             ? measured
                             : static_cast<uint32_t>(
                                   (3u * clockPeriodUs_ + measured) / 4u);
      }
      lastClockEdgeUs_ = latestSyncEdgeUs;
      lastClockStepUs_ = latestSyncEdgeUs;
      generatedSubsteps_ = 0u;
    }

    for (uint8_t edge = 0u; edge < edgesToApply; ++edge) {
      if (divider > 1u) {
        if (dividerEdgeCount_ < divider) {
          ++dividerEdgeCount_;
          continue;
        }
        dividerEdgeCount_ = 1u;
      }
      advanceStep();
      advanced = true;
    }
    return advanced;
  }

  if (!clockSeen_ || clockPeriodUs_ == 0u || multiplier <= 1u) return false;
  if (generatedSubsteps_ >= static_cast<uint8_t>(multiplier - 1u)) return false;

  uint32_t base = clockPeriodUs_ / multiplier;
  if (base == 0u) base = 1u;

  uint32_t duration = base;
  if (config_.swing != 0u) {
    const uint32_t longPercent = static_cast<uint32_t>(50u +
        (static_cast<uint16_t>(config_.swing) * 16u + 5u) / 11u);
    const uint32_t pair = 2u * base;
    const uint32_t longDuration = (pair * longPercent) / 100u;
    const uint32_t shortDuration = pair - longDuration;
    duration = (stepCounter_ & 1u) == 0u ? shortDuration : longDuration;
    if (duration == 0u) duration = 1u;
  }

  if (nowUs - lastClockStepUs_ < duration) return false;
  advanceStep();
  ++generatedSubsteps_;
  // Advance from the ideal scheduled instant rather than from the 1-kHz loop
  // observation time. The DAC still updates on control ticks, but phase error
  // no longer accumulates from one multiplied step to the next.
  lastClockStepUs_ += duration;
  return true;
}

ArpeggiatorOutput Arpeggiator::process(
    SemitoneQ8_8 basePitch, int8_t nominalSemitones,
    const bool notes[kNoteCount], bool syncEdge, uint32_t nowMs) {
  const uint32_t nowUs = nowMs * 1000u;
  return processTimed(basePitch, nominalSemitones, notes, syncEdge ? 1u : 0u,
                      nowUs, nowMs, nowUs);
}

ArpeggiatorOutput Arpeggiator::processTimed(
    SemitoneQ8_8 basePitch, int8_t nominalSemitones,
    const bool notes[kNoteCount], uint8_t syncEdgeCount,
    uint32_t latestSyncEdgeUs, uint32_t nowMs, uint32_t nowUs) {
  if (!config_.enabled) return {basePitch, false};

  const bool advanced = config_.syncMode == ArpeggiatorSyncMode::Clock
                            ? updateClock(syncEdgeCount, latestSyncEdgeUs, nowUs)
                            : updateFreeOrReset(syncEdgeCount != 0u, nowMs);

  const uint8_t activeNotes = countEnabledNotes(notes);
  if (activeNotes == 0u) return {basePitch, advanced};

  uint8_t effectiveLength = config_.length;
  if (effectiveLength < 1u) effectiveLength = 1u;
  if (effectiveLength > config::kArpMaximumLength) {
    effectiveLength = config::kArpMaximumLength;
  }
  const uint8_t position = patternPosition(config_.pattern, stepCounter_,
                                           effectiveLength, randomState_);
  const uint8_t degree = shapeDegree(config_.shape, position, activeNotes,
                                     config_.range);

  int16_t nominal = nominalSemitones;
  if (nominal < 0) nominal = 0;
  if (nominal > kMaxSemitone) nominal = kMaxSemitone;
  const uint8_t rootPitchClass = static_cast<uint8_t>(nominal % kNoteCount);
  const uint8_t semitoneOffset =
      semitoneOffsetForScaleDegree(notes, rootPitchClass, degree);

  const int32_t shifted = static_cast<int32_t>(basePitch) +
      static_cast<int32_t>(semitoneOffset) * kSemitoneOneQ8_8;
  const int32_t maximum = static_cast<int32_t>(kMaxSemitone) * kSemitoneOneQ8_8;
  return {static_cast<SemitoneQ8_8>(shifted > maximum ? maximum : shifted),
          advanced};
}

}  // namespace fmq
