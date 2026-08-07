/**
 * @file Crc16.h
 * Declares CRC-16 used to validate persisted records.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_CRC16_H
#define FM_QUANTIZER_CORE_CRC16_H

#include <stdint.h>

/**
 * CRC-16/CCITT-FALSE checksum used to protect persisted records.
 *
 * Parameters: polynomial 0x1021, initial value 0xFFFF, no input/output
 * reflection, no final XOR. This detects the single-bit and burst errors that a
 * worn or disturbed EEPROM cell can produce, which the original firmware's
 * single sentinel byte could not.
 */
namespace fmq {

/**
 * Compute the CRC-16/CCITT-FALSE of a byte buffer.
 * @param data   Pointer to the bytes to checksum.
 * @param length Number of bytes.
 * @return The 16-bit checksum.
 */
uint16_t crc16Ccitt(const uint8_t *data, uint16_t length);

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_CRC16_H
