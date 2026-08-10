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
#include "fmq/application/ArpeggiatorBank.h"
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
  ArpeggiatorBank arpeggiators;
  clearScale(q);
  // The firmware deliberately prevents disabling the last active scale note.
  // Keep C active so note 3 can be toggled on and off in this test.
  q.channels[0].config().notes[0] = true;
  QuantizationResult r = QuantizationResult::makeZero();

  MenuOutput out = menu.update(q, arpeggiators, keyPress(3), r, 100, store);
  TEST_ASSERT_TRUE(q.channels[0].config().notes[3]);
  TEST_ASSERT_TRUE(out.persistentStateChanged);

  menu.update(q, arpeggiators, keyRelease(), r, 110, store);
  menu.update(q, arpeggiators, keyPress(3), r, 200, store);
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
  ArpeggiatorBank arpeggiators;
  clearScale(q);
  QuantizationResult r = QuantizationResult::makeZero();

  menu.update(q, arpeggiators, keyPress(11, /*shift=*/true), r, 100, store);  // select B
  menu.update(q, arpeggiators, keyRelease(true), r, 110, store);
  menu.update(q, arpeggiators, keyPress(2), r, 200, store);  // toggle note on B

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
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();

  menu.update(q, arpeggiators, keyPress(2, /*shift=*/true), r, 100, store);  // open Glide
  menu.update(q, arpeggiators, keyRelease(true), r, 110, store);
  menu.update(q, arpeggiators, keyPress(7, /*shift=*/true), r, 200, store);  // set glide = 7

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
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();

  TEST_ASSERT_EQUAL(SampleMode::TrackAndHold, q.channels[0].config().sampleMode);

  MenuOutput firstToggle = menu.update(q, arpeggiators, keyPress(4, /*shift=*/true), r, 100, store);
  TEST_ASSERT_EQUAL(SampleMode::SampleAndHold, q.channels[0].config().sampleMode);
  TEST_ASSERT_EQUAL(LedColor::Red, firstToggle.frame[config::kSampleModeButtonIndex]);

  // Releasing the key must not immediately erase the status indication.
  MenuOutput heldFeedback = menu.update(q, arpeggiators, keyRelease(true), r, 110, store);
  TEST_ASSERT_EQUAL(LedColor::Red,
                    heldFeedback.frame[config::kSampleModeButtonIndex]);

  // After the configured feedback interval the menu returns to normal, then a
  // second SHIFT+4 toggles back to Track-and-Hold.
  menu.update(q, arpeggiators, noInput(), r,
              100 + config::kBoolOptionFeedbackMs + 1, store);
  MenuOutput secondToggle = menu.update(
      q, arpeggiators, keyPress(4, /*shift=*/true), r,
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
  ArpeggiatorBank arpeggiators;
  clearScale(q);
  // Keep one anchor note active so note 1 can later be toggled back off; the
  // UI intentionally refuses to remove the final active scale note.
  q.channels[0].config().notes[0] = true;
  QuantizationResult r = QuantizationResult::makeZero();

  // Edit, then SAVE (shift => full config) into slot 4.
  menu.update(q, arpeggiators, keyPress(1), r, 100, store);   // toggle note 1
  menu.update(q, arpeggiators, keyRelease(), r, 110, store);
  MenuInput saveDown = noInput();
  saveDown.saveButton = LongPressButtonState::ButtonJustDown;
  saveDown.shiftPressed = true;
  menu.update(q, arpeggiators, saveDown, r, 200, store);       // open full-config save menu
  menu.update(q, arpeggiators, keyPress(4, true), r, 260, store);  // save into slot 4
  menu.update(q, arpeggiators, keyRelease(true), r, 270, store);
  // Production services the asynchronous EEPROM writer from the main loop.
  // The host test completes the queued write explicitly before loading it.
  store.flush();

  // The "saved" splash blocks input for ~1 s; advance past it.
  menu.update(q, arpeggiators, noInput(), r, 1400, store);

  // Change the working state, then LOAD slot 4 back.
  menu.update(q, arpeggiators, keyPress(1), r, 1450, store);    // toggle note 1 back off
  menu.update(q, arpeggiators, keyRelease(), r, 1460, store);
  TEST_ASSERT_FALSE(q.channels[0].config().notes[1]);

  MenuInput loadDown = noInput();
  loadDown.loadButton = LongPressButtonState::ButtonJustDown;
  loadDown.shiftPressed = true;
  menu.update(q, arpeggiators, loadDown, r, 1500, store);           // open full-config load
  menu.update(q, arpeggiators, keyPress(4, true), r, 1560, store);  // load slot 4

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
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();

  // Start the combo (arms the timer) then hold past five seconds.
  menu.update(q, arpeggiators, comboHeld(), r, 1000, store);
  menu.update(q, arpeggiators, comboHeld(), r, 6000, store);  // >= 5 s -> enter
  TEST_ASSERT_TRUE(menu.inCalibration());

  // Advance past the entry blink so the editor becomes active. The editor
  // now keeps green, red and amber visible together for direct balance judging.
  menu.update(q, arpeggiators, noInput(), r, 7000, store);
  MenuOutput calibrationView = menu.update(q, arpeggiators, noInput(), r, 7001, store);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Green),
                        static_cast<int>(calibrationView.frame[0]));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Red),
                        static_cast<int>(calibrationView.frame[1]));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Amber),
                        static_cast<int>(calibrationView.frame[2]));

  // Channel A / green is selected by default: note 3 sets the green step and
  // marks that ring position briefly with a dark gap.
  MenuOutput greenStepView = menu.update(q, arpeggiators, keyPress(3), r, 7100, store);
  menu.update(q, arpeggiators, keyRelease(), r, 7150, store);
  TEST_ASSERT_EQUAL_UINT8(3, menu.activeBrightness().greenStep);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Off),
                        static_cast<int>(greenStepView.frame[3]));
  MenuOutput previewRestored = menu.update(
      q, arpeggiators, noInput(), r,
      7100 + config::kCalibrationStepMarkerMs, store);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Green),
                        static_cast<int>(previewRestored.frame[3]));

  // Switch to channel B / red (SHIFT+B, index 11), then note 7 sets red.
  menu.update(q, arpeggiators, keyPress(11, /*shift=*/true), r, 7500, store);
  menu.update(q, arpeggiators, keyRelease(true), r, 7550, store);
  menu.update(q, arpeggiators, keyPress(7), r, 7600, store);
  menu.update(q, arpeggiators, keyRelease(), r, 7650, store);
  TEST_ASSERT_EQUAL_UINT8(7, menu.activeBrightness().redStep);

  // Hold SHIFT alone: arm the save timer, then cross five seconds.
  menu.update(q, arpeggiators, shiftHeld(), r, 7700, store);
  MenuOutput saved = menu.update(q, arpeggiators, shiftHeld(), r, 12700, store);  // >= 5 s

  TEST_ASSERT_TRUE(saved.persistentStateChanged);
  TEST_ASSERT_EQUAL_UINT8(7, menu.committedBrightness().redStep);
  TEST_ASSERT_EQUAL_UINT8(3, menu.committedBrightness().greenStep);
}

