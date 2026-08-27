/**
 * @file test_eeprom_partition.cpp
 * Verifies hard EEPROM range isolation for maintenance writers.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/persistence/EepromPartition.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "FakeEeprom.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

static void test_partition_maps_relative_addresses_to_calibration_region(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  partition.writeByte(0u, 0x12u);
  partition.writeByte(static_cast<uint16_t>(kCalibrationRegionSize - 1u), 0x34u);
  TEST_ASSERT_EQUAL_HEX8(0x12u, eeprom.readByte(kCalibrationRegionBase));
  TEST_ASSERT_EQUAL_HEX8(0x34u, eeprom.readByte(kCalibrationRegionBase + kCalibrationRegionSize - 1u));
}

static void test_partition_rejects_writes_outside_calibration_region(void) {
  FakeEeprom eeprom;
  eeprom.corruptByte(kCalibrationRegionBase - 1u, 0x5Au);
  eeprom.corruptByte(kCalibrationRegionBase + kCalibrationRegionSize, 0xA5u);
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  partition.writeByte(kCalibrationRegionSize, 0x00u);
  partition.writeByte(0xFFFFu, 0x00u);
  TEST_ASSERT_EQUAL_HEX8(0x5Au, eeprom.readByte(kCalibrationRegionBase - 1u));
  TEST_ASSERT_EQUAL_HEX8(0xA5u, eeprom.readByte(kCalibrationRegionBase + kCalibrationRegionSize));
}

static void test_partition_returns_erased_data_for_out_of_range_reads(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  TEST_ASSERT_EQUAL_HEX8(kErasedByte, partition.readByte(kCalibrationRegionSize));
  uint8_t data[4] = {};
  partition.readBytes(static_cast<uint16_t>(kCalibrationRegionSize - 2u), data, 4u);
  for (uint8_t value : data) TEST_ASSERT_EQUAL_HEX8(kErasedByte, value);
}

static void test_partition_reports_relative_capacity_and_bounds(void) {
  FakeEeprom eeprom;
  EepromPartition partition(eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
  TEST_ASSERT_EQUAL_UINT16(kCalibrationRegionSize, partition.capacity());
  TEST_ASSERT_EQUAL_UINT16(kCalibrationRegionBase, partition.base());
  TEST_ASSERT_TRUE(partition.contains(0u, kCalibrationRegionSize));
  TEST_ASSERT_FALSE(partition.contains(kCalibrationRegionSize, 1u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_partition_maps_relative_addresses_to_calibration_region);
  RUN_TEST(test_partition_rejects_writes_outside_calibration_region);
  RUN_TEST(test_partition_returns_erased_data_for_out_of_range_reads);
  RUN_TEST(test_partition_reports_relative_capacity_and_bounds);
  return UNITY_END();
}
