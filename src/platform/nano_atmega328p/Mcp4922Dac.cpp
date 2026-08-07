/**
 * @file Mcp4922Dac.cpp
 * Implements 12-bit MCP4922 SPI writes.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/nano_atmega328p/Mcp4922Dac.h"

namespace {
constexpr uint16_t kDacDataMask = 0x0FFFu;
constexpr uint8_t kDacDataHighMask = 0x0Fu;
constexpr uint8_t kDacChannelBit = 7u;
constexpr uint8_t kDacControlBits = 0x30u;  // gain=1x, output active
constexpr uint16_t kByteMask = 0x00FFu;
}

namespace fmq {

void Mcp4922Dac::write(Channel channel, uint16_t value12) {
  if (value12 > kDacDataMask) {
    value12 = kDacDataMask;
  }

  // Command word (see MCP4922 datasheet):
  //   bit15 DAC (channel), bit14 BUF=0, bit13 GA=1 (1x), bit12 SHDN=1 (on),
  //   bits11..0 data. 0x30 sets GA and SHDN in the high byte.
  const uint8_t highByte = static_cast<uint8_t>(
      (static_cast<uint8_t>(channel) << kDacChannelBit) | kDacControlBits |
      ((value12 >> 8u) & kDacDataHighMask));
  const uint8_t lowByte = static_cast<uint8_t>(value12 & kByteMask);

  uint8_t frame[2] = {highByte, lowByte};
  chipSelect_.set(false);        // begin transfer
  spi_.transfer(frame, 2);
  chipSelect_.set(true);         // latch the new value (LDAC tied low)
}

}  // namespace fmq