// Active calibration always exposes four green, four red and four amber
// comparison points, independent of which emitter is currently edited.
static void test_calibration_preview_contains_balanced_colour_samples(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();

  menu.update(q, arpeggiators, comboHeld(), r, 1000, store);
  menu.update(q, arpeggiators, comboHeld(), r, 6000, store);
  menu.update(q, arpeggiators, noInput(), r, 7000, store);
  const MenuOutput view = menu.update(q, arpeggiators, noInput(), r, 7001, store);

  uint8_t green = 0;
  uint8_t red = 0;
  uint8_t amber = 0;
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    if (view.frame[i] == LedColor::Green) ++green;
    if (view.frame[i] == LedColor::Red) ++red;
    if (view.frame[i] == LedColor::Amber) ++amber;
  }
  TEST_ASSERT_EQUAL_UINT8(4, green);
  TEST_ASSERT_EQUAL_UINT8(4, red);
  TEST_ASSERT_EQUAL_UINT8(4, amber);
}

// The platform can identify which pair of discrete channel LEDs should be used
// as the calibration editor marker: A for green, B for red.
static void test_calibration_editor_channel_tracks_selected_colour(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();

  menu.update(q, arpeggiators, comboHeld(), r, 1000, store);
  menu.update(q, arpeggiators, comboHeld(), r, 6000, store);
  TEST_ASSERT_EQUAL_UINT8(kChannelAIndex, menu.calibrationEditorChannelIndex());

  menu.update(q, arpeggiators, noInput(), r, 7000, store);
  menu.update(q, arpeggiators, keyPress(config::kChannelBButtonIndex, true), r, 7100, store);
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, menu.calibrationEditorChannelIndex());

  menu.update(q, arpeggiators, keyRelease(true), r, 7150, store);
  menu.update(q, arpeggiators, keyPress(config::kChannelAButtonIndex, true), r, 7200, store);
  TEST_ASSERT_EQUAL_UINT8(kChannelAIndex, menu.calibrationEditorChannelIndex());
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
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();

  MenuInput erase = noInput();
  erase.loadButton = LongPressButtonState::ButtonHeldDownLong;
  erase.saveButton = LongPressButtonState::ButtonJustClickedLong;
  menu.update(q, arpeggiators, erase, r, 3000, store);
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
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();

  MenuInput loadDown = noInput();
  loadDown.loadButton = LongPressButtonState::ButtonJustDown;
  loadDown.shiftPressed = true;
  MenuOutput loadMenu = menu.update(q, arpeggiators, loadDown, r, 100, store);
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(LedColor::Amber),
                          static_cast<int>(loadMenu.frame[i]));
  }

  // Empty slot 1 (index 1) falls back to C major.
  menu.update(q, arpeggiators, keyPress(1, true), r, 150, store);
  const bool expectedMajor[kNoteCount] = {
      true, false, true, false, true, true,
      false, true, false, true, false, true};
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    TEST_ASSERT_EQUAL(expectedMajor[i], q.channels[0].config().notes[i]);
    TEST_ASSERT_EQUAL(expectedMajor[i], q.channels[1].config().notes[i]);
  }

  // A valid user save in that same slot takes precedence over the factory preset.
  StoredConfiguration userState;
  for (uint8_t channel = 0; channel < kChannelCount; ++channel) {
    for (uint8_t note = 0; note < kNoteCount; ++note) {
      userState.quantizer.channels[channel].config().notes[note] = (note == 0u);
    }
  }
  userState.arpeggiators[kChannelAIndex].enabled = true;
  userState.arpeggiators[kChannelAIndex].rateIndex = 9u;
  TEST_ASSERT_TRUE(store.writeConfig(1, userState));
  store.flush();

  Menu menu2;
  menu2.begin(store);
  QuantizerState loadedUser;
  ArpeggiatorBank loadedArpeggiators;
  menu2.update(loadedUser, loadedArpeggiators, loadDown, r, 300, store);
  menu2.update(loadedUser, loadedArpeggiators, keyPress(1, true), r, 350, store);
  TEST_ASSERT_TRUE(loadedUser.channels[0].config().notes[0]);
  TEST_ASSERT_FALSE(loadedUser.channels[0].config().notes[2]);
  TEST_ASSERT_TRUE(loadedArpeggiators.enabled(kChannelAIndex));
  TEST_ASSERT_EQUAL_UINT8(9u, loadedArpeggiators.config(kChannelAIndex).rateIndex);
}

