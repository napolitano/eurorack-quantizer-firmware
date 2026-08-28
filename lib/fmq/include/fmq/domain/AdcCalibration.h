/**
 * @file AdcCalibration.h
 * Pure integer two-point ADC calibration for the maintenance firmware.
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

/** Status returned by the two-point ADC calibration solver. */
enum class AdcCalibrationStatus : uint8_t {
  Ok = 0,
  NonIncreasing,
  SpanTooSmall,
  OffsetOutOfRange,
  EndpointVerificationFailed,
};

/** Result of fitting raw ADC readings at 0.000 V and 9.000 V. */
struct AdcCalibrationResult {
  AdcCalibrationStatus status = AdcCalibrationStatus::NonIncreasing;
  LinearCalibration calibration;
  uint16_t correctedZero = 0u;
  uint16_t correctedNineVolt = 0u;
};

/** Nominal calibrated ADC code corresponding to exactly 9.000 V of 10 V FS. */
uint16_t nineVoltNominalAdcCode();

/**
 * Fit an affine ADC correction from two measured raw codes.
 *
 * @param zeroRaw Mean raw ADC code while CV IN is shorted to local ground.
 * @param nineVoltRaw Mean raw ADC code while exactly 9.000 V is applied.
 * @return Fitted calibration plus endpoint verification status.
 */
AdcCalibrationResult solveAdcTwoPointCalibration(uint16_t zeroRaw,
                                                  uint16_t nineVoltRaw);

}  // namespace fmq

#endif
