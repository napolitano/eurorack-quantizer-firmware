/**
 * @file test_persistence_golden.cpp
 * Loads frozen 1 KiB EEPROM images through the production persistence readers.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdint.h>
#include <unity.h>

#include "FakeEeprom.h"
#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/LiveStateStore.h"
#include "fmq/persistence/SaveSlotStore.h"
#include "fmq/persistence/StartupSequenceStore.h"

using namespace fmq;
using namespace fmqtest;

namespace {
constexpr uint8_t kCurrentImage[1024] = {
#include "../../fixtures/persistence/current-v5-save-v6-live.inc"
};
constexpr uint8_t kLegacyImage[1024] = {
#include "../../fixtures/persistence/legacy-v4-save-v5-live.inc"
};

void loadImage(FakeEeprom &eeprom, const uint8_t (&image)[1024]) {
  for (uint16_t address = 0u; address < 1024u; ++address) {
    eeprom.corruptByte(address, image[address]);
  }
}

void assertExpectedScale(const bool notes[kNoteCount]) {
  const bool expected[kNoteCount] = {
      true, false, true, false, true, true,
      false, true, false, true, false, true};
  for (uint8_t i = 0u; i < kNoteCount; ++i) {
    TEST_ASSERT_EQUAL(expected[i], notes[i]);
  }
}

void assertExpectedConfiguration(const StoredConfiguration &state) {
  TEST_ASSERT_FALSE(state.quantizer.channelsLinked);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PitchMode::Relative),
                          static_cast<uint8_t>(state.quantizer.channelBMode));
  TEST_ASSERT_EQUAL_UINT8(kChannelBIndex, state.selectedChannelIndex);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(UiLayer::Arpeggiator),
                          static_cast<uint8_t>(state.uiLayer));
  TEST_ASSERT_EQUAL_UINT8(7u,
      state.quantizer.channels[kChannelAIndex].config().glideAmount);
  TEST_ASSERT_EQUAL_INT8(-3,
      state.quantizer.channels[kChannelAIndex].config().preShift);
  TEST_ASSERT_EQUAL_UINT8(9u,
      state.quantizer.channels[kChannelBIndex].config().triggerDelayAmount);
  TEST_ASSERT_EQUAL_INT8(4,
      state.quantizer.channels[kChannelBIndex].config().postShift);

  const ArpeggiatorConfig &arpA = state.arpeggiators[kChannelAIndex];
  TEST_ASSERT_TRUE(arpA.enabled);
  TEST_ASSERT_EQUAL_UINT8(8u, arpA.rateIndex);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ArpeggiatorPattern::OutsideIn),
                          static_cast<uint8_t>(arpA.pattern));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ArpeggiatorShape::Seventh1357),
                          static_cast<uint8_t>(arpA.shape));
  TEST_ASSERT_EQUAL_UINT8(7u, arpA.length);
  TEST_ASSERT_EQUAL_UINT8(3u, arpA.range);
  TEST_ASSERT_TRUE(arpA.stepTrigger);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ArpeggiatorSyncMode::Clock),
                          static_cast<uint8_t>(arpA.syncMode));
  TEST_ASSERT_EQUAL_UINT8(6u, arpA.swing);
}

void assertImageLoads(const uint8_t (&image)[1024]) {
  FakeEeprom eeprom;
  loadImage(eeprom, image);
  AsyncEepromWriter writer(eeprom);
  SaveSlotStore slots(eeprom, writer);
  LiveStateStore live(eeprom, writer);
  StartupSequenceStore startup(eeprom, writer);

  bool notes[kNoteCount] = {};
  TEST_ASSERT_TRUE(slots.readScale(2u, notes));
  assertExpectedScale(notes);

  StoredConfiguration configuration;
  TEST_ASSERT_TRUE(slots.readConfig(4u, configuration));
  assertExpectedConfiguration(configuration);

  LiveState liveState;
  TEST_ASSERT_TRUE(live.load(liveState));
  assertExpectedConfiguration(liveState.configuration);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(UiLayer::Arpeggiator),
                          static_cast<uint8_t>(liveState.uiLayer));
  TEST_ASSERT_EQUAL_UINT8(4u, liveState.brightness.redStep);
  TEST_ASSERT_EQUAL_UINT8(9u, liveState.brightness.greenStep);
  TEST_ASSERT_EQUAL_UINT32(1u, live.currentSequence());

  TEST_ASSERT_EQUAL_UINT8(3u, startup.loadSequenceToPlay(4u));
}
}  // namespace

void setUp() {}
void tearDown() {}

static void test_current_v5_save_v6_live_fixture_loads_exactly(void) {
  assertImageLoads(kCurrentImage);
}

static void test_supported_legacy_v4_save_v5_live_fixture_migrates_exactly(void) {
  assertImageLoads(kLegacyImage);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_current_v5_save_v6_live_fixture_loads_exactly);
  RUN_TEST(test_supported_legacy_v4_save_v5_live_fixture_migrates_exactly);
  return UNITY_END();
}
