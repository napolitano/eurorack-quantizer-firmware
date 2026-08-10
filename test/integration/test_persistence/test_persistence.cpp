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
#include "fmq/persistence/Crc16.h"
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


// Complete musical state round-trips Quantizer, per-channel Arpeggiator data
// selected channel and persistent UI layer without transient runtime phase.
static void test_stored_configuration_roundtrip(void) {
  StoredConfiguration source;
  source.selectedChannelIndex = kChannelBIndex;
  source.uiLayer = UiLayer::Arpeggiator;
  source.quantizer.channelBMode = PitchMode::Relative;
  source.quantizer.channels[kChannelAIndex].config().glideAmount = 7u;

  ArpeggiatorConfig a = ArpeggiatorConfig::makeDefault();
  a.enabled = true;
  a.rateIndex = 10u;
  a.pattern = ArpeggiatorPattern::OutsideIn;
  a.shape = ArpeggiatorShape::Seventh1357;
  a.length = 7u;
  a.range = 3u;
  a.stepTrigger = true;
  a.syncMode = ArpeggiatorSyncMode::Clock;
  a.swing = 9u;
  source.arpeggiators[kChannelAIndex] = a;

  ArpeggiatorConfig b = ArpeggiatorConfig::makeDefault();
  b.enabled = true;
  b.rateIndex = 2u;
  b.pattern = ArpeggiatorPattern::DownUp;
  b.shape = ArpeggiatorShape::OneFourFive;
  b.length = 5u;
  b.range = 2u;
  b.syncMode = ArpeggiatorSyncMode::Reset;
  b.swing = 4u;
  source.arpeggiators[kChannelBIndex] = b;

  uint8_t bytes[kStoredConfigurationBytes];
  encodeStoredConfiguration(source, bytes);
  const StoredConfiguration decoded = decodeStoredConfiguration(bytes);

  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, decoded.selectedChannelIndex);
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, decoded.uiLayer);
  TEST_ASSERT_EQUAL(PitchMode::Relative, decoded.quantizer.channelBMode);
  TEST_ASSERT_EQUAL_UINT8(7u, decoded.quantizer.channels[kChannelAIndex].config().glideAmount);
  const ArpeggiatorConfig &da = decoded.arpeggiators[kChannelAIndex];
  TEST_ASSERT_TRUE(da.enabled);
  TEST_ASSERT_EQUAL_UINT8(10u, da.rateIndex);
  TEST_ASSERT_EQUAL(ArpeggiatorPattern::OutsideIn, da.pattern);
  TEST_ASSERT_EQUAL(ArpeggiatorShape::Seventh1357, da.shape);
  TEST_ASSERT_EQUAL_UINT8(7u, da.length);
  TEST_ASSERT_EQUAL_UINT8(3u, da.range);
  TEST_ASSERT_TRUE(da.stepTrigger);
  TEST_ASSERT_EQUAL(ArpeggiatorSyncMode::Clock, da.syncMode);
  TEST_ASSERT_EQUAL_UINT8(9u, da.swing);
  const ArpeggiatorConfig &db = decoded.arpeggiators[kChannelBIndex];
  TEST_ASSERT_EQUAL_UINT8(2u, db.rateIndex);
  TEST_ASSERT_EQUAL(ArpeggiatorPattern::DownUp, db.pattern);
  TEST_ASSERT_EQUAL(ArpeggiatorSyncMode::Reset, db.syncMode);
}


static void test_stored_configuration_ui_layer_roundtrips_with_arp_off(void) {
  StoredConfiguration source;
  source.selectedChannelIndex = kChannelBIndex;
  source.uiLayer = UiLayer::Arpeggiator;
  TEST_ASSERT_FALSE(source.arpeggiators[kChannelAIndex].enabled);
  TEST_ASSERT_FALSE(source.arpeggiators[kChannelBIndex].enabled);

  uint8_t bytes[kStoredConfigurationBytes];
  encodeStoredConfiguration(source, bytes);
  const StoredConfiguration decoded = decodeStoredConfiguration(bytes);

  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, decoded.uiLayer);
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, decoded.selectedChannelIndex);
  TEST_ASSERT_FALSE(decoded.arpeggiators[kChannelAIndex].enabled);
  TEST_ASSERT_FALSE(decoded.arpeggiators[kChannelBIndex].enabled);
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
  StoredConfiguration s;
  store.writeConfig(1, s);
  store.flush();

  store.eraseAll();
  store.flush();

  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_EQUAL_HEX16(0, scales.bits);
  TEST_ASSERT_EQUAL_HEX16(0, configs.bits);
}


