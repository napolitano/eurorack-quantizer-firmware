/**
 * @file AdcCalibration.cpp
 * Implements integer two-point ADC calibration using 0.000 V and 9.000 V.
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

constexpr uint16_t kMinimumNineVoltSpan = 700u;
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

uint16_t nineVoltNominalAdcCode() {
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(config::kAdcMaximumCode) * 9u + 5u) / 10u);
}

AdcCalibrationResult solveAdcTwoPointCalibration(uint16_t zeroRaw,
                                                  uint16_t nineVoltRaw) {
  AdcCalibrationResult result;
  if (nineVoltRaw <= zeroRaw) {
    result.status = AdcCalibrationStatus::NonIncreasing;
    return result;
  }

  const uint16_t span = static_cast<uint16_t>(nineVoltRaw - zeroRaw);
  if (span < kMinimumNineVoltSpan) {
    result.status = AdcCalibrationStatus::SpanTooSmall;
    return result;
  }

  if (zeroRaw > static_cast<uint16_t>(INT16_MAX)) {
    result.status = AdcCalibrationStatus::OffsetOutOfRange;
    return result;
  }

  const uint16_t target = nineVoltNominalAdcCode();
  const uint16_t divisor = gcd16(target, span);
  result.calibration.offset = -static_cast<int32_t>(zeroRaw);
  result.calibration.gainNumerator = static_cast<uint16_t>(target / divisor);
  result.calibration.gainDenominator = static_cast<uint16_t>(span / divisor);

  result.correctedZero = applyAdcCalibration(zeroRaw, result.calibration);
  result.correctedNineVolt =
      applyAdcCalibration(nineVoltRaw, result.calibration);
  if (result.correctedZero > kMaximumEndpointError ||
      absoluteDifference(result.correctedNineVolt, target) >
          kMaximumEndpointError) {
    result.status = AdcCalibrationStatus::EndpointVerificationFailed;
    return result;
  }

  result.status = AdcCalibrationStatus::Ok;
  return result;
}

}  // namespace fmq
