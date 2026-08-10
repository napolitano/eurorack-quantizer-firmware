/**
 * @file test_eeprom_wear.cpp
 * Verifies the physical-write distribution assumptions used by the EEPROM
 * endurance audit.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdint.h>
#include <unity.h>

#include "FakeEeprom.h"
#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/LiveStateStore.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/persistence/SaveSlotStore.h"
#include "fmq/persistence/StartupSequenceStore.h"

using namespace fmq;
using namespace fmqtest;

void setUp() {}
void tearDown() {}

static void test_twenty_four_live_commits_are_evenly_wear_levelled(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  LiveStateStore live(eeprom, writer);
  LiveState blank;
  (void)live.load(blank);  // establishes the normal first-write ring head
  eeprom.resetWriteCount();

  StoredConfiguration state;
  for (uint8_t commit = 0u; commit < 24u; ++commit) {
    state.selectedChannelIndex = static_cast<uint8_t>(commit & 1u);
    state.quantizer.channels[kChannelAIndex].config().glideAmount =
        static_cast<uint8_t>(commit % 12u);
    const BrightnessCalibration brightness{
        static_cast<uint8_t>(commit % BrightnessCalibration::kStepCount),
        static_cast<uint8_t>((commit + 5u) % BrightnessCalibration::kStepCount)};
    TEST_ASSERT_TRUE(live.commit(
        state, brightness,
        (commit & 1u) == 0u ? UiLayer::Quantizer : UiLayer::Arpeggiator));
    live.flush();
  }

  TEST_ASSERT_EQUAL_UINT32(24u, live.currentSequence());
  for (uint16_t slot = 0u; slot < kLiveSlotCount; ++slot) {
    const uint16_t base = static_cast<uint16_t>(
        kLiveRingBase + slot * kLiveSlotSize);
    // First visit: erased marker -> A5. Second visit: A5 -> FF -> A5.
    TEST_ASSERT_EQUAL_UINT32(3u, eeprom.writeCount(base));
    for (uint16_t offset = 1u; offset < kLiveSlotSize; ++offset) {
      TEST_ASSERT_TRUE(eeprom.writeCount(static_cast<uint16_t>(base + offset)) <= 2u);
    }
  }

  for (uint16_t address = 0u; address < kLiveRingBase; ++address) {
    TEST_ASSERT_EQUAL_UINT32(0u, eeprom.writeCount(address));
  }
  for (uint16_t address = static_cast<uint16_t>(
           kLiveRingBase + kLiveSlotCount * kLiveSlotSize);
       address < kEepromSize; ++address) {
    TEST_ASSERT_EQUAL_UINT32(0u, eeprom.writeCount(address));
  }
}

static void test_eeprom_double_write_is_suppressed_for_unchanged_byte(void) {
  FakeEeprom eeprom;
  eeprom.writeByte(100u, 0x42u);
  eeprom.writeByte(100u, 0x42u);
  TEST_ASSERT_EQUAL_UINT32(1u, eeprom.writeCount(100u));
}

static void test_startup_sequence_rewriting_same_value_does_not_add_wear(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  StartupSequenceStore startup(eeprom, writer);

  TEST_ASSERT_TRUE(startup.storeNextSequence(2u));
  writer.flush();
  TEST_ASSERT_TRUE(startup.storeNextSequence(2u));
  writer.flush();

  TEST_ASSERT_EQUAL_UINT32(1u, eeprom.writeCount(kStartupSequenceAddress));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_twenty_four_live_commits_are_evenly_wear_levelled);
  RUN_TEST(test_eeprom_double_write_is_suppressed_for_unchanged_byte);
  RUN_TEST(test_startup_sequence_rewriting_same_value_does_not_add_wear);
  return UNITY_END();
}