static void test_v4_full_config_is_read_and_migrates_enabled_arp_layer(void) {
  FakeEeprom eep;
  StoredConfiguration legacy;
  legacy.selectedChannelIndex = kChannelBIndex;
  legacy.arpeggiators[kChannelBIndex].enabled = true;

  uint8_t payload[kStoredConfigurationBytes];
  encodeStoredConfiguration(legacy, payload);
  // v4 had no UI-layer bit; explicitly clear it to characterize the old
  // encoding even if this test fixture is changed later.
  payload[kStoredSelectedChannelOffset] &= 0x7Fu;

  uint8_t framed[2u + kStoredConfigurationBytes];
  framed[0] = kRecordMarker;
  framed[1] = kPreviousSaveFormatVersion;
  for (uint8_t i = 0u; i < kStoredConfigurationBytes; ++i) {
    framed[2u + i] = payload[i];
  }
  const uint16_t crc = crc16Ccitt(framed, sizeof(framed));
  const uint16_t address = static_cast<uint16_t>(
      kConfigRegionBase + 2u * kConfigSlotSize);
  eep.writeByte(address, kRecordMarker);
  eep.writeByte(static_cast<uint16_t>(address + 1u), kPreviousSaveFormatVersion);
  for (uint8_t i = 0u; i < kStoredConfigurationBytes; ++i) {
    eep.writeByte(static_cast<uint16_t>(address + 2u + i), payload[i]);
  }
  eep.writeByte(static_cast<uint16_t>(address + 2u + kStoredConfigurationBytes),
                static_cast<uint8_t>(crc >> 8u));
  eep.writeByte(static_cast<uint16_t>(address + 3u + kStoredConfigurationBytes),
                static_cast<uint8_t>(crc));

  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  StoredConfiguration restored;
  TEST_ASSERT_TRUE(store.readConfig(2u, restored));
  TEST_ASSERT_TRUE(restored.arpeggiators[kChannelBIndex].enabled);
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, restored.selectedChannelIndex);
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, restored.uiLayer);
}

// Live store returns defaults on blank EEPROM.
static void test_live_defaults_when_blank(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  LiveStateStore live(eep, writer);
  LiveState ls;
  TEST_ASSERT_FALSE(live.load(ls));
  TEST_ASSERT_FALSE(ls.configuration.quantizer.channelsLinked);
  TEST_ASSERT_EQUAL(PitchMode::Absolute, ls.configuration.quantizer.channelBMode);
  TEST_ASSERT_EQUAL_UINT8(BrightnessCalibration::kUseLegacyDefault,
                          ls.brightness.redStep);
  TEST_ASSERT_EQUAL(UiLayer::Quantizer, ls.uiLayer);
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
      StoredConfiguration s;
      s.quantizer.channels[0].config().preShift = static_cast<int8_t>(i);
      s.selectedChannelIndex = kChannelBIndex;
      s.arpeggiators[kChannelBIndex].enabled = true;
      s.arpeggiators[kChannelBIndex].rateIndex = static_cast<uint8_t>(i % 12);
      BrightnessCalibration b{static_cast<uint8_t>(i % 12),
                              static_cast<uint8_t>((i + 3) % 12)};
      live.commit(s, b, UiLayer::Arpeggiator);
      live.flush();
    }
  }

  // Fresh store (simulated power cycle) must recover the last commit (i == 5).
  LiveStateStore live2(eep, writer);
  LiveState ls;
  TEST_ASSERT_TRUE(live2.load(ls));
  TEST_ASSERT_EQUAL_INT8(5, ls.configuration.quantizer.channels[0].config().preShift);
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, ls.configuration.selectedChannelIndex);
  TEST_ASSERT_TRUE(ls.configuration.arpeggiators[kChannelBIndex].enabled);
  TEST_ASSERT_EQUAL_UINT8(5u, ls.configuration.arpeggiators[kChannelBIndex].rateIndex);
  TEST_ASSERT_EQUAL_UINT8(5 % 12, ls.brightness.redStep);
  TEST_ASSERT_EQUAL_UINT8((5 + 3) % 12, ls.brightness.greenStep);
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, ls.uiLayer);
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
    StoredConfiguration s;
    s.quantizer.channels[0].config().postShift = static_cast<int8_t>(i % 20);
    live.commit(s, BrightnessCalibration::makeDefault(), UiLayer::Quantizer);
    live.flush();
  }

  // Count how many distinct slots now carry a valid marker: should be all of
  // them, proving writes were distributed rather than hammering one cell.
  int occupied = 0;
  for (int i = 0; i < kLiveSlotCount; ++i) {
    const uint16_t address = static_cast<uint16_t>(
        kLiveRingBase + static_cast<uint16_t>(i) * kLiveSlotSize);
    if (eep.readByte(address) == kRecordMarker) {
      ++occupied;
    }
  }
  TEST_ASSERT_EQUAL_INT(kLiveSlotCount, occupied);

  // And the newest value is still recoverable.
  LiveStateStore live2(eep, writer);
  LiveState ls2;
  TEST_ASSERT_TRUE(live2.load(ls2));
  TEST_ASSERT_EQUAL_INT8(6, ls2.configuration.quantizer.channels[0].config().postShift);
}

