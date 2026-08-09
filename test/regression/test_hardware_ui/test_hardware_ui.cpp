/**
 * @file test_hardware_ui.cpp
 * Host regression or unit tests for hardware ui behaviour.
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
#include "fmq/ui/Menu.h"
#include "fmq/ui/LedFrameEncoder.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/persistence/SaveSlotStore.h"
#include "fmq/config/LedConfig.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

namespace {
RawControlInput raw(uint16_t ladder, bool shift=false, bool save=false, bool load=false) {
  return RawControlInput{ladder, shift, save, load};
}
void feed(ControlInputProcessor &controls, Menu &menu, QuantizerState &q,
          ArpeggiatorBank &arpeggiators, QuantizationResult &r,
          SaveSlotStore &store, uint32_t &t,
          const RawControlInput &value, uint32_t durationMs) {
  const uint32_t end=t+durationMs;
  while(t<=end){ menu.update(q, arpeggiators, controls.sample(t,value), r,t,store); ++t; }
}
}

static void test_original_ladder_values_decode_exactly(void) {
  const uint16_t v[13]={0,93,171,236,292,341,384,421,455,485,512,536,558};
  for(uint8_t i=0;i<12;++i) TEST_ASSERT_EQUAL_UINT8(i, buttonIndexForAdc(v[i],558));
  TEST_ASSERT_EQUAL_UINT8(kButtonLadderNoButton, buttonIndexForAdc(v[12],558));
}

static void test_simultaneous_shift_b_selects_channel_b(void) {
  FakeEeprom eep; AsyncEepromWriter writer(eep); SaveSlotStore store(eep,writer);
  Menu menu; menu.begin(store); QuantizerState q; ArpeggiatorBank arpeggiators; QuantizationResult r=QuantizationResult::makeZero();
  ControlInputProcessor controls; uint32_t t=0;
  feed(controls,menu,q,arpeggiators,r,store,t,raw(558),70);
  // Realistic gesture: modifier and B go down together, not SHIFT 30 ms earlier.
  feed(controls,menu,q,arpeggiators,r,store,t,raw(536,true),80);
  feed(controls,menu,q,arpeggiators,r,store,t,raw(558,false),80);
  feed(controls,menu,q,arpeggiators,r,store,t,raw(171),80);
  TEST_ASSERT_TRUE(q.channels[0].config().notes[2]);
  TEST_ASSERT_FALSE(q.channels[1].config().notes[2]);
}

static void test_normal_led_encoder_matches_rust_pair_format(void) {
  LedFrame frame; frame[0]=LedColor::Red; frame[1]=LedColor::Green; frame[6]=LedColor::Amber;
  uint8_t out[kTlc5947FrameBytes]; encodeLedFrame(frame,0x0800,0x0028,out);
  // Rust emits logical LEDs 6..11 then 0..5. LED 6 is first and amber:
  // ((red << 12) | green) => 0x800028.
  TEST_ASSERT_EQUAL_HEX8(0x80,out[0]); TEST_ASSERT_EQUAL_HEX8(0x00,out[1]); TEST_ASSERT_EQUAL_HEX8(0x28,out[2]);
  // Logical LED 0 is seventh and red => 0x800000.
  TEST_ASSERT_EQUAL_HEX8(0x80,out[18]); TEST_ASSERT_EQUAL_HEX8(0x00,out[19]); TEST_ASSERT_EQUAL_HEX8(0x00,out[20]);
  // Logical LED 1 is eighth and green => 0x000028.
  TEST_ASSERT_EQUAL_HEX8(0x00,out[21]); TEST_ASSERT_EQUAL_HEX8(0x00,out[22]); TEST_ASSERT_EQUAL_HEX8(0x28,out[23]);
}

int main(void){ UNITY_BEGIN(); RUN_TEST(test_original_ladder_values_decode_exactly); RUN_TEST(test_simultaneous_shift_b_selects_channel_b); RUN_TEST(test_normal_led_encoder_matches_rust_pair_format); return UNITY_END(); }
