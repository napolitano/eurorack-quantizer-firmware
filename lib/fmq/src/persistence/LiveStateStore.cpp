/**
 * @file LiveStateStore.cpp
 * Implements versioned wear-levelled live-state persistence.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/LiveStateStore.h"

#include "fmq/persistence/Crc16.h"
#include "fmq/persistence/Serialization.h"

namespace fmq {

namespace {
// Byte offsets within a live record.
constexpr uint16_t kOffMarker = 0;
constexpr uint16_t kOffSeq = 1;      // 4 bytes, little-endian
constexpr uint16_t kOffVersion = 5;  // 1 byte
constexpr uint16_t kOffState = 6;    // kStateBytes
constexpr uint16_t kOffBright = 6 + kStateBytes;      // 2 bytes
constexpr uint16_t kOffCrc = kOffBright + kBrightnessBytes;  // 2 bytes
// Number of bytes covered by the CRC (everything before the CRC field).
constexpr uint16_t kCrcCoverage = kOffCrc;

static_assert(kOffCrc + 2 == kLiveSlotSize, "live record framing mismatch");

/// Read and validate a live record into a raw buffer.
/// @return true if marker + CRC are valid; @p seqOut then holds its sequence.
bool readRecord(const IEeprom &eeprom, uint16_t address, uint8_t *buffer,
                uint32_t &seqOut) {
  if (eeprom.readByte(address) != kRecordMarker) {
    return false;
  }
  eeprom.readBytes(address, buffer, kLiveSlotSize);

  if (buffer[kOffVersion] != kLiveFormatVersion) {
    return false;
  }

  const uint16_t storedCrc = static_cast<uint16_t>(
      static_cast<uint16_t>(static_cast<uint16_t>(buffer[kOffCrc]) << 8u) |
      static_cast<uint16_t>(buffer[kOffCrc + 1u]));
  if (crc16Ccitt(buffer, kCrcCoverage) != storedCrc) {
    return false;
  }

  seqOut = static_cast<uint32_t>(buffer[kOffSeq]) |
           (static_cast<uint32_t>(buffer[kOffSeq + 1]) << 8) |
           (static_cast<uint32_t>(buffer[kOffSeq + 2]) << 16) |
           (static_cast<uint32_t>(buffer[kOffSeq + 3]) << 24);
  return true;
}
}  // namespace

bool LiveStateStore::load(LiveState &out) {
  uint8_t buffer[kLiveSlotSize];
  uint8_t bestBuffer[kLiveSlotSize];
  bool found = false;
  uint32_t bestSeq = 0;
  uint16_t bestIndex = 0;

  for (uint16_t i = 0; i < kLiveSlotCount; ++i) {
    uint32_t seq;
    if (readRecord(eeprom_, slotAddress(i), buffer, seq)) {
      if (!found || seq > bestSeq) {
        found = true;
        bestSeq = seq;
        bestIndex = i;
        for (uint16_t b = 0; b < kLiveSlotSize; ++b) {
          bestBuffer[b] = buffer[b];
        }
      }
    }
  }

  if (!found) {
    // Blank or unrecoverable: hand back defaults and arrange for the first
    // commit to land in slot 0.
    out.state = QuantizerState();
    out.brightness = BrightnessCalibration::makeDefault();
    head_ = static_cast<uint16_t>(kLiveSlotCount - 1);
    seq_ = 0;
    return false;
  }

  out.state = decodeState(&bestBuffer[kOffState]);
  out.brightness.redStep = bestBuffer[kOffBright];
  out.brightness.greenStep = bestBuffer[kOffBright + 1];
  const auto validBrightness = [](uint8_t step) {
    return step == BrightnessCalibration::kUseLegacyDefault ||
           step < BrightnessCalibration::kStepCount;
  };
  if (!validBrightness(out.brightness.redStep)) {
    out.brightness.redStep = BrightnessCalibration::kUseLegacyDefault;
  }
  if (!validBrightness(out.brightness.greenStep)) {
    out.brightness.greenStep = BrightnessCalibration::kUseLegacyDefault;
  }
  head_ = bestIndex;
  seq_ = bestSeq;
  return true;
}

bool LiveStateStore::commit(const QuantizerState &state,
                            const BrightnessCalibration &brightness) {
  if (writer_.busy() || pendingCommit_) return false;
  static_assert(kLiveSlotSize + 1u <= AsyncEepromWriter::kMaxOps,
                "EEPROM queue too small for live-state record");

  const uint16_t index = static_cast<uint16_t>((head_ + 1u) % kLiveSlotCount);
  const uint16_t address = slotAddress(index);
  const uint32_t newSeq = seq_ + 1u;

  uint8_t buffer[kLiveSlotSize];
  buffer[kOffMarker] = kRecordMarker;
  buffer[kOffSeq] = static_cast<uint8_t>(newSeq);
  buffer[kOffSeq + 1u] = static_cast<uint8_t>(newSeq >> 8);
  buffer[kOffSeq + 2u] = static_cast<uint8_t>(newSeq >> 16);
  buffer[kOffSeq + 3u] = static_cast<uint8_t>(newSeq >> 24);
  buffer[kOffVersion] = kLiveFormatVersion;
  encodeState(state, &buffer[kOffState]);
  buffer[kOffBright] = brightness.redStep;
  buffer[kOffBright + 1u] = brightness.greenStep;
  const uint16_t crc = crc16Ccitt(buffer, kCrcCoverage);
  buffer[kOffCrc] = static_cast<uint8_t>(crc >> 8);
  buffer[kOffCrc + 1u] = static_cast<uint8_t>(crc);

  uint16_t addresses[AsyncEepromWriter::kMaxOps];
  uint8_t values[AsyncEepromWriter::kMaxOps];
  uint8_t count = 0;
  addresses[count] = address;
  values[count++] = kErasedByte;
  for (uint16_t i = 1; i < kLiveSlotSize; ++i) {
    addresses[count] = static_cast<uint16_t>(address + i);
    values[count++] = buffer[i];
  }
  addresses[count] = address;
  values[count++] = kRecordMarker;

  if (!writer_.begin(addresses, values, count)) return false;
  pendingHead_ = index;
  pendingSeq_ = newSeq;
  pendingCommit_ = true;
  return true;
}

void LiveStateStore::observeWriter() {
  if (pendingCommit_ && !writer_.busy()) {
    head_ = pendingHead_;
    seq_ = pendingSeq_;
    pendingCommit_ = false;
  }
}

}  // namespace fmq
