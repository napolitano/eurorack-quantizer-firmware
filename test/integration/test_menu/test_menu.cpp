/**
 * @file test_menu.cpp
 * Host regression or unit tests for menu behaviour.
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
#include "fmq/ui/Menu.h"
#include "fmq/config/UiConfig.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/persistence/SaveSlotStore.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

namespace {

// Convenience builders for one cycle of menu input.
MenuInput noInput() {
  MenuInput in;
  in.keyEvent = ButtonEvent::none();
  in.loadButton = LongPressButtonState::ButtonIsUp;
  in.saveButton = LongPressButtonState::ButtonIsUp;
  in.shiftPressed = false;
  return in;
}

MenuInput keyPress(uint8_t n, bool shift = false) {
  MenuInput in = noInput();
  in.keyEvent = ButtonEvent::pressed(n);
  in.shiftPressed = shift;
  return in;
}

MenuInput keyRelease(bool shift = false) {
  MenuInput in = noInput();
  in.keyEvent = ButtonEvent::released();
  in.shiftPressed = shift;
  return in;
}

MenuInput shiftHeld() {
  MenuInput in = noInput();
  in.shiftPressed = true;
  return in;
}

void clearScale(QuantizerState &q) {
  for (uint8_t c = 0; c < 2; ++c) {
    for (uint8_t n = 0; n < 12; ++n) q.channels[c].config().notes[n] = false;
  }
}

// Combo input: SHIFT + LOAD + SAVE all held.
MenuInput comboHeld() {
  MenuInput in = noInput();
  in.shiftPressed = true;
  in.loadButton = LongPressButtonState::ButtonHeldDownShort;
  in.saveButton = LongPressButtonState::ButtonHeldDownShort;
  return in;
}

}  // namespace

// Toggling a note flips scale membership and reports a persistent change.
static void test_toggle_note(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  clearScale(q);
  // The firmware deliberately prevents disabling the last active scale note.
  // Keep C active so note 3 can be toggled on and off in this test.
  q.channels[0].config().notes[0] = true;
  QuantizationResult r = QuantizationResult::makeZero();

  MenuOutput out = menu.update(q, keyPress(3), r, 100, store);
  TEST_ASSERT_TRUE(q.channels[0].config().notes[3]);
  TEST_ASSERT_TRUE(out.persistentStateChanged);

  menu.update(q, keyRelease(), r, 110, store);
  menu.update(q, keyPress(3), r, 200, store);
  TEST_ASSERT_FALSE(q.channels[0].config().notes[3]);
}

// SHIFT+B selects channel B; a later note toggle then edits channel B.
static void test_select_channel_b(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  clearScale(q);
  QuantizationResult r = QuantizationResult::makeZero();

  menu.update(q, keyPress(11, /*shift=*/true), r, 100, store);  // select B
  menu.update(q, keyRelease(true), r, 110, store);
  menu.update(q, keyPress(2), r, 200, store);  // toggle note on B

  TEST_ASSERT_TRUE(q.channels[1].config().notes[2]);
  TEST_ASSERT_FALSE(q.channels[0].config().notes[2]);
}

// The glide sub-menu sets the glide amount from a note-button index.
static void test_glide_submenu(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  QuantizationResult r = QuantizationResult::makeZero();

  menu.update(q, keyPress(2, /*shift=*/true), r, 100, store);  // open Glide
  menu.update(q, keyRelease(true), r, 110, store);
  menu.update(q, keyPress(7, /*shift=*/true), r, 200, store);  // set glide = 7

  TEST_ASSERT_EQUAL_UINT8(7, q.channels[0].config().glideAmount);
}


