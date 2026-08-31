/**
 * @file test_calibration_measurement.cpp
 * Exhaustive tests for the panel-driven maintenance output test mode mapping.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/CalibrationMeasurement.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void test_all_121_semitones_have_valid_panel_selections(void) {
  uint8_t semitone = 0u;
  for (uint8_t octave = 0u; octave < 10u; ++octave) {
    for (uint8_t pitchClass = 0u; pitchClass < 12u; ++pitchClass) {
      TEST_ASSERT_TRUE(measurementSelectionValid(octave, pitchClass));
      TEST_ASSERT_EQUAL_UINT8(semitone,
                              measurementSemitone(octave, pitchClass));
      TEST_ASSERT_EQUAL_INT16(
          static_cast<int16_t>(semitone) * kSemitoneOneQ8_8,
          measurementPitchQ8_8(octave, pitchClass));
      ++semitone;
    }
  }
  TEST_ASSERT_EQUAL_UINT8(120u, semitone);
  TEST_ASSERT_TRUE(measurementSelectionValid(10u, 0u));
  TEST_ASSERT_EQUAL_UINT8(120u, measurementSemitone(10u, 0u));
}

static void test_octave_ten_accepts_only_top_c(void) {
  TEST_ASSERT_TRUE(measurementSelectionValid(10u, 0u));
  for (uint8_t pitchClass = 1u; pitchClass < 12u; ++pitchClass) {
    TEST_ASSERT_FALSE(measurementSelectionValid(10u, pitchClass));
  }
  TEST_ASSERT_FALSE(measurementSelectionValid(11u, 0u));
}

static void test_nominal_voltage_mapping_matches_one_volt_per_octave(void) {
  TEST_ASSERT_EQUAL_UINT32(0u, nominalMeasurementMicrovolts(0u));
  TEST_ASSERT_EQUAL_UINT32(83333u, nominalMeasurementMicrovolts(1u));
  TEST_ASSERT_EQUAL_UINT32(500000u, nominalMeasurementMicrovolts(6u));
  TEST_ASSERT_EQUAL_UINT32(1000000u, nominalMeasurementMicrovolts(12u));
  TEST_ASSERT_EQUAL_UINT32(4916667u, nominalMeasurementMicrovolts(59u));
  TEST_ASSERT_EQUAL_UINT32(9000000u, nominalMeasurementMicrovolts(108u));
  TEST_ASSERT_EQUAL_UINT32(10000000u, nominalMeasurementMicrovolts(120u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_all_121_semitones_have_valid_panel_selections);
  RUN_TEST(test_octave_ten_accepts_only_top_c);
  RUN_TEST(test_nominal_voltage_mapping_matches_one_volt_per_octave);
  return UNITY_END();
}