// FA-076..082 / acceptance criterion 15: unlinked A uses green scale notes and
// amber for the currently emitted note of the selected channel.
static void test_unlinked_note_ring_uses_selected_channel_colour_and_amber_current_note(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  ArpeggiatorBank arpeggiators;
  clearScale(q);
  q.channels[kChannelAIndex].config().notes[0] = true;
  q.channels[kChannelAIndex].config().notes[4] = true;
  QuantizationResult r = QuantizationResult::makeZero();
  r.channelA.nominalSemitones = 4;

  const MenuOutput out = menu.update(q, arpeggiators, noInput(), r, 100u, store);
  TEST_ASSERT_EQUAL(LedColor::Green, out.frame[0]);
  TEST_ASSERT_EQUAL(LedColor::Amber, out.frame[4]);
  TEST_ASSERT_EQUAL(LedColor::Off, out.frame[1]);
}

// FA-079..082: linked scales are amber, B's current note is red, A's current
// note is green, and A/green has deterministic priority on a collision.
static void test_linked_note_ring_colours_and_same_note_priority(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  ArpeggiatorBank arpeggiators;
  clearScale(q);
  q.channelsLinked = true;
  q.channels[kChannelAIndex].config().notes[0] = true;
  q.channels[kChannelAIndex].config().notes[4] = true;
  q.channels[kChannelAIndex].config().notes[7] = true;

  QuantizationResult separate = QuantizationResult::makeZero();
  separate.channelA.nominalSemitones = 4;
  separate.channelB.nominalSemitones = 7;
  MenuOutput out = menu.update(q, arpeggiators, noInput(), separate, 100u, store);
  TEST_ASSERT_EQUAL(LedColor::Amber, out.frame[0]);
  TEST_ASSERT_EQUAL(LedColor::Green, out.frame[4]);
  TEST_ASSERT_EQUAL(LedColor::Red, out.frame[7]);

  QuantizationResult collision = QuantizationResult::makeZero();
  collision.channelA.nominalSemitones = 4;
  collision.channelB.nominalSemitones = 4;
  out = menu.update(q, arpeggiators, noInput(), collision, 200u, store);
  TEST_ASSERT_EQUAL(LedColor::Green, out.frame[4]);
}

