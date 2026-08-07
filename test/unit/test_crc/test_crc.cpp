/**
 * @file test_crc.cpp
 * Host regression or unit tests for crc behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <string.h>
#include <unity.h>

#include "fmq/persistence/Crc16.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

// The canonical CCITT-FALSE check value for the ASCII string "123456789".
static void test_known_vector(void) {
  const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16Ccitt(data, sizeof(data)));
}

// Empty input returns the initial value.
static void test_empty(void) {
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16Ccitt(nullptr, 0));
}

// Any single-bit change must change the checksum (error detection).
static void test_detects_single_bit_flip(void) {
  uint8_t data[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
  const uint16_t good = crc16Ccitt(data, sizeof(data));
  for (int byte = 0; byte < 8; ++byte) {
    for (int bit = 0; bit < 8; ++bit) {
      uint8_t corrupted[8];
      memcpy(corrupted, data, sizeof(data));
      corrupted[byte] ^= static_cast<uint8_t>(1u << bit);
      TEST_ASSERT_NOT_EQUAL(good, crc16Ccitt(corrupted, sizeof(corrupted)));
    }
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_known_vector);
  RUN_TEST(test_empty);
  RUN_TEST(test_detects_single_bit_flip);
  return UNITY_END();
}
