/**
 * @file test_arpeggiator_matrix.cpp
 * Matrix coverage for scales, roots, rates, patterns, shapes and bounds.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/Arpeggiator.h"
#include "fmq/config/FactoryPresets.h"
#include "fmq/config/ProductConfig.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

namespace {
void notesFromMask(uint16_t mask, bool notes[kNoteCount]) {
  for (uint8_t pc = 0u; pc < kNoteCount; ++pc) {
    notes[pc] = (mask & static_cast<uint16_t>(1u << pc)) != 0u;
  }
}
SemitoneQ8_8 qPitch(int semitone) {
  return static_cast<SemitoneQ8_8>(semitone * kSemitoneOneQ8_8);
}
uint8_t expectedOffset(const bool notes[kNoteCount], uint8_t rootPc,
                       uint8_t degree) {
  if (degree == 0u) return 0u;
  uint8_t passed = 0u;
  for (uint8_t offset = 1u; offset <= kMaxSemitone; ++offset) {
    const uint8_t pc = static_cast<uint8_t>((rootPc + offset) % kNoteCount);
    if (!notes[pc]) continue;
    ++passed;
    if (passed == degree) return offset;
  }
  return 0u;
}
}

static void test_all_factory_scales_keep_default_root_third_fifth_semantics(void) {
  constexpr uint8_t degrees[3] = {0u, 2u, 4u};
  for (uint8_t preset = 0u; preset < config::kFactoryPresetCount; ++preset) {
    bool notes[kNoteCount];
    notesFromMask(config::kFactoryScalePresets[preset].noteMask, notes);
    Arpeggiator arp; arp.setEnabled(true, 0u);
    for (uint8_t step = 0u; step < 3u; ++step) {
      const uint32_t time = static_cast<uint32_t>(step) * 24u;
      const ArpeggiatorOutput out = arp.process(qPitch(60), 60, notes, false, time);
      const uint8_t offset = expectedOffset(notes, 0u, degrees[step]);
      TEST_ASSERT_EQUAL_INT16(qPitch(60 + offset), out.pitch);
    }
  }
}

static void test_all_twelve_root_pitch_classes_are_bounded(void) {
  bool notes[kNoteCount]; notesFromMask(0x0FFFu, notes);
  for (uint8_t root = 0u; root < kNoteCount; ++root) {
    Arpeggiator arp; arp.setEnabled(true, 0u);
    const int base = 48 + root;
    for (uint8_t step = 0u; step < 12u; ++step) {
      const ArpeggiatorOutput out = arp.process(
          qPitch(base), static_cast<int8_t>(base), notes, false,
          static_cast<uint32_t>(step) * 24u);
      TEST_ASSERT_TRUE(out.pitch >= qPitch(base));
      TEST_ASSERT_TRUE(out.pitch <= qPitch(120));
    }
  }
}

static void test_every_free_rate_advances_exactly_at_its_boundary(void) {
  bool notes[kNoteCount]; notesFromMask(0x0AB5u, notes);
  for (uint8_t rate = 0u; rate < config::kArpRateCount; ++rate) {
    Arpeggiator arp;
    ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
    cfg.enabled = true; cfg.rateIndex = rate;
    arp.setConfig(cfg, 100u);
    const uint16_t duration = Arpeggiator::freeRateMs(rate);
    TEST_ASSERT_FALSE(arp.process(qPitch(60), 60, notes, false,
                                  100u + duration - 1u).stepAdvanced);
    TEST_ASSERT_TRUE(arp.process(qPitch(60), 60, notes, false,
                                 100u + duration).stepAdvanced);
  }
}

static void test_all_patterns_produce_positions_inside_pitch_range(void) {
  bool notes[kNoteCount]; notesFromMask(0x0AB5u, notes);
  for (uint8_t pattern = 0u; pattern < Arpeggiator::patternCount(); ++pattern) {
    Arpeggiator arp;
    ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
    cfg.enabled = true;
    cfg.pattern = static_cast<ArpeggiatorPattern>(pattern);
    cfg.length = 12u;
    arp.setConfig(cfg, 0u);
    for (uint8_t step = 0u; step < 40u; ++step) {
      const ArpeggiatorOutput out = arp.process(qPitch(60), 60, notes, false,
                                                static_cast<uint32_t>(step) * 24u);
      TEST_ASSERT_TRUE(out.pitch >= qPitch(60));
      TEST_ASSERT_TRUE(out.pitch <= qPitch(120));
    }
  }
}

static void test_all_shapes_lengths_and_ranges_are_bounded(void) {
  bool notes[kNoteCount]; notesFromMask(0x05ADu, notes);
  for (uint8_t shape = 0u; shape < Arpeggiator::shapeCount(); ++shape) {
    for (uint8_t length = 1u; length <= config::kArpMaximumLength; ++length) {
      for (uint8_t range = 1u; range <= config::kArpMaximumRange; ++range) {
        Arpeggiator arp;
        ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
        cfg.enabled = true;
        cfg.shape = static_cast<ArpeggiatorShape>(shape);
        cfg.length = length;
        cfg.range = range;
        arp.setConfig(cfg, 0u);
        for (uint8_t step = 0u; step < length; ++step) {
          const ArpeggiatorOutput out = arp.process(
              qPitch(48), 48, notes, false,
              static_cast<uint32_t>(step) * 24u);
          TEST_ASSERT_TRUE(out.pitch >= qPitch(48));
          TEST_ASSERT_TRUE(out.pitch <= qPitch(120));
        }
      }
    }
  }
}

static void test_all_clock_ratio_indices_define_exactly_one_ratio_direction(void) {
  for (uint8_t rate = 0u; rate < config::kArpRateCount; ++rate) {
    const uint8_t divider = Arpeggiator::clockDivider(rate);
    const uint8_t multiplier = Arpeggiator::clockMultiplier(rate);
    TEST_ASSERT_TRUE(divider >= 1u);
    TEST_ASSERT_TRUE(multiplier >= 1u);
    TEST_ASSERT_TRUE(divider == 1u || multiplier == 1u);
  }
}

static void test_swing_levels_remain_in_valid_range(void) {
  bool notes[kNoteCount]; notesFromMask(0x0AB5u, notes);
  for (uint8_t swing = 0u; swing <= config::kArpMaximumSwingStep; ++swing) {
    Arpeggiator arp;
    ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
    cfg.enabled = true; cfg.swing = swing;
    arp.setConfig(cfg, 0u);
    for (uint32_t t = 0u; t < 1000u; ++t) {
      const ArpeggiatorOutput out = arp.process(qPitch(60), 60, notes, false, t);
      TEST_ASSERT_TRUE(out.pitch >= qPitch(60));
      TEST_ASSERT_TRUE(out.pitch <= qPitch(120));
    }
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_all_factory_scales_keep_default_root_third_fifth_semantics);
  RUN_TEST(test_all_twelve_root_pitch_classes_are_bounded);
  RUN_TEST(test_every_free_rate_advances_exactly_at_its_boundary);
  RUN_TEST(test_all_patterns_produce_positions_inside_pitch_range);
  RUN_TEST(test_all_shapes_lengths_and_ranges_are_bounded);
  RUN_TEST(test_all_clock_ratio_indices_define_exactly_one_ratio_direction);
  RUN_TEST(test_swing_levels_remain_in_valid_range);
  return UNITY_END();
}
