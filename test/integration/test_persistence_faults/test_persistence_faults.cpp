/**
 * @file test_persistence_faults.cpp
 * Fault-injection tests for EEPROM records, validation and atomic commit rules.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "FakeEeprom.h"
#include "TestScale.h"
#include "fmq/config/ProductConfig.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/persistence/SaveSlotStore.h"
#include "fmq/persistence/Serialization.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

static void major(bool notes[kNoteCount]) {
  const int pcs[] = {0, 2, 4, 5, 7, 9, 11};
  fmqtest::makeScale(notes, pcs, 7);
}

static void test_every_scale_record_byte_corruption_is_rejected(void) {
  for (uint8_t corruptOffset = 0; corruptOffset < kScaleSlotSize; ++corruptOffset) {
    FakeEeprom eep;
    AsyncEepromWriter writer(eep);
    SaveSlotStore store(eep, writer);
    bool notes[kNoteCount];
    major(notes);
    TEST_ASSERT_TRUE(store.writeScale(0, notes));
    store.flush();
    const uint16_t address = static_cast<uint16_t>(kScaleRegionBase + corruptOffset);
    eep.corruptByte(address, static_cast<uint8_t>(eep.readByte(address) ^ 0x01u));
    bool loaded[kNoteCount];
    TEST_ASSERT_FALSE(store.readScale(0, loaded));
  }
}

static void test_every_config_record_byte_corruption_is_rejected(void) {
  for (uint8_t corruptOffset = 0; corruptOffset < kConfigSlotSize; ++corruptOffset) {
    FakeEeprom eep;
    AsyncEepromWriter writer(eep);
    SaveSlotStore store(eep, writer);
    QuantizerState state;
    state.channels[0].config().preShift = -3;
    state.channels[1].config().postShift = 5;
    TEST_ASSERT_TRUE(store.writeConfig(0, state));
    store.flush();
    const uint16_t address = static_cast<uint16_t>(kConfigRegionBase + corruptOffset);
    eep.corruptByte(address, static_cast<uint8_t>(eep.readByte(address) ^ 0x01u));
    QuantizerState loaded;
    TEST_ASSERT_FALSE(store.readConfig(0, loaded));
  }
}

static void test_uncommitted_scale_record_is_never_visible(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool notes[kNoteCount];
  major(notes);
  TEST_ASSERT_TRUE(store.writeScale(2, notes));
  // No flush/service: writer has only queued the transaction, EEPROM must not
  // expose the slot as a valid committed record.
  bool loaded[kNoteCount];
  TEST_ASSERT_FALSE(store.readScale(2, loaded));
}

static void test_busy_writer_rejects_overlapping_save_operations(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool notes[kNoteCount];
  major(notes);
  TEST_ASSERT_TRUE(store.writeScale(0, notes));
  TEST_ASSERT_FALSE(store.writeScale(1, notes));
  QuantizerState state;
  TEST_ASSERT_FALSE(store.writeConfig(0, state));
  store.flush();
  TEST_ASSERT_TRUE(store.writeScale(1, notes));
}

static void test_out_of_range_slots_are_rejected_without_writes(void) {
  FakeEeprom eep;
  AsyncEepromWriter writer(eep);
  SaveSlotStore store(eep, writer);
  bool notes[kNoteCount];
  major(notes);
  TEST_ASSERT_FALSE(store.writeScale(kScaleSlotCount, notes));
  TEST_ASSERT_FALSE(store.readScale(kScaleSlotCount, notes));
  QuantizerState state;
  TEST_ASSERT_FALSE(store.writeConfig(kConfigSlotCount, state));
  TEST_ASSERT_FALSE(store.readConfig(kConfigSlotCount, state));
  TEST_ASSERT_FALSE(writer.busy());
}

// SM-005/006: decoded corrupt fields are clamped/defaulted rather than trusted.
static void test_decode_channel_config_clamps_all_numeric_fields(void) {
  uint8_t bytes[kChannelConfigBytes] = {};
  bytes[0] = 0xFFu;
  bytes[1] = 0x0Fu;
  bytes[2] = 0xFFu; // invalid sample mode
  bytes[3] = 250u;
  bytes[4] = 250u;
  bytes[5] = static_cast<uint8_t>(-120);
  bytes[6] = 120u;
  bytes[7] = 100u;
  const ChannelConfig cfg = decodeChannelConfig(bytes);
  TEST_ASSERT_EQUAL(SampleMode::TrackAndHold, cfg.sampleMode);
  TEST_ASSERT_EQUAL_UINT8(config::kMaxGlideAmount, cfg.glideAmount);
  TEST_ASSERT_EQUAL_UINT8(config::kMaxTriggerDelayAmount, cfg.triggerDelayAmount);
  TEST_ASSERT_EQUAL_INT8(config::kMinimumShift, cfg.preShift);
  TEST_ASSERT_EQUAL_INT8(config::kMaximumShift, cfg.scaleShift);
  TEST_ASSERT_EQUAL_INT8(config::kMaximumShift, cfg.postShift);
}

static void test_decode_empty_scale_falls_back_to_factory_scale(void) {
  uint8_t bytes[kChannelConfigBytes] = {};
  const ChannelConfig cfg = decodeChannelConfig(bytes);
  for (uint8_t note = 0; note < kNoteCount; ++note) {
    TEST_ASSERT_TRUE(cfg.notes[note]);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_every_scale_record_byte_corruption_is_rejected);
  RUN_TEST(test_every_config_record_byte_corruption_is_rejected);
  RUN_TEST(test_uncommitted_scale_record_is_never_visible);
  RUN_TEST(test_busy_writer_rejects_overlapping_save_operations);
  RUN_TEST(test_out_of_range_slots_are_rejected_without_writes);
  RUN_TEST(test_decode_channel_config_clamps_all_numeric_fields);
  RUN_TEST(test_decode_empty_scale_falls_back_to_factory_scale);
  return UNITY_END();
}
