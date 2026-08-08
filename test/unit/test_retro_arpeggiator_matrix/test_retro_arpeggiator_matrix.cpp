/**
 * @file test_retro_arpeggiator_matrix.cpp
 * Exhaustive/matrix verification of the scale-aware Retro Arpeggiator.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/RetroArpeggiator.h"
#include "fmq/config/FactoryPresets.h"
#include "fmq/config/ProductConfig.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void notesFromMask(uint16_t mask, bool notes[kNoteCount]) {
  for (uint8_t pc = 0; pc < kNoteCount; ++pc) {
    notes[pc] = (mask & static_cast<uint16_t>(1u << pc)) != 0u;
  }
}

static uint8_t expectedOffset(const bool notes[kNoteCount], uint8_t rootPc,
                              uint8_t selectedDegreeOffset) {
  if (selectedDegreeOffset == 0u) return 0u;
  uint8_t passed = 0u;
  for (uint8_t offset = 1u; offset <= 24u; ++offset) {
    const uint8_t pc = static_cast<uint8_t>((rootPc + offset) % kNoteCount);
    if (notes[pc]) {
      ++passed;
      if (passed == selectedDegreeOffset) return offset;
    }
  }
  return 0u;
}

static SemitoneQ8_8 qPitch(int semitone) {
  return static_cast<SemitoneQ8_8>(semitone * kSemitoneOneQ8_8);
}

// All twelve factory scales must generate root/third/fifth deterministically.
static void test_all_factory_presets_match_scale_degree_reference(void) {
  constexpr uint8_t degreeOffsets[3] = {0, 2, 4};
  for (uint8_t preset = 0; preset < config::kFactoryPresetCount; ++preset) {
    bool notes[kNoteCount];
    notesFromMask(config::kFactoryScalePresets[preset].noteMask, notes);
    RetroArpeggiator arp;
    arp.setEnabled(true, 100u);
    for (uint8_t step = 0; step < 3; ++step) {
      const uint8_t expected = expectedOffset(notes, 0, degreeOffsets[step]);
      const SemitoneQ8_8 actual = arp.process(
          qPitch(60), 60, notes,
          static_cast<uint32_t>(100u + step * config::kRetroArpStepMs));
      TEST_ASSERT_EQUAL_INT16(qPitch(60 + expected), actual);
    }
  }
}

// Transposition must preserve the interval logic for all twelve root classes.
static void test_all_twelve_root_pitch_classes_use_correct_scale_degrees(void) {
  constexpr uint16_t majorMask = 0x0AB5u;
  constexpr uint8_t degreeOffsets[3] = {0, 2, 4};
  bool notes[kNoteCount];
  notesFromMask(majorMask, notes);

  for (uint8_t root = 0; root < kNoteCount; ++root) {
    RetroArpeggiator arp;
    arp.setEnabled(true, 0u);
    const int base = 48 + root;
    for (uint8_t step = 0; step < 3; ++step) {
      const uint8_t offset = expectedOffset(notes, root, degreeOffsets[step]);
      const SemitoneQ8_8 actual = arp.process(
          qPitch(base), static_cast<int8_t>(base), notes,
          static_cast<uint32_t>(step * config::kRetroArpStepMs));
      TEST_ASSERT_EQUAL_INT16(qPitch(base + offset), actual);
    }
  }
}

static void test_step_boundary_is_exact_for_many_cycles(void) {
  bool notes[kNoteCount];
  notesFromMask(0x0AB5u, notes);
  RetroArpeggiator arp;
  arp.setEnabled(true, 1000u);
  const SemitoneQ8_8 expected[3] = {qPitch(60), qPitch(64), qPitch(67)};

  for (uint16_t cycle = 0; cycle < 100; ++cycle) {
    for (uint8_t step = 0; step < 3; ++step) {
      const uint32_t boundary = static_cast<uint32_t>(
          1000u + (static_cast<uint32_t>(cycle) * 3u + step) * config::kRetroArpStepMs);
      TEST_ASSERT_EQUAL_INT16(expected[step], arp.process(qPitch(60), 60, notes, boundary));
      if (step > 0 || cycle > 0) {
        const uint8_t previousStep = static_cast<uint8_t>((step + 2u) % 3u);
        TEST_ASSERT_EQUAL_INT16(expected[previousStep],
                                arp.process(qPitch(60), 60, notes, boundary - 1u));
      }
    }
  }
}

static void test_reenable_restarts_phase_at_root(void) {
  bool notes[kNoteCount];
  notesFromMask(0x0AB5u, notes);
  RetroArpeggiator arp;
  arp.setEnabled(true, 0u);
  TEST_ASSERT_EQUAL_INT16(qPitch(64), arp.process(qPitch(60), 60, notes,
                                                 config::kRetroArpStepMs));
  arp.setEnabled(false, 200u);
  TEST_ASSERT_EQUAL_INT16(qPitch(60), arp.process(qPitch(60), 60, notes, 500u));
  arp.setEnabled(true, 500u);
  TEST_ASSERT_EQUAL_INT16(qPitch(60), arp.process(qPitch(60), 60, notes, 500u));
}

static void test_toggle_restarts_phase_when_enabling(void) {
  bool notes[kNoteCount];
  notesFromMask(0x0AB5u, notes);
  RetroArpeggiator arp;
  arp.toggle(77u);
  TEST_ASSERT_TRUE(arp.enabled());
  TEST_ASSERT_EQUAL_INT16(qPitch(60), arp.process(qPitch(60), 60, notes, 77u));
  arp.toggle(100u);
  TEST_ASSERT_FALSE(arp.enabled());
}

// Sparse scales are deliberately tested to prevent hangs/out-of-range results.
static void test_one_note_scale_is_bounded_and_deterministic(void) {
  bool notes[kNoteCount] = {};
  notes[0] = true;
  RetroArpeggiator arp;
  arp.setEnabled(true, 0u);
  TEST_ASSERT_EQUAL_INT16(qPitch(60), arp.process(qPitch(60), 60, notes, 0u));
  TEST_ASSERT_EQUAL_INT16(qPitch(84), arp.process(qPitch(60), 60, notes,
                                                 config::kRetroArpStepMs));
  TEST_ASSERT_EQUAL_INT16(qPitch(60), arp.process(qPitch(60), 60, notes,
                                                 2u * config::kRetroArpStepMs));
}

static void test_two_note_scale_never_exceeds_pitch_ceiling(void) {
  bool notes[kNoteCount] = {};
  notes[0] = true;
  notes[7] = true;
  RetroArpeggiator arp;
  arp.setEnabled(true, 0u);
  for (uint32_t t = 0; t < 1000u; ++t) {
    const SemitoneQ8_8 out = arp.process(qPitch(108), 108, notes, t);
    TEST_ASSERT_TRUE(out >= qPitch(108));
    TEST_ASSERT_TRUE(out <= qPitch(120));
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_all_factory_presets_match_scale_degree_reference);
  RUN_TEST(test_all_twelve_root_pitch_classes_use_correct_scale_degrees);
  RUN_TEST(test_step_boundary_is_exact_for_many_cycles);
  RUN_TEST(test_reenable_restarts_phase_at_root);
  RUN_TEST(test_toggle_restarts_phase_when_enabling);
  RUN_TEST(test_one_note_scale_is_bounded_and_deterministic);
  RUN_TEST(test_two_note_scale_never_exceeds_pitch_ceiling);
  return UNITY_END();
}
