/**
 * @file test_adc_calibration.cpp
 * Unit tests for 0.000 V / 9.000 V ADC two-point calibration.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/AdcCalibration.h"
#include "fmq/domain/PitchConversion.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void test_nine_volt_target_has_headroom_below_full_scale(void) {
  TEST_ASSERT_EQUAL_UINT16(921u, nineVoltNominalAdcCode());
  TEST_ASSERT_LESS_THAN_UINT16(1023u, nineVoltNominalAdcCode());
}

static void test_ideal_endpoints_produce_unity_like_mapping(void) {
  const AdcCalibrationResult result =
      solveAdcTwoPointCalibration(0u, nineVoltNominalAdcCode());
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_EQUAL_INT32(0, result.calibration.offset);
  TEST_ASSERT_EQUAL_UINT16(1u, result.calibration.gainNumerator);
  TEST_ASSERT_EQUAL_UINT16(1u, result.calibration.gainDenominator);
  TEST_ASSERT_EQUAL_UINT16(0u, result.correctedZero);
  TEST_ASSERT_EQUAL_UINT16(921u, result.correctedNineVolt);
}

static void test_offset_and_gain_error_are_corrected(void) {
  const AdcCalibrationResult result = solveAdcTwoPointCalibration(7u, 910u);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_EQUAL_INT32(-7, result.calibration.offset);
  TEST_ASSERT_UINT16_WITHIN(2u, 0u,
                            applyAdcCalibration(7u, result.calibration));
  TEST_ASSERT_UINT16_WITHIN(2u, 921u,
                            applyAdcCalibration(910u, result.calibration));
}

static void test_non_increasing_or_small_span_is_rejected(void) {
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::NonIncreasing,
                    solveAdcTwoPointCalibration(100u, 100u).status);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::SpanTooSmall,
                    solveAdcTwoPointCalibration(10u, 600u).status);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_nine_volt_target_has_headroom_below_full_scale);
  RUN_TEST(test_ideal_endpoints_produce_unity_like_mapping);
  RUN_TEST(test_offset_and_gain_error_are_corrected);
  RUN_TEST(test_non_increasing_or_small_span_is_rejected);
  return UNITY_END();
}
