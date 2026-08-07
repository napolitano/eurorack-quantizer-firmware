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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_disabled_mode_is_transparent);
  RUN_TEST(test_major_scale_uses_root_third_and_fifth);
  RUN_TEST(test_minor_scale_uses_minor_third);
  RUN_TEST(test_arpeggio_clamps_at_top_of_pitch_range);
  return UNITY_END();
}
