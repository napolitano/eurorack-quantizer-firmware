/**
 * @file Crc16.cpp
 * Implements CRC-16/CCITT-FALSE for persistence integrity checks.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/Crc16.h"

namespace fmq {

uint16_t crc16Ccitt(const uint8_t *data, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < length; ++i) {
    crc = static_cast<uint16_t>(crc ^ static_cast<uint16_t>(static_cast<uint16_t>(data[i]) << 8u));
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000u) {
        crc = static_cast<uint16_t>(static_cast<uint16_t>(crc << 1u) ^ static_cast<uint16_t>(0x1021u));
      } else {
        crc = static_cast<uint16_t>(crc << 1u);
      }
    }
  }
  return crc;
}

}  // namespace fmq
