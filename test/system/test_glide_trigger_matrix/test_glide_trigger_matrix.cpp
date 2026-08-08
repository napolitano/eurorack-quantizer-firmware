/**
 * @file test_glide_trigger_matrix.cpp
 * Fine-grained Glide, trigger and output-LED timing system tests.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "QuantizerTestRig.h"
#include "fmq/config/ProductConfig.h"

using namespace fmq;
using fmqtest::QuantizerTestRig;

void setUp(void) {}
void tearDown(void) {}

static ChannelConfig configForGlide(uint8_t glide) {
  ChannelConfig cfg = ChannelConfig::makeDefault();
  for (uint8_t i = 0; i < kNoteCount; ++i) cfg.notes[i] = true;
  cfg.sampleMode = SampleMode::TrackAndHold;
  cfg.glideAmount = glide;
  return cfg;
}

static void test_glide_zero_jumps_to_target_in_single_tick(void) {
  QuantizerTestRig rig;
  rig.state().channels[0].setConfig(configForGlide(0));
  rig.setGateA(true);
  rig.setCvVoltsA(0.0);
  rig.tick();
  rig.setCvVoltsA(5.0);
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(60, rig.last().quantization.channelA.nominalSemitones);
  TEST_ASSERT_EQUAL_INT16(60 * kSemitoneOneQ8_8,
                          rig.last().quantization.channelA.actualSemitones);
}

// FA-064..067 / TA-041..042: every nonzero glide must move monotonically.
static void test_all_glide_levels_1_through_11_are_monotonic_without_overshoot(void) {
  for (uint8_t glide = 1; glide <= config::kMaxGlideAmount; ++glide) {
    QuantizerTestRig rig;
    rig.state().channels[0].setConfig(configForGlide(glide));
    rig.setGateA(true);
    rig.setCvVoltsA(0.0);
    rig.tick();
    rig.setCvVoltsA(2.0);
    int16_t previous = rig.last().quantization.channelA.actualSemitones;
    for (uint16_t tick = 0; tick < 2000; ++tick) {
      rig.tick();
      const int16_t current = rig.last().quantization.channelA.actualSemitones;
      TEST_ASSERT_TRUE(current >= previous);
      TEST_ASSERT_TRUE(current <= 24 * kSemitoneOneQ8_8);
      previous = current;
    }
  }
}

static void test_glide_down_is_monotonic_without_undershoot(void) {
  for (uint8_t glide = 1; glide <= config::kMaxGlideAmount; ++glide) {
    QuantizerTestRig rig;
    rig.state().channels[0].setConfig(configForGlide(glide));
    rig.setGateA(true);
    rig.setCvVoltsA(5.0);
    // Let the upward glide establish a high starting point before reversing.
    rig.runFor(20000);
    int16_t previous = rig.last().quantization.channelA.actualSemitones;
    TEST_ASSERT_TRUE(previous > 12 * kSemitoneOneQ8_8);
    rig.setCvVoltsA(1.0);
    for (uint16_t tick = 0; tick < 2000; ++tick) {
      rig.tick();
      const int16_t current = rig.last().quantization.channelA.actualSemitones;
      TEST_ASSERT_TRUE(current <= previous);
      TEST_ASSERT_TRUE(current >= 12 * kSemitoneOneQ8_8);
      previous = current;
    }
  }
}

static uint32_t ticksToReach95Percent(uint8_t glide) {
  QuantizerTestRig rig;
  rig.state().channels[0].setConfig(configForGlide(glide));
  rig.setGateA(true);
  rig.setCvVoltsA(0.0);
  rig.tick();
  rig.setCvVoltsA(2.0);
  const int16_t threshold = static_cast<int16_t>(
      (24 * kSemitoneOneQ8_8 * 95) / 100);
  for (uint32_t ticks = 1; ticks < 20000u; ++ticks) {
    rig.tick();
    if (rig.last().quantization.channelA.actualSemitones >= threshold) return ticks;
  }
  return 20000u;
}

static void test_glide_95_percent_time_increases_with_setting(void) {
  uint32_t previousTicks = 0u;
  for (uint8_t glide = 1; glide <= config::kMaxGlideAmount; ++glide) {
    const uint32_t ticks = ticksToReach95Percent(glide);
    TEST_ASSERT_TRUE(ticks > previousTicks);
    TEST_ASSERT_TRUE(ticks < 20000u);
    previousTicks = ticks;
  }
}

// FA-070/071/075: trigger jack 5 ticks, visible LED 65 ticks from note change.
static void test_output_trigger_and_led_have_independent_exact_durations(void) {
  QuantizerTestRig rig;
  rig.state().channels[0].setConfig(configForGlide(0));
  rig.setGateA(true);
  rig.setCvVoltsA(1.0);
  rig.tick();
  rig.setCvVoltsA(2.0);

  uint8_t triggerTicks = 0;
  uint8_t ledTicks = 0;
  for (uint8_t i = 0; i < 70; ++i) {
    rig.tick();
    if (rig.last().triggerA) ++triggerTicks;
    if (rig.last().outputLedA) ++ledTicks;
  }
  TEST_ASSERT_EQUAL_UINT8(config::kOutputTriggerCvSamples, triggerTicks);
  TEST_ASSERT_EQUAL_UINT8(config::kOutputTriggerLedSamples, ledTicks);
}

// FA-068/069: no amount of DAC movement during glide may retrigger the jack.
static void test_every_glide_level_generates_only_one_trigger_pulse_per_target_change(void) {
  for (uint8_t glide = 0; glide <= config::kMaxGlideAmount; ++glide) {
    QuantizerTestRig rig;
    rig.state().channels[0].setConfig(configForGlide(glide));
    rig.setGateA(true);
    rig.setCvVoltsA(0.0);
    rig.tick();
    rig.setCvVoltsA(4.0);
    uint16_t highTicks = 0;
    for (uint16_t i = 0; i < 500; ++i) {
      rig.tick();
      if (rig.last().triggerA) ++highTicks;
    }
    TEST_ASSERT_EQUAL_UINT16(config::kOutputTriggerCvSamples, highTicks);
  }
}

static void test_ui_suppression_consumes_exactly_one_change(void) {
  QuantizerTestRig rig;
  rig.state().channels[0].setConfig(configForGlide(0));
  rig.setGateA(true);
  rig.setCvVoltsA(1.0);
  rig.tick();
  rig.state().channels[0].suppressNextOutputTrigger();
  rig.setCvVoltsA(2.0);
  rig.tick();
  TEST_ASSERT_FALSE(rig.last().triggerA);
  TEST_ASSERT_FALSE(rig.last().outputLedA);

  rig.setCvVoltsA(3.0);
  rig.tick();
  TEST_ASSERT_TRUE(rig.last().triggerA);
  TEST_ASSERT_TRUE(rig.last().outputLedA);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_glide_zero_jumps_to_target_in_single_tick);
  RUN_TEST(test_all_glide_levels_1_through_11_are_monotonic_without_overshoot);
  RUN_TEST(test_glide_down_is_monotonic_without_undershoot);
  RUN_TEST(test_glide_95_percent_time_increases_with_setting);
  RUN_TEST(test_output_trigger_and_led_have_independent_exact_durations);
  RUN_TEST(test_every_glide_level_generates_only_one_trigger_pulse_per_target_change);
  RUN_TEST(test_ui_suppression_consumes_exactly_one_change);
  return UNITY_END();
}
