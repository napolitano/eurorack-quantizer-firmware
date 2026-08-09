/**
 * @file LiveStateStore.cpp
 * Implements versioned wear-levelled live-state persistence.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/LiveStateStore.h"

#include "fmq/persistence/Crc16.h"
#include "fmq/persistence/Serialization.h"

namespace fmq {
namespace {
constexpr uint16_t kOffMarker = 0;
constexpr uint16_t kOffSeq = 1;
constexpr uint16_t kOffVersion = 5;
constexpr uint16_t kOffConfiguration = 6;
constexpr uint16_t kOffBright = kOffConfiguration + kStoredConfigurationBytes;
constexpr uint16_t kOffCrc = kOffBright + kBrightnessBytes;
constexpr uint16_t kCrcCoverage = kOffCrc;
static_assert(kOffCrc + 2u == kLiveSlotSize, "live record framing mismatch");

bool readRecord(const IEeprom &eeprom, uint16_t address, uint8_t *buffer,
                uint32_t &seqOut, uint8_t &versionOut) {
  if (eeprom.readByte(address) != kRecordMarker) return false;
  eeprom.readBytes(address, buffer, kLiveSlotSize);
  const uint8_t version = buffer[kOffVersion];
  if (version != kLiveFormatVersion && version != kPreviousLiveFormatVersion) {
    return false;
  }

  const uint16_t storedCrc = static_cast<uint16_t>(
      (static_cast<uint16_t>(buffer[kOffCrc]) << 8u) |
      static_cast<uint16_t>(buffer[kOffCrc + 1u]));
  if (crc16Ccitt(buffer, kCrcCoverage) != storedCrc) return false;

  seqOut = static_cast<uint32_t>(buffer[kOffSeq]) |
           (static_cast<uint32_t>(buffer[kOffSeq + 1u]) << 8u) |
           (static_cast<uint32_t>(buffer[kOffSeq + 2u]) << 16u) |
           (static_cast<uint32_t>(buffer[kOffSeq + 3u]) << 24u);
  versionOut = version;
  return true;
}
}  // namespace

bool LiveStateStore::load(LiveState &out) {
  uint8_t buffer[kLiveSlotSize];
  uint8_t bestBuffer[kLiveSlotSize];
  bool found = false;
  uint32_t bestSeq = 0u;
  uint16_t bestIndex = 0u;
  uint8_t bestVersion = 0u;

  for (uint16_t i = 0; i < kLiveSlotCount; ++i) {
    uint32_t seq = 0u;
    uint8_t version = 0u;
    if (readRecord(eeprom_, slotAddress(i), buffer, seq, version) &&
        (!found || seq > bestSeq)) {
      found = true;
      bestSeq = seq;
      bestIndex = i;
      bestVersion = version;
      for (uint16_t b = 0; b < kLiveSlotSize; ++b) bestBuffer[b] = buffer[b];
    }
  }

  if (!found) {
    out.configuration = StoredConfiguration();
    out.brightness = BrightnessCalibration::makeDefault();
    out.uiLayer = UiLayer::Quantizer;
    head_ = static_cast<uint16_t>(kLiveSlotCount - 1u);
    seq_ = 0u;
    return false;
  }

  uint8_t configurationBytes[kStoredConfigurationBytes];
  for (uint8_t i = 0u; i < kStoredConfigurationBytes; ++i) {
    configurationBytes[i] = bestBuffer[kOffConfiguration + i];
  }

  out.configuration = decodeStoredConfiguration(configurationBytes);
  out.uiLayer = out.configuration.uiLayer;

  if (bestVersion == kPreviousLiveFormatVersion) {
    // v5 did not persist the UI layer. Its layer transition semantics guarantee
    // that leaving the Arpeggiator layer disabled both Arpeggiators, so an
    // enabled channel is the best available migration signal. This is only a
    // compatibility fallback; v6 stores the layer explicitly thereafter.
    out.uiLayer =
        out.configuration.arpeggiators[kChannelAIndex].enabled ||
                out.configuration.arpeggiators[kChannelBIndex].enabled
            ? UiLayer::Arpeggiator
            : UiLayer::Quantizer;
    out.configuration.uiLayer = out.uiLayer;
  }

  out.brightness.redStep = bestBuffer[kOffBright];
  out.brightness.greenStep = bestBuffer[kOffBright + 1u];
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

bool LiveStateStore::commit(const StoredConfiguration &configuration,
                            const BrightnessCalibration &brightness,
                            UiLayer uiLayer) {
  if (writer_.busy() || pendingCommit_) return false;
  static_assert(kLiveSlotSize + 1u <= AsyncEepromWriter::kMaxOps,
                "EEPROM queue too small for live-state record");

  const uint16_t index = static_cast<uint16_t>((head_ + 1u) % kLiveSlotCount);
  const uint16_t address = slotAddress(index);
  const uint32_t newSeq = seq_ + 1u;

  uint8_t buffer[kLiveSlotSize];
  buffer[kOffMarker] = kRecordMarker;
  buffer[kOffSeq] = static_cast<uint8_t>(newSeq);
  buffer[kOffSeq + 1u] = static_cast<uint8_t>(newSeq >> 8u);
  buffer[kOffSeq + 2u] = static_cast<uint8_t>(newSeq >> 16u);
  buffer[kOffSeq + 3u] = static_cast<uint8_t>(newSeq >> 24u);
  buffer[kOffVersion] = kLiveFormatVersion;
  StoredConfiguration persisted = configuration;
  persisted.uiLayer = uiLayer;
  encodeStoredConfiguration(persisted, &buffer[kOffConfiguration]);
  buffer[kOffBright] = brightness.redStep;
  buffer[kOffBright + 1u] = brightness.greenStep;
  const uint16_t crc = crc16Ccitt(buffer, kCrcCoverage);
  buffer[kOffCrc] = static_cast<uint8_t>(crc >> 8u);
  buffer[kOffCrc + 1u] = static_cast<uint8_t>(crc);

  uint16_t addresses[AsyncEepromWriter::kMaxOps];
  uint8_t values[AsyncEepromWriter::kMaxOps];
  uint8_t count = 0u;
  addresses[count] = address;
  values[count++] = kErasedByte;
  for (uint16_t i = 1u; i < kLiveSlotSize; ++i) {
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
