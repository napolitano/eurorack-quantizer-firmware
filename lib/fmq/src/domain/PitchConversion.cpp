/**
 * @file PitchConversion.cpp
 * Implements calibrated ADC-to-pitch and pitch-to-DAC conversion.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/domain/PitchConversion.h"

#include "fmq/config/AnalogConfig.h"
#include "fmq/config/ProductConfig.h"

namespace fmq {

namespace {

constexpr uint8_t kChannelA = 0;

LinearCalibration inputCalibrationFor(uint8_t channel) {
  if (channel == kChannelA) {
    return {config::kAdcOffsetA, config::kAdcGainNumeratorA,
            config::kAdcGainDenominatorA};
  }
  return {config::kAdcOffsetB, config::kAdcGainNumeratorB,
          config::kAdcGainDenominatorB};
}

LinearCalibration outputCalibrationFor(uint8_t channel) {
  if (channel == kChannelA) {
    return {config::kDacOffsetA, config::kDacGainNumeratorA,
            config::kDacGainDenominatorA};
  }
  return {config::kDacOffsetB, config::kDacGainNumeratorB,
          config::kDacGainDenominatorB};
}

}  // namespace

uint16_t applyAdcCalibration(uint16_t rawAdc,
                             const LinearCalibration &calibration) {
  int32_t corrected = static_cast<int32_t>(rawAdc) + calibration.offset;
  corrected = clampInt<int32_t>(corrected, 0, config::kAdcMaximumCode);

  const uint32_t scaled =
      (static_cast<uint32_t>(corrected) * calibration.gainNumerator +
       calibration.gainDenominator / 2u) /
      calibration.gainDenominator;
  return static_cast<uint16_t>(clampInt<int32_t>(
      static_cast<int32_t>(scaled), 0, config::kAdcMaximumCode));
}

uint16_t applyDacCalibration(int32_t rawDacCode,
                             const LinearCalibration &calibration) {
  const int32_t clampedRaw =
      clampInt<int32_t>(rawDacCode, 0, config::kDacMaximumCode);
  const uint32_t scaled =
      (static_cast<uint32_t>(clampedRaw) * calibration.gainNumerator +
       calibration.gainDenominator / 2u) /
      calibration.gainDenominator;
  return static_cast<uint16_t>(clampInt<int32_t>(
      static_cast<int32_t>(scaled) + calibration.offset, 0,
      config::kDacMaximumCode));
}

uint16_t calibratedAdcCode(uint16_t rawAdc, uint8_t channel) {
  return applyAdcCalibration(rawAdc, inputCalibrationFor(channel));
}

uint16_t calibratedDacCode(int32_t rawDacCode, uint8_t channel) {
  return applyDacCalibration(rawDacCode, outputCalibrationFor(channel));
}

SemitoneQ8_8 adcToSemitones(uint16_t adcValue, uint8_t channel) {
  const int32_t calibratedAdc = calibratedAdcCode(adcValue, channel);
  const int32_t maximumPitchQ8_8 =
      static_cast<int32_t>(config::kPitchRangeSemitones) * kSemitoneOneQ8_8;

  const int32_t pitchQ8_8 =
      (calibratedAdc * maximumPitchQ8_8 + config::kAdcMaximumCode / 2) /
      config::kAdcMaximumCode;
  return static_cast<SemitoneQ8_8>(pitchQ8_8);
}

uint16_t semitonesToDac(SemitoneQ8_8 semitones, uint8_t channel) {
  const int32_t maximumPitchQ8_8 =
      static_cast<int32_t>(config::kPitchRangeSemitones) * kSemitoneOneQ8_8;
  const int32_t clampedPitch =
      clampInt<int32_t>(semitones, 0, maximumPitchQ8_8);

  const int32_t rawDacCode =
      (clampedPitch * config::kDacMaximumCode + maximumPitchQ8_8 / 2) /
      maximumPitchQ8_8;
  return calibratedDacCode(rawDacCode, channel);
}

}  // namespace fmq
