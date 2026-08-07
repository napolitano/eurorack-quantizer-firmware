/**
 * @file test_startup_sequence_store.cpp
 * Tests persistence of the next startup-animation index.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/persistence/StartupSequenceStore.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "test/support/FakeEeprom.h"

using namespace fmq;
using fmqtest::FakeEeprom;

void setUp(void) {}
void tearDown(void) {}

static void test_erased_eeprom_starts_at_sequence_zero(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  StartupSequenceStore store(eeprom, writer);
  TEST_ASSERT_EQUAL_UINT8(0, store.loadSequenceToPlay(4));
}

static void test_valid_sequence_is_loaded(void) {
  FakeEeprom eeprom;
  eeprom.writeByte(kStartupSequenceAddress, 2);
  AsyncEepromWriter writer(eeprom);
  StartupSequenceStore store(eeprom, writer);
  TEST_ASSERT_EQUAL_UINT8(2, store.loadSequenceToPlay(4));
}

static void test_invalid_sequence_falls_back_to_zero(void) {
  FakeEeprom eeprom;
  eeprom.writeByte(kStartupSequenceAddress, 9);
  AsyncEepromWriter writer(eeprom);
  StartupSequenceStore store(eeprom, writer);
  TEST_ASSERT_EQUAL_UINT8(0, store.loadSequenceToPlay(4));
}

static void test_store_queues_one_byte(void) {
  FakeEeprom eeprom;
  AsyncEepromWriter writer(eeprom);
  StartupSequenceStore store(eeprom, writer);
  TEST_ASSERT_TRUE(store.storeNextSequence(3));
  writer.flush();
  TEST_ASSERT_EQUAL_UINT8(3, eeprom.readByte(kStartupSequenceAddress));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_erased_eeprom_starts_at_sequence_zero);
  RUN_TEST(test_valid_sequence_is_loaded);
  RUN_TEST(test_invalid_sequence_falls_back_to_zero);
  RUN_TEST(test_store_queues_one_byte);
  return UNITY_END();
}
