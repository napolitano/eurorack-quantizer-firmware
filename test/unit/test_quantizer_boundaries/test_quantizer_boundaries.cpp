/**
 * @file test_quantizer_boundaries.cpp
 * Fine-grained boundary and exhaustive tests for pitch quantization.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/Quantizer.h"
#include "TestScale.h"

using namespace fmq;
using fmqtest::makeScale;
using fmqtest::semis;

void setUp(void) {}
void tearDown(void) {}

static void clearScale(bool notes[kNoteCount]) {
  for (uint8_t i = 0; i < kNoteCount; ++i) notes[i] = false;
}

// FA-015..019: empty scale is defined and cannot loop forever.
static void test_empty_scale_returns_zero_across_full_input_range(void) {
  bool notes[kNoteCount];
  clearScale(notes);
  for (int input = 0; input <= kMaxSemitone; ++input) {
    Hysteresis h;
    TEST_ASSERT_EQUAL_INT8(0, h.quantize(semis(input), notes));
  }
}

// FA-018: a fresh channel has no previous note, so an exact half-semitone tie
// must use the normal upward tie-break before hysteresis becomes active.
static void test_fresh_chromatic_half_semitone_tie_rounds_up(void) {
  bool notes[kNoteCount];
  for (uint8_t i = 0; i < kNoteCount; ++i) notes[i] = true;
  Hysteresis h;
  TEST_ASSERT_EQUAL_INT8(1, h.quantize(kHalfSemitoneQ8_8, notes));
}

// FA-017..019: a single selected pitch class is found safely over all octaves.
static void test_each_single_pitch_class_quantizes_safely_over_full_range(void) {
  for (uint8_t pc = 0; pc < kNoteCount; ++pc) {
    bool notes[kNoteCount];
    clearScale(notes);
    notes[pc] = true;
    for (int input = 0; input <= kMaxSemitone; ++input) {
      Hysteresis h;
      const int8_t output = h.quantize(semis(input), notes);
      TEST_ASSERT_TRUE(output >= 0);
      TEST_ASSERT_TRUE(output <= kMaxSemitone);
      TEST_ASSERT_EQUAL_UINT8(pc, static_cast<uint8_t>(output % kNoteCount));
    }
  }
}

// FA-019: both hard pitch boundaries terminate and clamp to legal notes.
static void test_sparse_scale_respects_zero_and_120_boundaries(void) {
  const int pcs[] = {2, 7};
  bool notes[kNoteCount];
  makeScale(notes, pcs, 2);

  Hysteresis low;
  TEST_ASSERT_EQUAL_INT8(2, low.quantize(0, notes));

  Hysteresis high;
  TEST_ASSERT_EQUAL_INT8(115, high.quantize(semis(120), notes));
}

// FA-020..023: widened hysteresis band holds until midpoint + 0.4 semitone.
static void test_hysteresis_upper_boundary_is_inclusive_then_releases(void) {
  bool notes[kNoteCount];
  for (uint8_t i = 0; i < kNoteCount; ++i) notes[i] = true;
  Hysteresis h;
  TEST_ASSERT_EQUAL_INT8(60, h.quantize(semis(60), notes));

  const SemitoneQ8_8 upper = static_cast<SemitoneQ8_8>(
      60 * kSemitoneOneQ8_8 + kHalfSemitoneQ8_8 + 102);
  TEST_ASSERT_EQUAL_INT8(60, h.quantize(upper, notes));
  TEST_ASSERT_EQUAL_INT8(61, h.quantize(static_cast<SemitoneQ8_8>(upper + 1), notes));
}

static void test_hysteresis_lower_boundary_is_inclusive_then_releases(void) {
  bool notes[kNoteCount];
  for (uint8_t i = 0; i < kNoteCount; ++i) notes[i] = true;
  Hysteresis h;
  TEST_ASSERT_EQUAL_INT8(60, h.quantize(semis(60), notes));

  const SemitoneQ8_8 lower = static_cast<SemitoneQ8_8>(
      60 * kSemitoneOneQ8_8 - kHalfSemitoneQ8_8 - 102);
  TEST_ASSERT_EQUAL_INT8(60, h.quantize(lower, notes));
  TEST_ASSERT_EQUAL_INT8(59, h.quantize(static_cast<SemitoneQ8_8>(lower - 1), notes));
}

// FA-023/024: removing the held note invalidates hysteresis immediately.
static void test_disabling_held_note_forces_immediate_requantization(void) {
  bool notes[kNoteCount];
  for (uint8_t i = 0; i < kNoteCount; ++i) notes[i] = true;
  Hysteresis h;
  TEST_ASSERT_EQUAL_INT8(60, h.quantize(semis(60), notes));
  notes[0] = false;  // remove C in every octave, including MIDI-like note 60
  TEST_ASSERT_EQUAL_INT8(61, h.quantize(semis(60), notes));
}

// Stress all 4096 12-bit scale masks with representative boundary inputs.
static void test_all_nonempty_scale_masks_terminate_with_selected_note(void) {
  const SemitoneQ8_8 probes[] = {0, semis(0.5), semis(59.5), semis(60),
                                 semis(60.5), semis(119.5), semis(120)};
  for (uint16_t mask = 1; mask < 4096u; ++mask) {
    bool notes[kNoteCount];
    for (uint8_t pc = 0; pc < kNoteCount; ++pc) {
      notes[pc] = (mask & static_cast<uint16_t>(1u << pc)) != 0u;
    }
    for (const SemitoneQ8_8 probe : probes) {
      Hysteresis h;
      const int8_t output = h.quantize(probe, notes);
      TEST_ASSERT_TRUE(output >= 0 && output <= kMaxSemitone);
      TEST_ASSERT_TRUE(notes[static_cast<uint8_t>(output) % kNoteCount]);
    }
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_scale_returns_zero_across_full_input_range);
  RUN_TEST(test_fresh_chromatic_half_semitone_tie_rounds_up);
  RUN_TEST(test_each_single_pitch_class_quantizes_safely_over_full_range);
  RUN_TEST(test_sparse_scale_respects_zero_and_120_boundaries);
  RUN_TEST(test_hysteresis_upper_boundary_is_inclusive_then_releases);
  RUN_TEST(test_hysteresis_lower_boundary_is_inclusive_then_releases);
  RUN_TEST(test_disabling_held_note_forces_immediate_requantization);
  RUN_TEST(test_all_nonempty_scale_masks_terminate_with_selected_note);
  return UNITY_END();
}
