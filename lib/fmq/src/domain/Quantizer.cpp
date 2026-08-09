/**
 * @file Quantizer.cpp
 * Implements pitch quantization, trigger modes, hysteresis and glide.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/domain/Quantizer.h"
#include "fmq/config/ProductConfig.h"

#include "fmq/domain/ScaleMath.h"


namespace fmq {

namespace {
/// Duration (in 1 kHz samples) that the output-trigger LED stays lit.
constexpr uint8_t kTriggerLedTimeSamples = config::kOutputTriggerLedSamples;
/// Duration (in samples) of the actual CV trigger pulse (subset of the LED).
constexpr uint8_t kTriggerCvTimeSamples = config::kOutputTriggerCvSamples;
/// Hysteresis half-width added on each side, in Q8.8 (0.4 semitone).
constexpr SemitoneQ8_8 kHysteresisAmountQ8_8 = config::kHysteresisQ8_8;  // round(0.4 * 256)

/// Saturating +1 for uint8_t counters.
uint8_t saturatingInc(uint8_t value) {
  constexpr uint8_t kMaximum = static_cast<uint8_t>(0xFFu);
  return value == kMaximum ? kMaximum : static_cast<uint8_t>(value + 1u);
}

/// Saturating -1 for uint8_t counters.
uint8_t saturatingDec(uint8_t value) {
  return value == 0u ? 0u : static_cast<uint8_t>(value - 1u);
}
}  // namespace

// ---------------------------------------------------------------------------
// ChannelConfig / result factories
// ---------------------------------------------------------------------------

ChannelConfig ChannelConfig::makeDefault() {
  ChannelConfig c;
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    c.notes[i] = (config::kFactoryScaleMask & (1u << i)) != 0;
  }
  c.sampleMode = config::kFactorySampleMode;
  c.glideAmount = 0;
  c.triggerDelayAmount = 0;
  c.preShift = 0;
  c.scaleShift = 0;
  c.postShift = 0;
  return c;
}

QuantizationResult QuantizationResult::makeZero() {
  ChannelOutput zero;
  zero.nominalSemitones = 0;
  zero.actualSemitones = 0;
  zero.outputTrigger = false;
  zero.outputTriggerUi = false;
  zero.inputTriggerUi = false;
  QuantizationResult r;
  r.channelA = zero;
  r.channelB = zero;
  return r;
}

// ---------------------------------------------------------------------------
// Hysteresis
// ---------------------------------------------------------------------------

bool Hysteresis::computeThresholds(const bool notes[kNoteCount],
                                   SemitoneQ8_8 &lowerQ8_8,
                                   SemitoneQ8_8 &upperQ8_8) const {
  // Hysteresis is meaningful only after a real note has been emitted and while
  // that note remains part of the active scale. A fresh channel must perform
  // ordinary nearest-note quantization; treating C as an implicit previous note
  // biases the very first 0.5-semitone tie downward.
  if (!hasLastOutput_ || !notes[lastOutput_ % kNoteCount]) {
    return false;
  }

  const int32_t nextUp =
      static_cast<int32_t>(getNextSelectedNote(notes, lastOutput_,
                                               ScaleDirection::Up)) *
      kSemitoneOneQ8_8;
  const int32_t nextDown =
      static_cast<int32_t>(getNextSelectedNote(notes, lastOutput_,
                                               ScaleDirection::Down)) *
      kSemitoneOneQ8_8;
  const int32_t decimal =
      static_cast<int32_t>(lastOutput_) * kSemitoneOneQ8_8;

  // Half the distance to the neighbouring notes, widened by a fixed amount so
  // the band overlaps slightly past the midpoint (the hysteresis proper).
  const int32_t deltaUp = (nextUp - decimal) / 2 + kHysteresisAmountQ8_8;
  const int32_t deltaDown = (decimal - nextDown) / 2 + kHysteresisAmountQ8_8;

  upperQ8_8 = static_cast<SemitoneQ8_8>(decimal + deltaUp);
  lowerQ8_8 = static_cast<SemitoneQ8_8>(decimal - deltaDown);
  return true;
}

int8_t Hysteresis::quantize(SemitoneQ8_8 inputQ8_8,
                            const bool notes[kNoteCount]) {
  // An empty scale has no notes to snap to; emit the lowest pitch.
  if (!scaleHasAnyNote(notes)) {
    hasLastOutput_ = false;
    lastOutput_ = 0;
    return 0;
  }

  // If the input is still inside the hold band of the previous note, keep it.
  SemitoneQ8_8 lower, upper;
  if (computeThresholds(notes, lower, upper)) {
    if (inputQ8_8 <= upper && inputQ8_8 >= lower) {
      return lastOutput_;
    }
  }

  // Otherwise search outward from the input for the nearest selected note,
  // preferring the rounding direction indicated by the fractional part.
  const int32_t input = inputQ8_8;
  const int8_t floorNote =
      static_cast<int8_t>(input >> kSemitoneFractionBits);
  const SemitoneQ8_8 fractionalPart =
      static_cast<SemitoneQ8_8>(input & kSemitoneFractionMask);
  const bool shouldRoundUp = fractionalPart >= kHalfSemitoneQ8_8;
  int8_t upperBound = static_cast<int8_t>(floorNote + 1);
  int8_t lowerBound = floorNote;

  for (;;) {
    int8_t bounds[2];
    if (shouldRoundUp) {
      bounds[0] = upperBound;
      bounds[1] = lowerBound;
    } else {
      bounds[0] = lowerBound;
      bounds[1] = upperBound;
    }

    for (uint8_t k = 0; k < 2; ++k) {
      const int8_t bound = bounds[k];
      if (bound >= 0 && bound <= kMaxSemitone &&
          notes[bound % kNoteCount]) {
        lastOutput_ = bound;
        hasLastOutput_ = true;
        return bound;
      }
    }

    upperBound = static_cast<int8_t>(
        clampInt<int16_t>(static_cast<int16_t>(upperBound + 1), 0,
                          kMaxSemitone));
    lowerBound = static_cast<int8_t>(
        clampInt<int16_t>(static_cast<int16_t>(lowerBound - 1), 0,
                          kMaxSemitone));
  }
}

// ---------------------------------------------------------------------------
// QuantizerChannel
// ---------------------------------------------------------------------------

QuantizerChannel::QuantizerChannel() { setConfig(ChannelConfig::makeDefault()); }

void QuantizerChannel::setConfig(const ChannelConfig &config) {
  config_ = config;
  hysteresis_.reset();
  hasLastOutput_ = false;
  lastNominalSemitones_ = 0;
  lastGlideTarget_ = 0;
  lastGlideCurrent_ = 0;
  lastTriggerInput_ = false;
  outputTriggerCountdown_ = 0;
  inputTriggerUiTimer_ = kTriggerLedTimeSamples;
  triggerDelayTimer_ = 0;
  triggerDelayPending_ = false;
  suppressNextOutputTrigger_ = false;
}

void QuantizerChannel::calculateQuantizationWithTransposition(
    SemitoneQ8_8 inputSemitones, int8_t &nominalOut,
    GlideQ8_24 &glideTargetOut) {
  // Pre-shift is a whole-semitone offset applied before quantization; clamp the
  // result into the valid pitch range.
  const int32_t maxQ8_8 =
      static_cast<int32_t>(kMaxSemitone) * kSemitoneOneQ8_8;
  int32_t preShifted = static_cast<int32_t>(inputSemitones) +
                       static_cast<int32_t>(config_.preShift) * kSemitoneOneQ8_8;
  preShifted = clampInt<int32_t>(preShifted, 0, maxQ8_8);

  const int8_t quantized =
      hysteresis_.quantize(static_cast<SemitoneQ8_8>(preShifted),
                           config_.notes);
  const int8_t scaleShifted =
      stepInScale(config_.notes, quantized, config_.scaleShift);
  const int8_t postShifted = static_cast<int8_t>(clampInt<int16_t>(
      static_cast<int16_t>(scaleShifted + config_.postShift), 0, kMaxSemitone));

  // The nominal (displayed) note is the scale-shifted note *before* post-shift;
  // the CV target incorporates the post-shift.
  nominalOut = scaleShifted;
  glideTargetOut =
      static_cast<GlideQ8_24>(postShifted) * kSemitoneOneQ8_24;
}

GlideQ8_24 QuantizerChannel::calculateGlide(GlideQ8_24 current,
                                            GlideQ8_24 target) const {
  if (current == target) {
    return current;
  }

  // alpha = 2^-glideAmount, represented in Q8.24. glideAmount == 0 gives
  // alpha == 1.0, i.e. an instant jump to the target.
  const uint8_t glide = config_.glideAmount > config::kMaxGlideAmount
          ? config::kMaxGlideAmount
          : config_.glideAmount;
  const GlideQ8_24 alpha = kSemitoneOneQ8_24 >> glide;

  // delta = alpha * (target - current), evaluated with a 64-bit intermediate to
  // avoid overflow, then scaled back from Q8.24.
  const int64_t diff = static_cast<int64_t>(target) - current;
  int32_t delta = static_cast<int32_t>((static_cast<int64_t>(alpha) * diff) >> kGlideFractionBits);

  // Guarantee progress: if the scaled step rounds to zero, move one ulp toward
  // the target so the glide always eventually completes.
  if (delta == 0) {
    delta = (target > current) ? 1 : -1;
  }

  int32_t result = current + delta;

  // Never overshoot the target. The original firmware asserted this invariant
  // at runtime (which would hang the device on violation); clamping is safe and
  // behaviourally identical for the valid inputs used here.
  if (current < target) {
    if (result > target) {
      result = target;
    }
  } else {
    if (result < target) {
      result = target;
    }
  }
  return result;
}

ChannelOutput QuantizerChannel::step(SemitoneQ8_8 inputSemitones,
                                     bool sampleTrigger,
                                     bool forceContinuous) {
  // Track-and-Hold treats a HIGH gate as continuous trigger activity.
  // Sample-and-Hold treats only the LOW->HIGH edge as trigger activity.
  // Keep this UI activity timer separate from the trigger-delay timer: the
  // original hardware normalises an unpatched trigger jack HIGH, so the input
  // LED must remain lit in Track-and-Hold while the jack is unpatched.
  const bool risingEdge = !lastTriggerInput_ && sampleTrigger;
  bool shouldUpdate = false;

  if (forceContinuous) {
    // In Arpeggiator CLOCK mode the Sample/Gate jack is repurposed as a clock
    // input. Keep quantization continuously responsive without rewriting the
    // user's stored Track/Sample mode. The real gate level is still remembered
    // so returning to the Quantizer layer cannot fabricate an edge.
    inputTriggerUiTimer_ = saturatingInc(inputTriggerUiTimer_);
    triggerDelayTimer_ = 0;
    triggerDelayPending_ = false;
    shouldUpdate = true;
  } else {
    const bool inputTriggerActive =
        (config_.sampleMode == SampleMode::TrackAndHold) ? sampleTrigger
                                                         : risingEdge;
    if (inputTriggerActive) {
      inputTriggerUiTimer_ = 0;
    } else {
      inputTriggerUiTimer_ = saturatingInc(inputTriggerUiTimer_);
    }

    const bool firstSample = !hasLastOutput_;
    if (firstSample || risingEdge) {
      triggerDelayTimer_ = 0;
      triggerDelayPending_ = true;
    } else if (triggerDelayPending_) {
      triggerDelayTimer_ = saturatingInc(triggerDelayTimer_);
    }

    const uint8_t delay =
        config_.triggerDelayAmount > config::kMaxTriggerDelayAmount
            ? config::kMaxTriggerDelayAmount
            : config_.triggerDelayAmount;

    shouldUpdate = (config_.sampleMode == SampleMode::Continuous);
    if (triggerDelayPending_ && triggerDelayTimer_ >= delay) {
      shouldUpdate = true;
      triggerDelayPending_ = false;
    } else if (config_.sampleMode == SampleMode::TrackAndHold && sampleTrigger &&
               !triggerDelayPending_) {
      shouldUpdate = true;
    }
  }
  lastTriggerInput_ = sampleTrigger;

  int8_t nominalSemitones;
  GlideQ8_24 glideTarget;
  if (shouldUpdate) {
    calculateQuantizationWithTransposition(inputSemitones, nominalSemitones,
                                           glideTarget);
  } else {
    nominalSemitones = lastNominalSemitones_;
    glideTarget = lastGlideTarget_;
  }

  const bool didChange = hasLastOutput_ &&
      (lastNominalSemitones_ != nominalSemitones || lastGlideTarget_ != glideTarget);

  const GlideQ8_24 previousCurrent = hasLastOutput_ ? lastGlideCurrent_ : 0;
  const GlideQ8_24 actualOutput = calculateGlide(previousCurrent, glideTarget);

  // Commit the new live state.
  hasLastOutput_ = true;
  lastNominalSemitones_ = nominalSemitones;
  lastGlideTarget_ = glideTarget;
  lastGlideCurrent_ = actualOutput;

  // Manage the output-trigger countdown that drives the trigger jack and LED.
  if (didChange && !suppressNextOutputTrigger_) {
    outputTriggerCountdown_ = kTriggerLedTimeSamples;
  } else {
    outputTriggerCountdown_ = saturatingDec(outputTriggerCountdown_);
  }
  // Suppression is deliberately one-shot. It is armed after a UI edit and
  // consumed on the following quantizer step, regardless of whether that edit
  // actually changes the current pitch.
  suppressNextOutputTrigger_ = false;

  ChannelOutput out;
  out.nominalSemitones = nominalSemitones;
  // Convert the Q8.24 glide value down to the Q8.8 pitch used by the DAC path.
  out.actualSemitones =
      static_cast<SemitoneQ8_8>(actualOutput >> kGlideToSemitoneShift);
  out.outputTrigger =
      outputTriggerCountdown_ > (kTriggerLedTimeSamples - kTriggerCvTimeSamples);
  out.outputTriggerUi = outputTriggerCountdown_ != 0;
  out.inputTriggerUi = inputTriggerUiTimer_ < kTriggerLedTimeSamples;
  return out;
}

// ---------------------------------------------------------------------------
// QuantizerState
// ---------------------------------------------------------------------------

QuantizerState::QuantizerState()
    : channelsLinked(config::kFactoryChannelsLinked),
      channelBMode(config::kFactoryChannelBAbsolute ? PitchMode::Absolute
                                                   : PitchMode::Relative) {}

QuantizationResult QuantizerState::step(SemitoneQ8_8 inputSemitonesA,
                                        SemitoneQ8_8 inputSemitonesB,
                                        bool triggerA, bool triggerB,
                                        bool forceContinuousA,
                                        bool forceContinuousB) {
  // In relative mode channel B is offset by channel A's raw input, clamped to
  // the module's pitch ceiling. In absolute mode it stands alone.
  SemitoneQ8_8 inputB;
  if (channelBMode == PitchMode::Relative) {
    const int32_t maxQ8_8 =
        static_cast<int32_t>(kMaxSemitone) * kSemitoneOneQ8_8;
    int32_t sum = static_cast<int32_t>(inputSemitonesA) + inputSemitonesB;
    inputB = static_cast<SemitoneQ8_8>(clampInt<int32_t>(sum, 0, maxQ8_8));
  } else {
    inputB = inputSemitonesB;
  }

  QuantizationResult result;
  result.channelA = channels[kChannelAIndex].step(
      inputSemitonesA, triggerA, forceContinuousA);
  result.channelB = channels[kChannelBIndex].step(
      inputB, triggerB, forceContinuousB);
  return result;
}

}  // namespace fmq
