/**
 * @file test_transposition_matrix.cpp
 * Matrix tests for pre-, scale- and post-transposition semantics.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/config/ProductConfig.h"
#include "fmq/domain/Quantizer.h"
#include "TestScale.h"

using namespace fmq;
using fmqtest::makeScale;
using fmqtest::semis;

void setUp(void) {}
void tearDown(void) {}

static ChannelOutput process(ChannelConfig config, SemitoneQ8_8 input) {
  config.sampleMode = SampleMode::Continuous;
  config.glideAmount = 0;
  QuantizerChannel channel;
  channel.setConfig(config);
  return channel.step(input, true);
}

static void setChromatic(ChannelConfig &config) {
  for (uint8_t i = 0; i < kNoteCount; ++i) config.notes[i] = true;
}

// FA-049..052: every supported pre-shift value is applied before quantization.
static void test_all_pre_shift_values_minus5_through_plus6(void) {
  for (int shift = config::kMinimumShift; shift <= config::kMaximumShift; ++shift) {
    ChannelConfig cfg = ChannelConfig::makeDefault();
    setChromatic(cfg);
    cfg.preShift = static_cast<int8_t>(shift);
    const ChannelOutput out = process(cfg, semis(60));
    TEST_ASSERT_EQUAL_INT8(60 + shift, out.nominalSemitones);
    TEST_ASSERT_EQUAL_INT16(semis(60 + shift), out.actualSemitones);
  }
}

static void test_pre_shift_clamps_at_both_pitch_boundaries(void) {
  ChannelConfig cfg = ChannelConfig::makeDefault();
  setChromatic(cfg);
  cfg.preShift = -5;
  TEST_ASSERT_EQUAL_INT8(0, process(cfg, semis(2)).nominalSemitones);
  cfg.preShift = 6;
  TEST_ASSERT_EQUAL_INT8(120, process(cfg, semis(118)).nominalSemitones);
}

// FA-053..056 / RF-002: signed scale shifts walk actual selected scale degrees.
static void test_all_scale_shift_values_walk_major_scale_both_directions(void) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  const int8_t expected[] = {52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71};
  for (int shift = -5; shift <= 6; ++shift) {
    ChannelConfig cfg = ChannelConfig::makeDefault();
    makeScale(cfg.notes, pcs, 7);
    cfg.scaleShift = static_cast<int8_t>(shift);
    const ChannelOutput out = process(cfg, semis(60));
    TEST_ASSERT_EQUAL_INT8(expected[shift + 5], out.nominalSemitones);
  }
}

// FA-057..060: post-shift changes DAC target without changing nominal scale note.
static void test_all_post_shift_values_preserve_nominal_note(void) {
  for (int shift = config::kMinimumShift; shift <= config::kMaximumShift; ++shift) {
    ChannelConfig cfg = ChannelConfig::makeDefault();
    setChromatic(cfg);
    cfg.postShift = static_cast<int8_t>(shift);
    const ChannelOutput out = process(cfg, semis(60));
    TEST_ASSERT_EQUAL_INT8(60, out.nominalSemitones);
    TEST_ASSERT_EQUAL_INT16(semis(60 + shift), out.actualSemitones);
  }
}

static void test_post_shift_clamps_output_but_not_nominal_note(void) {
  ChannelConfig cfg = ChannelConfig::makeDefault();
  setChromatic(cfg);
  cfg.postShift = -5;
  ChannelOutput low = process(cfg, semis(2));
  TEST_ASSERT_EQUAL_INT8(2, low.nominalSemitones);
  TEST_ASSERT_EQUAL_INT16(0, low.actualSemitones);

  cfg.postShift = 6;
  ChannelOutput high = process(cfg, semis(118));
  TEST_ASSERT_EQUAL_INT8(118, high.nominalSemitones);
  TEST_ASSERT_EQUAL_INT16(semis(120), high.actualSemitones);
}

// FA-049..060: order is pre -> quantize -> scale shift -> post shift.
static void test_combined_transpositions_follow_required_processing_order(void) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  ChannelConfig cfg = ChannelConfig::makeDefault();
  makeScale(cfg.notes, pcs, 7);
  cfg.preShift = 1;    // 60 -> 61, quantizes to 62 (D)
  cfg.scaleShift = 2;  // D -> F
  cfg.postShift = -1;  // F -> E at DAC
  const ChannelOutput out = process(cfg, semis(60));
  TEST_ASSERT_EQUAL_INT8(65, out.nominalSemitones);
  TEST_ASSERT_EQUAL_INT16(semis(64), out.actualSemitones);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_all_pre_shift_values_minus5_through_plus6);
  RUN_TEST(test_pre_shift_clamps_at_both_pitch_boundaries);
  RUN_TEST(test_all_scale_shift_values_walk_major_scale_both_directions);
  RUN_TEST(test_all_post_shift_values_preserve_nominal_note);
  RUN_TEST(test_post_shift_clamps_output_but_not_nominal_note);
  RUN_TEST(test_combined_transpositions_follow_required_processing_order);
  return UNITY_END();
}
