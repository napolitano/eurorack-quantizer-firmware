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
#include "fmq/application/ArpeggiatorBank.h"
#include "fmq/application/UiLayerGesture.h"
#include "fmq/config/UiConfig.h"
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
          ArpeggiatorBank &arpeggiators, QuantizationResult &r,
          SaveSlotStore &store, uint32_t &t,
          const RawControlInput &value, uint32_t durationMs) {
  const uint32_t end = t + durationMs;
  while (t <= end) {
    const MenuInput in = controls.sample(t, value);
    menu.update(q, arpeggiators, in, r, t, store);
    ++t;
  }
}
}


static void test_physical_ladder_activity_is_visible_before_debounce(void) {
  ControlInputProcessor controls;
  controls.calibrateLadderRest(558u);

  const MenuInput input = controls.sample(100u, raw(236u, true));
  TEST_ASSERT_TRUE(input.noteButtonDown);
  TEST_ASSERT_EQUAL(ButtonEventType::None, input.keyEvent.type);
}


static void test_physical_note_press_cancels_pending_layer_double_click(void) {
  ControlInputProcessor controls;
  controls.calibrateLadderRest(558u);
  UiLayerGesture gesture;

  // First clean SHIFT click.
  MenuInput input = controls.sample(1000u, raw(558u, true));
  TEST_ASSERT_EQUAL(UiLayerGestureAction::None,
                    gesture.update(input.shiftPressed, input.noteButtonDown,
                                   false, 1000u));
  input = controls.sample(1000u + config::kUiLayerDoubleClickDebounceMs,
                          raw(558u, true));
  TEST_ASSERT_EQUAL(UiLayerGestureAction::None,
                    gesture.update(input.shiftPressed, input.noteButtonDown,
                                   false,
                                   1000u + config::kUiLayerDoubleClickDebounceMs));
  input = controls.sample(1100u, raw(558u, false));
  (void)gesture.update(input.shiftPressed, input.noteButtonDown, false, 1100u);
  input = controls.sample(1100u + config::kUiLayerDoubleClickDebounceMs,
                          raw(558u, false));
  TEST_ASSERT_EQUAL(UiLayerGestureAction::None,
                    gesture.update(input.shiftPressed, input.noteButtonDown,
                                   false,
                                   1100u + config::kUiLayerDoubleClickDebounceMs));

  // Begin the second SHIFT press, then physically press a note before its 64 ms
  // ladder debounce can complete. Raw ladder activity must cancel the gesture.
  input = controls.sample(1250u, raw(558u, true));
  (void)gesture.update(input.shiftPressed, input.noteButtonDown, false, 1250u);
  input = controls.sample(1250u + config::kUiLayerDoubleClickDebounceMs,
                          raw(558u, true));
  (void)gesture.update(input.shiftPressed, input.noteButtonDown, false,
                       1250u + config::kUiLayerDoubleClickDebounceMs);

  input = controls.sample(1300u, raw(236u, true));
  TEST_ASSERT_TRUE(input.noteButtonDown);
  TEST_ASSERT_EQUAL(ButtonEventType::None, input.keyEvent.type);
  TEST_ASSERT_EQUAL(UiLayerGestureAction::None,
                    gesture.update(input.shiftPressed, input.noteButtonDown,
                                   false, 1300u));

  input = controls.sample(1350u, raw(558u, false));
  (void)gesture.update(input.shiftPressed, input.noteButtonDown, false, 1350u);
  input = controls.sample(1350u + config::kUiLayerDoubleClickDebounceMs,
                          raw(558u, false));
  TEST_ASSERT_EQUAL(UiLayerGestureAction::None,
                    gesture.update(input.shiftPressed, input.noteButtonDown,
                                   false,
                                   1350u + config::kUiLayerDoubleClickDebounceMs));
}

static void test_real_adc_press_toggles_note(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();
  ControlInputProcessor controls;
  controls.calibrateLadderRest(558);
  uint32_t t = 0;

  feed(controls, menu, q, arpeggiators, r, store, t, raw(558), 70);
  // Original hardware uses the proven absolute AVCC-referenced ladder values.
  feed(controls, menu, q, arpeggiators, r, store, t, raw(236), 70);
  TEST_ASSERT_FALSE(q.channels[0].config().notes[3]);
  feed(controls, menu, q, arpeggiators, r, store, t, raw(558), 70);
}

static void test_debounced_shift_plus_b_selects_channel_b(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();
  ControlInputProcessor controls;
  controls.calibrateLadderRest(558);
  uint32_t t = 0;

  feed(controls, menu, q, arpeggiators, r, store, t, raw(558), 70);
  feed(controls, menu, q, arpeggiators, r, store, t, raw(558, true), 70);
  feed(controls, menu, q, arpeggiators, r, store, t, raw(536, true), 70); // SHIFT+B
  feed(controls, menu, q, arpeggiators, r, store, t, raw(558, true), 70);
  feed(controls, menu, q, arpeggiators, r, store, t, raw(558, false), 70);

  feed(controls, menu, q, arpeggiators, r, store, t, raw(171), 70); // toggle note 2
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
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();
  ControlInputProcessor controls;
  controls.calibrateLadderRest(558);
  uint32_t t = 0;

  feed(controls, menu, q, arpeggiators, r, store, t, raw(558), 70);
  feed(controls, menu, q, arpeggiators, r, store, t, raw(558, true), 70);
  feed(controls, menu, q, arpeggiators, r, store, t, raw(171, true), 70); // SHIFT+2
  feed(controls, menu, q, arpeggiators, r, store, t, raw(558, true), 70);
  feed(controls, menu, q, arpeggiators, r, store, t, raw(421, true), 70); // set 7

  TEST_ASSERT_EQUAL_UINT8(7, q.channels[0].config().glideAmount);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_physical_ladder_activity_is_visible_before_debounce);
  RUN_TEST(test_physical_note_press_cancels_pending_layer_double_click);
  RUN_TEST(test_real_adc_press_toggles_note);
  RUN_TEST(test_debounced_shift_plus_b_selects_channel_b);
  RUN_TEST(test_shift_glide_menu_through_raw_controls);
  return UNITY_END();
}
