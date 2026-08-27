/**
 * @file test_startup.cpp
 * Host regression tests for the fixed <=750 ms startup fade.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/StartupAnimation.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void test_startup_fade_finishes_within_750_ms(void) {
  StartupAnimation animation;
  TEST_ASSERT_EQUAL_UINT16(700u, StartupAnimation::kDurationMs);
  TEST_ASSERT_FALSE(animation.isDone(699u));
  TEST_ASSERT_TRUE(animation.isDone(700u));
}

static void test_startup_fades_in_green_first(void) {
  StartupAnimation animation;
  const StartupAnimationSample sample = animation.sampleAt(75u);
  TEST_ASSERT_EQUAL_UINT16(0u, sample.redQ12);
  TEST_ASSERT_TRUE(sample.greenQ12 > 0u);
  TEST_ASSERT_TRUE(sample.greenQ12 < 4095u);
}

static void test_startup_crossfades_green_to_amber(void) {
  StartupAnimation animation;
  const StartupAnimationSample sample = animation.sampleAt(250u);
  TEST_ASSERT_EQUAL_UINT16(4095u, sample.greenQ12);
  TEST_ASSERT_TRUE(sample.redQ12 > 0u);
  TEST_ASSERT_TRUE(sample.redQ12 < 4095u);
}

static void test_startup_crossfades_amber_to_red(void) {
  StartupAnimation animation;
  const StartupAnimationSample sample = animation.sampleAt(450u);
  TEST_ASSERT_EQUAL_UINT16(4095u, sample.redQ12);
  TEST_ASSERT_TRUE(sample.greenQ12 > 0u);
  TEST_ASSERT_TRUE(sample.greenQ12 < 4095u);
}

static void test_startup_reaches_full_amber_at_second_transition(void) {
  StartupAnimation animation;
  const StartupAnimationSample sample = animation.sampleAt(350u);
  TEST_ASSERT_EQUAL_UINT16(4095u, sample.redQ12);
  TEST_ASSERT_EQUAL_UINT16(4095u, sample.greenQ12);
}

static void test_startup_fades_red_to_off(void) {
  StartupAnimation animation;
  const StartupAnimationSample sample = animation.sampleAt(625u);
  TEST_ASSERT_EQUAL_UINT16(0u, sample.greenQ12);
  TEST_ASSERT_TRUE(sample.redQ12 > 0u);
  TEST_ASSERT_TRUE(sample.redQ12 < 4095u);
  const StartupAnimationSample done = animation.sampleAt(700u);
  TEST_ASSERT_EQUAL_UINT16(0u, done.redQ12);
  TEST_ASSERT_EQUAL_UINT16(0u, done.greenQ12);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_startup_fade_finishes_within_750_ms);
  RUN_TEST(test_startup_fades_in_green_first);
  RUN_TEST(test_startup_crossfades_green_to_amber);
  RUN_TEST(test_startup_crossfades_amber_to_red);
  RUN_TEST(test_startup_reaches_full_amber_at_second_transition);
  RUN_TEST(test_startup_fades_red_to_off);
  return UNITY_END();
}
