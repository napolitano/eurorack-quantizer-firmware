/**
 * @file test_adc_calibration.cpp
 * Unit tests for 1.000 V / 9.000 V ADC two-point calibration.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/AdcCalibration.h"
#include "fmq/domain/PitchConversion.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void test_reference_targets_stay_inside_adc_range(void) {
  TEST_ASSERT_EQUAL_UINT16(102u, oneVoltNominalAdcCode());
  TEST_ASSERT_EQUAL_UINT16(512u, fiveVoltNominalAdcCode());
  TEST_ASSERT_EQUAL_UINT16(921u, nineVoltNominalAdcCode());
  TEST_ASSERT_LESS_THAN_UINT16(1023u, nineVoltNominalAdcCode());
}

static void test_ideal_one_and_nine_volt_points_produce_unity_mapping(void) {
  const AdcCalibrationResult result = solveAdcTwoPointCalibration(
      oneVoltNominalAdcCode(), nineVoltNominalAdcCode());
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_EQUAL_INT32(0, result.calibration.offset);
  TEST_ASSERT_EQUAL_UINT16(1u, result.calibration.gainNumerator);
  TEST_ASSERT_EQUAL_UINT16(1u, result.calibration.gainDenominator);
  TEST_ASSERT_EQUAL_UINT16(102u, result.correctedOneVolt);
  TEST_ASSERT_EQUAL_UINT16(921u, result.correctedNineVolt);
}

static void test_offset_and_gain_error_are_corrected_from_linear_points(void) {
  const AdcCalibrationResult result = solveAdcTwoPointCalibration(110u, 912u);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_EQUAL_INT32(-10, result.calibration.offset);
  TEST_ASSERT_UINT16_WITHIN(2u, 102u,
                            applyAdcCalibration(110u, result.calibration));
  TEST_ASSERT_UINT16_WITHIN(2u, 921u,
                            applyAdcCalibration(912u, result.calibration));
}

static void test_positive_inverse_offset_keeps_saturated_zero_at_zero(void) {
  const AdcCalibrationResult result = solveAdcTwoPointCalibration(95u, 914u);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_EQUAL_INT32(7, result.calibration.offset);
  TEST_ASSERT_EQUAL_UINT16(0u, applyAdcCalibration(0u, result.calibration));
  TEST_ASSERT_UINT16_WITHIN(2u, 512u,
                            applyAdcCalibration(505u, result.calibration));
}

static void test_non_increasing_or_small_span_is_rejected(void) {
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::NonIncreasing,
                    solveAdcTwoPointCalibration(100u, 100u).status);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::SpanTooSmall,
                    solveAdcTwoPointCalibration(150u, 700u).status);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_reference_targets_stay_inside_adc_range);
  RUN_TEST(test_ideal_one_and_nine_volt_points_produce_unity_mapping);
  RUN_TEST(test_offset_and_gain_error_are_corrected_from_linear_points);
  RUN_TEST(test_positive_inverse_offset_keeps_saturated_zero_at_zero);
  RUN_TEST(test_non_increasing_or_small_span_is_rejected);
  return UNITY_END();
}
