/**
 * @file CalibrationProfile.h
 * Runtime representation of analogue ADC/DAC calibration.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_DOMAIN_CALIBRATION_PROFILE_H
#define FMQ_DOMAIN_CALIBRATION_PROFILE_H

#include <stdint.h>

namespace fmq {

/** Integer affine calibration used by one analogue converter channel. */
struct LinearCalibration {
  int32_t offset = 0;
  uint16_t gainNumerator = 1;
  uint16_t gainDenominator = 1;
};

/** Complete per-module calibration profile for both CV inputs and outputs. */
struct CalibrationProfile {
  LinearCalibration adc[2];
  LinearCalibration dac[2];
};

/** Build the immutable factory/fallback profile from AnalogConfig.h. */
CalibrationProfile factoryCalibrationProfile();

/** Return the profile currently used by the real-time conversion path. */
const CalibrationProfile &activeCalibrationProfile();

/**
 * Replace the active profile when all four affine calibrations are valid.
 *
 * @return true when @p profile became active, false when it was rejected.
 */
bool setActiveCalibrationProfile(const CalibrationProfile &profile);

/** Restore the compile-time factory/fallback profile. */
void resetActiveCalibrationProfile();

/** Return true when numerator and denominator define a usable affine mapping. */
bool isLinearCalibrationValid(const LinearCalibration &calibration);

/** Return true when every calibration in @p profile is usable. */
bool isCalibrationProfileValid(const CalibrationProfile &profile);

}  // namespace fmq

#endif
