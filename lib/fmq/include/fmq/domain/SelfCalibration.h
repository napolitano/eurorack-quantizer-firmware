/**
 * @file SelfCalibration.h
 * Pure integer solver for loopback DAC affine self-calibration.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_DOMAIN_SELF_CALIBRATION_H
#define FMQ_DOMAIN_SELF_CALIBRATION_H

#include <stdint.h>

#include "fmq/domain/CalibrationProfile.h"

namespace fmq {

/** One raw-DAC / calibrated-ADC observation from a physical loopback. */
struct SelfCalibrationSample {
  uint16_t dacCode;
  uint16_t adcCode;
};

/** Solver outcome before hardware verification/persistence. */
enum class SelfCalibrationStatus : uint8_t {
  Ok = 0,
  TooFewSamples,
  NonMonotonic,
  SpanTooSmall,
  ImplausibleGain,
  OffsetOutOfRange,
};

/** Result of fitting the inverse output-stage affine correction. */
struct SelfCalibrationResult {
  SelfCalibrationStatus status = SelfCalibrationStatus::TooFewSamples;
  LinearCalibration calibration;
  uint16_t maxFitResidualDacCodes = 0u;
};

/** Convert a calibrated 10-bit ADC code to the equivalent nominal DAC code. */
uint16_t adcCodeToEquivalentDac(uint16_t adcCode);

/**
 * Fit a DAC correction from monotonically increasing raw-DAC loopback samples.
 *
 * The calibrated ADC is the reference. The solver therefore corrects the DAC
 * output chain only; it cannot independently discover ADC and DAC errors from
 * the same loop.
 */
SelfCalibrationResult solveDacSelfCalibration(const SelfCalibrationSample *samples,
                                               uint8_t count);

}  // namespace fmq

#endif
