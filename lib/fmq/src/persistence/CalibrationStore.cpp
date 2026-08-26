/**
 * @file CalibrationStore.cpp
 * Implements the redundant analogue-calibration EEPROM journal.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/CalibrationStore.h"

#include <limits.h>

#include "fmq/persistence/Crc16.h"

namespace fmq {
namespace {

constexpr uint8_t kCalibrationMarker = 0xC5u;
constexpr uint8_t kCalibrationFormatVersion = 1u;
constexpr uint8_t kVersionShift = 4u;
constexpr uint8_t kGenerationMask = 0x0Fu;
constexpr uint16_t kOffMarker = 0u;
constexpr uint16_t kOffMeta = 1u;
constexpr uint16_t kOffPayload = 2u;
constexpr uint16_t kOffCrc = kOffPayload + kCalibrationPayloadBytes;
constexpr uint16_t kCrcCoverage = kOffCrc;
constexpr uint8_t kCalibrationCount = 4u;
constexpr uint8_t kCalibrationBytes = 6u;

static_assert(kOffCrc + 2u == kCalibrationRecordSize,
              "calibration record framing mismatch");
static_assert(kCalibrationCount * kCalibrationBytes == kCalibrationPayloadBytes,
              "calibration payload framing mismatch");
static_assert(kCalibrationRecordSize + 1u <= AsyncEepromWriter::kMaxOps,
              "EEPROM queue too small for calibration record");

uint16_t readU16(const uint8_t *data) {
  return static_cast<uint16_t>(data[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8u);
}

int16_t readI16(const uint8_t *data) {
  return static_cast<int16_t>(readU16(data));
}

void writeU16(uint8_t *data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value);
  data[1] = static_cast<uint8_t>(value >> 8u);
}

void writeI16(uint8_t *data, int16_t value) {
  writeU16(data, static_cast<uint16_t>(value));
}

bool offsetFitsPersistentFormat(int32_t offset) {
  return offset >= INT16_MIN && offset <= INT16_MAX;
}

bool profileFitsPersistentFormat(const CalibrationProfile &profile) {
  if (!isCalibrationProfileValid(profile)) return false;
  for (uint8_t channel = 0u; channel < 2u; ++channel) {
    if (!offsetFitsPersistentFormat(profile.adc[channel].offset) ||
        !offsetFitsPersistentFormat(profile.dac[channel].offset)) {
      return false;
    }
  }
  return true;
}

void encodeLinear(const LinearCalibration &calibration, uint8_t *out) {
  writeI16(out, static_cast<int16_t>(calibration.offset));
  writeU16(out + 2u, calibration.gainNumerator);
  writeU16(out + 4u, calibration.gainDenominator);
}

LinearCalibration decodeLinear(const uint8_t *data) {
  LinearCalibration calibration;
  calibration.offset = readI16(data);
  calibration.gainNumerator = readU16(data + 2u);
  calibration.gainDenominator = readU16(data + 4u);
  return calibration;
}

void encodeProfile(const CalibrationProfile &profile, uint8_t *payload) {
  encodeLinear(profile.adc[0], payload + 0u * kCalibrationBytes);
  encodeLinear(profile.adc[1], payload + 1u * kCalibrationBytes);
  encodeLinear(profile.dac[0], payload + 2u * kCalibrationBytes);
  encodeLinear(profile.dac[1], payload + 3u * kCalibrationBytes);
}

CalibrationProfile decodeProfile(const uint8_t *payload) {
  CalibrationProfile profile;
  profile.adc[0] = decodeLinear(payload + 0u * kCalibrationBytes);
  profile.adc[1] = decodeLinear(payload + 1u * kCalibrationBytes);
  profile.dac[0] = decodeLinear(payload + 2u * kCalibrationBytes);
  profile.dac[1] = decodeLinear(payload + 3u * kCalibrationBytes);
  return profile;
}

uint8_t metaByte(uint8_t generation) {
  return static_cast<uint8_t>((kCalibrationFormatVersion << kVersionShift) |
                              (generation & kGenerationMask));
}

bool generationIsNewer(uint8_t candidate, uint8_t reference) {
  const uint8_t delta =
      static_cast<uint8_t>((candidate - reference) & kGenerationMask);
  return delta != 0u && delta < 8u;
}

bool readRecord(const IEeprom &eeprom, uint16_t address,
                CalibrationProfile &profile, uint8_t &generation) {
  if (eeprom.readByte(address + kOffMarker) != kCalibrationMarker) return false;

  uint8_t buffer[kCalibrationRecordSize];
  eeprom.readBytes(address, buffer, kCalibrationRecordSize);
  const uint8_t meta = buffer[kOffMeta];
  if ((meta >> kVersionShift) != kCalibrationFormatVersion) return false;

  const uint16_t storedCrc = static_cast<uint16_t>(
      (static_cast<uint16_t>(buffer[kOffCrc]) << 8u) |
      static_cast<uint16_t>(buffer[kOffCrc + 1u]));
  if (crc16Ccitt(buffer, kCrcCoverage) != storedCrc) return false;

  CalibrationProfile decoded = decodeProfile(&buffer[kOffPayload]);
  if (!profileFitsPersistentFormat(decoded)) return false;

  profile = decoded;
  generation = static_cast<uint8_t>(meta & kGenerationMask);
  return true;
}

}  // namespace

bool CalibrationStore::load(CalibrationProfile &out) {
  bool found = false;
  uint8_t bestIndex = 0u;
  uint8_t bestGeneration = 0u;
  CalibrationProfile bestProfile;

  for (uint8_t index = 0u; index < kCalibrationRecordCount; ++index) {
    CalibrationProfile candidate;
    uint8_t generation = 0u;
    if (!readRecord(eeprom_, recordAddress(index), candidate, generation)) {
      continue;
    }
    if (!found || generationIsNewer(generation, bestGeneration)) {
      found = true;
      bestIndex = index;
      bestGeneration = generation;
      bestProfile = candidate;
    }
  }

  if (!found) {
    activeIndex_ = 1u;
    generation_ = 15u;
    return false;
  }

  activeIndex_ = bestIndex;
  generation_ = bestGeneration;
  out = bestProfile;
  return true;
}

bool CalibrationStore::save(const CalibrationProfile &profile) {
  if (writer_.busy() || pendingCommit_ || !profileFitsPersistentFormat(profile)) {
    return false;
  }

  const uint8_t nextIndex = static_cast<uint8_t>((activeIndex_ + 1u) %
                                                  kCalibrationRecordCount);
  const uint8_t nextGeneration =
      static_cast<uint8_t>((generation_ + 1u) & kGenerationMask);
  const uint16_t address = recordAddress(nextIndex);

  uint8_t buffer[kCalibrationRecordSize] = {};
  buffer[kOffMarker] = kCalibrationMarker;
  buffer[kOffMeta] = metaByte(nextGeneration);
  encodeProfile(profile, &buffer[kOffPayload]);
  const uint16_t crc = crc16Ccitt(buffer, kCrcCoverage);
  buffer[kOffCrc] = static_cast<uint8_t>(crc >> 8u);
  buffer[kOffCrc + 1u] = static_cast<uint8_t>(crc);

  uint16_t addresses[AsyncEepromWriter::kMaxOps];
  uint8_t values[AsyncEepromWriter::kMaxOps];
  uint8_t count = 0u;

  addresses[count] = address;
  values[count++] = kErasedByte;
  for (uint16_t i = 1u; i < kCalibrationRecordSize; ++i) {
    addresses[count] = static_cast<uint16_t>(address + i);
    values[count++] = buffer[i];
  }
  addresses[count] = address;
  values[count++] = kCalibrationMarker;

  if (!writer_.begin(addresses, values, count)) return false;
  pendingIndex_ = nextIndex;
  pendingGeneration_ = nextGeneration;
  pendingCommit_ = true;
  return true;
}

void CalibrationStore::observeWriter() {
  if (pendingCommit_ && !writer_.busy()) {
    activeIndex_ = pendingIndex_;
    generation_ = pendingGeneration_;
    pendingCommit_ = false;
  }
}

}  // namespace fmq
