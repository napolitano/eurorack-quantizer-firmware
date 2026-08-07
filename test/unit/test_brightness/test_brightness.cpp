/**
 * @file test_brightness.cpp
 * Host regression or unit tests for brightness behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/ui/BrightnessCalibration.h"
#include "fmq/ui/LedFrameEncoder.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

// Default calibration uses conservative power-on levels.
static void test_default_levels(void) {
  BrightnessCalibration b = BrightnessCalibration::makeDefault();
  TEST_ASSERT_EQUAL_HEX16(0x0480, b.redLevel());
  TEST_ASSERT_EQUAL_HEX16(0x0D00, b.greenLevel());
}

// The twelve clockwise steps span the full TLC5947 range from true off to max.
static void test_step_mapping(void) {
  TEST_ASSERT_EQUAL_UINT16(0, BrightnessCalibration::stepToLevel(0));
  TEST_ASSERT_EQUAL_UINT16(4095, BrightnessCalibration::stepToLevel(11));

  uint16_t previous = BrightnessCalibration::stepToLevel(0);
  for (uint8_t step = 1; step < BrightnessCalibration::kStepCount; ++step) {
    const uint16_t level = BrightnessCalibration::stepToLevel(step);
    TEST_ASSERT_TRUE(level > previous);
    previous = level;
  }
}

// Arbitrary PWM values are represented by the nearest calibration position.
static void test_nearest_step_mapping(void) {
  TEST_ASSERT_EQUAL_UINT8(0, BrightnessCalibration::nearestStepForLevel(0));
  TEST_ASSERT_EQUAL_UINT8(11, BrightnessCalibration::nearestStepForLevel(4095));

  const uint8_t redNearest =
      BrightnessCalibration::nearestStepForLevel(config::kDefaultRedPwm);
  const uint8_t greenNearest =
      BrightnessCalibration::nearestStepForLevel(config::kDefaultGreenPwm);

  TEST_ASSERT_EQUAL_UINT8(redNearest,
                          BrightnessCalibration::makeDefault().redDisplayStep());
  TEST_ASSERT_EQUAL_UINT8(greenNearest,
                          BrightnessCalibration::makeDefault().greenDisplayStep());
}

// Encoding lights the correct emitter channels at the configured levels.
static void test_encode_uses_levels(void) {
  LedFrame frame;
  frame[0] = LedColor::Red;    // logical index 0 -> physical position 6
  frame[6] = LedColor::Green;  // logical index 6 -> physical position 0
  frame[11] = LedColor::Amber;

  uint8_t out[kTlc5947FrameBytes];
  encodeLedFrame(frame, 0x0ABC, 0x0123, out);

  // Physical order is {6,7,8,9,10,11,0,1,2,3,4,5}. Physical slot 0 = logical 6
  // (Green): word = (0 << 12) | 0x123 -> bytes 00 01 23.
  TEST_ASSERT_EQUAL_HEX8(0x00, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x01, out[1]);
  TEST_ASSERT_EQUAL_HEX8(0x23, out[2]);

  // Physical slot 6 = logical 0 (Red): word = (0xABC << 12) | 0 -> AB C0 00.
  TEST_ASSERT_EQUAL_HEX8(0xAB, out[18]);
  TEST_ASSERT_EQUAL_HEX8(0xC0, out[19]);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[20]);

  // Physical slot 5 = logical 11 (Amber): word = (0xABC << 12) | 0x123.
  TEST_ASSERT_EQUAL_HEX8(0xAB, out[15]);
  TEST_ASSERT_EQUAL_HEX8(0xC1, out[16]);
  TEST_ASSERT_EQUAL_HEX8(0x23, out[17]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_default_levels);
  RUN_TEST(test_step_mapping);
  RUN_TEST(test_nearest_step_mapping);
  RUN_TEST(test_encode_uses_levels);
  return UNITY_END();
}
