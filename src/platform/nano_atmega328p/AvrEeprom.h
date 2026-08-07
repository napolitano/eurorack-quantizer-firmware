/**
 * @file AvrEeprom.h
 * Implements non-blocking AVR EEPROM primitives.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_AVR_EEPROM_H
#define FM_QUANTIZER_HAL_AVR_EEPROM_H

#include <Arduino.h>
#include <avr/eeprom.h>
#include "fmq/ports/Eeprom.h"

namespace fmq {

/**
 * AVR EEPROM primitive used by the shared AsyncEepromWriter.
 *
 * Reads do not call eeprom_busy_wait(); callers must guard them with the shared
 * writer's busy state. All runtime writes are submitted one byte at a time by
 * AsyncEepromWriter, so the 1 kHz control path is never deliberately blocked.
 */
class AvrEeprom : public IEeprom {
 public:
  uint16_t capacity() const override {
    return static_cast<uint16_t>(E2END) + 1u;
  }

  bool isReady() const override { return eeprom_is_ready(); }

  uint8_t readByte(uint16_t address) const override {
    return eeprom_read_byte(reinterpret_cast<const uint8_t *>(address));
  }

  void writeByte(uint16_t address, uint8_t value) override {
    if (!eeprom_is_ready()) return;
    const uint8_t old =
        eeprom_read_byte(reinterpret_cast<const uint8_t *>(address));
    if (old == value) return;

    EEAR = address;
    EEDR = value;
    const uint8_t savedSreg = SREG;
    cli();
    EECR |= _BV(EEMPE);
    EECR |= _BV(EEPE);
    SREG = savedSreg;
  }

  void readBytes(uint16_t address, uint8_t *buffer,
                 uint16_t length) const override {
    eeprom_read_block(buffer, reinterpret_cast<const void *>(address), length);
  }
};

}  // namespace fmq
#endif
