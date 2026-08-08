/**
 * @file test_retro_arpeggiator.cpp
 * Unit tests for the scale-aware Retro Arpeggiator feature.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/RetroArpeggiator.h"
#include "fmq/config/ProductConfig.h"
#include "TestScale.h"

using namespace fmq;
using fmqtest::makeScale;
using fmqtest::semis;

void setUp(void) {}
void tearDown(void) {}

static void test_disabled_mode_is_transparent(void) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  bool notes[kNoteCount];
  makeScale(notes, pcs, 7);

  RetroArpeggiator arp;
  TEST_ASSERT_EQUAL_INT16(semis(60.0), arp.process(semis(60.0), 60, notes, 1000));
}

static void test_major_scale_uses_root_third_and_fifth(void) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  bool notes[kNoteCount];
  makeScale(notes, pcs, 7);

  RetroArpeggiator arp;
  arp.setEnabled(true, 1000);

  TEST_ASSERT_EQUAL_INT16(semis(60.0), arp.process(semis(60.0), 60, notes, 1000));
  TEST_ASSERT_EQUAL_INT16(semis(64.0), arp.process(semis(60.0), 60, notes,
      1000 + config::kRetroArpStepMs));
  TEST_ASSERT_EQUAL_INT16(semis(67.0), arp.process(semis(60.0), 60, notes,
      1000 + 2u * config::kRetroArpStepMs));
  TEST_ASSERT_EQUAL_INT16(semis(60.0), arp.process(semis(60.0), 60, notes,
      1000 + 3u * config::kRetroArpStepMs));
}

static void test_minor_scale_uses_minor_third(void) {
  const int pcs[] = {0, 2, 3, 5, 7, 8, 10};
  bool notes[kNoteCount];
  makeScale(notes, pcs, 7);

  RetroArpeggiator arp;
  arp.setEnabled(true, 0);

  TEST_ASSERT_EQUAL_INT16(semis(63.0), arp.process(semis(60.0), 60, notes,
      config::kRetroArpStepMs));
  TEST_ASSERT_EQUAL_INT16(semis(67.0), arp.process(semis(60.0), 60, notes,
      2u * config::kRetroArpStepMs));
}

static void test_arpeggio_clamps_at_top_of_pitch_range(void) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  bool notes[kNoteCount];
  makeScale(notes, pcs, 7);

  RetroArpeggiator arp;
  arp.setEnabled(true, 0);

  TEST_ASSERT_EQUAL_INT16(semis(120.0), arp.process(semis(119.0), 119, notes,
      2u * config::kRetroArpStepMs));
}


static void test_step_boundary_changes_exactly_at_24_ms(void) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  bool notes[kNoteCount];
  makeScale(notes, pcs, 7);

  RetroArpeggiator arp;
  arp.setEnabled(true, 500);

  TEST_ASSERT_EQUAL_INT16(semis(60.0), arp.process(
      semis(60.0), 60, notes, 500 + config::kRetroArpStepMs - 1u));
  TEST_ASSERT_EQUAL_INT16(semis(64.0), arp.process(
      semis(60.0), 60, notes, 500 + config::kRetroArpStepMs));
}

static void test_d_major_uses_f_sharp_and_a(void) {
  const int pcs[] = {1, 2, 4, 6, 7, 9, 11};  // D major pitch classes
  bool notes[kNoteCount];
  makeScale(notes, pcs, 7);

  RetroArpeggiator arp;
  arp.setEnabled(true, 0);

  TEST_ASSERT_EQUAL_INT16(semis(62.0), arp.process(semis(62.0), 62, notes, 0));
  TEST_ASSERT_EQUAL_INT16(semis(66.0), arp.process(
      semis(62.0), 62, notes, config::kRetroArpStepMs));
  TEST_ASSERT_EQUAL_INT16(semis(69.0), arp.process(
      semis(62.0), 62, notes, 2u * config::kRetroArpStepMs));
}

static void test_major_pentatonic_uses_scale_degrees(void) {
  const int pcs[] = {0, 2, 4, 7, 9};
  bool notes[kNoteCount];
  makeScale(notes, pcs, 5);

  RetroArpeggiator arp;
  arp.setEnabled(true, 0);

  TEST_ASSERT_EQUAL_INT16(semis(64.0), arp.process(
      semis(60.0), 60, notes, config::kRetroArpStepMs));
  TEST_ASSERT_EQUAL_INT16(semis(69.0), arp.process(
      semis(60.0), 60, notes, 2u * config::kRetroArpStepMs));
}

static void test_empty_scale_never_changes_base_pitch(void) {
  bool notes[kNoteCount] = {};
  RetroArpeggiator arp;
  arp.setEnabled(true, 0);

  TEST_ASSERT_EQUAL_INT16(semis(60.0), arp.process(
      semis(60.0), 60, notes, config::kRetroArpStepMs));
  TEST_ASSERT_EQUAL_INT16(semis(60.0), arp.process(
      semis(60.0), 60, notes, 2u * config::kRetroArpStepMs));
}

static void test_time_wraparound_preserves_phase(void) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  bool notes[kNoteCount];
  makeScale(notes, pcs, 7);

  RetroArpeggiator arp;
  const uint32_t start = 0xFFFFFFF0u;
  arp.setEnabled(true, start);
  TEST_ASSERT_EQUAL_INT16(semis(64.0), arp.process(
      semis(60.0), 60, notes, start + config::kRetroArpStepMs));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_disabled_mode_is_transparent);
  RUN_TEST(test_major_scale_uses_root_third_and_fifth);
  RUN_TEST(test_minor_scale_uses_minor_third);
  RUN_TEST(test_arpeggio_clamps_at_top_of_pitch_range);
  RUN_TEST(test_step_boundary_changes_exactly_at_24_ms);
  RUN_TEST(test_d_major_uses_f_sharp_and_a);
  RUN_TEST(test_major_pentatonic_uses_scale_degrees);
  RUN_TEST(test_empty_scale_never_changes_base_pitch);
  RUN_TEST(test_time_wraparound_preserves_phase);
  return UNITY_END();
}
