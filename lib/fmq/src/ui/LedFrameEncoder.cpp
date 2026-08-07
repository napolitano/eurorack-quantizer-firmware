/**
 * @file LedFrameEncoder.cpp
 * Encodes logical note-ring colours into TLC5947 PWM data.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/ui/LedFrameEncoder.h"

#include "fmq/config/LedConfig.h"
#include "fmq/domain/FixedPoint.h"

namespace fmq {
namespace {
constexpr uint8_t kTlcChannelBits = 12;
constexpr uint16_t kTlcChannelMask = 0x0FFFu;
constexpr uint8_t kByteBits = 8;
constexpr uint32_t kByteMask = 0xFFu;
constexpr uint8_t kPhysicalOrder[kNoteCount] = {6, 7, 8, 9, 10, 11,
                                                0, 1, 2, 3, 4, 5};

uint16_t scaleLevel(uint16_t level, uint16_t intensityQ12) {
  const uint16_t boundedIntensity =
      intensityQ12 > config::kLedPwmMaximum ? config::kLedPwmMaximum
                                            : intensityQ12;
  const uint32_t scaled =
      static_cast<uint32_t>(level) * boundedIntensity +
      config::kLedPwmMaximum / 2u;
  return static_cast<uint16_t>(scaled / config::kLedPwmMaximum);
}

void writeLedWord(LedColor color, uint16_t redLevel, uint16_t greenLevel,
                  uint8_t out[kTlc5947FrameBytes], uint8_t &byteIndex) {
  const uint16_t red = ledUsesRed(color) ? (redLevel & kTlcChannelMask) : 0;
  const uint16_t green =
      ledUsesGreen(color) ? (greenLevel & kTlcChannelMask) : 0;
  const uint32_t word = (static_cast<uint32_t>(red) << kTlcChannelBits) |
                        static_cast<uint32_t>(green);
  out[byteIndex++] =
      static_cast<uint8_t>((word >> (2u * kByteBits)) & kByteMask);
  out[byteIndex++] = static_cast<uint8_t>((word >> kByteBits) & kByteMask);
  out[byteIndex++] = static_cast<uint8_t>(word & kByteMask);
}
}  // namespace

void encodeLedFrame(const LedFrame &frame, uint16_t redLevel,
                    uint16_t greenLevel, uint8_t out[kTlc5947FrameBytes]) {
  uint8_t byteIndex = 0;
  for (uint8_t physicalIndex = 0; physicalIndex < kNoteCount; ++physicalIndex) {
    const uint8_t logicalIndex = kPhysicalOrder[physicalIndex];
    writeLedWord(frame[logicalIndex], redLevel, greenLevel, out, byteIndex);
  }
}

void encodeLedFrameScaled(
    const LedFrame &frame, uint16_t redLevel, uint16_t greenLevel,
    const uint16_t intensityQ12[kNoteCount],
    uint8_t out[kTlc5947FrameBytes]) {
  uint8_t byteIndex = 0;
  for (uint8_t physicalIndex = 0; physicalIndex < kNoteCount; ++physicalIndex) {
    const uint8_t logicalIndex = kPhysicalOrder[physicalIndex];
    const uint16_t intensity = intensityQ12[logicalIndex];
    writeLedWord(frame[logicalIndex], scaleLevel(redLevel, intensity),
                 scaleLevel(greenLevel, intensity), out, byteIndex);
  }
}

}  // namespace fmq
