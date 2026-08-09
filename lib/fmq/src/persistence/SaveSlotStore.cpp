/**
 * @file SaveSlotStore.cpp
 * Implements asynchronous scale/config save-slot persistence.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/SaveSlotStore.h"

#include "fmq/config/ProductConfig.h"
#include "fmq/persistence/Crc16.h"
#include "fmq/persistence/Serialization.h"

namespace fmq {
namespace {
constexpr uint16_t kMaxPayload = kStoredConfigurationBytes;
constexpr uint8_t kMaxRecordOps = static_cast<uint8_t>(kConfigSlotSize + 1u);
static_assert(kMaxRecordOps <= AsyncEepromWriter::kMaxOps, "EEPROM queue too small");
static_assert(kScaleSlotCount + kConfigSlotCount <= AsyncEepromWriter::kMaxOps,
              "EEPROM queue too small for erase-all");

bool readValidatedRecord(const IEeprom &eeprom, uint16_t address,
                         uint8_t *payload, uint16_t length) {
  if (eeprom.readByte(address) != kRecordMarker) return false;
  const uint8_t version =
      eeprom.readByte(static_cast<uint16_t>(address + 1u));
  if (version != kSaveFormatVersion && version != kPreviousSaveFormatVersion) {
    return false;
  }
  eeprom.readBytes(static_cast<uint16_t>(address + 2u), payload, length);
  const uint16_t stored = static_cast<uint16_t>(
      static_cast<uint16_t>(static_cast<uint16_t>(eeprom.readByte(
          static_cast<uint16_t>(address + 2u + length))) << 8u) |
      static_cast<uint16_t>(eeprom.readByte(
          static_cast<uint16_t>(address + 3u + length))));
  uint8_t framed[2u + kMaxPayload];
  framed[0] = kRecordMarker;
  framed[1] = version;
  for (uint16_t i = 0; i < length; ++i) framed[2u + i] = payload[i];
  return crc16Ccitt(framed, static_cast<uint16_t>(2u + length)) == stored;
}

bool queueRecord(AsyncEepromWriter &writer, uint16_t address,
                 const uint8_t *payload, uint16_t length) {
  uint8_t framed[2u + kMaxPayload];
  framed[0] = kRecordMarker;
  framed[1] = kSaveFormatVersion;
  for (uint16_t i = 0; i < length; ++i) framed[2u + i] = payload[i];
  const uint16_t crc = crc16Ccitt(framed, static_cast<uint16_t>(2u + length));

  uint16_t addresses[AsyncEepromWriter::kMaxOps];
  uint8_t values[AsyncEepromWriter::kMaxOps];
  uint8_t count = 0;
  addresses[count] = address;
  values[count++] = kErasedByte;  // invalidate old record first
  addresses[count] = static_cast<uint16_t>(address + 1u);
  values[count++] = kSaveFormatVersion;
  for (uint16_t i = 0; i < length; ++i) {
    addresses[count] = static_cast<uint16_t>(address + 2u + i);
    values[count++] = payload[i];
  }
  addresses[count] = static_cast<uint16_t>(address + 2u + length);
  values[count++] = static_cast<uint8_t>(crc >> 8);
  addresses[count] = static_cast<uint16_t>(address + 3u + length);
  values[count++] = static_cast<uint8_t>(crc);
  addresses[count] = address;
  values[count++] = kRecordMarker;  // commit marker last
  return writer.begin(addresses, values, count);
}
}  // namespace

void SaveSlotStore::scan(SlotOccupancy &scale, SlotOccupancy &config) const {
  if (busy()) return;
  uint8_t payload[kMaxPayload];
  for (uint8_t i = 0; i < kScaleSlotCount; ++i) {
    scale.set(i, readValidatedRecord(eeprom_, scaleSlotAddress(i), payload, kScaleBytes));
  }
  for (uint8_t i = 0; i < kConfigSlotCount; ++i) {
    config.set(i, readValidatedRecord(eeprom_, configSlotAddress(i), payload, kStoredConfigurationBytes));
  }
}

bool SaveSlotStore::writeScale(uint8_t slot, const bool notes[kNoteCount]) {
  if (slot >= kScaleSlotCount || busy()) return false;
  uint8_t payload[kScaleBytes];
  encodeNotes(notes, payload);
  return queueRecord(writer_, scaleSlotAddress(slot), payload, kScaleBytes);
}

bool SaveSlotStore::readScale(uint8_t slot, bool notes[kNoteCount]) const {
  if (slot >= kScaleSlotCount || busy()) return false;
  uint8_t payload[kScaleBytes];
  if (!readValidatedRecord(eeprom_, scaleSlotAddress(slot), payload, kScaleBytes)) return false;
  decodeNotes(payload, notes);
  bool any = false;
  for (uint8_t i = 0; i < kNoteCount; ++i) any = any || notes[i];
  if (!any) {
    for (uint8_t i = 0; i < kNoteCount; ++i) {
      notes[i] = (config::kFactoryScaleMask & (1u << i)) != 0;
    }
  }
  return true;
}

bool SaveSlotStore::writeConfig(uint8_t slot, const StoredConfiguration &state) {
  if (slot >= kConfigSlotCount || busy()) return false;
  uint8_t payload[kStoredConfigurationBytes];
  encodeStoredConfiguration(state, payload);
  return queueRecord(writer_, configSlotAddress(slot), payload,
                     kStoredConfigurationBytes);
}

bool SaveSlotStore::readConfig(uint8_t slot, StoredConfiguration &state) const {
  if (slot >= kConfigSlotCount || busy()) return false;
  uint8_t payload[kStoredConfigurationBytes];
  if (!readValidatedRecord(eeprom_, configSlotAddress(slot), payload,
                           kStoredConfigurationBytes)) {
    return false;
  }
  state = decodeStoredConfiguration(payload);
  return true;
}

bool SaveSlotStore::eraseAll() {
  if (busy()) return false;
  uint16_t addresses[AsyncEepromWriter::kMaxOps];
  uint8_t values[AsyncEepromWriter::kMaxOps];
  uint8_t count = 0;
  for (uint8_t i = 0; i < kScaleSlotCount; ++i) {
    addresses[count] = scaleSlotAddress(i);
    values[count++] = kErasedByte;
  }
  for (uint8_t i = 0; i < kConfigSlotCount; ++i) {
    addresses[count] = configSlotAddress(i);
    values[count++] = kErasedByte;
  }
  return writer_.begin(addresses, values, count);
}

}  // namespace fmq
