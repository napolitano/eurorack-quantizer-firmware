/**
 * @file Mcp4922Dac.h
 * Declares the MCP4922 dual pitch-CV DAC driver.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_AVR_MCP4922_DAC_H
#define FM_QUANTIZER_HAL_AVR_MCP4922_DAC_H

#include <stdint.h>

#include "fmq/ports/DigitalOutput.h"
#include "fmq/ports/SpiBus.h"

/**
 * Driver for the MCP4922 dual 12-bit SPI DAC (pitch CV outputs).
 *
 * The driver is bus-agnostic: it talks to the DAC through an @ref ISpiBus and an
 * @ref IDigitalOutput chip-select, so it can be exercised with a fake bus in
 * host tests. The board must tie the DAC's LDAC pin low so each value latches on
 * the rising edge of chip-select.
 */
namespace fmq {

class Mcp4922Dac {
 public:
  /// DAC output channel.
  enum class Channel : uint8_t { A = 0, B = 1 };

  Mcp4922Dac(ISpiBus &spi, IDigitalOutput &chipSelect)
      : spi_(spi), chipSelect_(chipSelect) {}

  /**
   * @brief Write a 12-bit value to a channel (blocking).
   *
   * Uses the DAC's default configuration: unbuffered reference, 1x gain, output
   * enabled. The value is latched immediately (LDAC tied low).
   *
   * @param channel Which output to update.
   * @param value12 12-bit code (0..4095); higher bits are ignored.
   */
  void write(Channel channel, uint16_t value12);

 private:
  ISpiBus &spi_;
  IDigitalOutput &chipSelect_;
};

}  // namespace fmq

#endif  // FM_QUANTIZER_HAL_AVR_MCP4922_DAC_H
