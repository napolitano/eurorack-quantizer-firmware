/**
 * @file test_ladder_boundaries.cpp
 * Boundary tests for the twelve-button resistor ladder and debounce timing.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/ButtonLadder.h"
#include "fmq/config/AnalogConfig.h"
#include "fmq/config/UiConfig.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void test_all_nominal_ladder_values_decode_to_exact_button(void) {
  for (uint8_t i = 0; i < config::kLadderButtonCount; ++i) {
    TEST_ASSERT_EQUAL_UINT8(i, closestButtonIndex(config::kLadderExpectedValues[i]));
  }
  TEST_ASSERT_EQUAL_UINT8(config::kLadderNoButton,
                          closestButtonIndex(config::kLadderNominalRest));
}

// Reverse scanning plus strict '<' defines deterministic midpoint tie behaviour.
static void test_every_adjacent_midpoint_has_stable_tie_breaking(void) {
  for (uint8_t i = 0; i < config::kLadderButtonCount; ++i) {
    const uint16_t a = config::kLadderExpectedValues[i];
    const uint16_t b = config::kLadderExpectedValues[i + 1u];
    const uint16_t midpoint = static_cast<uint16_t>((a + b) / 2u);
    const uint8_t decoded = closestButtonIndex(midpoint);
    TEST_ASSERT_TRUE(decoded == i || decoded == static_cast<uint8_t>(i + 1u));
    if (((a + b) & 1u) == 0u) {
      // Exact equidistance keeps the later candidate already selected by the
      // reverse scan (higher index / closer to rest).
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i + 1u), decoded);
    }
  }
}

static void test_rest_calibration_accepts_only_documented_range(void) {
  ButtonLadder ladder;
  const uint16_t original = ladder.restAdc();
  ladder.calibrateRest(static_cast<uint16_t>(config::kLadderMinimumValidRest - 1u));
  TEST_ASSERT_EQUAL_UINT16(original, ladder.restAdc());
  ladder.calibrateRest(config::kLadderMinimumValidRest);
  TEST_ASSERT_EQUAL_UINT16(config::kLadderMinimumValidRest, ladder.restAdc());
  ladder.calibrateRest(config::kLadderMaximumValidRest);
  TEST_ASSERT_EQUAL_UINT16(config::kLadderMaximumValidRest, ladder.restAdc());
  ladder.calibrateRest(static_cast<uint16_t>(config::kLadderMaximumValidRest + 1u));
  TEST_ASSERT_EQUAL_UINT16(config::kLadderMaximumValidRest, ladder.restAdc());
}

static void test_press_requires_more_than_64_ms_stability(void) {
  ButtonLadder ladder;
  ButtonEvent event = ladder.sample(0u, config::kLadderExpectedValues[4]);
  TEST_ASSERT_EQUAL(ButtonEventType::None, event.type);
  event = ladder.sample(config::kLadderDebounceMs,
                        config::kLadderExpectedValues[4]);
  TEST_ASSERT_EQUAL(ButtonEventType::None, event.type);
  event = ladder.sample(config::kLadderDebounceMs + 1u,
                        config::kLadderExpectedValues[4]);
  TEST_ASSERT_EQUAL(ButtonEventType::JustPressed, event.type);
  TEST_ASSERT_EQUAL_UINT8(4, event.index);
}

static void test_bounce_resets_the_full_ladder_debounce_window(void) {
  ButtonLadder ladder;
  ladder.sample(0u, config::kLadderExpectedValues[3]);
  ladder.sample(40u, config::kLadderExpectedValues[4]);
  ButtonEvent event = ladder.sample(100u, config::kLadderExpectedValues[4]);
  TEST_ASSERT_EQUAL(ButtonEventType::None, event.type);
  event = ladder.sample(105u, config::kLadderExpectedValues[4]);
  TEST_ASSERT_EQUAL(ButtonEventType::JustPressed, event.type);
  TEST_ASSERT_EQUAL_UINT8(4, event.index);
}

// TA-064/065: nearest-value decoding is accepted only inside a documented
// plausibility window; values in the gaps fail safe as "no button".
static void test_implausible_mid_ladder_value_is_rejected(void) {
  TEST_ASSERT_EQUAL_UINT8(config::kLadderNoButton, buttonIndexForAdc(70u, 558u));
}

static void test_button_acceptance_window_is_inclusive_and_bounded(void) {
  const uint8_t button = 5u;
  const uint16_t nominal = config::kLadderExpectedValues[button];
  const uint16_t tolerance = config::kLadderButtonAcceptanceDelta;
  TEST_ASSERT_EQUAL_UINT8(button,
      buttonIndexForAdc(static_cast<uint16_t>(nominal - tolerance), 558u));
  TEST_ASSERT_EQUAL_UINT8(button,
      buttonIndexForAdc(static_cast<uint16_t>(nominal + tolerance), 558u));
  TEST_ASSERT_EQUAL_UINT8(config::kLadderNoButton,
      buttonIndexForAdc(static_cast<uint16_t>(nominal + tolerance + 1u), 558u));
}

static void test_high_open_circuit_values_are_rejected_as_no_button(void) {
  TEST_ASSERT_EQUAL_UINT8(config::kLadderNoButton,
                          buttonIndexForAdc(config::kLadderMinimumValidRest, 558u));
  TEST_ASSERT_EQUAL_UINT8(config::kLadderNoButton, buttonIndexForAdc(1023u, 558u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_all_nominal_ladder_values_decode_to_exact_button);
  RUN_TEST(test_every_adjacent_midpoint_has_stable_tie_breaking);
  RUN_TEST(test_rest_calibration_accepts_only_documented_range);
  RUN_TEST(test_press_requires_more_than_64_ms_stability);
  RUN_TEST(test_bounce_resets_the_full_ladder_debounce_window);
  RUN_TEST(test_implausible_mid_ladder_value_is_rejected);
  RUN_TEST(test_button_acceptance_window_is_inclusive_and_bounded);
  RUN_TEST(test_high_open_circuit_values_are_rejected_as_no_button);
  return UNITY_END();
}