// FA-101/102: loading a scale edits only the selected channel when unlinked,
// but both channel configurations when linked.
static void test_scale_load_respects_selected_channel_and_link_state(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool saved[kNoteCount] = {};
  saved[2] = true;
  saved[7] = true;
  TEST_ASSERT_TRUE(store.writeScale(3u, saved));
  store.flush();

  QuantizationResult r = QuantizationResult::makeZero();
  ArpeggiatorBank arpeggiators;

  // Unlinked: select B, then load scale slot 3.
  Menu unlinkedMenu;
  unlinkedMenu.begin(store);
  QuantizerState unlinked;
  clearScale(unlinked);
  unlinked.channels[kChannelAIndex].config().notes[0] = true;
  unlinked.channels[kChannelBIndex].config().notes[0] = true;
  unlinkedMenu.update(unlinked, arpeggiators, keyPress(11u, true), r, 100u, store);
  unlinkedMenu.update(unlinked, arpeggiators, keyRelease(true), r, 110u, store);
  MenuInput load = noInput();
  load.loadButton = LongPressButtonState::ButtonJustDown;
  unlinkedMenu.update(unlinked, arpeggiators, load, r, 200u, store);
  unlinkedMenu.update(unlinked, arpeggiators, keyPress(3u), r, 250u, store);
  TEST_ASSERT_TRUE(unlinked.channels[kChannelAIndex].config().notes[0]);
  TEST_ASSERT_FALSE(unlinked.channels[kChannelAIndex].config().notes[2]);
  TEST_ASSERT_TRUE(unlinked.channels[kChannelBIndex].config().notes[2]);
  TEST_ASSERT_TRUE(unlinked.channels[kChannelBIndex].config().notes[7]);

  // Linked: the same scale load must update both channels.
  Menu linkedMenu;
  linkedMenu.begin(store);
  QuantizerState linked;
  clearScale(linked);
  linked.channelsLinked = true;
  linked.channels[kChannelAIndex].config().notes[0] = true;
  linked.channels[kChannelBIndex].config().notes[0] = true;
  linkedMenu.update(linked, arpeggiators, load, r, 400u, store);
  linkedMenu.update(linked, arpeggiators, keyPress(3u), r, 450u, store);
  for (uint8_t channel = 0u; channel < kChannelCount; ++channel) {
    TEST_ASSERT_FALSE(linked.channels[channel].config().notes[0]);
    TEST_ASSERT_TRUE(linked.channels[channel].config().notes[2]);
    TEST_ASSERT_TRUE(linked.channels[channel].config().notes[7]);
  }
}

