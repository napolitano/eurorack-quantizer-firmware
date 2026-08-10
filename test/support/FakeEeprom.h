/**
 * @file FakeEeprom.h
 * Provides an in-memory EEPROM test double.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_TEST_SUPPORT_FAKE_EEPROM_H
#define FM_QUANTIZER_TEST_SUPPORT_FAKE_EEPROM_H

#include <stdint.h>

#include "fmq/ports/Eeprom.h"

namespace fmqtest {

/**
 * In-memory IEeprom used by host tests.
 *
 * Starts fully erased (0xFF), mimics the AVR layer's "only write changed cells"
 * behaviour and counts physical byte writes so wear-levelling can be asserted.
 * A single byte can be forced to a value to simulate corruption.
 */
class FakeEeprom : public fmq::IEeprom {
 public:
  FakeEeprom() : writeCount_(0) {
    for (uint16_t i = 0; i < kSize; ++i) {
      data_[i] = 0xFF;
      writeCounts_[i] = 0u;
    }
  }

  uint16_t capacity() const override { return kSize; }

  uint8_t readByte(uint16_t address) const override { return data_[address]; }

  void writeByte(uint16_t address, uint8_t value) override {
    if (data_[address] != value) {
      data_[address] = value;
      ++writeCount_;
      ++writeCounts_[address];
    }
  }

  void readBytes(uint16_t address, uint8_t *buffer,
                 uint16_t length) const override {
    for (uint16_t i = 0; i < length; ++i) {
      buffer[i] = data_[address + i];
    }
  }



  // --- Test instrumentation -------------------------------------------------
  uint32_t writeCount() const { return writeCount_; }
  uint32_t writeCount(uint16_t address) const { return writeCounts_[address]; }
  void resetWriteCount() {
    writeCount_ = 0;
    for (uint16_t i = 0; i < kSize; ++i) writeCounts_[i] = 0u;
  }
  void corruptByte(uint16_t address, uint8_t value) { data_[address] = value; }

 private:
  static constexpr uint16_t kSize = 1024;
  uint8_t data_[kSize];
  uint32_t writeCounts_[kSize];
  uint32_t writeCount_;
};

}  // namespace fmqtest

#endif  // FM_QUANTIZER_TEST_SUPPORT_FAKE_EEPROM_H
