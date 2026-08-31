/**
 * @file AdcCalibration.cpp
 * Implements integer least-squares ADC calibration using 1/3/5/7/9 V.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/domain/AdcCalibration.h"

#include <limits.h>

#include "fmq/config/AnalogConfig.h"
#include "fmq/domain/PitchConversion.h"

namespace fmq {
namespace {

constexpr uint16_t kMinimumEightVoltSpan = 650u;
constexpr uint32_t kGainQuantizationDenominator = 32768u;

uint16_t gcd16(uint16_t a, uint16_t b) {
  while (b != 0u) {
    const uint16_t remainder = static_cast<uint16_t>(a % b);
    a = b;
    b = remainder;
  }
  return a == 0u ? 1u : a;
}

uint16_t absoluteDifference(uint16_t a, uint16_t b) {
  return a >= b ? static_cast<uint16_t>(a - b)
                : static_cast<uint16_t>(b - a);
}

int64_t roundedSignedDivision(int64_t numerator, int64_t denominator) {
  if (denominator <= 0) return 0;
  if (numerator >= 0) {
    return (numerator + denominator / 2) / denominator;
  }
  return -((-numerator + denominator / 2) / denominator);
}

}  // namespace

uint16_t nominalAdcCodeForVolts(uint8_t volts) {
  const uint8_t clamped = volts > 10u ? 10u : volts;
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(config::kAdcMaximumCode) * clamped + 5u) / 10u);
}

uint16_t oneVoltNominalAdcCode() { return nominalAdcCodeForVolts(1u); }

uint16_t fiveVoltNominalAdcCode() { return nominalAdcCodeForVolts(5u); }

uint16_t nineVoltNominalAdcCode() { return nominalAdcCodeForVolts(9u); }

AdcCalibrationResult solveAdcMultiPointCalibration(
    const uint16_t rawFitPoints[kAdcCalibrationFitPointCount]) {
  AdcCalibrationResult result;

  for (uint8_t i = 1u; i < kAdcCalibrationFitPointCount; ++i) {
    if (rawFitPoints[i] <= rawFitPoints[i - 1u]) {
      result.status = AdcCalibrationStatus::NonIncreasing;
      return result;
    }
  }

  const uint16_t rawSpan = static_cast<uint16_t>(
      rawFitPoints[kAdcCalibrationFitPointCount - 1u] - rawFitPoints[0u]);
  if (rawSpan < kMinimumEightVoltSpan) {
    result.status = AdcCalibrationStatus::SpanTooSmall;
    return result;
  }

  uint64_t sumX = 0u;
  uint64_t sumY = 0u;
  uint64_t sumXX = 0u;
  uint64_t sumXY = 0u;
  for (uint8_t i = 0u; i < kAdcCalibrationFitPointCount; ++i) {
    const uint32_t x = rawFitPoints[i];
    const uint32_t y = nominalAdcCodeForVolts(kAdcCalibrationFitVolts[i]);
    sumX += x;
    sumY += y;
    sumXX += static_cast<uint64_t>(x) * x;
    sumXY += static_cast<uint64_t>(x) * y;
  }

  const uint64_t n = kAdcCalibrationFitPointCount;
  const uint64_t slopeNumerator = n * sumXY - sumX * sumY;
  const uint64_t slopeDenominator = n * sumXX - sumX * sumX;
  if (slopeNumerator == 0u || slopeDenominator == 0u) {
    result.status = AdcCalibrationStatus::NonIncreasing;
    return result;
  }

  const uint64_t quantizedNumerator64 =
      (slopeNumerator * kGainQuantizationDenominator + slopeDenominator / 2u) /
      slopeDenominator;
  if (quantizedNumerator64 == 0u || quantizedNumerator64 > UINT16_MAX) {
    result.status = AdcCalibrationStatus::GainOutOfRange;
    return result;
  }

  uint16_t gainNumerator = static_cast<uint16_t>(quantizedNumerator64);
  uint16_t gainDenominator =
      static_cast<uint16_t>(kGainQuantizationDenominator);
  const uint16_t divisor = gcd16(gainNumerator, gainDenominator);
  gainNumerator = static_cast<uint16_t>(gainNumerator / divisor);
  gainDenominator = static_cast<uint16_t>(gainDenominator / divisor);

  // The runtime form is y = (x + offset) * gain. Derive the input-domain
  // offset from the fitted line's centroid after quantizing the gain ratio.
  const int64_t offsetNumerator =
      static_cast<int64_t>(sumY * gainDenominator) -
      static_cast<int64_t>(sumX * gainNumerator);
  const int64_t offsetDenominator =
      static_cast<int64_t>(n) * gainNumerator;
  const int64_t offset =
      roundedSignedDivision(offsetNumerator, offsetDenominator);
  if (offset < INT16_MIN || offset > INT16_MAX) {
    result.status = AdcCalibrationStatus::OffsetOutOfRange;
    return result;
  }

  result.calibration.offset = static_cast<int32_t>(offset);
  result.calibration.gainNumerator = gainNumerator;
  result.calibration.gainDenominator = gainDenominator;

  uint16_t maxResidual = 0u;
  for (uint8_t i = 0u; i < kAdcCalibrationFitPointCount; ++i) {
    const uint16_t corrected =
        applyAdcCalibration(rawFitPoints[i], result.calibration);
    result.correctedFitPoints[i] = corrected;
    const uint16_t residual = absoluteDifference(
        corrected, nominalAdcCodeForVolts(kAdcCalibrationFitVolts[i]));
    if (residual > maxResidual) maxResidual = residual;
  }
  result.maxFitResidualCodes = maxResidual;
  result.status = AdcCalibrationStatus::Ok;
  return result;
}

}  // namespace fmq
