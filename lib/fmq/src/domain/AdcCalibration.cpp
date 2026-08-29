/**
 * @file AdcCalibration.cpp
 * Implements integer two-point ADC calibration using 1.000 V and 9.000 V.
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
constexpr uint16_t kMaximumEndpointError = 2u;

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

}  // namespace

uint16_t oneVoltNominalAdcCode() {
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(config::kAdcMaximumCode) + 5u) / 10u);
}

uint16_t fiveVoltNominalAdcCode() {
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(config::kAdcMaximumCode) * 5u + 5u) / 10u);
}

uint16_t nineVoltNominalAdcCode() {
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(config::kAdcMaximumCode) * 9u + 5u) / 10u);
}

AdcCalibrationResult solveAdcTwoPointCalibration(uint16_t oneVoltRaw,
                                                  uint16_t nineVoltRaw) {
  AdcCalibrationResult result;
  if (nineVoltRaw <= oneVoltRaw) {
    result.status = AdcCalibrationStatus::NonIncreasing;
    return result;
  }

  const uint16_t rawSpan = static_cast<uint16_t>(nineVoltRaw - oneVoltRaw);
  if (rawSpan < kMinimumEightVoltSpan) {
    result.status = AdcCalibrationStatus::SpanTooSmall;
    return result;
  }

  const uint16_t targetOne = oneVoltNominalAdcCode();
  const uint16_t targetNine = nineVoltNominalAdcCode();
  const uint16_t targetSpan = static_cast<uint16_t>(targetNine - targetOne);
  const uint16_t divisor = gcd16(targetSpan, rawSpan);
  const uint16_t numerator = static_cast<uint16_t>(targetSpan / divisor);
  const uint16_t denominator = static_cast<uint16_t>(rawSpan / divisor);

  // ADC calibration applies offset before gain. Solve the input-domain
  // offset from the 1 V point and round to the nearest integer ADC code.
  const uint32_t equivalentRawAtOne =
      (static_cast<uint32_t>(targetOne) * denominator + numerator / 2u) /
      numerator;
  const int32_t offset = static_cast<int32_t>(equivalentRawAtOne) -
                         static_cast<int32_t>(oneVoltRaw);
  if (offset < INT16_MIN || offset > INT16_MAX) {
    result.status = AdcCalibrationStatus::OffsetOutOfRange;
    return result;
  }

  result.calibration.offset = offset;
  result.calibration.gainNumerator = numerator;
  result.calibration.gainDenominator = denominator;

  result.correctedOneVolt =
      applyAdcCalibration(oneVoltRaw, result.calibration);
  result.correctedNineVolt =
      applyAdcCalibration(nineVoltRaw, result.calibration);
  if (absoluteDifference(result.correctedOneVolt, targetOne) >
          kMaximumEndpointError ||
      absoluteDifference(result.correctedNineVolt, targetNine) >
          kMaximumEndpointError) {
    result.status = AdcCalibrationStatus::EndpointVerificationFailed;
    return result;
  }

  result.status = AdcCalibrationStatus::Ok;
  return result;
}

}  // namespace fmq
