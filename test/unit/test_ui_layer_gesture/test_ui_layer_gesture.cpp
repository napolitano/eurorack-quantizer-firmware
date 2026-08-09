/**
 * @file test_ui_layer_gesture.cpp
 * Unit tests for the official Arpeggiator SHIFT-hold gesture.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/UiLayerGesture.h"
#include "fmq/config/UiConfig.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void assertAction(UiLayerGestureAction expected,
                         UiLayerGestureAction actual) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}

static void test_toggles_once_after_exact_hold_time(void) {
  UiLayerGesture gesture;
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, false, false, 1000));
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, false, false,
                              1000 + config::kUiLayerToggleHoldMs - 1u));
  assertAction(UiLayerGestureAction::ToggleLayer,
               gesture.update(true, false, false,
                              1000 + config::kUiLayerToggleHoldMs));
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, false, false,
                              1000 + config::kUiLayerToggleHoldMs + 1000u));
}

static void test_normal_shift_shortcut_cancels_until_release(void) {
  UiLayerGesture gesture;
  (void)gesture.update(true, false, false, 0);
  (void)gesture.update(true, true, false, 100);
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, false, false,
                              config::kUiLayerToggleHoldMs + 500u));

  (void)gesture.update(false, false, false,
                       config::kUiLayerToggleHoldMs + 501u);
  (void)gesture.update(true, false, false,
                       config::kUiLayerToggleHoldMs + 600u);
  assertAction(UiLayerGestureAction::ToggleLayer,
               gesture.update(true, false, false,
                              2u * config::kUiLayerToggleHoldMs + 600u));
}

static void test_blocked_mode_never_toggles(void) {
  UiLayerGesture gesture;
  (void)gesture.update(true, false, true, 0);
  assertAction(UiLayerGestureAction::None,
               gesture.update(true, false, true,
                              config::kUiLayerToggleHoldMs + 1u));
}

static void test_unsigned_clock_wraparound_is_safe(void) {
  UiLayerGesture gesture;
  const uint32_t start = 0xFFFFFF00u;
  (void)gesture.update(true, false, false, start);
  const uint32_t finish = start + config::kUiLayerToggleHoldMs;
  assertAction(UiLayerGestureAction::ToggleLayer,
               gesture.update(true, false, false, finish));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_toggles_once_after_exact_hold_time);
  RUN_TEST(test_normal_shift_shortcut_cancels_until_release);
  RUN_TEST(test_blocked_mode_never_toggles);
  RUN_TEST(test_unsigned_clock_wraparound_is_safe);
  return UNITY_END();
}
