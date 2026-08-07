/**
 * @file test_controls.cpp
 * Host regression or unit tests for controls behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "FakeEeprom.h"
#include "fmq/application/ControlInputProcessor.h"
#include "fmq/ui/Menu.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/persistence/SaveSlotStore.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

namespace {
RawControlInput raw(uint16_t ladder, bool shift = false,
                    bool save = false, bool load = false) {
  return RawControlInput{ladder, shift, save, load};
}

void feed(ControlInputProcessor &controls, Menu &menu, QuantizerState &q,
          QuantizationResult &r, SaveSlotStore &store, uint32_t &t,
          const RawControlInput &value, uint32_t durationMs) {
  const uint32_t end = t + durationMs;
  while (t <= end) {
    const MenuInput in = controls.sample(t, value);
    menu.update(q, in, r, t, store);
    ++t;
  }
}
}

static void test_real_adc_press_toggles_note(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  QuantizationResult r = QuantizationResult::makeZero();
  ControlInputProcessor controls;
  controls.calibrateLadderRest(558);
  uint32_t t = 0;

  feed(controls, menu, q, r, store, t, raw(558), 70);
  // Original hardware uses the proven absolute AVCC-referenced ladder values.
  feed(controls, menu, q, r, store, t, raw(236), 70);
  TEST_ASSERT_FALSE(q.channels[0].config().notes[3]);
  feed(controls, menu, q, r, store, t, raw(558), 70);
}

static void test_debounced_shift_plus_b_selects_channel_b(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  QuantizationResult r = QuantizationResult::makeZero();
  ControlInputProcessor controls;
  controls.calibrateLadderRest(558);
  uint32_t t = 0;

  feed(controls, menu, q, r, store, t, raw(558), 70);
  feed(controls, menu, q, r, store, t, raw(558, true), 70);
  feed(controls, menu, q, r, store, t, raw(536, true), 70); // SHIFT+B
  feed(controls, menu, q, r, store, t, raw(558, true), 70);
  feed(controls, menu, q, r, store, t, raw(558, false), 70);

  feed(controls, menu, q, r, store, t, raw(171), 70); // toggle note 2
  TEST_ASSERT_FALSE(q.channels[1].config().notes[2]);
  TEST_ASSERT_TRUE(q.channels[0].config().notes[2]);
}

static void test_shift_glide_menu_through_raw_controls(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  QuantizationResult r = QuantizationResult::makeZero();
  ControlInputProcessor controls;
  controls.calibrateLadderRest(558);
  uint32_t t = 0;

  feed(controls, menu, q, r, store, t, raw(558), 70);
  feed(controls, menu, q, r, store, t, raw(558, true), 70);
  feed(controls, menu, q, r, store, t, raw(171, true), 70); // SHIFT+2
  feed(controls, menu, q, r, store, t, raw(558, true), 70);
  feed(controls, menu, q, r, store, t, raw(421, true), 70); // set 7

  TEST_ASSERT_EQUAL_UINT8(7, q.channels[0].config().glideAmount);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_real_adc_press_toggles_note);
  RUN_TEST(test_debounced_shift_plus_b_selects_channel_b);
  RUN_TEST(test_shift_glide_menu_through_raw_controls);
  return UNITY_END();
}
