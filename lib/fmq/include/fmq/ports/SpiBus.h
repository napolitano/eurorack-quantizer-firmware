/**
 * @file SpiBus.h
 * Defines the platform-independent SPI bus port.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_SPI_BUS_H
#define FM_QUANTIZER_HAL_SPI_BUS_H

#include <stdint.h>

/**
 * Full-duplex SPI byte transfer.
 */
namespace fmq {

/// Abstract SPI master. Chip-select is managed by the caller via IDigitalOutput
/// so that a single bus can be shared between the DAC and the LED driver.
class ISpiBus {
 public:
  virtual ~ISpiBus() {}
  /**
   * @brief Transfer @p length bytes in place (write out, read in).
   * @param buffer Data to send; overwritten with the bytes received.
   * @param length Number of bytes.
   */
  virtual void transfer(uint8_t *buffer, uint16_t length) = 0;
};

}  // namespace fmq
#endif  // FM_QUANTIZER_HAL_SPI_BUS_H
