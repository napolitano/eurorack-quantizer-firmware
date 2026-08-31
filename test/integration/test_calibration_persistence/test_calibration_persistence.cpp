/**
 * @file test_calibration_persistence.cpp
 * Tests redundant CRC-protected persistence of analogue calibration profiles.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/CalibrationProfile.h"
#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/CalibrationStore.h"
#include "fmq/persistence/EepromPartition.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "FakeEeprom.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

static CalibrationProfile makeProfile(int16_t seed) {
  CalibrationProfile profile = factoryCalibrationProfile();
  profile.adc[0] = {seed, 1001u, 1000u};
  profile.adc[1] = {static_cast<int16_t>(-seed), 999u, 1000u};
  profile.dac[0] = {static_cast<int16_t>(seed + 2), 1003u, 1000u};
  profile.dac[1] = {static_cast<int16_t>(-seed - 3), 997u, 1000u};
  return profile;
}

static void assertProfileEqual(const CalibrationProfile &expected,
                               const CalibrationProfile &actual) {
  for (uint8_t channel = 0u; channel < 2u; ++channel) {
    TEST_ASSERT_EQUAL_INT32(expected.adc[channel].offset,
                            actual.adc[channel].offset);
    TEST_ASSERT_EQUAL_UINT16(expected.adc[channel].gainNumerator,
                             actual.adc[channel].gainNumerator);
    TEST_ASSERT_EQUAL_UINT16(expected.adc[channel].gainDenominator,
                             actual.adc[channel].gainDenominator);
    TEST_ASSERT_EQUAL_INT32(expected.dac[channel].offset,
                            actual.dac[channel].offset);
    TEST_ASSERT_EQUAL_UINT16(expected.dac[channel].gainNumerator,
                             actual.dac[channel].gainNumerator);
    TEST_ASSERT_EQUAL_UINT16(expected.dac[channel].gainDenominator,
                             actual.dac[channel].gainDenominator);
  }
}

static void test_blank_eeprom_has_no_calibration_record(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  AsyncEepromWriter writer(partition);
  CalibrationStore store(partition, writer);
  CalibrationProfile loaded;
  TEST_ASSERT_FALSE(store.load(loaded));
}

static void test_profile_roundtrips_through_redundant_store(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  AsyncEepromWriter writer(partition);
  CalibrationStore store(partition, writer);
  const CalibrationProfile expected = makeProfile(7);
  TEST_ASSERT_TRUE(store.save(expected));
  store.flush();

  CalibrationStore rebooted(partition, writer);
  CalibrationProfile loaded;
  TEST_ASSERT_TRUE(rebooted.load(loaded));
  assertProfileEqual(expected, loaded);
}

static void test_newest_valid_record_wins(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  AsyncEepromWriter writer(partition);
  CalibrationStore store(partition, writer);
  const CalibrationProfile first = makeProfile(3);
  const CalibrationProfile second = makeProfile(11);
  TEST_ASSERT_TRUE(store.save(first));
  store.flush();
  TEST_ASSERT_TRUE(store.save(second));
  store.flush();

  CalibrationStore rebooted(partition, writer);
  CalibrationProfile loaded;
  TEST_ASSERT_TRUE(rebooted.load(loaded));
  assertProfileEqual(second, loaded);
}

static void test_corrupt_newest_record_falls_back_to_previous(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  AsyncEepromWriter writer(partition);
  CalibrationStore store(partition, writer);
  const CalibrationProfile first = makeProfile(4);
  const CalibrationProfile second = makeProfile(12);
  TEST_ASSERT_TRUE(store.save(first));
  store.flush();
  TEST_ASSERT_TRUE(store.save(second));
  store.flush();

  // First save goes to record 0, second save to record 1.
  const uint16_t corruptAddress = static_cast<uint16_t>(
      kCalibrationRegionBase + kCalibrationRecordSize + 8u);
  eeprom.corruptByte(corruptAddress,
                     static_cast<uint8_t>(eeprom.readByte(corruptAddress) ^ 0x01u));

  CalibrationStore rebooted(partition, writer);
  CalibrationProfile loaded;
  TEST_ASSERT_TRUE(rebooted.load(loaded));
  assertProfileEqual(first, loaded);
}

static void test_interrupted_replacement_keeps_previous_record_valid(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  AsyncEepromWriter writer(partition);
  CalibrationStore store(partition, writer);
  const CalibrationProfile first = makeProfile(5);
  const CalibrationProfile second = makeProfile(13);
  TEST_ASSERT_TRUE(store.save(first));
  store.flush();
  TEST_ASSERT_TRUE(store.save(second));

  // Advance only part of the queued replacement. The destination marker was
  // invalidated first and cannot become valid until the final queued write.
  for (uint8_t i = 0u; i < 10u; ++i) writer.service();

  CalibrationStore rebooted(partition, writer);
  CalibrationProfile loaded;
  TEST_ASSERT_TRUE(rebooted.load(loaded));
  assertProfileEqual(first, loaded);
}

static void test_journal_status_reports_both_records_and_newest_active(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  AsyncEepromWriter writer(partition);
  CalibrationStore store(partition, writer);
  TEST_ASSERT_TRUE(store.save(makeProfile(1)));
  store.flush();
  TEST_ASSERT_TRUE(store.save(makeProfile(2)));
  store.flush();
  const CalibrationJournalStatus status = store.inspect();
  TEST_ASSERT_TRUE(status.records[0].valid);
  TEST_ASSERT_TRUE(status.records[1].valid);
  TEST_ASSERT_EQUAL_INT8(1, status.activeIndex);
}


static void test_erase_invalidates_both_calibration_records(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  AsyncEepromWriter writer(partition);
  CalibrationStore store(partition, writer);
  TEST_ASSERT_TRUE(store.save(makeProfile(4)));
  store.flush();
  TEST_ASSERT_TRUE(store.save(makeProfile(8)));
  store.flush();

  TEST_ASSERT_TRUE(store.erase());
  store.flush();

  CalibrationProfile loaded;
  TEST_ASSERT_FALSE(store.load(loaded));
  const CalibrationJournalStatus status = store.inspect();
  TEST_ASSERT_FALSE(status.records[0].valid);
  TEST_ASSERT_FALSE(status.records[1].valid);
  TEST_ASSERT_EQUAL_INT8(-1, status.activeIndex);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_blank_eeprom_has_no_calibration_record);
  RUN_TEST(test_profile_roundtrips_through_redundant_store);
  RUN_TEST(test_newest_valid_record_wins);
  RUN_TEST(test_corrupt_newest_record_falls_back_to_previous);
  RUN_TEST(test_interrupted_replacement_keeps_previous_record_valid);
  RUN_TEST(test_journal_status_reports_both_records_and_newest_active);
  RUN_TEST(test_erase_invalidates_both_calibration_records);
  return UNITY_END();
}
