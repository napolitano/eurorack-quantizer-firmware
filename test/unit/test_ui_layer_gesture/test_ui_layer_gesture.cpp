/**
 * @file test_ui_layer_gesture.cpp
 * Unit tests for the official Arpeggiator SHIFT double-click gesture.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/UiLayerGesture.h"
#include "fmq/config/UiConfig.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

namespace {
void assertAction(UiLayerGestureAction expected,
                  UiLayerGestureAction actual) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}

UiLayerGestureAction settle(UiLayerGesture &gesture, bool shift,
                            bool companion, bool blocked, uint32_t changedAt) {
  assertAction(UiLayerGestureAction::None,
               gesture.update(shift, companion, blocked, changedAt));
  return gesture.update(shift, companion, blocked,
                        changedAt + config::kUiLayerDoubleClickDebounceMs);
}

UiLayerGestureAction click(UiLayerGesture &gesture, uint32_t pressAt,
                           uint32_t pressDurationMs = 100u) {
  assertAction(UiLayerGestureAction::None,
               settle(gesture, true, false, false, pressAt));
  return settle(gesture, false, false, false, pressAt + pressDurationMs);
}
}  // namespace

static void test_clean_double_click_toggles_on_second_release(void) {
  UiLayerGesture gesture;
  assertAction(UiLayerGestureAction::None, click(gesture, 1000u));
  assertAction(UiLayerGestureAction::ToggleLayer, click(gesture, 1250u));

  // The completed sequence is consumed; remaining released samples are inert.
  assertAction(UiLayerGestureAction::None,
               gesture.update(false, false, false, 2000u));
}

static void test_single_click_expires_without_toggling(void) {
  UiLayerGesture gesture;
  const uint32_t firstPress = 1000u;
  assertAction(UiLayerGestureAction::None, click(gesture, firstPress));
  const uint32_t firstReleaseStable =
      firstPress + 100u + config::kUiLayerDoubleClickDebounceMs;
  assertAction(UiLayerGestureAction::None,
               gesture.update(false, false, false,
                              firstReleaseStable +
                                  config::kUiLayerDoubleClickGapMs + 1u));
  assertAction(UiLayerGestureAction::None, click(gesture, 2000u));
}

static void test_second_press_after_gap_starts_new_sequence(void) {
  UiLayerGesture gesture;
  assertAction(UiLayerGestureAction::None, click(gesture, 1000u));

  const uint32_t secondPress =
      1000u + 100u + config::kUiLayerDoubleClickDebounceMs +
      config::kUiLayerDoubleClickGapMs + 1u;
  assertAction(UiLayerGestureAction::None, click(gesture, secondPress));
  assertAction(UiLayerGestureAction::ToggleLayer,
               click(gesture, secondPress + 250u));
}

static void test_long_shift_press_does_not_count_as_click(void) {
  UiLayerGesture gesture;
  const uint32_t pressAt = 1000u;
  assertAction(UiLayerGestureAction::None,
               settle(gesture, true, false, false, pressAt));
  assertAction(UiLayerGestureAction::None,
               settle(gesture, false, false, false,
                      pressAt + config::kUiLayerDoubleClickMaxPressMs + 1u));

  // One subsequent short click is only the first click of a new sequence.
  assertAction(UiLayerGestureAction::None, click(gesture, 2000u));
}

static void test_normal_shift_shortcut_cancels_pending_sequence(void) {
  UiLayerGesture gesture;
  assertAction(UiLayerGestureAction::None, click(gesture, 1000u));

  const uint32_t secondPress = 1250u;
  assertAction(UiLayerGestureAction::None,
               settle(gesture, true, false, false, secondPress));
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, true, false, secondPress + 50u));
  assertAction(UiLayerGestureAction::None,
               settle(gesture, false, false, false, secondPress + 100u));

  // Cancellation survives until release and no stale first click remains.
  assertAction(UiLayerGestureAction::None, click(gesture, 1800u));
  assertAction(UiLayerGestureAction::ToggleLayer, click(gesture, 2050u));
}

static void test_companion_activity_between_clicks_cancels_sequence(void) {
  UiLayerGesture gesture;
  assertAction(UiLayerGestureAction::None, click(gesture, 1000u));
  assertAction(UiLayerGestureAction::None,
               gesture.update(false, true, false, 1200u));
  assertAction(UiLayerGestureAction::None, click(gesture, 1250u));
}

static void test_blocked_mode_never_toggles(void) {
  UiLayerGesture gesture;
  assertAction(UiLayerGestureAction::None,
               settle(gesture, true, false, true, 1000u));
  assertAction(UiLayerGestureAction::None,
               settle(gesture, false, false, true, 1100u));
  assertAction(UiLayerGestureAction::None,
               settle(gesture, true, false, true, 1250u));
  assertAction(UiLayerGestureAction::None,
               settle(gesture, false, false, true, 1350u));
}

static void test_raw_shift_bounce_is_rejected_by_gesture_debounce(void) {
  UiLayerGesture gesture;
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, false, false, 1000u));
  assertAction(UiLayerGestureAction::None,
               gesture.update(false, false, false, 1010u));
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, false, false, 1015u));
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, false, false,
                              1015u + config::kUiLayerDoubleClickDebounceMs));
  assertAction(UiLayerGestureAction::None,
               settle(gesture, false, false, false, 1100u));
  assertAction(UiLayerGestureAction::ToggleLayer, click(gesture, 1300u));
}

static void test_unsigned_clock_wraparound_is_safe(void) {
  UiLayerGesture gesture;
  const uint32_t firstPress = 0xFFFFFF00u;
  assertAction(UiLayerGestureAction::None, click(gesture, firstPress, 80u));
  const uint32_t secondPress = firstPress + 220u;
  assertAction(UiLayerGestureAction::ToggleLayer,
               click(gesture, secondPress, 80u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_clean_double_click_toggles_on_second_release);
  RUN_TEST(test_single_click_expires_without_toggling);
  RUN_TEST(test_second_press_after_gap_starts_new_sequence);
  RUN_TEST(test_long_shift_press_does_not_count_as_click);
  RUN_TEST(test_normal_shift_shortcut_cancels_pending_sequence);
  RUN_TEST(test_companion_activity_between_clicks_cancels_sequence);
  RUN_TEST(test_blocked_mode_never_toggles);
  RUN_TEST(test_raw_shift_bounce_is_rejected_by_gesture_debounce);
  RUN_TEST(test_unsigned_clock_wraparound_is_safe);
  return UNITY_END();
}