// v6 stores the UI layer explicitly. ARP-layer + ARP-off is a valid UI state,
// while a running Arpeggiator is never allowed to be hidden by Quantizer UI.
static void test_live_ui_layer_roundtrips_and_normalizes_inconsistent_state(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  LiveStateStore live(eep, writer);
  LiveState initial;
  live.load(initial);

  StoredConfiguration s;
  s.selectedChannelIndex = kChannelBIndex;
  TEST_ASSERT_TRUE(live.commit(s, BrightnessCalibration::makeDefault(),
                               UiLayer::Arpeggiator));
  live.flush();

  LiveStateStore restoredStore(eep, writer);
  LiveState restored;
  TEST_ASSERT_TRUE(restoredStore.load(restored));
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, restored.uiLayer);
  TEST_ASSERT_FALSE(restored.configuration.arpeggiators[kChannelAIndex].enabled);
  TEST_ASSERT_FALSE(restored.configuration.arpeggiators[kChannelBIndex].enabled);
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, restored.configuration.selectedChannelIndex);

  // A pre-fix/in-memory state requesting Quantizer UI while ARP is enabled is
  // normalized on decode so the running function is always represented by UI.
  restored.configuration.arpeggiators[kChannelAIndex].enabled = true;
  TEST_ASSERT_TRUE(restoredStore.commit(
      restored.configuration, restored.brightness, UiLayer::Quantizer));
  restoredStore.flush();

  LiveStateStore finalStore(eep, writer);
  LiveState finalState;
  TEST_ASSERT_TRUE(finalStore.load(finalState));
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, finalState.uiLayer);
  TEST_ASSERT_TRUE(finalState.configuration.arpeggiators[kChannelAIndex].enabled);
}

