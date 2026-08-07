/**
 * @file test_buttons.cpp
 * Host regression or unit tests for buttons behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/Button.h"
#include "fmq/application/ButtonLadder.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

// --- resistor ladder -------------------------------------------------------

static void test_ladder_nearest_value(void) {
  TEST_ASSERT_EQUAL_UINT8(0, closestButtonIndex(0));
  TEST_ASSERT_EQUAL_UINT8(12, closestButtonIndex(40));   // outside all valid windows
  TEST_ASSERT_EQUAL_UINT8(1, closestButtonIndex(90));    // ~93
  TEST_ASSERT_EQUAL_UINT8(5, closestButtonIndex(340));   // ~341
  TEST_ASSERT_EQUAL_UINT8(12, closestButtonIndex(558));  // rest value -> none
  TEST_ASSERT_EQUAL_UINT8(12, closestButtonIndex(1023)); // far -> none
  // The same ladder powered/read at a different reference scale still decodes.
  TEST_ASSERT_EQUAL_UINT8(5, buttonIndexForAdc(626, 1023));
  TEST_ASSERT_EQUAL_UINT8(12, buttonIndexForAdc(1023, 1023));
}

static void test_ladder_press_and_release(void) {
  ButtonLadder ladder;
  uint32_t t = 0;

  // Hold button 5 (~341). First sample starts debouncing.
  ButtonEvent e = ladder.sample(t, 341);
  // Not yet accepted; still "none".
  TEST_ASSERT_EQUAL(ButtonEventType::None, e.type);

  // After the debounce window, a JustPressed edge is emitted once.
  t += 100;
  e = ladder.sample(t, 341);
  TEST_ASSERT_EQUAL(ButtonEventType::JustPressed, e.type);
  TEST_ASSERT_EQUAL_UINT8(5, e.index);

  // Continued hold reports Held.
  t += 10;
  e = ladder.sample(t, 341);
  TEST_ASSERT_EQUAL(ButtonEventType::Held, e.type);
  TEST_ASSERT_EQUAL_UINT8(5, e.index);

  // Release: reading returns to rest; after debounce, JustReleased once.
  t += 10;
  ladder.sample(t, 558);
  t += 100;
  e = ladder.sample(t, 558);
  TEST_ASSERT_EQUAL(ButtonEventType::JustReleased, e.type);
}

// --- simple debouncer ------------------------------------------------------

static void test_debouncer_rejects_bounce(void) {
  ButtonDebouncer db(32);
  uint32_t t = 0;

  // Rapid bounce within the window must not latch a press.
  TEST_ASSERT_EQUAL(ButtonState::IsUp, db.sample(true, t));
  t += 5;
  TEST_ASSERT_EQUAL(ButtonState::IsUp, db.sample(false, t));
  t += 5;
  db.sample(true, t);

  // Stable press beyond the window latches exactly one JustPressed.
  t += 40;
  TEST_ASSERT_EQUAL(ButtonState::JustPressed, db.sample(true, t));
  t += 5;
  TEST_ASSERT_EQUAL(ButtonState::HeldDown, db.sample(true, t));
}

// --- long press ------------------------------------------------------------

static void test_long_press_short_click(void) {
  ButtonWithLongPress btn(10, 2000);
  uint32_t t = 0;

  // The press is only registered once the level is stable past the debounce
  // window (matching the original firmware's principled debouncer).
  TEST_ASSERT_EQUAL(LongPressButtonState::ButtonIsUp, btn.sample(true, t));
  t += 20;
  TEST_ASSERT_EQUAL(LongPressButtonState::ButtonJustDown, btn.sample(true, t));
  t += 5;
  TEST_ASSERT_EQUAL(LongPressButtonState::ButtonHeldDownShort,
                    btn.sample(true, t));
  // Release before 2000 ms -> short click (after the release debounces).
  t += 5;
  btn.sample(false, t);
  t += 20;
  TEST_ASSERT_EQUAL(LongPressButtonState::ButtonJustClickedShort,
                    btn.sample(false, t));
}

static void test_long_press_fires_once(void) {
  ButtonWithLongPress btn(10, 2000);
  uint32_t t = 0;
  btn.sample(true, t);  // JustDown
  t += 20;

  // Hold across the threshold: ButtonJustClickedLong exactly once.
  int longFires = 0;
  for (int i = 0; i < 300; ++i) {
    t += 10;
    LongPressButtonState s = btn.sample(true, t);
    if (s == LongPressButtonState::ButtonJustClickedLong) {
      ++longFires;
    }
  }
  TEST_ASSERT_EQUAL_INT(1, longFires);

  // Release afterwards -> ButtonJustReleasedLong.
  t += 5;
  btn.sample(false, t);
  t += 20;
  TEST_ASSERT_EQUAL(LongPressButtonState::ButtonJustReleasedLong,
                    btn.sample(false, t));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ladder_nearest_value);
  RUN_TEST(test_ladder_press_and_release);
  RUN_TEST(test_debouncer_rejects_bounce);
  RUN_TEST(test_long_press_short_click);
  RUN_TEST(test_long_press_fires_once);
  return UNITY_END();
}
