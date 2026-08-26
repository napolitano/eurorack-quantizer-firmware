/**
 * @file SelfCalibration.cpp
 * Integer-only affine solver for CV OUT -> CV IN loopback calibration.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/domain/SelfCalibration.h"

#include "fmq/config/AnalogConfig.h"

namespace fmq {
namespace {

constexpr uint16_t kMinimumRawSpan = 2048u;
constexpr int32_t kMaximumAbsoluteOffset = 512;

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

uint16_t nominalFromRaw(uint16_t raw, const LinearCalibration &calibration) {
  int32_t shifted = static_cast<int32_t>(raw) - calibration.offset;
  if (shifted < 0) shifted = 0;
  const uint32_t numerator =
      static_cast<uint32_t>(shifted) * calibration.gainDenominator;
  const uint32_t rounded =
      (numerator + calibration.gainNumerator / 2u) /
      calibration.gainNumerator;
  return static_cast<uint16_t>(
      rounded > config::kDacMaximumCode ? config::kDacMaximumCode : rounded);
}

}  // namespace

uint16_t adcCodeToEquivalentDac(uint16_t adcCode) {
  const uint16_t clamped =
      adcCode > config::kAdcMaximumCode ? config::kAdcMaximumCode : adcCode;
  const uint32_t scaled =
      static_cast<uint32_t>(clamped) * config::kDacMaximumCode +
      config::kAdcMaximumCode / 2u;
  return static_cast<uint16_t>(scaled / config::kAdcMaximumCode);
}

SelfCalibrationResult solveDacSelfCalibration(const SelfCalibrationSample *samples,
                                               uint8_t count) {
  SelfCalibrationResult result;
  if (samples == nullptr || count < 3u) {
    result.status = SelfCalibrationStatus::TooFewSamples;
    return result;
  }

  for (uint8_t i = 1u; i < count; ++i) {
    if (samples[i].dacCode <= samples[i - 1u].dacCode ||
        samples[i].adcCode <= samples[i - 1u].adcCode) {
      result.status = SelfCalibrationStatus::NonMonotonic;
      return result;
    }
  }

  const uint16_t rawSpan = static_cast<uint16_t>(
      samples[count - 1u].dacCode - samples[0].dacCode);
  const uint16_t firstEquivalent = adcCodeToEquivalentDac(samples[0].adcCode);
  const uint16_t lastEquivalent =
      adcCodeToEquivalentDac(samples[count - 1u].adcCode);
  if (lastEquivalent <= firstEquivalent) {
    result.status = SelfCalibrationStatus::NonMonotonic;
    return result;
  }
  const uint16_t measuredSpan =
      static_cast<uint16_t>(lastEquivalent - firstEquivalent);

  if (rawSpan < kMinimumRawSpan || measuredSpan < kMinimumRawSpan / 2u) {
    result.status = SelfCalibrationStatus::SpanTooSmall;
    return result;
  }

  // A correct loop should be close to unity once both sides are expressed in
  // equivalent converter codes. +/-25% is intentionally generous but rejects
  // missing/mispatched loops and grossly wrong signal scaling.
  const uint32_t measuredScaled = static_cast<uint32_t>(measuredSpan) * 4u;
  const uint32_t rawScaled = static_cast<uint32_t>(rawSpan);
  if (measuredScaled < rawScaled * 3u ||
      measuredScaled > rawScaled * 5u) {
    result.status = SelfCalibrationStatus::ImplausibleGain;
    return result;
  }

  const uint16_t divisor = gcd16(rawSpan, measuredSpan);
  const uint16_t gainNumerator = static_cast<uint16_t>(rawSpan / divisor);
  const uint16_t gainDenominator =
      static_cast<uint16_t>(measuredSpan / divisor);

  const uint32_t firstScaled =
      static_cast<uint32_t>(firstEquivalent) * gainNumerator;
  const int32_t firstCorrected = static_cast<int32_t>(
      (firstScaled + gainDenominator / 2u) / gainDenominator);
  const int32_t offset =
      static_cast<int32_t>(samples[0].dacCode) - firstCorrected;
  if (offset < -kMaximumAbsoluteOffset || offset > kMaximumAbsoluteOffset) {
    result.status = SelfCalibrationStatus::OffsetOutOfRange;
    return result;
  }

  result.calibration = {offset, gainNumerator, gainDenominator};
  uint16_t maxResidual = 0u;
  for (uint8_t i = 0u; i < count; ++i) {
    const uint16_t measuredEquivalent =
        adcCodeToEquivalentDac(samples[i].adcCode);
    const uint16_t predictedNominal =
        nominalFromRaw(samples[i].dacCode, result.calibration);
    const uint16_t residual =
        absoluteDifference(measuredEquivalent, predictedNominal);
    if (residual > maxResidual) maxResidual = residual;
  }
  result.maxFitResidualDacCodes = maxResidual;
  result.status = SelfCalibrationStatus::Ok;
  return result;
}

}  // namespace fmq
