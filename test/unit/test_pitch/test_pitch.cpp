/**
 * @file test_pitch.cpp
 * Host regression or unit tests for pitch behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/config/AnalogConfig.h"
#include "fmq/domain/PitchConversion.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

// ADC 0 must map to exactly 0 semitones.
static void test_adc_zero_is_zero(void) {
  TEST_ASSERT_EQUAL_INT16(0, adcToSemitones(0));
}

// The conversion is linear over the full 10-bit ADC range and maps the
// endpoint exactly to the configured 120-semitone input range.
static void test_adc_scale_is_exact(void) {
  TEST_ASSERT_EQUAL_INT16(30, adcToSemitones(1));
  TEST_ASSERT_EQUAL_INT16(300, adcToSemitones(10));
  TEST_ASSERT_EQUAL_INT16(120 * kSemitoneOneQ8_8, adcToSemitones(1023));
}

// Out-of-range ADC values are clamped to the top code.
static void test_adc_clamped(void) {
  TEST_ASSERT_EQUAL_INT16(adcToSemitones(1023), adcToSemitones(5000));
}

// DAC endpoints: 0 semitones -> 0, 120 semitones -> full scale 4095.
static void test_dac_endpoints(void) {
  TEST_ASSERT_EQUAL_UINT16(0, semitonesToDac(0));
  TEST_ASSERT_EQUAL_UINT16(4095, semitonesToDac(kMaxSemitone * kSemitoneOneQ8_8));
}

// One octave (12 semitones) should be almost exactly 1/10 of full scale.
static void test_dac_one_octave(void) {
  const uint16_t dac = semitonesToDac(12 * kSemitoneOneQ8_8);
  // 12/120 * 4096 = 409.6 -> rounds to 410.
  TEST_ASSERT_EQUAL_UINT16(410, dac);
}

// A single semitone rounds to nearest (34), not truncated (33).
static void test_dac_rounds_to_nearest(void) {
  TEST_ASSERT_EQUAL_UINT16(34, semitonesToDac(1 * kSemitoneOneQ8_8));
}

// Values above the range are clamped to full scale, not wrapped.
static void test_dac_clamped(void) {
  // 127 semitones is the largest value representable in int16 Q8.8; it is
  // above the 120-semitone range and must clamp to full scale.
  TEST_ASSERT_EQUAL_UINT16(4095, semitonesToDac(127 * kSemitoneOneQ8_8));
  TEST_ASSERT_EQUAL_UINT16(0, semitonesToDac(-50));
}


// ADC calibration applies offset before gain and rounds to nearest. This is
// the concrete two-point case that exposed the calibration-console ambiguity:
// raw 7 at 0 V and raw 510 at 5 V -> offset -7, gain 1023/1006.
static void test_adc_calibration_offset_then_gain(void) {
  const LinearCalibration calibration{-7, 1023, 1006};
  TEST_ASSERT_EQUAL_UINT16(512, applyAdcCalibration(510, calibration));
}

// ADC calibration must clamp both below zero and above the 10-bit range.
static void test_adc_calibration_clamps(void) {
  const LinearCalibration negative{-7, 1, 1};
  const LinearCalibration positive{5, 2, 1};
  const LinearCalibration high{0, 2, 1};
  TEST_ASSERT_EQUAL_UINT16(0, applyAdcCalibration(3, negative));
  TEST_ASSERT_EQUAL_UINT16(30, applyAdcCalibration(10, positive));
  TEST_ASSERT_EQUAL_UINT16(1023, applyAdcCalibration(1023, high));
}

// Integer gain correction rounds to the nearest code rather than truncating.
static void test_adc_calibration_rounds_to_nearest(void) {
  const LinearCalibration calibration{0, 3, 2};
  TEST_ASSERT_EQUAL_UINT16(2, applyAdcCalibration(1, calibration));
}

// DAC calibration deliberately applies gain first and offset second.
static void test_dac_calibration_gain_then_offset(void) {
  const LinearCalibration calibration{10, 2, 1};
  const LinearCalibration rounded{0, 3, 2};
  TEST_ASSERT_EQUAL_UINT16(210, applyDacCalibration(100, calibration));
  TEST_ASSERT_EQUAL_UINT16(2, applyDacCalibration(1, rounded));
}

// DAC correction clamps both its nominal input and its corrected output.
static void test_dac_calibration_clamps(void) {
  const LinearCalibration negative{-7, 1, 1};
  const LinearCalibration high{0, 2, 1};
  TEST_ASSERT_EQUAL_UINT16(0, applyDacCalibration(3, negative));
  TEST_ASSERT_EQUAL_UINT16(0, applyDacCalibration(-20, high));
  TEST_ASSERT_EQUAL_UINT16(4095, applyDacCalibration(4095, high));
}

// The channel wrappers used by both runtime conversion and the calibration
// console must be exactly equivalent to applying the configured constants.
static void test_runtime_calibration_wrappers_use_configured_values(void) {
  const LinearCalibration adcA{config::kAdcOffsetA,
                               config::kAdcGainNumeratorA,
                               config::kAdcGainDenominatorA};
  const LinearCalibration adcB{config::kAdcOffsetB,
                               config::kAdcGainNumeratorB,
                               config::kAdcGainDenominatorB};
  const LinearCalibration dacA{config::kDacOffsetA,
                               config::kDacGainNumeratorA,
                               config::kDacGainDenominatorA};
  const LinearCalibration dacB{config::kDacOffsetB,
                               config::kDacGainNumeratorB,
                               config::kDacGainDenominatorB};
  TEST_ASSERT_EQUAL_UINT16(applyAdcCalibration(510, adcA),
                           calibratedAdcCode(510, 0));
  TEST_ASSERT_EQUAL_UINT16(applyAdcCalibration(508, adcB),
                           calibratedAdcCode(508, 1));
  TEST_ASSERT_EQUAL_UINT16(applyDacCalibration(2048, dacA),
                           calibratedDacCode(2048, 0));
  TEST_ASSERT_EQUAL_UINT16(applyDacCalibration(2048, dacB),
                           calibratedDacCode(2048, 1));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_adc_zero_is_zero);
  RUN_TEST(test_adc_scale_is_exact);
  RUN_TEST(test_adc_clamped);
  RUN_TEST(test_dac_endpoints);
  RUN_TEST(test_dac_one_octave);
  RUN_TEST(test_dac_rounds_to_nearest);
  RUN_TEST(test_dac_clamped);
  RUN_TEST(test_adc_calibration_offset_then_gain);
  RUN_TEST(test_adc_calibration_clamps);
  RUN_TEST(test_adc_calibration_rounds_to_nearest);
  RUN_TEST(test_dac_calibration_gain_then_offset);
  RUN_TEST(test_dac_calibration_clamps);
  RUN_TEST(test_runtime_calibration_wrappers_use_configured_values);
  return UNITY_END();
}
