/**
 * @file EepromPartition.h
 * Bounds an EEPROM client to one explicit address partition.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PERSISTENCE_EEPROM_PARTITION_H
#define FMQ_PERSISTENCE_EEPROM_PARTITION_H

#include <stdint.h>

#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/ports/Eeprom.h"

namespace fmq {

/**
 * @brief Relative-address EEPROM view with hard range isolation.
 *
 * Clients see addresses `0..size-1`; valid operations are translated to the
 * selected parent EEPROM region. Out-of-range reads return erased bytes and
 * out-of-range writes are ignored. This keeps the maintenance calibration
 * writer structurally unable to touch musical presets or live-state records.
 */
class EepromPartition final : public IEeprom {
 public:
  EepromPartition(IEeprom &parent, uint16_t base, uint16_t size)
      : parent_(parent), base_(base), size_(size) {}

  uint16_t capacity() const override { return size_; }

  uint8_t readByte(uint16_t address) const override {
    return address < size_
               ? parent_.readByte(static_cast<uint16_t>(base_ + address))
               : kErasedByte;
  }

  bool isReady() const override { return parent_.isReady(); }

  void writeByte(uint16_t address, uint8_t value) override {
    if (address < size_) {
      parent_.writeByte(static_cast<uint16_t>(base_ + address), value);
    }
  }

  void readBytes(uint16_t address, uint8_t *buffer,
                 uint16_t length) const override {
    if (buffer == nullptr) return;
    if (address <= size_ && length <= static_cast<uint16_t>(size_ - address)) {
      parent_.readBytes(static_cast<uint16_t>(base_ + address), buffer, length);
      return;
    }
    for (uint16_t i = 0u; i < length; ++i) buffer[i] = kErasedByte;
  }

  bool contains(uint16_t address, uint16_t length = 1u) const {
    return address <= size_ && length <= static_cast<uint16_t>(size_ - address);
  }

  uint16_t base() const { return base_; }

 private:
  IEeprom &parent_;
  uint16_t base_;
  uint16_t size_;
};

}  // namespace fmq

#endif