// Existing v5 live records did not contain an explicit layer. Preserve update
// compatibility by inferring Arpeggiator mode when at least one saved Arp was
// enabled, which matches the old layer-transition semantics.
static void test_v5_live_state_migrates_enabled_arp_to_arpeggiator_layer(void) {
  FakeEeprom eep;
  uint8_t record[kLiveSlotSize] = {};
  constexpr uint16_t kOffSeq = 1u;
  constexpr uint16_t kOffVersion = 5u;
  constexpr uint16_t kOffConfiguration = 6u;
  constexpr uint16_t kOffBrightness =
      kOffConfiguration + kStoredConfigurationBytes;
  constexpr uint16_t kOffCrc = kOffBrightness + kBrightnessBytes;

  record[0] = kRecordMarker;
  record[kOffSeq] = 1u;
  record[kOffVersion] = kPreviousLiveFormatVersion;
  StoredConfiguration s;
  s.selectedChannelIndex = kChannelBIndex;
  s.arpeggiators[kChannelBIndex].enabled = true;
  encodeStoredConfiguration(s, &record[kOffConfiguration]);
  record[kOffBrightness] = BrightnessCalibration::kUseLegacyDefault;
  record[kOffBrightness + 1u] = BrightnessCalibration::kUseLegacyDefault;
  const uint16_t crc = crc16Ccitt(record, kOffCrc);
  record[kOffCrc] = static_cast<uint8_t>(crc >> 8u);
  record[kOffCrc + 1u] = static_cast<uint8_t>(crc);
  for (uint16_t i = 0u; i < kLiveSlotSize; ++i) {
    eep.writeByte(static_cast<uint16_t>(kLiveRingBase + i), record[i]);
  }

  AsyncEepromWriter writer(eep);
  LiveStateStore live(eep, writer);
  LiveState restored;
  TEST_ASSERT_TRUE(live.load(restored));
  TEST_ASSERT_EQUAL(UiLayer::Arpeggiator, restored.uiLayer);
  TEST_ASSERT_TRUE(restored.configuration.arpeggiators[kChannelBIndex].enabled);
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, restored.configuration.selectedChannelIndex);
}

// FA-099..102 / acceptance criterion 16: every scale slot round-trips and
// reports occupancy independently.
static void test_all_twelve_scale_slots_roundtrip_independently(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);

  for (uint8_t slot = 0u; slot < kScaleSlotCount; ++slot) {
    bool notes[kNoteCount] = {};
    notes[slot] = true;
    notes[static_cast<uint8_t>((slot + 5u) % kNoteCount)] = true;
    TEST_ASSERT_TRUE(store.writeScale(slot, notes));
    store.flush();
  }

  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_EQUAL_HEX16(0x0FFFu, scales.bits);
  TEST_ASSERT_EQUAL_HEX16(0u, configs.bits);

  for (uint8_t slot = 0u; slot < kScaleSlotCount; ++slot) {
    bool loaded[kNoteCount] = {};
    TEST_ASSERT_TRUE(store.readScale(slot, loaded));
    for (uint8_t note = 0u; note < kNoteCount; ++note) {
      const bool expected = note == slot ||
          note == static_cast<uint8_t>((slot + 5u) % kNoteCount);
      TEST_ASSERT_EQUAL(expected, loaded[note]);
    }
  }
}

// FA-103..106 / acceptance criterion 17: every full-configuration slot
// round-trips distinct quantizer, Arpeggiator, selected-channel and UI-layer
// state without leaking into neighbouring slots.
static void test_all_twelve_full_configuration_slots_roundtrip_independently(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);

  for (uint8_t slot = 0u; slot < kConfigSlotCount; ++slot) {
    StoredConfiguration state;
    state.quantizer.channels[kChannelAIndex].config().preShift =
        static_cast<int8_t>(static_cast<int8_t>(slot % 12u) - 5);
    state.arpeggiators[kChannelAIndex].rateIndex = slot;
    state.arpeggiators[kChannelAIndex].enabled = (slot & 1u) != 0u;
    state.selectedChannelIndex = (slot & 1u) != 0u ? kChannelBIndex
                                                    : kChannelAIndex;
    state.uiLayer = (slot & 1u) != 0u ? UiLayer::Arpeggiator
                                      : UiLayer::Quantizer;
    TEST_ASSERT_TRUE(store.writeConfig(slot, state));
    store.flush();
  }

  SlotOccupancy scales, configs;
  store.scan(scales, configs);
  TEST_ASSERT_EQUAL_HEX16(0u, scales.bits);
  TEST_ASSERT_EQUAL_HEX16(0x0FFFu, configs.bits);

  for (uint8_t slot = 0u; slot < kConfigSlotCount; ++slot) {
    StoredConfiguration loaded;
    TEST_ASSERT_TRUE(store.readConfig(slot, loaded));
    TEST_ASSERT_EQUAL_INT8(static_cast<int8_t>(static_cast<int8_t>(slot % 12u) - 5),
                           loaded.quantizer.channels[kChannelAIndex].config().preShift);
    TEST_ASSERT_EQUAL_UINT8(slot, loaded.arpeggiators[kChannelAIndex].rateIndex);
    TEST_ASSERT_EQUAL((slot & 1u) != 0u,
                      loaded.arpeggiators[kChannelAIndex].enabled);
    TEST_ASSERT_EQUAL_UINT8((slot & 1u) != 0u ? kChannelBIndex : kChannelAIndex,
                            loaded.selectedChannelIndex);
    TEST_ASSERT_EQUAL((slot & 1u) != 0u ? UiLayer::Arpeggiator
                                        : UiLayer::Quantizer,
                      loaded.uiLayer);
  }
}

