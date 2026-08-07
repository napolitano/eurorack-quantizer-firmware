/**
 * @file AvrSpiBus.h
 * Implements the SPI bus port using Arduino SPI.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_AVR_SPI_BUS_H
#define FM_QUANTIZER_HAL_AVR_SPI_BUS_H

#include <Arduino.h>
#include <SPI.h>

#include "fmq/ports/SpiBus.h"

/**
 * ISpiBus backed by the ATmega328P hardware SPI peripheral.
 *
 * Both the DAC and the LED driver share this bus; each manages its own
 * chip-select. Configured MSB-first, mode 0, at F_CPU/2 (8 MHz), which both
 * devices tolerate.
 */
namespace fmq {

class AvrSpiBus : public ISpiBus {
 public:
  /// Initialise the SPI peripheral. Call once from setup().
  void begin() {
    SPI.begin();

  }
  void transfer(uint8_t *buffer, uint16_t length) override {
    SPI.beginTransaction(SPISettings(8000000UL, MSBFIRST, SPI_MODE0));
    for (uint16_t i = 0; i < length; ++i) buffer[i] = SPI.transfer(buffer[i]);
    SPI.endTransaction();
  }
};

}  // namespace fmq
#endif  // FM_QUANTIZER_HAL_AVR_SPI_BUS_H
