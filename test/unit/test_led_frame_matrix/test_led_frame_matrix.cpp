/**
 * @file test_led_frame_matrix.cpp
 * Exhaustive logical-to-physical TLC5947 frame encoding tests.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/config/LedConfig.h"
#include "fmq/ui/LedFrameEncoder.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static uint32_t wordAt(const uint8_t bytes[kTlc5947FrameBytes], uint8_t physical) {
  const uint8_t i = static_cast<uint8_t>(physical * 3u);
  return (static_cast<uint32_t>(bytes[i]) << 16u) |
         (static_cast<uint32_t>(bytes[i + 1u]) << 8u) |
         static_cast<uint32_t>(bytes[i + 2u]);
}

static uint8_t physicalForLogical(uint8_t logical) {
  return logical < 6u ? static_cast<uint8_t>(logical + 6u)
                      : static_cast<uint8_t>(logical - 6u);
}

static void test_each_logical_led_maps_to_documented_physical_position(void) {
  for (uint8_t logical = 0; logical < kNoteCount; ++logical) {
    LedFrame frame;
    frame[logical] = LedColor::Red;
    uint8_t bytes[kTlc5947FrameBytes] = {};
    encodeLedFrame(frame, 0x0123u, 0x0456u, bytes);
    for (uint8_t physical = 0; physical < kNoteCount; ++physical) {
      const uint32_t expected =
          physical == physicalForLogical(logical) ? 0x123000u : 0u;
      TEST_ASSERT_EQUAL_UINT32(expected, wordAt(bytes, physical));
    }
  }
}

static void test_all_four_colors_encode_correct_red_green_channels(void) {
  const LedColor colors[] = {LedColor::Off, LedColor::Red,
                             LedColor::Green, LedColor::Amber};
  const uint32_t expected[] = {0x000000u, 0xABC000u, 0x000123u, 0xABC123u};
  for (uint8_t c = 0; c < 4u; ++c) {
    LedFrame frame;
    frame[6] = colors[c];  // physical slot zero
    uint8_t bytes[kTlc5947FrameBytes] = {};
    encodeLedFrame(frame, 0x0ABCu, 0x0123u, bytes);
    TEST_ASSERT_EQUAL_UINT32(expected[c], wordAt(bytes, 0));
  }
}

static void test_pwm_levels_are_masked_to_12_bits(void) {
  LedFrame frame;
  frame[6] = LedColor::Amber;
  uint8_t bytes[kTlc5947FrameBytes] = {};
  encodeLedFrame(frame, 0xF123u, 0xE456u, bytes);
  TEST_ASSERT_EQUAL_UINT32(0x123456u, wordAt(bytes, 0));
}

static void test_scaled_intensity_zero_and_full_are_exact(void) {
  LedFrame frame;
  frame[6] = LedColor::Amber;
  uint16_t intensity[kNoteCount] = {};
  uint8_t bytes[kTlc5947FrameBytes] = {};
  encodeLedFrameScaled(frame, 0x0800u, 0x0400u, intensity, bytes);
  TEST_ASSERT_EQUAL_UINT32(0u, wordAt(bytes, 0));
  intensity[6] = config::kLedPwmMaximum;
  encodeLedFrameScaled(frame, 0x0800u, 0x0400u, intensity, bytes);
  TEST_ASSERT_EQUAL_UINT32(0x800400u, wordAt(bytes, 0));
}

static void test_scaled_intensity_above_4095_is_clamped(void) {
  LedFrame frame;
  frame[6] = LedColor::Red;
  uint16_t intensity[kNoteCount] = {};
  intensity[6] = 65535u;
  uint8_t bytes[kTlc5947FrameBytes] = {};
  encodeLedFrameScaled(frame, 0x0555u, 0x0222u, intensity, bytes);
  TEST_ASSERT_EQUAL_UINT32(0x555000u, wordAt(bytes, 0));
}

static void test_scaled_intensity_is_monotonic_for_every_q12_level(void) {
  LedFrame frame;
  frame[6] = LedColor::Amber;
  uint16_t intensity[kNoteCount] = {};
  uint8_t bytes[kTlc5947FrameBytes] = {};
  uint32_t previousRed = 0u;
  uint32_t previousGreen = 0u;
  for (uint16_t level = 0; level <= config::kLedPwmMaximum; ++level) {
    intensity[6] = level;
    encodeLedFrameScaled(frame, 0x0FFFu, 0x0800u, intensity, bytes);
    const uint32_t word = wordAt(bytes, 0);
    const uint32_t red = (word >> 12u) & 0x0FFFu;
    const uint32_t green = word & 0x0FFFu;
    TEST_ASSERT_TRUE(red >= previousRed);
    TEST_ASSERT_TRUE(green >= previousGreen);
    previousRed = red;
    previousGreen = green;
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_each_logical_led_maps_to_documented_physical_position);
  RUN_TEST(test_all_four_colors_encode_correct_red_green_channels);
  RUN_TEST(test_pwm_levels_are_masked_to_12_bits);
  RUN_TEST(test_scaled_intensity_zero_and_full_are_exact);
  RUN_TEST(test_scaled_intensity_above_4095_is_clamped);
  RUN_TEST(test_scaled_intensity_is_monotonic_for_every_q12_level);
  return UNITY_END();
}
