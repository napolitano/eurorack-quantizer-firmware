/**
 * @file CalibrationMeasurement.h
 * Helpers for the panel-driven maintenance output measurement mode.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_DOMAIN_CALIBRATION_MEASUREMENT_H
#define FMQ_DOMAIN_CALIBRATION_MEASUREMENT_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"

namespace fmq {

constexpr uint8_t kMeasurementMaximumOctave = 10u;
constexpr uint8_t kMeasurementPitchClassCount = 12u;

/** Return whether one octave/pitch-class selection is inside 0..120 semitones. */
constexpr bool measurementSelectionValid(uint8_t octave, uint8_t pitchClass) {
  return octave < kMeasurementMaximumOctave
             ? pitchClass < kMeasurementPitchClassCount
             : octave == kMeasurementMaximumOctave && pitchClass == 0u;
}

/** Convert one valid maintenance output selection to an integer semitone. */
constexpr uint8_t measurementSemitone(uint8_t octave, uint8_t pitchClass) {
  return measurementSelectionValid(octave, pitchClass)
             ? static_cast<uint8_t>(octave * 12u + pitchClass)
             : 120u;
}

/** Convert one valid selection to the normal Q8.8 pitch representation. */
constexpr SemitoneQ8_8 measurementPitchQ8_8(uint8_t octave,
                                             uint8_t pitchClass) {
  return static_cast<SemitoneQ8_8>(measurementSemitone(octave, pitchClass) *
                                   kSemitoneOneQ8_8);
}

/**
 * Nominal output voltage in microvolts for one semitone index.
 *
 * One semitone is exactly 1/12 V in the 1 V/octave convention. The return
 * value is rounded to the nearest microvolt and is intended for documentation
 * and host-side verification only.
 */
constexpr uint32_t nominalMeasurementMicrovolts(uint8_t semitone) {
  const uint8_t bounded = semitone > 120u ? 120u : semitone;
  return (static_cast<uint32_t>(bounded) * 1000000u + 6u) / 12u;
}

}  // namespace fmq

#endif
