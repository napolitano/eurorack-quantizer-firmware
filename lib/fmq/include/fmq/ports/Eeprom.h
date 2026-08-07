/**
 * @file Eeprom.h
 * Defines the platform-independent EEPROM access port.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_EEPROM_H
#define FM_QUANTIZER_HAL_EEPROM_H

#include <stdint.h>

/**
 * Byte-addressable non-volatile storage.
 */
namespace fmq {

/// Abstract EEPROM. The AVR implementation only physically writes cells whose
/// value actually changes, which reduces wear.
class IEeprom {
 public:
  virtual ~IEeprom() {}
  /// @return Total number of addressable bytes (1024 on the ATmega328P).
  virtual uint16_t capacity() const = 0;
  /// @return The byte stored at @p address.
  virtual uint8_t readByte(uint16_t address) const = 0;
  /// True when a new physical write may be started.
  virtual bool isReady() const { return true; }
  /// Store @p value at @p address.
  virtual void writeByte(uint16_t address, uint8_t value) = 0;
  /// Read @p length bytes starting at @p address into @p buffer.
  virtual void readBytes(uint16_t address, uint8_t *buffer,
                         uint16_t length) const = 0;
};

}  // namespace fmq
#endif  // FM_QUANTIZER_HAL_EEPROM_H
