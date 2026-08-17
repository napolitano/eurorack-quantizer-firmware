/**
 * @file PitchConversion.h
 * Declares calibrated conversion between ADC pitch and DAC codes.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_PITCH_CONVERSION_H
#define FM_QUANTIZER_CORE_PITCH_CONVERSION_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"

namespace fmq {

/**
 * Integer affine calibration used by the analogue input and output paths.
 *
 * ADC calibration applies @p offset before the gain ratio. DAC calibration
 * applies the gain ratio before @p offset. The public helpers below expose
 * those exact runtime operations so diagnostics can verify the same path
 * without duplicating the arithmetic.
 */
struct LinearCalibration {
  int32_t offset;
  uint16_t gainNumerator;
  uint16_t gainDenominator;
};

/**
 * Apply an ADC calibration to a raw 10-bit code.
 *
 * The operation is `(raw + offset) * numerator / denominator`, rounded to the
 * nearest integer and clamped to the configured ADC range.
 *
 * @param rawAdc Raw ADC code.
 * @param calibration Calibration to apply. The denominator must be non-zero.
 * @return Calibrated ADC code in the configured valid range.
 */
uint16_t applyAdcCalibration(uint16_t rawAdc,
                             const LinearCalibration &calibration);

/**
 * Apply a DAC calibration to a nominal 12-bit code.
 *
 * The operation is `raw * numerator / denominator + offset`, rounded to the
 * nearest integer and clamped to the configured DAC range.
 *
 * @param rawDacCode Nominal DAC code before analogue correction.
 * @param calibration Calibration to apply. The denominator must be non-zero.
 * @return Corrected DAC code in the configured valid range.
 */
uint16_t applyDacCalibration(int32_t rawDacCode,
                             const LinearCalibration &calibration);

/** Return the runtime-calibrated ADC code for channel A (0) or B (1). */
uint16_t calibratedAdcCode(uint16_t rawAdc, uint8_t channel = 0);

/** Return the runtime-calibrated DAC code for channel A (0) or B (1). */
uint16_t calibratedDacCode(int32_t rawDacCode, uint8_t channel = 0);

SemitoneQ8_8 adcToSemitones(uint16_t adcValue, uint8_t channel = 0);
uint16_t semitonesToDac(SemitoneQ8_8 semitones, uint8_t channel = 0);

}  // namespace fmq

#endif