// Factory behaviour matches the Rust original: Track-and-Hold is the default
// and SHIFT+4 toggles only Track-and-Hold <-> Sample-and-Hold.
static void test_sample_mode_matches_original_ui_cycle(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  QuantizationResult r = QuantizationResult::makeZero();

  TEST_ASSERT_EQUAL(SampleMode::TrackAndHold, q.channels[0].config().sampleMode);

  MenuOutput firstToggle = menu.update(q, keyPress(4, /*shift=*/true), r, 100, store);
  TEST_ASSERT_EQUAL(SampleMode::SampleAndHold, q.channels[0].config().sampleMode);
  TEST_ASSERT_EQUAL(LedColor::Red, firstToggle.frame[config::kSampleModeButtonIndex]);

  // Releasing the key must not immediately erase the status indication.
  MenuOutput heldFeedback = menu.update(q, keyRelease(true), r, 110, store);
  TEST_ASSERT_EQUAL(LedColor::Red,
                    heldFeedback.frame[config::kSampleModeButtonIndex]);

  // After the configured feedback interval the menu returns to normal, then a
  // second SHIFT+4 toggles back to Track-and-Hold.
  menu.update(q, noInput(), r,
              100 + config::kBoolOptionFeedbackMs + 1, store);
  MenuOutput secondToggle = menu.update(
      q, keyPress(4, /*shift=*/true), r,
      100 + config::kBoolOptionFeedbackMs + 10, store);
  TEST_ASSERT_EQUAL(SampleMode::TrackAndHold, q.channels[0].config().sampleMode);
  TEST_ASSERT_EQUAL(LedColor::Green, secondToggle.frame[config::kSampleModeButtonIndex]);
}

// A full config saved to a slot can be loaded back after edits.
static void test_save_and_load_config(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  clearScale(q);
  // Keep one anchor note active so note 1 can later be toggled back off; the
  // UI intentionally refuses to remove the final active scale note.
  q.channels[0].config().notes[0] = true;
  QuantizationResult r = QuantizationResult::makeZero();

  // Edit, then SAVE (shift => full config) into slot 4.
  menu.update(q, keyPress(1), r, 100, store);   // toggle note 1
  menu.update(q, keyRelease(), r, 110, store);
  MenuInput saveDown = noInput();
  saveDown.saveButton = LongPressButtonState::ButtonJustDown;
  saveDown.shiftPressed = true;
  menu.update(q, saveDown, r, 200, store);       // open full-config save menu
  menu.update(q, keyPress(4, true), r, 260, store);  // save into slot 4
  menu.update(q, keyRelease(true), r, 270, store);
  // Production services the asynchronous EEPROM writer from the main loop.
  // The host test completes the queued write explicitly before loading it.
  store.flush();

  // The "saved" splash blocks input for ~1 s; advance past it.
  menu.update(q, noInput(), r, 1400, store);

  // Change the working state, then LOAD slot 4 back.
  menu.update(q, keyPress(1), r, 1450, store);    // toggle note 1 back off
  menu.update(q, keyRelease(), r, 1460, store);
  TEST_ASSERT_FALSE(q.channels[0].config().notes[1]);

  MenuInput loadDown = noInput();
  loadDown.loadButton = LongPressButtonState::ButtonJustDown;
  loadDown.shiftPressed = true;
  menu.update(q, loadDown, r, 1500, store);           // open full-config load
  menu.update(q, keyPress(4, true), r, 1560, store);  // load slot 4

  TEST_ASSERT_TRUE(q.channels[0].config().notes[1]);
}

// Holding SHIFT+LOAD+SAVE for five seconds enters brightness calibration;
// note keys then set the red/green levels and a five-second SHIFT hold saves.
static void test_brightness_calibration_flow(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  QuantizationResult r = QuantizationResult::makeZero();

  // Start the combo (arms the timer) then hold past five seconds.
  menu.update(q, comboHeld(), r, 1000, store);
  menu.update(q, comboHeld(), r, 6000, store);  // >= 5 s -> enter
  TEST_ASSERT_TRUE(menu.inCalibration());

  // Advance past the entry blink so the editor becomes active.
  // The transition cycle finishes the entry animation; the following cycle
  // renders the active calibration bar.
  menu.update(q, noInput(), r, 7000, store);
  MenuOutput calibrationView = menu.update(q, noInput(), r, 7001, store);
  const uint8_t initialGreenStep = menu.activeBrightness().greenDisplayStep();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Green),
                        static_cast<int>(calibrationView.frame[initialGreenStep]));

  // Channel A / green is selected by default: note 3 sets the green step.
  MenuOutput greenStepView = menu.update(q, keyPress(3), r, 7100, store);
  menu.update(q, keyRelease(), r, 7150, store);
  TEST_ASSERT_EQUAL_UINT8(3, menu.activeBrightness().greenStep);
  for (uint8_t i = 0; i <= 3; ++i) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Green),
                          static_cast<int>(greenStepView.frame[i]));
  }
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Off),
                        static_cast<int>(greenStepView.frame[4]));

  // Switch to channel B / red (SHIFT+B, index 11), then note 7 sets red.
  menu.update(q, keyPress(11, /*shift=*/true), r, 7200, store);
  menu.update(q, keyRelease(true), r, 7250, store);
  menu.update(q, keyPress(7), r, 7300, store);
  menu.update(q, keyRelease(), r, 7350, store);
  TEST_ASSERT_EQUAL_UINT8(7, menu.activeBrightness().redStep);

  // Hold SHIFT alone: arm the save timer, then cross five seconds.
  menu.update(q, shiftHeld(), r, 7400, store);
  MenuOutput saved = menu.update(q, shiftHeld(), r, 12500, store);  // >= 5 s

  TEST_ASSERT_TRUE(saved.persistentStateChanged);
  TEST_ASSERT_EQUAL_UINT8(7, menu.committedBrightness().redStep);
  TEST_ASSERT_EQUAL_UINT8(3, menu.committedBrightness().greenStep);
}

