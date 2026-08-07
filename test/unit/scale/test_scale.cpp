/**
 * @file test_scale.cpp
 * Host regression or unit tests for scale behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/ScaleMath.h"
#include "TestScale.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

using fmqtest::makeScale;

static void test_empty_scale_has_no_notes(void) {
  bool notes[12];
  makeScale(notes, nullptr, 0);
  TEST_ASSERT_FALSE(scaleHasAnyNote(notes));
}

static void test_next_note_up_within_octave(void) {
  const int pcs[] = {0, 4, 7};  // C major triad
  bool notes[12];
  makeScale(notes, pcs, 3);
  // From C (0), next up is E (4).
  TEST_ASSERT_EQUAL_INT8(4, getNextSelectedNote(notes, 0, ScaleDirection::Up));
  // From E (4), next up is G (7).
  TEST_ASSERT_EQUAL_INT8(7, getNextSelectedNote(notes, 4, ScaleDirection::Up));
  // From G (7), next up wraps to next octave C (12).
  TEST_ASSERT_EQUAL_INT8(12, getNextSelectedNote(notes, 7, ScaleDirection::Up));
}

static void test_next_note_down(void) {
  const int pcs[] = {0, 4, 7};
  bool notes[12];
  makeScale(notes, pcs, 3);
  // From G (7) down is E (4).
  TEST_ASSERT_EQUAL_INT8(4, getNextSelectedNote(notes, 7, ScaleDirection::Down));
  // From C (12) down is G (7).
  TEST_ASSERT_EQUAL_INT8(7, getNextSelectedNote(notes, 12, ScaleDirection::Down));
}

static void test_boundaries_clamp(void) {
  const int pcs[] = {0};  // only C selected
  bool notes[12];
  makeScale(notes, pcs, 1);
  // Searching up from the top C (120) has nowhere to go -> clamps at 120.
  TEST_ASSERT_EQUAL_INT8(120, getNextSelectedNote(notes, 120, ScaleDirection::Up));
  // Searching down from 0 clamps at 0.
  TEST_ASSERT_EQUAL_INT8(0, getNextSelectedNote(notes, 0, ScaleDirection::Down));
}

static void test_step_in_scale(void) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};  // C major scale
  bool notes[12];
  makeScale(notes, pcs, 7);
  // Up two scale degrees from C (0): C -> D (2) -> E (4).
  TEST_ASSERT_EQUAL_INT8(4, stepInScale(notes, 0, 2));
  // Down one scale degree from C (0) clamps at 0 (nothing below in range).
  TEST_ASSERT_EQUAL_INT8(0, stepInScale(notes, 0, -1));
  // Zero steps returns the input.
  TEST_ASSERT_EQUAL_INT8(7, stepInScale(notes, 7, 0));
}

static void test_step_in_empty_scale_is_identity(void) {
  bool notes[12];
  makeScale(notes, nullptr, 0);
  TEST_ASSERT_EQUAL_INT8(42, stepInScale(notes, 42, 3));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_scale_has_no_notes);
  RUN_TEST(test_next_note_up_within_octave);
  RUN_TEST(test_next_note_down);
  RUN_TEST(test_boundaries_clamp);
  RUN_TEST(test_step_in_scale);
  RUN_TEST(test_step_in_empty_scale_is_identity);
  return UNITY_END();
}
