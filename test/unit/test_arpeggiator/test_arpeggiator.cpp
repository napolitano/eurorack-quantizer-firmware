/**
 * @file test_arpeggiator.cpp
 * Focused unit tests for the configurable scale-aware Arpeggiator.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "TestScale.h"
#include "fmq/application/Arpeggiator.h"

using namespace fmq;
using fmqtest::makeScale;
using fmqtest::semis;

void setUp(void) {}
void tearDown(void) {}

namespace {
void major(bool notes[kNoteCount]) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  makeScale(notes, pcs, 7);
}
void minor(bool notes[kNoteCount]) {
  const int pcs[] = {0, 2, 3, 5, 7, 8, 10};
  makeScale(notes, pcs, 7);
}
ArpeggiatorOutput tick(Arpeggiator &arp, const bool notes[kNoteCount],
                       uint32_t now, bool edge = false,
                       int base = 60) {
  return arp.process(semis(static_cast<double>(base)),
                     static_cast<int8_t>(base), notes, edge, now);
}
}

static void test_disabled_mode_is_transparent(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  const ArpeggiatorOutput out = tick(arp, notes, 1000u);
  TEST_ASSERT_EQUAL_INT16(semis(60.0), out.pitch);
  TEST_ASSERT_FALSE(out.stepAdvanced);
}

static void test_default_mode_reproduces_24ms_root_third_fifth(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  arp.setEnabled(true, 1000u);
  TEST_ASSERT_EQUAL_INT16(semis(60.0), tick(arp, notes, 1000u).pitch);
  TEST_ASSERT_EQUAL_INT16(semis(60.0), tick(arp, notes, 1023u).pitch);
  const ArpeggiatorOutput third = tick(arp, notes, 1024u);
  TEST_ASSERT_TRUE(third.stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(64.0), third.pitch);
  TEST_ASSERT_EQUAL_INT16(semis(67.0), tick(arp, notes, 1048u).pitch);
  TEST_ASSERT_EQUAL_INT16(semis(60.0), tick(arp, notes, 1072u).pitch);
}

static void test_minor_scale_uses_minor_third(void) {
  bool notes[kNoteCount]; minor(notes);
  Arpeggiator arp; arp.setEnabled(true, 0u);
  TEST_ASSERT_EQUAL_INT16(semis(63.0), tick(arp, notes, 24u).pitch);
  TEST_ASSERT_EQUAL_INT16(semis(67.0), tick(arp, notes, 48u).pitch);
}

static void test_top_range_is_clamped(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp; arp.setEnabled(true, 0u);
  (void)tick(arp, notes, 24u, false, 119);
  const ArpeggiatorOutput out = tick(arp, notes, 48u, false, 119);
  TEST_ASSERT_EQUAL_INT16(semis(120.0), out.pitch);
}

static void test_reset_mode_restarts_at_root_on_rising_edge(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.syncMode = ArpeggiatorSyncMode::Reset;
  arp.setConfig(cfg, 0u);
  TEST_ASSERT_EQUAL_INT16(semis(64.0), tick(arp, notes, 24u).pitch);
  const ArpeggiatorOutput reset = tick(arp, notes, 30u, true);
  TEST_ASSERT_TRUE(reset.stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(60.0), reset.pitch);
}

static void test_clock_x1_advances_on_external_edges(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.syncMode = ArpeggiatorSyncMode::Clock;
  cfg.rateIndex = 5u;  // x1
  arp.setConfig(cfg, 0u);
  TEST_ASSERT_TRUE(tick(arp, notes, 100u, true).stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(60.0), tick(arp, notes, 100u).pitch);
  TEST_ASSERT_TRUE(tick(arp, notes, 200u, true).stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(64.0), tick(arp, notes, 200u).pitch);
  TEST_ASSERT_TRUE(tick(arp, notes, 300u, true).stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(67.0), tick(arp, notes, 300u).pitch);
}

static void test_clock_x4_generates_three_substeps_between_edges(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.syncMode = ArpeggiatorSyncMode::Clock;
  cfg.rateIndex = 8u;  // x4
  arp.setConfig(cfg, 0u);
  (void)tick(arp, notes, 100u, true);  // first edge, period unknown
  (void)tick(arp, notes, 200u, true);  // establishes 100 ms period and advances
  TEST_ASSERT_EQUAL_INT16(semis(64.0), tick(arp, notes, 200u).pitch);
  TEST_ASSERT_TRUE(tick(arp, notes, 225u).stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(67.0), tick(arp, notes, 225u).pitch);
  TEST_ASSERT_TRUE(tick(arp, notes, 250u).stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(60.0), tick(arp, notes, 250u).pitch);
  TEST_ASSERT_TRUE(tick(arp, notes, 275u).stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(64.0), tick(arp, notes, 275u).pitch);
}

static void test_clock_divide_by_two_advances_every_second_edge_after_lock(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.syncMode = ArpeggiatorSyncMode::Clock;
  cfg.rateIndex = 4u;  // /2
  arp.setConfig(cfg, 0u);
  TEST_ASSERT_TRUE(tick(arp, notes, 100u, true).stepAdvanced);
  TEST_ASSERT_FALSE(tick(arp, notes, 200u, true).stepAdvanced);
  TEST_ASSERT_TRUE(tick(arp, notes, 300u, true).stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(64.0), tick(arp, notes, 300u).pitch);
}

static void test_down_pattern_starts_at_last_selected_position(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.pattern = ArpeggiatorPattern::Down;
  arp.setConfig(cfg, 0u);
  TEST_ASSERT_EQUAL_INT16(semis(67.0), tick(arp, notes, 0u).pitch);
  TEST_ASSERT_EQUAL_INT16(semis(64.0), tick(arp, notes, 24u).pitch);
}

static void test_step_trigger_flag_does_not_change_pitch_engine(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.stepTrigger = true;
  arp.setConfig(cfg, 0u);
  const ArpeggiatorOutput out = tick(arp, notes, 24u);
  TEST_ASSERT_TRUE(out.stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(64.0), out.pitch);
}

static void test_empty_scale_is_safe_and_transparent(void) {
  bool notes[kNoteCount] = {};
  Arpeggiator arp; arp.setEnabled(true, 0u);
  TEST_ASSERT_EQUAL_INT16(semis(60.0), tick(arp, notes, 24u).pitch);
}


static void test_max_swing_preserves_two_step_pair_duration(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.rateIndex = 3u;  // base 24 ms, pair duration 48 ms
  cfg.swing = 11u;    // approximately 66:34
  arp.setConfig(cfg, 0u);

  TEST_ASSERT_FALSE(tick(arp, notes, 16u).stepAdvanced);
  TEST_ASSERT_TRUE(tick(arp, notes, 17u).stepAdvanced);
  TEST_ASSERT_FALSE(tick(arp, notes, 47u).stepAdvanced);
  TEST_ASSERT_TRUE(tick(arp, notes, 48u).stepAdvanced);
}


static void test_random_pattern_is_stable_between_musical_steps(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.pattern = ArpeggiatorPattern::Random;
  cfg.length = 7u;
  cfg.rateIndex = 3u;  // 24 ms
  arp.setConfig(cfg, 0u);

  const SemitoneQ8_8 first = tick(arp, notes, 0u).pitch;
  for (uint32_t now = 1u; now < 24u; ++now) {
    const ArpeggiatorOutput out = tick(arp, notes, now);
    TEST_ASSERT_FALSE(out.stepAdvanced);
    TEST_ASSERT_EQUAL_INT16(first, out.pitch);
  }

  const ArpeggiatorOutput next = tick(arp, notes, 24u);
  TEST_ASSERT_TRUE(next.stepAdvanced);
}

static void test_random_pattern_restart_is_deterministic(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.pattern = ArpeggiatorPattern::Random;
  cfg.length = 7u;
  arp.setConfig(cfg, 100u);

  const SemitoneQ8_8 first = tick(arp, notes, 100u).pitch;
  (void)tick(arp, notes, 124u);
  (void)tick(arp, notes, 148u);
  arp.resetPhase(200u);
  const SemitoneQ8_8 restarted = tick(arp, notes, 200u).pitch;
  TEST_ASSERT_EQUAL_INT16(first, restarted);
}

static void test_time_wraparound_preserves_free_clock(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  const uint32_t start = 0xFFFFFFF0u;
  arp.setEnabled(true, start);
  TEST_ASSERT_EQUAL_INT16(semis(64.0),
                          tick(arp, notes, start + 24u).pitch);
}


static void test_clock_timestamp_uses_microsecond_period_for_x12_substeps(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.syncMode = ArpeggiatorSyncMode::Clock;
  cfg.rateIndex = 11u;  // x12
  arp.setConfig(cfg, 0u);

  const SemitoneQ8_8 base = semis(60.0);
  TEST_ASSERT_TRUE(arp.processTimed(base, 60, notes, 1u, 100000u,
                                   100u, 100000u).stepAdvanced);
  TEST_ASSERT_TRUE(arp.processTimed(base, 60, notes, 1u, 183333u,
                                   183u, 183333u).stepAdvanced);

  // 83,333 us / 12 = 6,944 us. The old millisecond path could only measure
  // this source as 83 ms; the timed path preserves the ISR-captured period.
  TEST_ASSERT_FALSE(arp.processTimed(base, 60, notes, 0u, 0u,
                                     190u, 190276u).stepAdvanced);
  const ArpeggiatorOutput substep = arp.processTimed(
      base, 60, notes, 0u, 0u, 190u, 190277u);
  TEST_ASSERT_TRUE(substep.stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(67.0), substep.pitch);
}

static void test_clock_edge_count_does_not_collapse_multiple_edges(void) {
  bool notes[kNoteCount]; major(notes);
  Arpeggiator arp;
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.syncMode = ArpeggiatorSyncMode::Clock;
  cfg.rateIndex = 5u;  // x1
  arp.setConfig(cfg, 0u);

  const SemitoneQ8_8 base = semis(60.0);
  (void)arp.processTimed(base, 60, notes, 1u, 100000u, 100u, 100000u);
  const ArpeggiatorOutput twoEdges = arp.processTimed(
      base, 60, notes, 2u, 300000u, 300u, 300000u);
  TEST_ASSERT_TRUE(twoEdges.stepAdvanced);
  TEST_ASSERT_EQUAL_INT16(semis(67.0), twoEdges.pitch);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_disabled_mode_is_transparent);
  RUN_TEST(test_default_mode_reproduces_24ms_root_third_fifth);
  RUN_TEST(test_minor_scale_uses_minor_third);
  RUN_TEST(test_top_range_is_clamped);
  RUN_TEST(test_reset_mode_restarts_at_root_on_rising_edge);
  RUN_TEST(test_clock_x1_advances_on_external_edges);
  RUN_TEST(test_clock_x4_generates_three_substeps_between_edges);
  RUN_TEST(test_clock_timestamp_uses_microsecond_period_for_x12_substeps);
  RUN_TEST(test_clock_edge_count_does_not_collapse_multiple_edges);
  RUN_TEST(test_clock_divide_by_two_advances_every_second_edge_after_lock);
  RUN_TEST(test_down_pattern_starts_at_last_selected_position);
  RUN_TEST(test_step_trigger_flag_does_not_change_pitch_engine);
  RUN_TEST(test_empty_scale_is_safe_and_transparent);
  RUN_TEST(test_max_swing_preserves_two_step_pair_duration);
  RUN_TEST(test_random_pattern_is_stable_between_musical_steps);
  RUN_TEST(test_random_pattern_restart_is_deterministic);
  RUN_TEST(test_time_wraparound_preserves_free_clock);
  return UNITY_END();
}
