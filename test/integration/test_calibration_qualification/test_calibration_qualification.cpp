/**
 * @file test_calibration_qualification.cpp
 * Tests compact generation-bound calibration qualification persistence.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/CalibrationQualificationStore.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "FakeEeprom.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

static void test_blank_eeprom_has_no_qualification(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  CalibrationQualificationStore store(eeprom, writer);
  TEST_ASSERT_FALSE(store.load(0u).valid);
}

static void test_roundtrip_is_bound_to_calibration_generation(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  CalibrationQualificationStore store(eeprom, writer);
  TEST_ASSERT_TRUE(store.save(7u, 2u, 5u));
  store.flush();

  const CalibrationQualification matching = store.load(7u);
  TEST_ASSERT_TRUE(matching.valid);
  TEST_ASSERT_EQUAL_UINT8(7u, matching.generation);
  TEST_ASSERT_EQUAL_UINT8(2u, matching.maxResidualA);
  TEST_ASSERT_EQUAL_UINT8(5u, matching.maxResidualB);
  TEST_ASSERT_FALSE(store.load(8u).valid);
}

static void test_commit_marker_is_written_last(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  CalibrationQualificationStore store(eeprom, writer);
  TEST_ASSERT_TRUE(store.save(3u, 1u, 2u));

  writer.service();  // invalidate marker
  writer.service();  // write residual byte
  TEST_ASSERT_FALSE(store.load(3u).valid);
  writer.service();  // submit commit marker
  writer.service();  // observe physical completion
  TEST_ASSERT_TRUE(store.load(3u).valid);
}

static void test_marker_cannot_be_misread_as_legacy_live_state(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  CalibrationQualificationStore store(eeprom, writer);
  TEST_ASSERT_TRUE(store.save(5u, 3u, 4u));
  store.flush();
  TEST_ASSERT_NOT_EQUAL(kRecordMarker,
                        eeprom.readByte(kQualificationMarkerAddress));
}

static void test_residuals_above_four_bit_range_are_rejected(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  CalibrationQualificationStore store(eeprom, writer);
  TEST_ASSERT_FALSE(store.save(1u, 16u, 0u));
  TEST_ASSERT_FALSE(store.save(1u, 0u, 16u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_blank_eeprom_has_no_qualification);
  RUN_TEST(test_roundtrip_is_bound_to_calibration_generation);
  RUN_TEST(test_commit_marker_is_written_last);
  RUN_TEST(test_marker_cannot_be_misread_as_legacy_live_state);
  RUN_TEST(test_residuals_above_four_bit_range_are_rejected);
  return UNITY_END();
}
