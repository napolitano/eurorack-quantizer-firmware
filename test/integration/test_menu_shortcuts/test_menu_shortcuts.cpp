/**
 * @file test_menu_shortcuts.cpp
 * One-test-per-command verification of all twelve SHIFT + note shortcuts.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "FakeEeprom.h"
#include "fmq/ui/Menu.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

namespace {
struct Fixture {
  FakeEeprom eeprom;
  AsyncEepromWriter writer;
  SaveSlotStore store;
  Menu menu;
  QuantizerState quantizer;
  QuantizationResult result;
  Fixture() : writer(eeprom), store(eeprom, writer), result(QuantizationResult::makeZero()) {
    menu.begin(store);
  }
};

MenuInput idle() {
  MenuInput in{};
  in.keyEvent = ButtonEvent::none();
  in.loadButton = LongPressButtonState::ButtonIsUp;
  in.saveButton = LongPressButtonState::ButtonIsUp;
  in.shiftPressed = false;
  return in;
}
MenuInput press(uint8_t index, bool shift=true) {
  MenuInput in=idle(); in.keyEvent=ButtonEvent::pressed(index); in.shiftPressed=shift; return in;
}
MenuInput release(bool shift=true) {
  MenuInput in=idle(); in.keyEvent=ButtonEvent::released(); in.shiftPressed=shift; return in;
}
void onlyC(ChannelConfig &cfg) {
  for(uint8_t i=0;i<kNoteCount;++i) cfg.notes[i]=false;
  cfg.notes[0]=true;
}
void openScalarAndSet(Fixture &f, uint8_t command, uint8_t value) {
  f.menu.update(f.quantizer, press(command,true), f.result, 100u, f.store);
  f.menu.update(f.quantizer, release(true), f.result, 110u, f.store);
  f.menu.update(f.quantizer, press(value,true), f.result, 200u, f.store);
}
}

// FA-083 SHIFT+C
static void test_shift_c_rotates_scale_down_one_pitch_class(void) {
  Fixture f; onlyC(f.quantizer.channels[0].config());
  f.menu.update(f.quantizer, press(0), f.result, 100u, f.store);
  TEST_ASSERT_FALSE(f.quantizer.channels[0].config().notes[0]);
  TEST_ASSERT_TRUE(f.quantizer.channels[0].config().notes[11]);
}

// FA-084 SHIFT+C#
static void test_shift_csharp_rotates_scale_up_one_pitch_class(void) {
  Fixture f; onlyC(f.quantizer.channels[0].config());
  f.menu.update(f.quantizer, press(1), f.result, 100u, f.store);
  TEST_ASSERT_FALSE(f.quantizer.channels[0].config().notes[0]);
  TEST_ASSERT_TRUE(f.quantizer.channels[0].config().notes[1]);
}

// FA-085 SHIFT+D
static void test_shift_d_opens_glide_and_accepts_0_to_11_value(void) {
  Fixture f; openScalarAndSet(f,2,11);
  TEST_ASSERT_EQUAL_UINT8(11, f.quantizer.channels[0].config().glideAmount);
}

// FA-086 SHIFT+D#
static void test_shift_dsharp_opens_delay_and_accepts_0_to_11_value(void) {
  Fixture f; openScalarAndSet(f,3,11);
  TEST_ASSERT_EQUAL_UINT8(11, f.quantizer.channels[0].config().triggerDelayAmount);
}

// FA-087 SHIFT+E
static void test_shift_e_toggles_track_and_sample(void) {
  Fixture f;
  TEST_ASSERT_EQUAL(SampleMode::TrackAndHold, f.quantizer.channels[0].config().sampleMode);
  f.menu.update(f.quantizer, press(4), f.result, 100u, f.store);
  TEST_ASSERT_EQUAL(SampleMode::SampleAndHold, f.quantizer.channels[0].config().sampleMode);
}

// FA-088 SHIFT+F
static void test_shift_f_opens_post_shift_and_maps_b_to_minus1(void) {
  Fixture f; openScalarAndSet(f,5,11);
  TEST_ASSERT_EQUAL_INT8(-1, f.quantizer.channels[0].config().postShift);
}

// FA-089 SHIFT+F#
static void test_shift_fsharp_opens_scale_shift_and_maps_g_to_minus5(void) {
  Fixture f; openScalarAndSet(f,6,7);
  TEST_ASSERT_EQUAL_INT8(-5, f.quantizer.channels[0].config().scaleShift);
}

// FA-090 SHIFT+G
static void test_shift_g_opens_pre_shift_and_maps_fsharp_to_plus6(void) {
  Fixture f; openScalarAndSet(f,7,6);
  TEST_ASSERT_EQUAL_INT8(6, f.quantizer.channels[0].config().preShift);
}

// FA-091 SHIFT+G#
static void test_shift_gsharp_toggles_channel_b_relative_mode(void) {
  Fixture f;
  TEST_ASSERT_EQUAL(PitchMode::Absolute, f.quantizer.channelBMode);
  f.menu.update(f.quantizer, press(8), f.result, 100u, f.store);
  TEST_ASSERT_EQUAL(PitchMode::Relative, f.quantizer.channelBMode);
}

// FA-092 / FA-032..034 SHIFT+A
static void test_shift_a_links_and_copies_entire_channel_a_config_to_b(void) {
  Fixture f;
  ChannelConfig &a=f.quantizer.channels[0].config();
  a.glideAmount=7; a.triggerDelayAmount=9; a.preShift=-3; a.scaleShift=4;
  a.postShift=-2; a.sampleMode=SampleMode::SampleAndHold; onlyC(a);
  ChannelConfig &b=f.quantizer.channels[1].config();
  b.glideAmount=1; b.preShift=6;
  f.menu.update(f.quantizer, press(9), f.result, 100u, f.store);
  TEST_ASSERT_TRUE(f.quantizer.channelsLinked);
  const ChannelConfig &copied=f.quantizer.channels[1].config();
  TEST_ASSERT_EQUAL_UINT8(7,copied.glideAmount);
  TEST_ASSERT_EQUAL_UINT8(9,copied.triggerDelayAmount);
  TEST_ASSERT_EQUAL_INT8(-3,copied.preShift);
  TEST_ASSERT_EQUAL_INT8(4,copied.scaleShift);
  TEST_ASSERT_EQUAL_INT8(-2,copied.postShift);
  TEST_ASSERT_EQUAL(SampleMode::SampleAndHold,copied.sampleMode);
  for(uint8_t i=0;i<kNoteCount;++i) TEST_ASSERT_EQUAL(a.notes[i],copied.notes[i]);
}

// FA-093 SHIFT+A#
static void test_shift_asharp_selects_a_for_subsequent_note_edit(void) {
  Fixture f;
  f.menu.update(f.quantizer, press(11), f.result, 50u, f.store); // first select B
  f.menu.update(f.quantizer, release(true), f.result, 60u, f.store);
  f.menu.update(f.quantizer, press(10), f.result, 100u, f.store); // then A
  f.menu.update(f.quantizer, release(true), f.result, 110u, f.store);
  const bool beforeB=f.quantizer.channels[1].config().notes[1];
  f.menu.update(f.quantizer, press(1,false), f.result, 200u, f.store);
  TEST_ASSERT_FALSE(f.quantizer.channels[0].config().notes[1]);
  TEST_ASSERT_EQUAL(beforeB,f.quantizer.channels[1].config().notes[1]);
}

// FA-094 SHIFT+B
static void test_shift_b_selects_b_for_subsequent_note_edit(void) {
  Fixture f;
  const bool beforeA=f.quantizer.channels[0].config().notes[1];
  f.menu.update(f.quantizer, press(11), f.result, 100u, f.store);
  f.menu.update(f.quantizer, release(true), f.result, 110u, f.store);
  f.menu.update(f.quantizer, press(1,false), f.result, 200u, f.store);
  TEST_ASSERT_EQUAL(beforeA,f.quantizer.channels[0].config().notes[1]);
  TEST_ASSERT_FALSE(f.quantizer.channels[1].config().notes[1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_shift_c_rotates_scale_down_one_pitch_class);
  RUN_TEST(test_shift_csharp_rotates_scale_up_one_pitch_class);
  RUN_TEST(test_shift_d_opens_glide_and_accepts_0_to_11_value);
  RUN_TEST(test_shift_dsharp_opens_delay_and_accepts_0_to_11_value);
  RUN_TEST(test_shift_e_toggles_track_and_sample);
  RUN_TEST(test_shift_f_opens_post_shift_and_maps_b_to_minus1);
  RUN_TEST(test_shift_fsharp_opens_scale_shift_and_maps_g_to_minus5);
  RUN_TEST(test_shift_g_opens_pre_shift_and_maps_fsharp_to_plus6);
  RUN_TEST(test_shift_gsharp_toggles_channel_b_relative_mode);
  RUN_TEST(test_shift_a_links_and_copies_entire_channel_a_config_to_b);
  RUN_TEST(test_shift_asharp_selects_a_for_subsequent_note_edit);
  RUN_TEST(test_shift_b_selects_b_for_subsequent_note_edit);
  return UNITY_END();
}
