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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_adc_zero_is_zero);
  RUN_TEST(test_adc_scale_is_exact);
  RUN_TEST(test_adc_clamped);
  RUN_TEST(test_dac_endpoints);
  RUN_TEST(test_dac_one_octave);
  RUN_TEST(test_dac_rounds_to_nearest);
  RUN_TEST(test_dac_clamped);
  return UNITY_END();
}