static void test_live_state_invalid_brightness_steps_fall_back_safely(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  LiveStateStore live(eep, writer);
  LiveState initial;
  (void)live.load(initial);

  StoredConfiguration configuration;
  BrightnessCalibration invalid{254u, 253u};
  TEST_ASSERT_TRUE(live.commit(configuration, invalid, UiLayer::Quantizer));
  live.flush();

  LiveStateStore restoredStore(eep, writer);
  LiveState restored;
  TEST_ASSERT_TRUE(restoredStore.load(restored));
  TEST_ASSERT_EQUAL_UINT8(BrightnessCalibration::kUseLegacyDefault,
                          restored.brightness.redStep);
  TEST_ASSERT_EQUAL_UINT8(BrightnessCalibration::kUseLegacyDefault,
                          restored.brightness.greenStep);
}

static void test_linked_stored_configuration_normalizes_channel_and_arpeggiator_b(void) {
  StoredConfiguration source;
  source.quantizer.channelsLinked = true;
  source.selectedChannelIndex = kChannelBIndex;
  source.arpeggiators[kChannelAIndex].rateIndex = 2u;
  source.arpeggiators[kChannelAIndex].pattern = ArpeggiatorPattern::Down;
  source.arpeggiators[kChannelBIndex].rateIndex = 10u;
  source.arpeggiators[kChannelBIndex].pattern = ArpeggiatorPattern::Random;

  uint8_t bytes[kStoredConfigurationBytes];
  encodeStoredConfiguration(source, bytes);
  const StoredConfiguration decoded = decodeStoredConfiguration(bytes);

  TEST_ASSERT_TRUE(decoded.quantizer.channelsLinked);
  TEST_ASSERT_EQUAL_UINT8(kChannelAIndex, decoded.selectedChannelIndex);
  TEST_ASSERT_EQUAL_UINT8(decoded.arpeggiators[kChannelAIndex].rateIndex,
                          decoded.arpeggiators[kChannelBIndex].rateIndex);
  TEST_ASSERT_EQUAL(decoded.arpeggiators[kChannelAIndex].pattern,
                    decoded.arpeggiators[kChannelBIndex].pattern);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_state_roundtrip);
  RUN_TEST(test_stored_configuration_roundtrip);
  RUN_TEST(test_stored_configuration_ui_layer_roundtrips_with_arp_off);
  RUN_TEST(test_blank_slots_empty);
  RUN_TEST(test_scale_save_load);
  RUN_TEST(test_all_twelve_scale_slots_roundtrip_independently);
  RUN_TEST(test_all_twelve_full_configuration_slots_roundtrip_independently);
  RUN_TEST(test_corruption_is_detected);
  RUN_TEST(test_erase_all);
  RUN_TEST(test_v4_full_config_is_read_and_migrates_enabled_arp_layer);
  RUN_TEST(test_live_defaults_when_blank);
  RUN_TEST(test_live_roundtrip_and_newest_wins);
  RUN_TEST(test_live_wear_levelling_spreads_writes);
  RUN_TEST(test_live_ui_layer_roundtrips_and_normalizes_inconsistent_state);
  RUN_TEST(test_live_state_invalid_brightness_steps_fall_back_safely);
  RUN_TEST(test_linked_stored_configuration_normalizes_channel_and_arpeggiator_b);
  RUN_TEST(test_v5_live_state_migrates_enabled_arp_to_arpeggiator_layer);
  return UNITY_END();
}
