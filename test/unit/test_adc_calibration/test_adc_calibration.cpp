/**
 * @file test_adc_calibration.cpp
 * Unit tests for five-point ADC fit and independent interleaved verification.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/AdcCalibration.h"
#include "fmq/domain/PitchConversion.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void fillIdealFitPoints(uint16_t points[kAdcCalibrationFitPointCount]) {
  for (uint8_t i = 0u; i < kAdcCalibrationFitPointCount; ++i) {
    points[i] = nominalAdcCodeForVolts(kAdcCalibrationFitVolts[i]);
  }
}

static void test_reference_targets_cover_fit_and_verification_grids(void) {
  const uint16_t expected[11] = {0u,   102u, 205u, 307u, 409u, 512u,
                                 614u, 716u, 818u, 921u, 1023u};
  for (uint8_t volts = 0u; volts <= 10u; ++volts) {
    TEST_ASSERT_EQUAL_UINT16(expected[volts], nominalAdcCodeForVolts(volts));
  }
  TEST_ASSERT_EQUAL_UINT16(102u, oneVoltNominalAdcCode());
  TEST_ASSERT_EQUAL_UINT16(512u, fiveVoltNominalAdcCode());
  TEST_ASSERT_EQUAL_UINT16(921u, nineVoltNominalAdcCode());
}

static void test_ideal_five_point_fit_produces_unity_mapping(void) {
  uint16_t points[kAdcCalibrationFitPointCount];
  fillIdealFitPoints(points);

  const AdcCalibrationResult result = solveAdcMultiPointCalibration(points);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_EQUAL_INT32(0, result.calibration.offset);
  TEST_ASSERT_EQUAL_UINT16(1u, result.calibration.gainNumerator);
  TEST_ASSERT_EQUAL_UINT16(1u, result.calibration.gainDenominator);
  TEST_ASSERT_EQUAL_UINT16(0u, result.maxFitResidualCodes);
}

static void test_constant_input_offset_is_recovered_from_all_fit_points(void) {
  uint16_t points[kAdcCalibrationFitPointCount];
  fillIdealFitPoints(points);
  for (uint8_t i = 0u; i < kAdcCalibrationFitPointCount; ++i) {
    points[i] = static_cast<uint16_t>(points[i] + 8u);
  }

  const AdcCalibrationResult result = solveAdcMultiPointCalibration(points);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_EQUAL_INT32(-8, result.calibration.offset);
  TEST_ASSERT_EQUAL_UINT16(1u, result.calibration.gainNumerator);
  TEST_ASSERT_EQUAL_UINT16(1u, result.calibration.gainDenominator);
  TEST_ASSERT_EQUAL_UINT16(0u, result.maxFitResidualCodes);
}

static void test_least_squares_fit_tolerates_one_noisy_fit_point(void) {
  uint16_t points[kAdcCalibrationFitPointCount];
  fillIdealFitPoints(points);
  points[2] = static_cast<uint16_t>(points[2] + 2u);

  const AdcCalibrationResult result = solveAdcMultiPointCalibration(points);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(2u, result.maxFitResidualCodes);
  TEST_ASSERT_UINT16_WITHIN(2u, nominalAdcCodeForVolts(2u),
                            applyAdcCalibration(nominalAdcCodeForVolts(2u),
                                                result.calibration));
  TEST_ASSERT_UINT16_WITHIN(2u, nominalAdcCodeForVolts(8u),
                            applyAdcCalibration(nominalAdcCodeForVolts(8u),
                                                result.calibration));
}

static void test_gain_and_offset_error_are_corrected_across_full_fit_grid(void) {
  // Synthetic raw path: approximately target / 0.98 + 6 raw codes.
  uint16_t points[kAdcCalibrationFitPointCount];
  for (uint8_t i = 0u; i < kAdcCalibrationFitPointCount; ++i) {
    const uint32_t target = nominalAdcCodeForVolts(kAdcCalibrationFitVolts[i]);
    points[i] = static_cast<uint16_t>((target * 100u + 49u) / 98u + 6u);
  }

  const AdcCalibrationResult result = solveAdcMultiPointCalibration(points);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(2u, result.maxFitResidualCodes);
  for (uint8_t i = 0u; i < kAdcCalibrationFitPointCount; ++i) {
    TEST_ASSERT_UINT16_WITHIN(
        2u, nominalAdcCodeForVolts(kAdcCalibrationFitVolts[i]),
        applyAdcCalibration(points[i], result.calibration));
  }
}

static void test_zero_floor_remains_exact_with_positive_inverse_offset(void) {
  const uint16_t points[kAdcCalibrationFitPointCount] = {
      95u, 300u, 505u, 710u, 914u};
  const AdcCalibrationResult result = solveAdcMultiPointCalibration(points);
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::Ok, result.status);
  TEST_ASSERT_GREATER_THAN_INT32(0, result.calibration.offset);
  TEST_ASSERT_EQUAL_UINT16(0u, applyAdcCalibration(0u, result.calibration));
}

static void test_non_increasing_or_small_span_is_rejected(void) {
  const uint16_t nonIncreasing[kAdcCalibrationFitPointCount] = {
      100u, 300u, 300u, 700u, 900u};
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::NonIncreasing,
                    solveAdcMultiPointCalibration(nonIncreasing).status);

  const uint16_t smallSpan[kAdcCalibrationFitPointCount] = {
      150u, 250u, 350u, 450u, 700u};
  TEST_ASSERT_EQUAL(AdcCalibrationStatus::SpanTooSmall,
                    solveAdcMultiPointCalibration(smallSpan).status);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_reference_targets_cover_fit_and_verification_grids);
  RUN_TEST(test_ideal_five_point_fit_produces_unity_mapping);
  RUN_TEST(test_constant_input_offset_is_recovered_from_all_fit_points);
  RUN_TEST(test_least_squares_fit_tolerates_one_noisy_fit_point);
  RUN_TEST(test_gain_and_offset_error_are_corrected_across_full_fit_grid);
  RUN_TEST(test_zero_floor_remains_exact_with_positive_inverse_offset);
  RUN_TEST(test_non_increasing_or_small_span_is_rejected);
  return UNITY_END();
}
