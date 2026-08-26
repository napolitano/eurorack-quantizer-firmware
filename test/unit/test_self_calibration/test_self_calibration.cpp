/**
 * @file test_self_calibration.cpp
 * Unit tests for the integer loopback DAC calibration solver.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/SelfCalibration.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static uint16_t adcForEquivalentDac(uint16_t equivalent) {
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(equivalent) * 1023u + 2047u) / 4095u);
}

static void test_ideal_loop_returns_unity_calibration(void) {
  const SelfCalibrationSample samples[] = {
      {384u, adcForEquivalentDac(384u)},
      {1024u, adcForEquivalentDac(1024u)},
      {2048u, adcForEquivalentDac(2048u)},
      {3072u, adcForEquivalentDac(3072u)},
      {3712u, adcForEquivalentDac(3712u)},
  };
  const SelfCalibrationResult result = solveDacSelfCalibration(samples, 5u);
  TEST_ASSERT_EQUAL(SelfCalibrationStatus::Ok, result.status);
  TEST_ASSERT_INT_WITHIN(3, 0, result.calibration.offset);
  TEST_ASSERT_UINT16_WITHIN(3, result.calibration.gainDenominator,
                            result.calibration.gainNumerator);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(5u, result.maxFitResidualDacCodes);
}

static void test_gain_and_offset_error_produces_inverse_correction(void) {
  // Physical loop equivalent: measured = raw * 101/100 + 20 codes.
  const uint16_t raw[] = {384u, 1024u, 2048u, 3072u, 3712u};
  SelfCalibrationSample samples[5];
  for (uint8_t i = 0u; i < 5u; ++i) {
    const uint16_t measured = static_cast<uint16_t>(
        (static_cast<uint32_t>(raw[i]) * 101u + 50u) / 100u + 20u);
    samples[i] = {raw[i], adcForEquivalentDac(measured)};
  }
  const SelfCalibrationResult result = solveDacSelfCalibration(samples, 5u);
  TEST_ASSERT_EQUAL(SelfCalibrationStatus::Ok, result.status);
  TEST_ASSERT_INT_WITHIN(6, -20, result.calibration.offset);
  const uint16_t corrected = applyDacCalibration(2048, result.calibration);
  TEST_ASSERT_INT_WITHIN(10, 2008, corrected);
}

static void test_missing_loop_is_rejected_by_span_check(void) {
  const SelfCalibrationSample samples[] = {
      {384u, 500u}, {1024u, 501u}, {2048u, 502u},
      {3072u, 503u}, {3712u, 504u},
  };
  const SelfCalibrationResult result = solveDacSelfCalibration(samples, 5u);
  TEST_ASSERT_EQUAL(SelfCalibrationStatus::SpanTooSmall, result.status);
}

static void test_non_monotonic_loop_is_rejected(void) {
  const SelfCalibrationSample samples[] = {
      {384u, 100u}, {1024u, 300u}, {2048u, 280u},
      {3072u, 700u}, {3712u, 900u},
  };
  const SelfCalibrationResult result = solveDacSelfCalibration(samples, 5u);
  TEST_ASSERT_EQUAL(SelfCalibrationStatus::NonMonotonic, result.status);
}

static void test_implausible_gain_is_rejected(void) {
  const SelfCalibrationSample samples[] = {
      {384u, 100u}, {1024u, 180u}, {2048u, 300u},
      {3072u, 420u}, {3712u, 520u},
  };
  const SelfCalibrationResult result = solveDacSelfCalibration(samples, 5u);
  TEST_ASSERT_EQUAL(SelfCalibrationStatus::ImplausibleGain, result.status);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ideal_loop_returns_unity_calibration);
  RUN_TEST(test_gain_and_offset_error_produces_inverse_correction);
  RUN_TEST(test_missing_loop_is_rejected_by_span_check);
  RUN_TEST(test_non_monotonic_loop_is_rejected);
  RUN_TEST(test_implausible_gain_is_rejected);
  return UNITY_END();
}
