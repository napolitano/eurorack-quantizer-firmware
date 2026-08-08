/**
 * @file test_channel_matrix.cpp
 * Two-channel interaction and independence tests at the virtual module boundary.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "QuantizerTestRig.h"
#include "TestScale.h"
#include "fmq/config/ProductConfig.h"

using namespace fmq;
using fmqtest::QuantizerTestRig;

void setUp(void) {}
void tearDown(void) {}

static ChannelConfig chromaticConfig(void) {
  ChannelConfig cfg = ChannelConfig::makeDefault();
  for (uint8_t i = 0; i < kNoteCount; ++i) cfg.notes[i] = true;
  cfg.glideAmount = 0;
  return cfg;
}

// FA-001..003: both channels process in parallel and independently.
static void test_channels_quantize_different_inputs_in_same_tick(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = chromaticConfig();
  rig.state().channels[0].setConfig(cfg);
  rig.state().channels[1].setConfig(cfg);
  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvVoltsA(1.0);
  rig.setCvVoltsB(2.0);
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(12, rig.last().quantization.channelA.nominalSemitones);
  TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelB.nominalSemitones);
}

static void test_gate_a_does_not_change_channel_b_sample_state(void) {
  QuantizerTestRig rig;
  ChannelConfig a = chromaticConfig();
  ChannelConfig b = chromaticConfig();
  a.sampleMode = SampleMode::TrackAndHold;
  b.sampleMode = SampleMode::SampleAndHold;
  rig.state().channels[0].setConfig(a);
  rig.state().channels[1].setConfig(b);
  rig.setGateA(true);
  rig.setGateB(false);
  rig.setCvVoltsA(1.0);
  rig.setCvVoltsB(2.0);
  rig.tick();
  const int8_t heldB = rig.last().quantization.channelB.nominalSemitones;

  rig.setCvVoltsA(3.0);
  rig.setCvVoltsB(4.0);
  rig.runFor(20);
  TEST_ASSERT_EQUAL_INT8(36, rig.last().quantization.channelA.nominalSemitones);
  TEST_ASSERT_EQUAL_INT8(heldB, rig.last().quantization.channelB.nominalSemitones);
}

// FA-026..029: relative B sums A+B and clamps before quantization.
static void test_relative_b_clamps_sum_to_120_semitones(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = chromaticConfig();
  rig.state().channels[0].setConfig(cfg);
  rig.state().channels[1].setConfig(cfg);
  rig.state().channelBMode = PitchMode::Relative;
  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvVoltsA(8.0);
  rig.setCvVoltsB(8.0);
  rig.tick();
  TEST_ASSERT_EQUAL_INT8(96, rig.last().quantization.channelA.nominalSemitones);
  TEST_ASSERT_EQUAL_INT8(120, rig.last().quantization.channelB.nominalSemitones);
}

static void test_absolute_b_ignores_channel_a_input_changes(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = chromaticConfig();
  rig.state().channels[0].setConfig(cfg);
  rig.state().channels[1].setConfig(cfg);
  rig.state().channelBMode = PitchMode::Absolute;
  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvVoltsB(2.0);
  for (int voltsA = 0; voltsA <= 10; ++voltsA) {
    rig.setCvVoltsA(static_cast<double>(voltsA));
    rig.tick();
    TEST_ASSERT_EQUAL_INT8(24, rig.last().quantization.channelB.nominalSemitones);
  }
}

// Regression guard for hardware observation: an unpatched/0 V B input in
// Absolute mode must never mirror A through the digital signal path.
static void test_absolute_b_zero_input_never_follows_a_across_full_adc_range(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = chromaticConfig();
  rig.state().channels[0].setConfig(cfg);
  rig.state().channels[1].setConfig(cfg);
  rig.state().channelBMode = PitchMode::Absolute;
  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvRawB(0);

  for (uint16_t rawA = 0; rawA <= config::kAdcMaximumCode; ++rawA) {
    rig.setCvRawA(rawA);
    rig.tick();
    TEST_ASSERT_EQUAL_INT8(0, rig.last().quantization.channelB.nominalSemitones);
    TEST_ASSERT_EQUAL_INT16(0, rig.last().outputPitchB);
    TEST_ASSERT_EQUAL_UINT16(0, rig.last().dacCodeB);
  }
}

static void test_absolute_b_fixed_input_keeps_identical_dac_while_a_sweeps(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = chromaticConfig();
  rig.state().channels[0].setConfig(cfg);
  rig.state().channels[1].setConfig(cfg);
  rig.state().channelBMode = PitchMode::Absolute;
  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvVoltsB(2.0);
  rig.setCvRawA(0);
  rig.tick();
  const int8_t nominalB = rig.last().quantization.channelB.nominalSemitones;
  const SemitoneQ8_8 pitchB = rig.last().outputPitchB;
  const uint16_t dacB = rig.last().dacCodeB;

  for (uint16_t rawA = 0; rawA <= config::kAdcMaximumCode; ++rawA) {
    rig.setCvRawA(rawA);
    rig.tick();
    TEST_ASSERT_EQUAL_INT8(nominalB,
                           rig.last().quantization.channelB.nominalSemitones);
    TEST_ASSERT_EQUAL_INT16(pitchB, rig.last().outputPitchB);
    TEST_ASSERT_EQUAL_UINT16(dacB, rig.last().dacCodeB);
  }
}

static void test_absolute_b_sample_and_hold_ignores_a_activity(void) {
  QuantizerTestRig rig;
  ChannelConfig a = chromaticConfig();
  ChannelConfig b = chromaticConfig();
  b.sampleMode = SampleMode::SampleAndHold;
  rig.state().channels[0].setConfig(a);
  rig.state().channels[1].setConfig(b);
  rig.state().channelBMode = PitchMode::Absolute;
  rig.setGateA(true);
  rig.setGateB(false);
  rig.setCvVoltsB(3.0);

  // Capture B once.
  rig.tick();
  rig.setGateB(true);
  rig.tick();
  rig.setGateB(false);
  rig.tick();
  const int8_t heldB = rig.last().quantization.channelB.nominalSemitones;
  const uint16_t heldDacB = rig.last().dacCodeB;

  for (uint16_t rawA = 0; rawA <= config::kAdcMaximumCode; rawA += 7u) {
    rig.setCvRawA(rawA);
    rig.setCvVoltsB(8.0);  // must also be ignored until the next B edge
    rig.tick();
    TEST_ASSERT_EQUAL_INT8(heldB,
                           rig.last().quantization.channelB.nominalSemitones);
    TEST_ASSERT_EQUAL_UINT16(heldDacB, rig.last().dacCodeB);
  }
}

static void test_relative_b_with_zero_b_input_tracks_a_by_design(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = chromaticConfig();
  rig.state().channels[0].setConfig(cfg);
  rig.state().channels[1].setConfig(cfg);
  rig.state().channelBMode = PitchMode::Relative;
  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvRawB(0);

  for (int voltsA = 0; voltsA <= 10; ++voltsA) {
    rig.setCvVoltsA(static_cast<double>(voltsA));
    rig.tick();
    TEST_ASSERT_EQUAL_INT8(rig.last().quantization.channelA.nominalSemitones,
                           rig.last().quantization.channelB.nominalSemitones);
    TEST_ASSERT_EQUAL_UINT16(rig.last().dacCodeA, rig.last().dacCodeB);
  }
}

static void test_channel_a_is_independent_of_b_input_in_both_b_modes(void) {
  for (uint8_t mode = 0; mode < 2u; ++mode) {
    QuantizerTestRig rig;
    ChannelConfig cfg = chromaticConfig();
    rig.state().channels[0].setConfig(cfg);
    rig.state().channels[1].setConfig(cfg);
    rig.state().channelBMode =
        mode == 0u ? PitchMode::Absolute : PitchMode::Relative;
    rig.setGateA(true);
    rig.setGateB(true);
    rig.setCvVoltsA(4.0);
    rig.setCvRawB(0);
    rig.tick();
    const int8_t nominalA = rig.last().quantization.channelA.nominalSemitones;
    const uint16_t dacA = rig.last().dacCodeA;

    for (uint16_t rawB = 0; rawB <= config::kAdcMaximumCode; rawB += 5u) {
      rig.setCvRawB(rawB);
      rig.tick();
      TEST_ASSERT_EQUAL_INT8(nominalA,
                             rig.last().quantization.channelA.nominalSemitones);
      TEST_ASSERT_EQUAL_UINT16(dacA, rig.last().dacCodeA);
    }
  }
}

// FA-030: channel configurations may differ without cross-contamination.
static void test_channels_use_separate_scales_and_transposition(void) {
  QuantizerTestRig rig;
  ChannelConfig a = chromaticConfig();
  ChannelConfig b = chromaticConfig();
  const int majorPcs[] = {0, 2, 4, 5, 7, 9, 11};
  fmqtest::makeScale(b.notes, majorPcs, 7);
  b.postShift = 1;
  rig.state().channels[0].setConfig(a);
  rig.state().channels[1].setConfig(b);
  rig.setGateA(true);
  rig.setGateB(true);
  // About 61 semitones; chromatic A -> C#, major B -> D; B DAC gets +1.
  rig.setCvRawA(520);
  rig.setCvRawB(520);
  rig.tick();
  TEST_ASSERT_TRUE(rig.last().quantization.channelA.nominalSemitones !=
                   rig.last().quantization.channelB.nominalSemitones);
  TEST_ASSERT_TRUE(rig.last().outputPitchB >
                   rig.last().quantization.channelB.nominalSemitones * kSemitoneOneQ8_8);
}

static void test_output_triggers_are_independent_between_channels(void) {
  QuantizerTestRig rig;
  ChannelConfig cfg = chromaticConfig();
  rig.state().channels[0].setConfig(cfg);
  rig.state().channels[1].setConfig(cfg);
  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvVoltsA(1.0);
  rig.setCvVoltsB(1.0);
  rig.tick();
  rig.setCvVoltsA(2.0);
  rig.tick();
  TEST_ASSERT_TRUE(rig.last().triggerA);
  TEST_ASSERT_FALSE(rig.last().triggerB);
}

static void test_retro_arpeggiator_uses_each_channels_own_scale(void) {
  QuantizerTestRig rig;
  const int majorPcs[] = {0, 2, 4, 5, 7, 9, 11};
  const int minorPcs[] = {0, 2, 3, 5, 7, 8, 10};
  ChannelConfig a = chromaticConfig();
  ChannelConfig b = chromaticConfig();
  fmqtest::makeScale(a.notes, majorPcs, 7);
  fmqtest::makeScale(b.notes, minorPcs, 7);
  rig.state().channels[0].setConfig(a);
  rig.state().channels[1].setConfig(b);
  rig.setGateA(true);
  rig.setGateB(true);
  rig.setCvVoltsA(5.0);
  rig.setCvVoltsB(5.0);
  rig.arpeggiator().setEnabled(true, rig.nowMs());
  rig.tick();
  rig.runFor(config::kRetroArpStepMs - 1u);
  rig.tick();
  // Same root, different scale: A major third, B minor third.
  TEST_ASSERT_TRUE(rig.last().outputPitchA > rig.last().outputPitchB);
  TEST_ASSERT_EQUAL_INT16(kSemitoneOneQ8_8,
      static_cast<int16_t>(rig.last().outputPitchA - rig.last().outputPitchB));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_channels_quantize_different_inputs_in_same_tick);
  RUN_TEST(test_gate_a_does_not_change_channel_b_sample_state);
  RUN_TEST(test_relative_b_clamps_sum_to_120_semitones);
  RUN_TEST(test_absolute_b_ignores_channel_a_input_changes);
  RUN_TEST(test_absolute_b_zero_input_never_follows_a_across_full_adc_range);
  RUN_TEST(test_absolute_b_fixed_input_keeps_identical_dac_while_a_sweeps);
  RUN_TEST(test_absolute_b_sample_and_hold_ignores_a_activity);
  RUN_TEST(test_relative_b_with_zero_b_input_tracks_a_by_design);
  RUN_TEST(test_channel_a_is_independent_of_b_input_in_both_b_modes);
  RUN_TEST(test_channels_use_separate_scales_and_transposition);
  RUN_TEST(test_output_triggers_are_independent_between_channels);
  RUN_TEST(test_retro_arpeggiator_uses_each_channels_own_scale);
  return UNITY_END();
}
