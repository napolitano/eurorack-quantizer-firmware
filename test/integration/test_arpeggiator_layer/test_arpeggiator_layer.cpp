/**
 * @file test_arpeggiator_layer.cpp
 * Integration tests for the complete second Arpeggiator UI layer.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "FakeEeprom.h"
#include "fmq/application/ArpeggiatorBank.h"
#include "fmq/application/UiLayerGesture.h"
#include "fmq/config/ProductConfig.h"
#include "fmq/config/UiConfig.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/persistence/LiveStateStore.h"
#include "fmq/persistence/SaveSlotStore.h"
#include "fmq/ui/Menu.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

namespace {
MenuInput input(ButtonEvent event = ButtonEvent::none(), bool shift = false) {
  MenuInput in;
  in.keyEvent = event;
  in.shiftPressed = shift;
  return in;
}

MenuInput press(uint8_t button, bool shift = false) {
  return input(ButtonEvent::pressed(button), shift);
}

MenuInput release(bool shift = false) {
  return input(ButtonEvent::released(), shift);
}

void assertEntireRing(const LedFrame &frame, LedColor expected) {
  for (uint8_t i = 0u; i < kNoteCount; ++i) {
    TEST_ASSERT_EQUAL(expected, frame[i]);
  }
}

struct Fixture {
  FakeEeprom eeprom;
  AsyncEepromWriter writer;
  SaveSlotStore store;
  Menu menu;
  QuantizerState quantizer;
  ArpeggiatorBank arpeggiators;
  QuantizationResult result;
  uint32_t now;

  Fixture()
      : eeprom(),
        writer(eeprom),
        store(eeprom, writer),
        menu(),
        quantizer(),
        arpeggiators(),
        result(QuantizationResult::makeZero()),
        now(100u) {
    menu.begin(store);
    (void)menu.toggleLayer(quantizer, arpeggiators, now);
    now += config::kArpToggleFeedbackMs + 1u;
    (void)menu.update(quantizer, arpeggiators, input(), result, now, store);
    now += 10u;
  }

  MenuOutput update(const MenuInput &in) {
    const MenuOutput out =
        menu.update(quantizer, arpeggiators, in, result, now, store);
    now += 10u;
    return out;
  }

  void choose(uint8_t functionButton, uint8_t valueButton) {
    // Same grammar as the Quantizer layer: SHIFT + function, release SHIFT,
    // then choose the scalar value with an unmodified note key.
    update(press(functionButton, true));
    update(release(false));
    update(press(valueButton, false));
    update(release(false));
  }
};
}  // namespace


static void test_full_three_second_gesture_enters_layer_and_arp_runs(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  SaveSlotStore store(eeprom, writer);
  Menu menu;
  QuantizerState quantizer;
  ArpeggiatorBank arpeggiators;
  UiLayerGesture gesture;
  menu.begin(store);

  TEST_ASSERT_EQUAL(UiLayerGestureAction::None,
                    gesture.update(true, false, false, 1000u));
  const uint32_t toggleTime = 1000u + config::kUiLayerToggleHoldMs;
  TEST_ASSERT_EQUAL(UiLayerGestureAction::ToggleLayer,
                    gesture.update(true, false, false, toggleTime));

  const bool changed = menu.toggleLayer(quantizer, arpeggiators, toggleTime);
  TEST_ASSERT_TRUE(changed);
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, menu.layer());
  TEST_ASSERT_TRUE(arpeggiators.enabled(kChannelAIndex));

  const SemitoneQ8_8 basePitch =
      static_cast<SemitoneQ8_8>(60 * kSemitoneOneQ8_8);
  const bool *notes = quantizer.channels[kChannelAIndex].config().notes;
  const ArpeggiatorOutput first = arpeggiators.process(
      kChannelAIndex, basePitch, 60, notes, false, toggleTime);
  const ArpeggiatorOutput next = arpeggiators.process(
      kChannelAIndex, basePitch, 60, notes, false,
      toggleTime + Arpeggiator::freeRateMs(
                       config::kArpDefaultRateIndex));

  TEST_ASSERT_EQUAL(basePitch, first.pitch);
  TEST_ASSERT_TRUE(next.stepAdvanced);
  TEST_ASSERT_TRUE(next.pitch > basePitch);
}

static void test_arpeggiator_layer_preserves_normal_scale_display(void) {
  Fixture f;
  const MenuOutput out = f.update(input());

  // Factory scale is chromatic. In Channel A view every enabled scale degree
  // remains green except the currently quantized degree, which is amber.
  for (uint8_t i = 1u; i < kNoteCount; ++i) {
    TEST_ASSERT_EQUAL(LedColor::Green, out.frame[i]);
  }
  TEST_ASSERT_EQUAL(LedColor::Amber, out.frame[0]);
}

static void test_default_arpeggiator_is_free_running_not_clocked(void) {
  Fixture f;
  const ArpeggiatorConfig &arp = f.arpeggiators.config(kChannelAIndex);
  TEST_ASSERT_TRUE(arp.enabled);
  TEST_ASSERT_EQUAL(ArpeggiatorSyncMode::Free, arp.syncMode);
}

static void test_entering_layer_enables_selected_arpeggiator_immediately(void) {
  Fixture f;
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, f.menu.layer());
  TEST_ASSERT_TRUE(f.arpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_FALSE(f.arpeggiators.enabled(kChannelBIndex));
}


static void test_entering_layer_enables_selected_b_only_when_unlinked(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  SaveSlotStore store(eeprom, writer);
  Menu menu;
  QuantizerState quantizer;
  ArpeggiatorBank arpeggiators;
  menu.begin(store);
  menu.setSelectedChannelIndex(kChannelBIndex);

  TEST_ASSERT_TRUE(menu.toggleLayer(quantizer, arpeggiators, 100u));
  TEST_ASSERT_FALSE(arpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_TRUE(arpeggiators.enabled(kChannelBIndex));
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, menu.selectedChannelIndex());
}

static void test_entering_layer_enables_both_channels_when_linked(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  SaveSlotStore store(eeprom, writer);
  Menu menu;
  QuantizerState quantizer;
  ArpeggiatorBank arpeggiators;
  menu.begin(store);
  quantizer.channelsLinked = true;

  TEST_ASSERT_TRUE(menu.toggleLayer(quantizer, arpeggiators, 100u));
  TEST_ASSERT_TRUE(arpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_TRUE(arpeggiators.enabled(kChannelBIndex));
  TEST_ASSERT_EQUAL_UINT8(kChannelAIndex, menu.selectedChannelIndex());
}

static void test_clock_mode_is_only_selected_explicitly(void) {
  Fixture f;
  TEST_ASSERT_EQUAL(ArpeggiatorSyncMode::Free,
                    f.arpeggiators.config(kChannelAIndex).syncMode);

  f.choose(config::kArpSyncButtonIndex, 2u);
  TEST_ASSERT_EQUAL(ArpeggiatorSyncMode::Clock,
                    f.arpeggiators.config(kChannelAIndex).syncMode);
}

static void test_returning_to_quantizer_layer_stops_all_arpeggiators(void) {
  Fixture f;
  f.arpeggiators.setEnabled(kChannelBIndex, true, f.now);

  const bool changed =
      f.menu.toggleLayer(f.quantizer, f.arpeggiators, f.now);

  TEST_ASSERT_TRUE(changed);
  TEST_ASSERT_EQUAL(UiLayer::Quantizer, f.menu.layer());
  TEST_ASSERT_FALSE(f.arpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_FALSE(f.arpeggiators.enabled(kChannelBIndex));
}

static void test_layer_entry_flashes_green_twice_then_restores_scale(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  SaveSlotStore store(eeprom, writer);
  Menu menu;
  QuantizerState quantizer;
  ArpeggiatorBank arpeggiators;
  const QuantizationResult result = QuantizationResult::makeZero();
  menu.begin(store);
  const LedFrame expected =
      menu.update(quantizer, arpeggiators, input(), result, 90u, store).frame;

  const uint32_t start = 100u;
  TEST_ASSERT_TRUE(menu.toggleLayer(quantizer, arpeggiators, start));

  MenuOutput out = menu.update(quantizer, arpeggiators, input(), result,
                               start, store);
  assertEntireRing(out.frame, LedColor::Green);

  out = menu.update(quantizer, arpeggiators, input(), result,
                    start + config::kArpToggleBlinkHalfPeriodMs, store);
  assertEntireRing(out.frame, LedColor::Off);

  out = menu.update(quantizer, arpeggiators, input(), result,
                    start + 2u * config::kArpToggleBlinkHalfPeriodMs, store);
  assertEntireRing(out.frame, LedColor::Green);

  out = menu.update(quantizer, arpeggiators, input(), result,
                    start + 3u * config::kArpToggleBlinkHalfPeriodMs, store);
  assertEntireRing(out.frame, LedColor::Off);

  out = menu.update(quantizer, arpeggiators, input(), result,
                    start + config::kArpToggleFeedbackMs, store);
  TEST_ASSERT_TRUE(out.frame == expected);
}

static void test_layer_exit_flashes_red_twice_then_restores_scale(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  SaveSlotStore store(eeprom, writer);
  Menu menu;
  QuantizerState quantizer;
  ArpeggiatorBank arpeggiators;
  const QuantizationResult result = QuantizationResult::makeZero();
  menu.begin(store);

  (void)menu.toggleLayer(quantizer, arpeggiators, 100u);
  const LedFrame expected =
      menu.update(quantizer, arpeggiators, input(), result,
                  100u + config::kArpToggleFeedbackMs, store).frame;
  arpeggiators.setEnabled(kChannelBIndex, true, 800u);

  const uint32_t start = 1000u;
  TEST_ASSERT_TRUE(menu.toggleLayer(quantizer, arpeggiators, start));
  TEST_ASSERT_FALSE(arpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_FALSE(arpeggiators.enabled(kChannelBIndex));

  MenuOutput out = menu.update(quantizer, arpeggiators, input(), result,
                               start, store);
  assertEntireRing(out.frame, LedColor::Red);

  out = menu.update(quantizer, arpeggiators, input(), result,
                    start + config::kArpToggleBlinkHalfPeriodMs, store);
  assertEntireRing(out.frame, LedColor::Off);

  out = menu.update(quantizer, arpeggiators, input(), result,
                    start + 2u * config::kArpToggleBlinkHalfPeriodMs, store);
  assertEntireRing(out.frame, LedColor::Red);

  out = menu.update(quantizer, arpeggiators, input(), result,
                    start + 3u * config::kArpToggleBlinkHalfPeriodMs, store);
  assertEntireRing(out.frame, LedColor::Off);

  out = menu.update(quantizer, arpeggiators, input(), result,
                    start + config::kArpToggleFeedbackMs, store);
  TEST_ASSERT_TRUE(out.frame == expected);
}

static void test_unmodified_note_still_edits_scale_in_arpeggiator_layer(void) {
  Fixture f;
  const uint8_t note = 4u;
  TEST_ASSERT_TRUE(f.quantizer.channels[kChannelAIndex].config().notes[note]);

  const MenuOutput out = f.update(press(note, false));
  TEST_ASSERT_FALSE(f.quantizer.channels[kChannelAIndex].config().notes[note]);
  TEST_ASSERT_TRUE(out.persistentStateChanged);
}

static void test_shift_arp_function_selects_menu_without_editing_scale(void) {
  Fixture f;
  const uint8_t button = config::kArpLengthButtonIndex;
  const bool before = f.quantizer.channels[kChannelAIndex].config().notes[button];

  const MenuOutput out = f.update(press(button, true));
  TEST_ASSERT_EQUAL(before,
                    f.quantizer.channels[kChannelAIndex].config().notes[button]);
  TEST_ASSERT_FALSE(out.persistentStateChanged);
  TEST_ASSERT_FALSE(f.menu.layerToggleAllowed());
}

static void test_enable_uses_shift_modifier_and_is_per_channel(void) {
  Fixture f;
  MenuOutput out = f.update(press(config::kArpEnableButtonIndex, true));
  TEST_ASSERT_FALSE(f.arpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_FALSE(f.arpeggiators.enabled(kChannelBIndex));
  TEST_ASSERT_TRUE(out.persistentStateChanged);

  f.update(release(false));
  f.now += config::kBoolOptionFeedbackMs;
  f.update(input());
  f.update(press(config::kChannelBButtonIndex, true));
  f.update(release(false));
  out = f.update(press(config::kArpEnableButtonIndex, true));
  TEST_ASSERT_FALSE(f.arpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_TRUE(f.arpeggiators.enabled(kChannelBIndex));
  TEST_ASSERT_TRUE(out.persistentStateChanged);
}


static void test_arp_enable_feedback_flashes_entire_ring_green_twice_then_restores_scale(void) {
  Fixture f;
  f.arpeggiators.setEnabled(kChannelAIndex, false, f.now);
  const LedFrame expectedScale = f.update(input()).frame;
  const uint32_t start = f.now;

  MenuOutput out = f.update(press(config::kArpEnableButtonIndex, true));
  TEST_ASSERT_TRUE(f.arpeggiators.enabled(kChannelAIndex));
  assertEntireRing(out.frame, LedColor::Green);

  f.now = start + config::kArpToggleBlinkHalfPeriodMs;
  out = f.update(input());
  assertEntireRing(out.frame, LedColor::Off);

  f.now = start + 2u * config::kArpToggleBlinkHalfPeriodMs;
  out = f.update(input());
  assertEntireRing(out.frame, LedColor::Green);

  f.now = start + 3u * config::kArpToggleBlinkHalfPeriodMs;
  out = f.update(input());
  assertEntireRing(out.frame, LedColor::Off);

  f.now = start + config::kArpToggleFeedbackMs;
  out = f.update(input());
  TEST_ASSERT_TRUE(out.frame == expectedScale);
}

static void test_arp_disable_feedback_flashes_entire_ring_red_twice_then_restores_scale(void) {
  Fixture f;
  const LedFrame expectedScale = f.update(input()).frame;
  const uint32_t start = f.now;

  MenuOutput out = f.update(press(config::kArpEnableButtonIndex, true));
  TEST_ASSERT_FALSE(f.arpeggiators.enabled(kChannelAIndex));
  assertEntireRing(out.frame, LedColor::Red);

  f.now = start + config::kArpToggleBlinkHalfPeriodMs;
  out = f.update(input());
  assertEntireRing(out.frame, LedColor::Off);

  f.now = start + 2u * config::kArpToggleBlinkHalfPeriodMs;
  out = f.update(input());
  assertEntireRing(out.frame, LedColor::Red);

  f.now = start + 3u * config::kArpToggleBlinkHalfPeriodMs;
  out = f.update(input());
  assertEntireRing(out.frame, LedColor::Off);

  f.now = start + config::kArpToggleFeedbackMs;
  out = f.update(input());
  TEST_ASSERT_TRUE(out.frame == expectedScale);
}

static void test_rate_pattern_shape_length_range_sync_and_swing_are_editable(void) {
  Fixture f;
  f.choose(config::kArpRateButtonIndex, 10u);
  TEST_ASSERT_EQUAL_UINT8(10u, f.arpeggiators.config(0).rateIndex);

  f.choose(config::kArpPatternButtonIndex, 5u);
  TEST_ASSERT_EQUAL(ArpeggiatorPattern::OutsideIn,
                    f.arpeggiators.config(0).pattern);

  f.choose(config::kArpShapeButtonIndex, 6u);
  TEST_ASSERT_EQUAL(ArpeggiatorShape::Seventh1357,
                    f.arpeggiators.config(0).shape);

  f.choose(config::kArpLengthButtonIndex, 8u);
  TEST_ASSERT_EQUAL_UINT8(9u, f.arpeggiators.config(0).length);

  f.choose(config::kArpRangeButtonIndex, 2u);
  TEST_ASSERT_EQUAL_UINT8(3u, f.arpeggiators.config(0).range);

  f.choose(config::kArpSyncButtonIndex, 2u);
  TEST_ASSERT_EQUAL(ArpeggiatorSyncMode::Clock,
                    f.arpeggiators.config(0).syncMode);

  f.choose(config::kArpSwingButtonIndex, 11u);
  TEST_ASSERT_EQUAL_UINT8(11u, f.arpeggiators.config(0).swing);
}

static void test_range_and_sync_ignore_buttons_outside_their_domains(void) {
  Fixture f;
  const uint8_t rangeBefore = f.arpeggiators.config(0).range;
  f.choose(config::kArpRangeButtonIndex, 11u);
  TEST_ASSERT_EQUAL_UINT8(rangeBefore, f.arpeggiators.config(0).range);

  const ArpeggiatorSyncMode syncBefore = f.arpeggiators.config(0).syncMode;
  f.choose(config::kArpSyncButtonIndex, 11u);
  TEST_ASSERT_EQUAL(syncBefore, f.arpeggiators.config(0).syncMode);
}

static void test_step_trigger_toggle_is_per_channel(void) {
  Fixture f;
  f.update(press(config::kArpStepTriggerButtonIndex, true));
  TEST_ASSERT_TRUE(f.arpeggiators.config(0).stepTrigger);
  TEST_ASSERT_FALSE(f.arpeggiators.config(1).stepTrigger);
}

static void test_link_copies_complete_arpeggiator_config_a_to_b(void) {
  Fixture f;
  ArpeggiatorConfig a = ArpeggiatorConfig::makeDefault();
  a.enabled = true;
  a.rateIndex = 9u;
  a.pattern = ArpeggiatorPattern::InsideOut;
  a.shape = ArpeggiatorShape::StackedThirds;
  a.length = 10u;
  a.range = 4u;
  a.stepTrigger = true;
  a.syncMode = ArpeggiatorSyncMode::Clock;
  a.swing = 8u;
  f.arpeggiators.setConfig(kChannelAIndex, a, f.now);

  f.update(press(config::kChannelLinkButtonIndex, true));
  TEST_ASSERT_TRUE(f.quantizer.channelsLinked);
  TEST_ASSERT_EQUAL_UINT8(kChannelAIndex, f.menu.selectedChannelIndex());
  const ArpeggiatorConfig &b = f.arpeggiators.config(kChannelBIndex);
  TEST_ASSERT_EQUAL(a.enabled, b.enabled);
  TEST_ASSERT_EQUAL_UINT8(a.rateIndex, b.rateIndex);
  TEST_ASSERT_EQUAL(a.pattern, b.pattern);
  TEST_ASSERT_EQUAL(a.shape, b.shape);
  TEST_ASSERT_EQUAL_UINT8(a.length, b.length);
  TEST_ASSERT_EQUAL_UINT8(a.range, b.range);
  TEST_ASSERT_EQUAL(a.stepTrigger, b.stepTrigger);
  TEST_ASSERT_EQUAL(a.syncMode, b.syncMode);
  TEST_ASSERT_EQUAL_UINT8(a.swing, b.swing);
}

static void test_linked_parameter_changes_apply_to_both_channels(void) {
  Fixture f;
  f.update(press(config::kChannelLinkButtonIndex, true));
  f.update(release(false));
  f.now += config::kBoolOptionFeedbackMs;
  f.update(input());
  f.choose(config::kArpRateButtonIndex, 7u);
  TEST_ASSERT_EQUAL_UINT8(7u, f.arpeggiators.config(0).rateIndex);
  TEST_ASSERT_EQUAL_UINT8(7u, f.arpeggiators.config(1).rateIndex);
}

static void test_a_and_b_navigation_remains_available_via_shift_in_arp_layer(void) {
  Fixture f;
  f.update(press(config::kChannelBButtonIndex, true));
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, f.menu.selectedChannelIndex());
  f.update(release());
  f.update(press(config::kChannelAButtonIndex, true));
  TEST_ASSERT_EQUAL_UINT8(kChannelAIndex, f.menu.selectedChannelIndex());
}

static void test_full_configuration_capture_contains_arpeggiator_and_channel(void) {
  Fixture f;
  f.update(press(config::kChannelBButtonIndex, true));
  ArpeggiatorConfig b = ArpeggiatorConfig::makeDefault();
  b.enabled = true;
  b.rateIndex = 11u;
  b.pattern = ArpeggiatorPattern::Random;
  b.syncMode = ArpeggiatorSyncMode::Clock;
  b.swing = 10u;
  f.arpeggiators.setConfig(kChannelBIndex, b, f.now);

  const StoredConfiguration stored =
      f.menu.captureStoredConfiguration(f.quantizer, f.arpeggiators);
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, stored.selectedChannelIndex);
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, stored.uiLayer);
  TEST_ASSERT_TRUE(stored.arpeggiators[kChannelBIndex].enabled);
  TEST_ASSERT_EQUAL_UINT8(11u, stored.arpeggiators[kChannelBIndex].rateIndex);
  TEST_ASSERT_EQUAL(ArpeggiatorPattern::Random,
                    stored.arpeggiators[kChannelBIndex].pattern);
  TEST_ASSERT_EQUAL(ArpeggiatorSyncMode::Clock,
                    stored.arpeggiators[kChannelBIndex].syncMode);
}


static void test_full_configuration_save_and_load_restores_arpeggiator_state(void) {
  Fixture f;
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, f.menu.layer());

  f.update(press(config::kChannelBButtonIndex, true));
  f.update(release());

  ArpeggiatorConfig saved = ArpeggiatorConfig::makeDefault();
  saved.enabled = true;
  saved.rateIndex = 9u;
  saved.pattern = ArpeggiatorPattern::InsideOut;
  saved.shape = ArpeggiatorShape::StackedThirds;
  saved.length = 10u;
  saved.range = 4u;
  saved.stepTrigger = true;
  saved.syncMode = ArpeggiatorSyncMode::Clock;
  saved.swing = 8u;
  f.arpeggiators.setConfig(kChannelBIndex, saved, f.now);
  f.quantizer.channels[kChannelBIndex].config().glideAmount = 6u;

  MenuInput save = input();
  save.saveButton = LongPressButtonState::ButtonJustDown;
  save.shiftPressed = true;
  f.update(save);
  f.update(press(3u, true));
  f.update(release(true));
  f.writer.flush();

  f.now += config::kSaveConfirmationMs + 1u;
  f.update(input());

  ArpeggiatorConfig changed = ArpeggiatorConfig::makeDefault();
  changed.enabled = false;
  changed.rateIndex = 0u;
  f.arpeggiators.setConfig(kChannelBIndex, changed, f.now);
  f.quantizer.channels[kChannelBIndex].config().glideAmount = 0u;
  f.menu.setSelectedChannelIndex(kChannelAIndex);
  // Move to Quantizer UI without using the normal exit gesture, which would
  // deliberately disable ARP and add feedback; the load must restore the saved
  // layer side-effect free.
  f.menu.restoreLayer(UiLayer::Quantizer);

  MenuInput load = input();
  load.loadButton = LongPressButtonState::ButtonJustDown;
  load.shiftPressed = true;
  f.update(load);
  f.update(press(3u, true));

  const ArpeggiatorConfig &restored = f.arpeggiators.config(kChannelBIndex);
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, f.menu.layer());
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, f.menu.selectedChannelIndex());
  TEST_ASSERT_EQUAL_UINT8(6u,
      f.quantizer.channels[kChannelBIndex].config().glideAmount);
  TEST_ASSERT_TRUE(restored.enabled);
  TEST_ASSERT_EQUAL_UINT8(9u, restored.rateIndex);
  TEST_ASSERT_EQUAL(ArpeggiatorPattern::InsideOut, restored.pattern);
  TEST_ASSERT_EQUAL(ArpeggiatorShape::StackedThirds, restored.shape);
  TEST_ASSERT_EQUAL_UINT8(10u, restored.length);
  TEST_ASSERT_EQUAL_UINT8(4u, restored.range);
  TEST_ASSERT_TRUE(restored.stepTrigger);
  TEST_ASSERT_EQUAL(ArpeggiatorSyncMode::Clock, restored.syncMode);
  TEST_ASSERT_EQUAL_UINT8(8u, restored.swing);
}

static void test_reboot_restores_arpeggiator_layer_and_first_hold_turns_it_off(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);

  // Simulate the state written when power is removed in the Arpeggiator layer.
  {
    LiveStateStore live(eeprom, writer);
    LiveState blank;
    live.load(blank);
    StoredConfiguration stored;
    stored.selectedChannelIndex = kChannelAIndex;
    stored.arpeggiators[kChannelAIndex].enabled = true;
    TEST_ASSERT_TRUE(live.commit(stored, BrightnessCalibration::makeDefault(),
                                 UiLayer::Arpeggiator));
    live.flush();
  }

  // Simulated reboot: restore musical state and UI layer without invoking the
  // toggle operation. The very first 3 s action must therefore be an exit/off.
  LiveStateStore live(eeprom, writer);
  LiveState restored;
  TEST_ASSERT_TRUE(live.load(restored));

  SaveSlotStore store(eeprom, writer);
  Menu menu;
  QuantizerState quantizer;
  ArpeggiatorBank arpeggiators;
  menu.applyStoredConfiguration(restored.configuration, quantizer,
                                arpeggiators, 100u);
  menu.begin(store);

  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, menu.layer());
  TEST_ASSERT_TRUE(arpeggiators.enabled(kChannelAIndex));

  UiLayerGesture gesture;
  TEST_ASSERT_EQUAL(UiLayerGestureAction::None,
                    gesture.update(true, false, false, 1000u));
  const uint32_t toggleTime = 1000u + config::kUiLayerToggleHoldMs;
  TEST_ASSERT_EQUAL(UiLayerGestureAction::ToggleLayer,
                    gesture.update(true, false, false, toggleTime));
  TEST_ASSERT_TRUE(menu.toggleLayer(quantizer, arpeggiators, toggleTime));

  TEST_ASSERT_EQUAL(UiLayer::Quantizer, menu.layer());
  TEST_ASSERT_FALSE(arpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_FALSE(arpeggiators.enabled(kChannelBIndex));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_full_three_second_gesture_enters_layer_and_arp_runs);
  RUN_TEST(test_reboot_restores_arpeggiator_layer_and_first_hold_turns_it_off);
  RUN_TEST(test_arpeggiator_layer_preserves_normal_scale_display);
  RUN_TEST(test_default_arpeggiator_is_free_running_not_clocked);
  RUN_TEST(test_entering_layer_enables_selected_arpeggiator_immediately);
  RUN_TEST(test_entering_layer_enables_selected_b_only_when_unlinked);
  RUN_TEST(test_entering_layer_enables_both_channels_when_linked);
  RUN_TEST(test_clock_mode_is_only_selected_explicitly);
  RUN_TEST(test_returning_to_quantizer_layer_stops_all_arpeggiators);
  RUN_TEST(test_layer_entry_flashes_green_twice_then_restores_scale);
  RUN_TEST(test_layer_exit_flashes_red_twice_then_restores_scale);
  RUN_TEST(test_unmodified_note_still_edits_scale_in_arpeggiator_layer);
  RUN_TEST(test_shift_arp_function_selects_menu_without_editing_scale);
  RUN_TEST(test_enable_uses_shift_modifier_and_is_per_channel);
  RUN_TEST(test_arp_enable_feedback_flashes_entire_ring_green_twice_then_restores_scale);
  RUN_TEST(test_arp_disable_feedback_flashes_entire_ring_red_twice_then_restores_scale);
  RUN_TEST(test_rate_pattern_shape_length_range_sync_and_swing_are_editable);
  RUN_TEST(test_range_and_sync_ignore_buttons_outside_their_domains);
  RUN_TEST(test_step_trigger_toggle_is_per_channel);
  RUN_TEST(test_link_copies_complete_arpeggiator_config_a_to_b);
  RUN_TEST(test_linked_parameter_changes_apply_to_both_channels);
  RUN_TEST(test_a_and_b_navigation_remains_available_via_shift_in_arp_layer);
  RUN_TEST(test_full_configuration_capture_contains_arpeggiator_and_channel);
  RUN_TEST(test_full_configuration_save_and_load_restores_arpeggiator_state);
  return UNITY_END();
}