// The erase-all combo (LOAD+SAVE long, no SHIFT) clears saved slots.
static void test_erase_all_combo(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool notes[12] = {true, false, true, false, false, false,
                    false, false, false, false, false, false};
  store.writeScale(0, notes);
  store.flush();

  Menu menu;
  menu.begin(store);
  QuantizerState q;
  QuantizationResult r = QuantizationResult::makeZero();

  MenuInput erase = noInput();
  erase.loadButton = LongPressButtonState::ButtonHeldDownLong;
  erase.saveButton = LongPressButtonState::ButtonJustClickedLong;
  menu.update(q, erase, r, 3000, store);
  store.flush();

  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_EQUAL_HEX16(0, scales.bits);
}

// Empty full-config slots expose firmware factory presets; a user save in the
// same slot overrides the fallback without changing the menu gesture.
static void test_factory_config_fallback_and_user_override(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  QuantizationResult r = QuantizationResult::makeZero();

  MenuInput loadDown = noInput();
  loadDown.loadButton = LongPressButtonState::ButtonJustDown;
  loadDown.shiftPressed = true;
  MenuOutput loadMenu = menu.update(q, loadDown, r, 100, store);
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Amber),
                          static_cast<int>(loadMenu.frame[i]));
  }

  // Empty slot 1 (index 1) falls back to C major.
  menu.update(q, keyPress(1, true), r, 150, store);
  const bool expectedMajor[kNoteCount] = {
      true, false, true, false, true, true,
      false, true, false, true, false, true};
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    TEST_ASSERT_EQUAL(expectedMajor[i], q.channels[0].config().notes[i]);
    TEST_ASSERT_EQUAL(expectedMajor[i], q.channels[1].config().notes[i]);
  }

  // A valid user save in that same slot takes precedence over the factory preset.
  QuantizerState userState;
  for (uint8_t channel = 0; channel < kChannelCount; ++channel) {
    for (uint8_t note = 0; note < kNoteCount; ++note) {
      userState.channels[channel].config().notes[note] = (note == 0u);
    }
  }
  TEST_ASSERT_TRUE(store.writeConfig(1, userState));
  store.flush();

  Menu menu2;
  menu2.begin(store);
  QuantizerState loadedUser;
  menu2.update(loadedUser, loadDown, r, 300, store);
  menu2.update(loadedUser, keyPress(1, true), r, 350, store);
  TEST_ASSERT_TRUE(loadedUser.channels[0].config().notes[0]);
  TEST_ASSERT_FALSE(loadedUser.channels[0].config().notes[2]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_toggle_note);
  RUN_TEST(test_select_channel_b);
  RUN_TEST(test_glide_submenu);
  RUN_TEST(test_sample_mode_matches_original_ui_cycle);
  RUN_TEST(test_save_and_load_config);
  RUN_TEST(test_brightness_calibration_flow);
  RUN_TEST(test_erase_all_combo);
  RUN_TEST(test_factory_config_fallback_and_user_override);
  return UNITY_END();
}
