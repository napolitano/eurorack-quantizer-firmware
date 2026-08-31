/**
 * @file AdcCalibration.h
 * Integer multi-point ADC calibration for the maintenance firmware.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_DOMAIN_ADC_CALIBRATION_H
#define FMQ_DOMAIN_ADC_CALIBRATION_H

#include <stdint.h>

#include "fmq/domain/CalibrationProfile.h"

namespace fmq {

/** Number of voltage points used to fit the affine ADC correction. */
constexpr uint8_t kAdcCalibrationFitPointCount = 5u;

/** Number of independent voltage points used by the maintenance verifier. */
constexpr uint8_t kAdcCalibrationVerificationPointCount = 5u;

/** Fit voltages in volts: C#, D#, F, G, A on the front-panel note ring. */
constexpr uint8_t kAdcCalibrationFitVolts[kAdcCalibrationFitPointCount] = {
    1u, 3u, 5u, 7u, 9u};

/** Verification voltages in volts: C, D, E, F#, G# on the note ring. */
constexpr uint8_t
    kAdcCalibrationVerificationVolts[kAdcCalibrationVerificationPointCount] = {
        0u, 2u, 4u, 6u, 8u};

/** Status returned by the multi-point ADC calibration solver. */
enum class AdcCalibrationStatus : uint8_t {
  Ok = 0,
  NonIncreasing,
  SpanTooSmall,
  GainOutOfRange,
  OffsetOutOfRange,
};

/** Result of the five-point affine least-squares fit. */
struct AdcCalibrationResult {
  AdcCalibrationStatus status = AdcCalibrationStatus::NonIncreasing;
  LinearCalibration calibration;
  uint16_t correctedFitPoints[kAdcCalibrationFitPointCount] = {};
  uint16_t maxFitResidualCodes = 0u;
};

/** Return the nominal ADC code corresponding to an exact 0..10 V input. */
uint16_t nominalAdcCodeForVolts(uint8_t volts);

/** Nominal calibrated ADC code corresponding to exactly 1.000 V. */
uint16_t oneVoltNominalAdcCode();

/** Nominal calibrated ADC code corresponding to exactly 5.000 V. */
uint16_t fiveVoltNominalAdcCode();

/** Nominal calibrated ADC code corresponding to exactly 9.000 V. */
uint16_t nineVoltNominalAdcCode();

/**
 * Fit one affine ADC correction from five measured raw codes.
 *
 * The points must correspond, in order, to 1, 3, 5, 7 and 9 V. A single
 * least-squares line is fitted; no piecewise or lookup-table correction is
 * introduced, so the persisted calibration format remains unchanged.
 *
 * @param rawFitPoints Mean raw ADC codes measured at 1/3/5/7/9 V.
 * @return Fitted calibration and residual diagnostics.
 */
AdcCalibrationResult solveAdcMultiPointCalibration(
    const uint16_t rawFitPoints[kAdcCalibrationFitPointCount]);

}  // namespace fmq

#endif
