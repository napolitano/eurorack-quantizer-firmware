/**
 * @file test_sample_timing_matrix.cpp
 * Millisecond-exact Track/Sample/Delay/status-LED system tests.
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

static void chromatic(ChannelConfig &cfg) {
  for (uint8_t i = 0; i < kNoteCount; ++i) cfg.notes[i] = true;
  cfg.glideAmount = 0;
}

static void primeAtOneVolt(QuantizerTestRig &rig, ChannelConfig cfg) {
  rig.state().channels[kChannelAIndex].setConfig(cfg);
  rig.setGateA(false);
  rig.setCvVoltsA(1.0);
  rig.runFor(static_cast<uint32_t>(cfg.triggerDelayAmount) + 1u);
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
}

// FA-044..048: all 12 Track delay values fire exactly on the configured tick.
static void test_track_delay_matrix_0_through_11(void) {
  for (uint8_t delay = 0; delay <= config::kMaxTriggerDelayAmount; ++delay) {
    QuantizerTestRig rig;
    ChannelConfig cfg = ChannelConfig::makeDefault();
    chromatic(cfg);
    cfg.sampleMode = SampleMode::TrackAndHold;
    cfg.triggerDelayAmount = delay;
    primeAtOneVolt(rig, cfg);

    rig.setCvVoltsA(2.0);
    rig.setGateA(true);
    for (uint8_t elapsed = 0; elapsed < delay; ++elapsed) {
      rig.tick();
      TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
    }
    rig.tick();
    TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelA.nominalSemitones);
    rig.setCvVoltsA(3.0);
    rig.tick();
    TEST_ASSERT_EQUAL_INT8(36, rig.last().quantization.channelA.nominalSemitones);
  }
}

// FA-041..048: all 12 Sample delay values sample exactly once after rising edge.
static void test_sample_delay_matrix_0_through_11(void) {
  for (uint8_t delay = 0; delay <= config::kMaxTriggerDelayAmount; ++delay) {
    QuantizerTestRig rig;
    ChannelConfig cfg = ChannelConfig::makeDefault();
    chromatic(cfg);
    cfg.sampleMode = SampleMode::SampleAndHold;
    cfg.triggerDelayAmount = delay;
    primeAtOneVolt(rig, cfg);

    rig.setCvVoltsA(2.0);
    rig.setGateA(true);
    for (uint8_t elapsed = 0; elapsed < delay; ++elapsed) {
      rig.tick();
      TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
    }
    rig.tick();
    TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelA.nominalSemitones);

    rig.setCvVoltsA(3.0);
    rig.runFor(20);
    TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelA.nominalSemitones);
  }
}

static void test_sample_static_high_never_resamples_without_new_edge(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = ChannelConfig::makeDefault();
  chromatic(cfg);
  cfg.sampleMode = SampleMode::SampleAndHold;
  rig.state().channels[kChannelAIndex].setConfig(cfg);
  rig.setGateA(true);
  rig.setCvVoltsA(1.0);
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
  for (int volts = 2; volts <= 9; ++volts) {
    rig.setCvVoltsA(static_cast<double>(volts));
    rig.runFor(5);
    TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
  }
}

static void test_track_input_led_stays_on_for_entire_high_gate(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = ChannelConfig::makeDefault();
  chromatic(cfg);
  cfg.sampleMode = SampleMode::TrackAndHold;
  rig.state().channels[kChannelAIndex].setConfig(cfg);
  rig.setGateA(true);
  for (uint16_t i = 0; i < 500; ++i) {
    rig.tick();
    TEST_ASSERT_TRUE(rig.last().inputLedA);
  }
}

static void test_track_input_led_expires_exactly_65_ticks_after_gate_low(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = ChannelConfig::makeDefault();
  chromatic(cfg);
  cfg.sampleMode = SampleMode::TrackAndHold;
  rig.state().channels[kChannelAIndex].setConfig(cfg);
  rig.setGateA(true);
  rig.tick();
  rig.setGateA(false);
  for (uint8_t i = 1; i < config::kOutputTriggerLedSamples; ++i) {
    rig.tick();
    TEST_ASSERT_TRUE(rig.last().inputLedA);
  }
  rig.tick();
  TEST_ASSERT_FALSE(rig.last().inputLedA);
}

static void test_sample_input_led_pulses_for_exactly_65_ticks_per_edge(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = ChannelConfig::makeDefault();
  chromatic(cfg);
  cfg.sampleMode = SampleMode::SampleAndHold;
  rig.state().channels[kChannelAIndex].setConfig(cfg);
  rig.setGateA(false);
  rig.tick();
  rig.setGateA(true);
  rig.tick();
  TEST_ASSERT_TRUE(rig.last().inputLedA);
  rig.setGateA(false);
  for (uint8_t i = 1; i < config::kOutputTriggerLedSamples; ++i) {
    rig.tick();
    TEST_ASSERT_TRUE(rig.last().inputLedA);
  }
  rig.tick();
  TEST_ASSERT_FALSE(rig.last().inputLedA);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_track_delay_matrix_0_through_11);
  RUN_TEST(test_sample_delay_matrix_0_through_11);
  RUN_TEST(test_sample_static_high_never_resamples_without_new_edge);
  RUN_TEST(test_track_input_led_stays_on_for_entire_high_gate);
  RUN_TEST(test_track_input_led_expires_exactly_65_ticks_after_gate_low);
  RUN_TEST(test_sample_input_led_pulses_for_exactly_65_ticks_per_edge);
  return UNITY_END();
}
