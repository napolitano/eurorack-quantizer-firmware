/**
 * @file test_startup.cpp
 * Host regression tests for all startup-animation sequences.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/StartupAnimation.h"
#include "fmq/config/LedConfig.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

namespace {

uint8_t countLit(const StartupAnimationSample &sample) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    if (sample.frame[i] != LedColor::Off && sample.intensityQ12[i] > 0) {
      ++count;
    }
  }
  return count;
}

void assertAllColour(const StartupAnimationSample &sample, LedColor expected) {
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    TEST_ASSERT_EQUAL(expected, sample.frame[i]);
  }
}

}  // namespace

static void test_color_fade_keeps_original_green_red_amber_order(void) {
  StartupAnimation animation(StartupSequence::ColorFade);
  const uint32_t phase = config::kStartupFadeInMs +
                         config::kStartupFadeOutMs +
                         config::kStartupInterColorPauseMs;

  assertAllColour(animation.sampleAt(config::kStartupFadeInMs / 2u),
                  LedColor::Green);
  assertAllColour(animation.sampleAt(phase + config::kStartupFadeInMs / 2u),
                  LedColor::Red);
  assertAllColour(animation.sampleAt(2u * phase + config::kStartupFadeInMs / 2u),
                  LedColor::Amber);
}

static void test_glowworm_has_bright_head_and_dimming_tail(void) {
  StartupAnimation animation(StartupSequence::Glowworm);
  const StartupAnimationSample sample =
      animation.sampleAt(5u * config::kGlowwormStepMs);

  TEST_ASSERT_EQUAL_UINT8(config::kGlowwormTailLength, countLit(sample));
  TEST_ASSERT_EQUAL(LedColor::Green, sample.frame[5]);
  TEST_ASSERT_EQUAL_UINT16(config::kGlowwormTailIntensityQ12[0],
                           sample.intensityQ12[5]);
  TEST_ASSERT_TRUE(sample.intensityQ12[4] < sample.intensityQ12[5]);
  TEST_ASSERT_TRUE(sample.intensityQ12[3] < sample.intensityQ12[4]);
}

static void test_glowworm_cycles_green_red_amber(void) {
  StartupAnimation animation(StartupSequence::Glowworm);
  const uint32_t colourDuration =
      static_cast<uint32_t>(config::kGlowwormStepMs) * kNoteCount;
  TEST_ASSERT_EQUAL(LedColor::Green, animation.sampleAt(0).frame[0]);
  TEST_ASSERT_EQUAL(LedColor::Red,
                    animation.sampleAt(colourDuration).frame[0]);
  TEST_ASSERT_EQUAL(LedColor::Amber,
                    animation.sampleAt(2u * colourDuration).frame[0]);
}

static void test_cog_uses_pairs_and_rotates(void) {
  StartupAnimation animation(StartupSequence::Cog);
  const StartupAnimationSample first = animation.sampleAt(0);
  TEST_ASSERT_EQUAL(LedColor::Red, first.frame[0]);
  TEST_ASSERT_EQUAL(LedColor::Red, first.frame[1]);
  TEST_ASSERT_EQUAL(LedColor::Green, first.frame[2]);
  TEST_ASSERT_EQUAL(LedColor::Green, first.frame[3]);
  TEST_ASSERT_EQUAL(LedColor::Amber, first.frame[4]);
  TEST_ASSERT_EQUAL(LedColor::Amber, first.frame[5]);

  const StartupAnimationSample next =
      animation.sampleAt(config::kCogStepMs);
  TEST_ASSERT_EQUAL(first.frame[0], next.frame[1]);
  TEST_ASSERT_EQUAL(first.frame[1], next.frame[2]);
}

static void test_sparkles_use_configured_short_duration(void) {
  StartupAnimation animation(StartupSequence::Sparkles);
  TEST_ASSERT_EQUAL_UINT32(config::kSparklesDurationMs, animation.durationMs());
  TEST_ASSERT_FALSE(animation.isDone(config::kSparklesDurationMs - 1u));
  TEST_ASSERT_TRUE(animation.isDone(config::kSparklesDurationMs));
  TEST_ASSERT_TRUE(countLit(animation.sampleAt(350u)) > 0u);
}

static void test_all_note_ring_sequences_respect_startup_duration_limit(void) {
  for (uint8_t index = 0; index < StartupAnimation::kSequenceCount; ++index) {
    const StartupAnimation animation(static_cast<StartupSequence>(index));
    TEST_ASSERT_TRUE(animation.durationMs() <=
                     config::kStartupRingMaximumDurationMs);
    TEST_ASSERT_FALSE(animation.isDone(animation.durationMs() - 1u));
    TEST_ASSERT_TRUE(animation.isDone(animation.durationMs()));
  }
}

static void test_sequence_count_is_four(void) {
  TEST_ASSERT_EQUAL_UINT8(4, StartupAnimation::kSequenceCount);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_color_fade_keeps_original_green_red_amber_order);
  RUN_TEST(test_glowworm_has_bright_head_and_dimming_tail);
  RUN_TEST(test_glowworm_cycles_green_red_amber);
  RUN_TEST(test_cog_uses_pairs_and_rotates);
  RUN_TEST(test_sparkles_use_configured_short_duration);
  RUN_TEST(test_all_note_ring_sequences_respect_startup_duration_limit);
  RUN_TEST(test_sequence_count_is_four);
  return UNITY_END();
}
