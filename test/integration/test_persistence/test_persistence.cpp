/**
 * @file test_persistence.cpp
 * Host regression or unit tests for persistence behaviour.
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
#include "TestScale.h"
#include "fmq/persistence/LiveStateStore.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/persistence/SaveSlotStore.h"
#include "fmq/persistence/Serialization.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

// State round-trips exactly through encode/decode.
static void test_state_roundtrip(void) {
  QuantizerState s;
  s.channelsLinked = true;
  s.channelBMode = PitchMode::Relative;
  ChannelConfig ca = ChannelConfig::makeDefault();
  const int pcs[] = {0, 3, 7, 11};
  fmqtest::makeScale(ca.notes, pcs, 4);
  ca.sampleMode = SampleMode::SampleAndHold;
  ca.glideAmount = 5;
  ca.triggerDelayAmount = 9;
  ca.preShift = -3;
  ca.scaleShift = 2;
  ca.postShift = -7;
  s.channels[0].setConfig(ca);

  uint8_t bytes[kStateBytes];
  encodeState(s, bytes);
  QuantizerState d = decodeState(bytes);

  TEST_ASSERT_TRUE(d.channelsLinked);
  TEST_ASSERT_EQUAL(PitchMode::Relative, d.channelBMode);
  const ChannelConfig &dc = d.channels[0].config();
  for (int i = 0; i < 12; ++i) TEST_ASSERT_EQUAL(ca.notes[i], dc.notes[i]);
  TEST_ASSERT_EQUAL(SampleMode::SampleAndHold, dc.sampleMode);
  TEST_ASSERT_EQUAL_UINT8(5, dc.glideAmount);
  TEST_ASSERT_EQUAL_UINT8(9, dc.triggerDelayAmount);
  TEST_ASSERT_EQUAL_INT8(-3, dc.preShift);
  TEST_ASSERT_EQUAL_INT8(2, dc.scaleShift);
  TEST_ASSERT_EQUAL_INT8(-5, dc.postShift);
}

// A blank EEPROM reports no occupied slots.
static void test_blank_slots_empty(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_EQUAL_HEX16(0, scales.bits);
  TEST_ASSERT_EQUAL_HEX16(0, configs.bits);
}

// A saved scale can be read back and marks its slot occupied.
static void test_scale_save_load(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool notes[12];
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  fmqtest::makeScale(notes, pcs, 7);

  store.writeScale(3, notes);
  store.flush();

  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_TRUE(scales.get(3));
  TEST_ASSERT_FALSE(scales.get(2));

  bool loaded[12];
  TEST_ASSERT_TRUE(store.readScale(3, loaded));
  for (int i = 0; i < 12; ++i) TEST_ASSERT_EQUAL(notes[i], loaded[i]);
}

// A corrupted slot reads back as empty rather than returning garbage.
static void test_corruption_is_detected(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool notes[12];
  const int pcs[] = {1, 6, 10};
  fmqtest::makeScale(notes, pcs, 3);
  store.writeScale(5, notes);
  store.flush();

  // Flip a bit inside the payload of slot 5.
  const uint16_t addr = kScaleRegionBase + 5 * kScaleSlotSize + 1;
  eep.corruptByte(addr, static_cast<uint8_t>(eep.readByte(addr) ^ 0x01));

  bool loaded[12];
  TEST_ASSERT_FALSE(store.readScale(5, loaded));
  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_FALSE(scales.get(5));
}

// Erase-all clears every slot.
static void test_erase_all(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool notes[12];
  const int pcs[] = {0};
  fmqtest::makeScale(notes, pcs, 1);
  store.writeScale(0, notes);
  store.flush();
  QuantizerState s;
  store.writeConfig(1, s);
  store.flush();

  store.eraseAll();
  store.flush();

  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_EQUAL_HEX16(0, scales.bits);
  TEST_ASSERT_EQUAL_HEX16(0, configs.bits);
}

// Live store returns defaults on blank EEPROM.
static void test_live_defaults_when_blank(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  LiveStateStore live(eep, writer);
  LiveState ls;
  TEST_ASSERT_FALSE(live.load(ls));
  TEST_ASSERT_FALSE(ls.state.channelsLinked);
  TEST_ASSERT_EQUAL(PitchMode::Absolute, ls.state.channelBMode);
  TEST_ASSERT_EQUAL_UINT8(BrightnessCalibration::kUseLegacyDefault,
                          ls.brightness.redStep);
}

// Live store round-trips the newest committed state across a "reboot".
static void test_live_roundtrip_and_newest_wins(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  {
    LiveStateStore live(eep, writer);
    LiveState ls;
    live.load(ls);  // primes cursor

    // Commit several evolving states.
    for (int i = 1; i <= 5; ++i) {
      QuantizerState s;
      s.channels[0].config().preShift = static_cast<int8_t>(i);
      BrightnessCalibration b{static_cast<uint8_t>(i % 12),
                              static_cast<uint8_t>((i + 3) % 12)};
      live.commit(s, b);
      live.flush();
    }
  }

  // Fresh store (simulated power cycle) must recover the last commit (i == 5).
  LiveStateStore live2(eep, writer);
  LiveState ls;
  TEST_ASSERT_TRUE(live2.load(ls));
  TEST_ASSERT_EQUAL_INT8(5, ls.state.channels[0].config().preShift);
  TEST_ASSERT_EQUAL_UINT8(5 % 12, ls.brightness.redStep);
  TEST_ASSERT_EQUAL_UINT8((5 + 3) % 12, ls.brightness.greenStep);
}

// Commits spread across multiple physical slots (wear-levelling).
static void test_live_wear_levelling_spreads_writes(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  LiveStateStore live(eep, writer);
  LiveState ls;
  live.load(ls);

  // Commit more times than there are slots; the ring must wrap.
  for (int i = 0; i < kLiveSlotCount + 3; ++i) {
    QuantizerState s;
    s.channels[0].config().postShift = static_cast<int8_t>(i % 20);
    live.commit(s, BrightnessCalibration::makeDefault());
    live.flush();
  }

  // Count how many distinct slots now carry a valid marker: should be all of
  // them, proving writes were distributed rather than hammering one cell.
  int occupied = 0;
  for (int i = 0; i < kLiveSlotCount; ++i) {
    if (eep.readByte(kLiveRingBase + i * kLiveSlotSize) == kRecordMarker) {
      ++occupied;
    }
  }
  TEST_ASSERT_EQUAL_INT(kLiveSlotCount, occupied);

  // And the newest value is still recoverable.
  LiveStateStore live2(eep, writer);
  LiveState ls2;
  TEST_ASSERT_TRUE(live2.load(ls2));
  TEST_ASSERT_EQUAL_INT8(6, ls2.state.channels[0].config().postShift);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_state_roundtrip);
  RUN_TEST(test_blank_slots_empty);
  RUN_TEST(test_scale_save_load);
  RUN_TEST(test_corruption_is_detected);
  RUN_TEST(test_erase_all);
  RUN_TEST(test_live_defaults_when_blank);
  RUN_TEST(test_live_roundtrip_and_newest_wins);
  RUN_TEST(test_live_wear_levelling_spreads_writes);
  return UNITY_END();
}
