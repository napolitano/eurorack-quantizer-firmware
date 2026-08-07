/**
 * @file AnalogConfig.h
 * Central configuration for ADC, DAC and resistor-ladder assumptions.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_ANALOG_CONFIG_H
#define FMQ_CONFIG_ANALOG_CONFIG_H
#include <stdint.h>
namespace fmq::config {
constexpr uint8_t kLadderButtonCount = 12;
constexpr uint8_t kLadderNoButton = 12;

// Proven values from the original Rust firmware for the original PCB:
// 10 kOhm to 5 V, up to twelve 1 kOhm ladder resistors to ground, 10-bit ADC,
// AVCC reference. Element 12 is the unpressed/rest value.
constexpr uint16_t kLadderExpectedValues[kLadderButtonCount + 1] = {
    0, 93, 171, 236, 292, 341, 384, 421, 455, 485, 512, 536, 558};
constexpr uint16_t kLadderNominalRest = kLadderExpectedValues[kLadderButtonCount];

// Original hardware defaults to the exact nearest-value decoder used by the
// Rust firmware. Rest normalisation is intentionally disabled: with the ladder
// and AREF derived from the same 5 V rail, ratios are already ratiometric, and
// scaling the readings can move keys across the narrow upper thresholds.
constexpr bool kLadderUseRestNormalization = false;
constexpr uint16_t kLadderMinimumValidRest = 548;
constexpr uint16_t kLadderMaximumValidRest = 570;
constexpr uint16_t kLadderRestStabilitySpan = 8;
constexpr uint8_t kLadderCalibrationSamples = 24;

constexpr uint8_t kAdcChannelCount = 3;
constexpr uint8_t kAdcMovingAverageSamples = 3;
// fm-lib calls this an averaging ADC, but for WINDOW=3 it actually returns the
// median. Preserve that behaviour: it rejects one-sample spikes much better on
// the closely spaced resistor ladder.
constexpr bool kAdcUseMedianFilter = true;
constexpr bool kDiscardFirstAdcReadAfterMuxChange = true;
constexpr uint16_t kAdcMaximumCode = 1023;
constexpr uint16_t kDacMaximumCode = 4095;
constexpr int16_t kAdcOffsetA = 0, kAdcOffsetB = 0;
constexpr uint16_t kAdcGainNumeratorA = 1, kAdcGainDenominatorA = 1;
constexpr uint16_t kAdcGainNumeratorB = 1, kAdcGainDenominatorB = 1;
constexpr int16_t kDacOffsetA = 0, kDacOffsetB = 0;
constexpr uint16_t kDacGainNumeratorA = 1, kDacGainDenominatorA = 1;
constexpr uint16_t kDacGainNumeratorB = 1, kDacGainDenominatorB = 1;
static_assert(kAdcMovingAverageSamples >= 1 && kAdcMovingAverageSamples <= 8, "ADC average out of range");
static_assert(kAdcMaximumCode > 0, "invalid ADC maximum");
static_assert(kDacMaximumCode > 0, "invalid DAC maximum");
static_assert(kAdcGainDenominatorA && kAdcGainDenominatorB && kDacGainDenominatorA && kDacGainDenominatorB, "calibration denominator must be nonzero");
}
#endif