// FA-110/111 / acceptance criterion 18: exact simultaneous long state on both
// buttons must erase all user scale and full-config slots.
static void test_exact_simultaneous_long_load_save_erases_both_slot_banks(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool notes[kNoteCount] = {};
  notes[0] = true;
  TEST_ASSERT_TRUE(store.writeScale(0u, notes));
  store.flush();
  StoredConfiguration state;
  TEST_ASSERT_TRUE(store.writeConfig(0u, state));
  store.flush();

  Menu menu;
  menu.begin(store);
  QuantizerState q;
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();
  MenuInput erase = noInput();
  erase.loadButton = LongPressButtonState::ButtonHeldDownLong;
  erase.saveButton = LongPressButtonState::ButtonHeldDownLong;
  menu.update(q, arpeggiators, erase, r, 3000u, store);
  store.flush();

  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_EQUAL_HEX16(0u, scales.bits);
  TEST_ASSERT_EQUAL_HEX16(0u, configs.bits);
}

// FA-009..014 / acceptance criterion 2: every physical note button maps to
// exactly one pitch class. Keep C as an anchor while toggling the other notes,
// then verify C itself can be toggled while another note remains active.
static void test_all_twelve_note_buttons_toggle_their_pitch_classes(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  Menu menu;
  menu.begin(store);
  QuantizerState q;
  ArpeggiatorBank arpeggiators;
  QuantizationResult r = QuantizationResult::makeZero();
  clearScale(q);
  q.channels[kChannelAIndex].config().notes[0] = true;

  uint32_t now = 100u;
  for (uint8_t note = 1u; note < kNoteCount; ++note) {
    menu.update(q, arpeggiators, keyPress(note), r, now, store);
    TEST_ASSERT_TRUE(q.channels[kChannelAIndex].config().notes[note]);
    menu.update(q, arpeggiators, keyRelease(), r, now + 1u, store);
    now += 10u;
  }

  // Leave D active so C is no longer the last selected pitch class.
  for (uint8_t note = 2u; note < kNoteCount; ++note) {
    menu.update(q, arpeggiators, keyPress(note), r, now, store);
    menu.update(q, arpeggiators, keyRelease(), r, now + 1u, store);
    now += 10u;
  }
  TEST_ASSERT_TRUE(q.channels[kChannelAIndex].config().notes[0]);
  TEST_ASSERT_TRUE(q.channels[kChannelAIndex].config().notes[1]);
  menu.update(q, arpeggiators, keyPress(0u), r, now, store);
  TEST_ASSERT_FALSE(q.channels[kChannelAIndex].config().notes[0]);
  TEST_ASSERT_TRUE(q.channels[kChannelAIndex].config().notes[1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_toggle_note);
  RUN_TEST(test_all_twelve_note_buttons_toggle_their_pitch_classes);
  RUN_TEST(test_select_channel_b);
  RUN_TEST(test_glide_submenu);
  RUN_TEST(test_sample_mode_matches_original_ui_cycle);
  RUN_TEST(test_save_and_load_config);
  RUN_TEST(test_brightness_calibration_flow);
  RUN_TEST(test_calibration_preview_contains_balanced_colour_samples);
  RUN_TEST(test_calibration_editor_channel_tracks_selected_colour);
  RUN_TEST(test_erase_all_combo);
  RUN_TEST(test_factory_config_fallback_and_user_override);
  RUN_TEST(test_unlinked_note_ring_uses_selected_channel_colour_and_amber_current_note);
  RUN_TEST(test_linked_note_ring_colours_and_same_note_priority);
  RUN_TEST(test_scale_load_respects_selected_channel_and_link_state);
  RUN_TEST(test_exact_simultaneous_long_load_save_erases_both_slot_banks);
  return UNITY_END();
}
