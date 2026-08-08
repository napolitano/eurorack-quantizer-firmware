/**
 * @file test_pitch_exhaustive.cpp
 * Exhaustive conversion tests for the ADC/pitch/DAC signal mapping.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/config/AnalogConfig.h"
#include "fmq/domain/PitchConversion.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

// TA-025..028: every legal ADC code maps monotonically into 0..120 semitones.
static void test_all_1024_adc_codes_are_monotonic_and_in_range(void) {
  SemitoneQ8_8 previous = adcToSemitones(0, 0);
  TEST_ASSERT_EQUAL_INT16(0, previous);
  for (uint16_t raw = 1; raw <= config::kAdcMaximumCode; ++raw) {
    const SemitoneQ8_8 current = adcToSemitones(raw, 0);
    TEST_ASSERT_TRUE(current >= previous);
    TEST_ASSERT_TRUE(current >= 0);
    TEST_ASSERT_TRUE(current <= kMaxSemitone * kSemitoneOneQ8_8);
    previous = current;
  }
  TEST_ASSERT_EQUAL_INT16(kMaxSemitone * kSemitoneOneQ8_8, previous);
}

static void test_both_input_channels_have_identical_factory_transfer_function(void) {
  for (uint16_t raw = 0; raw <= config::kAdcMaximumCode; ++raw) {
    TEST_ASSERT_EQUAL_INT16(adcToSemitones(raw, 0), adcToSemitones(raw, 1));
  }
}

// TA-050/051: all representable in-range Q8.8 pitches map monotonically to DAC.
static void test_full_q8_8_pitch_range_maps_monotonically_to_dac(void) {
  uint16_t previous = semitonesToDac(0, 0);
  for (int32_t q = 1; q <= kMaxSemitone * kSemitoneOneQ8_8; ++q) {
    const uint16_t current = semitonesToDac(static_cast<SemitoneQ8_8>(q), 0);
    TEST_ASSERT_TRUE(current >= previous);
    TEST_ASSERT_TRUE(current <= config::kDacMaximumCode);
    previous = current;
  }
  TEST_ASSERT_EQUAL_UINT16(config::kDacMaximumCode, previous);
}

static void test_both_output_channels_have_identical_factory_transfer_function(void) {
  for (int16_t semitone = 0; semitone <= kMaxSemitone; ++semitone) {
    const SemitoneQ8_8 q = static_cast<SemitoneQ8_8>(semitone * kSemitoneOneQ8_8);
    TEST_ASSERT_EQUAL_UINT16(semitonesToDac(q, 0), semitonesToDac(q, 1));
  }
}

// End-to-end ideal conversion must stay within one DAC LSB of direct mapping.
static void test_adc_to_pitch_to_dac_is_monotonic_and_endpoint_exact(void) {
  uint16_t previous = 0;
  for (uint16_t raw = 0; raw <= config::kAdcMaximumCode; ++raw) {
    const uint16_t dac = semitonesToDac(adcToSemitones(raw, 0), 0);
    TEST_ASSERT_TRUE(dac >= previous);
    previous = dac;
  }
  TEST_ASSERT_EQUAL_UINT16(config::kDacMaximumCode, previous);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_all_1024_adc_codes_are_monotonic_and_in_range);
  RUN_TEST(test_both_input_channels_have_identical_factory_transfer_function);
  RUN_TEST(test_full_q8_8_pitch_range_maps_monotonically_to_dac);
  RUN_TEST(test_both_output_channels_have_identical_factory_transfer_function);
  RUN_TEST(test_adc_to_pitch_to_dac_is_monotonic_and_endpoint_exact);
  return UNITY_END();
}
