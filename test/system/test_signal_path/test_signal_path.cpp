/**
 * @file test_signal_path.cpp
 * System tests for the quantizer's observable input/output signal behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/config/ProductConfig.h"
#include "fmq/domain/FixedPoint.h"
#include "QuantizerTestRig.h"
#include "TestScale.h"

using namespace fmq;
using fmqtest::QuantizerTestRig;

void setUp(void) {}
void tearDown(void) {}

static void setChromatic(ChannelConfig &config) {
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    config.notes[i] = true;
  }
}

static void setMajor(ChannelConfig &config) {
  const int notes[] = {0, 2, 4, 5, 7, 9, 11};
  fmqtest::makeScale(config.notes, notes, 7);
}

// FA-036..040: the hardware-normalised HIGH gate makes Track-and-Hold follow
// CV continuously; LOW holds the last sampled pitch.
static void test_track_and_hold_follows_high_and_holds_low(void) {
  QuantizerTestRig rig;
  ChannelConfig config = ChannelConfig::makeDefault();
  setChromatic(config);
  config.sampleMode = SampleMode::TrackAndHold;
  rig.state().channels[kChannelAIndex].setConfig(config);

  rig.setGateA(true);
  rig.setCvVoltsA(1.0);  // 12 semitones
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
  TEST_ASSERT_TRUE(rig.last().inputLedA);

  rig.setCvVoltsA(2.0);  // 24 semitones
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelA.nominalSemitones);

  rig.setGateA(false);
  rig.setCvVoltsA(3.0);
  rig.runFor(10);
  TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelA.nominalSemitones);
}

// FA-041..043: Sample-and-Hold accepts exactly one new pitch per rising edge.
static void test_sample_and_hold_only_samples_on_rising_edge(void) {
  QuantizerTestRig rig;
  ChannelConfig config = ChannelConfig::makeDefault();
  setChromatic(config);
  config.sampleMode = SampleMode::SampleAndHold;
  rig.state().channels[kChannelAIndex].setConfig(config);

  rig.setGateA(false);
  rig.setCvVoltsA(1.0);
  rig.tick();  // defined first sample

  rig.setCvVoltsA(2.0);
  rig.runFor(5);
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);

  rig.setGateA(true);
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelA.nominalSemitones);

  rig.setCvVoltsA(3.0);
  rig.runFor(5);
  TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelA.nominalSemitones);

  rig.setGateA(false);
  rig.tick();
  rig.setGateA(true);
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(36, rig.last().quantization.channelA.nominalSemitones);
}

// FA-044..048 / RF-001: Track delay starts once at the opening edge and then
// stops blocking continuous tracking for the remainder of the HIGH gate.
static void test_track_delay_runs_once_then_tracks_continuously(void) {
  QuantizerTestRig rig;
  ChannelConfig config = ChannelConfig::makeDefault();
  setChromatic(config);
  config.sampleMode = SampleMode::TrackAndHold;
  config.triggerDelayAmount = 3;
  rig.state().channels[kChannelAIndex].setConfig(config);

  rig.setGateA(false);
  rig.setCvVoltsA(1.0);
  // The channel's defined first sample also honours the configured delay.
  rig.runFor(4);
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);

  rig.setCvVoltsA(2.0);
  rig.setGateA(true);
  rig.tick();  // t = 0 ms relative to the edge
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
  rig.tick();  // +1 ms
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
  rig.tick();  // +2 ms
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
  rig.tick();  // +3 ms: configured delay has elapsed
  TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelA.nominalSemitones);

  rig.setCvVoltsA(3.0);
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(36, rig.last().quantization.channelA.nominalSemitones);
}

// FA-070..071: one target-note change produces a five-tick CV trigger pulse.
static void test_output_trigger_is_exactly_five_control_ticks(void) {
  QuantizerTestRig rig;
  ChannelConfig config = ChannelConfig::makeDefault();
  setChromatic(config);
  rig.state().channels[kChannelAIndex].setConfig(config);

  rig.setGateA(true);
  rig.setCvVoltsA(1.0);
  rig.tick();
  rig.setCvVoltsA(2.0);

  uint8_t highTicks = 0;
  for (uint8_t i = 0; i < 10; ++i) {
    rig.tick();
    if (rig.last().triggerA) {
      ++highTicks;
    }
  }
  TEST_ASSERT_EQUAL_UINT8(config::kOutputTriggerCvSamples, highTicks);
  TEST_ASSERT_FALSE(rig.last().triggerA);
  TEST_ASSERT_TRUE(rig.last().outputLedA);
}

// FA-064..069: DAC codes may move every millisecond during glide, but those
// intermediate analogue steps must not create additional musical triggers.
static void test_glide_updates_dac_without_retriggering(void) {
  QuantizerTestRig rig;
  ChannelConfig config = ChannelConfig::makeDefault();
  setChromatic(config);
  config.glideAmount = 5;
  rig.state().channels[kChannelAIndex].setConfig(config);

  rig.setGateA(true);
  rig.setCvVoltsA(0.0);
  rig.tick();
  rig.setCvVoltsA(2.0);

  uint16_t previousDac = rig.last().dacCodeA;
  uint8_t triggerHighTicks = 0;
  uint16_t dacChanges = 0;
  for (uint16_t i = 0; i < 100; ++i) {
    rig.tick();
    if (rig.last().triggerA) {
      ++triggerHighTicks;
    }
    if (rig.last().dacCodeA != previousDac) {
      ++dacChanges;
      TEST_ASSERT_TRUE(rig.last().dacCodeA > previousDac);
      previousDac = rig.last().dacCodeA;
    }
  }

  TEST_ASSERT_EQUAL_UINT8(config::kOutputTriggerCvSamples, triggerHighTicks);
  TEST_ASSERT_TRUE(dacChanges > config::kOutputTriggerCvSamples);
}

// FA-026..029: Channel B relative mode quantizes the sum of the two CV inputs.
static void test_relative_channel_b_uses_sum_of_real_adc_inputs(void) {
  QuantizerTestRig rig;
  ChannelConfig config = ChannelConfig::makeDefault();
  setChromatic(config);
  rig.state().channels[kChannelAIndex].setConfig(config);
  rig.state().channels[kChannelBIndex].setConfig(config);
  rig.state().channelBMode = PitchMode::Relative;

  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvVoltsA(1.0);
  rig.setCvVoltsB(0.5);
  rig.tick();

  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
  TEST_ASSERT_EQUAL_INT8(18, rig.last().quantization.channelB.nominalSemitones);
}

// Retro Arpeggiator is intentionally downstream of the quantizer trigger
// detector. Its pitch steps alter the DAC output but do not synthesize trigger
// pulses on the normal output-trigger jack.
static void test_retro_arpeggiator_changes_dac_without_output_triggers(void) {
  QuantizerTestRig rig;
  ChannelConfig config = ChannelConfig::makeDefault();
  setMajor(config);
  rig.state().channels[kChannelAIndex].setConfig(config);
  rig.setGateA(true);
  rig.setCvVoltsA(5.0);  // C5 / 60 semitones ideally
  rig.arpeggiator().setEnabled(true, rig.nowMs());

  rig.tick();
  const uint16_t rootCode = rig.last().dacCodeA;
  TEST_ASSERT_FALSE(rig.last().triggerA);

  rig.runFor(config::kRetroArpStepMs - 1u);
  rig.tick();
  const uint16_t thirdCode = rig.last().dacCodeA;
  TEST_ASSERT_TRUE(thirdCode > rootCode);
  TEST_ASSERT_FALSE(rig.last().triggerA);

  rig.runFor(config::kRetroArpStepMs - 1u);
  rig.tick();
  const uint16_t fifthCode = rig.last().dacCodeA;
  TEST_ASSERT_TRUE(fifthCode > thirdCode);
  TEST_ASSERT_FALSE(rig.last().triggerA);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_track_and_hold_follows_high_and_holds_low);
  RUN_TEST(test_sample_and_hold_only_samples_on_rising_edge);
  RUN_TEST(test_track_delay_runs_once_then_tracks_continuously);
  RUN_TEST(test_output_trigger_is_exactly_five_control_ticks);
  RUN_TEST(test_glide_updates_dac_without_retriggering);
  RUN_TEST(test_relative_channel_b_uses_sum_of_real_adc_inputs);
  RUN_TEST(test_retro_arpeggiator_changes_dac_without_output_triggers);
  return UNITY_END();
}
