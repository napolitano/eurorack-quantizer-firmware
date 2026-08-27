/**
 * @file CalibrationReader.cpp
 * Implements read-only analogue-calibration journal decoding.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/CalibrationReader.h"

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

uint16_t readU16(const uint8_t *data) {
  return static_cast<uint16_t>(data[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8u);
}

int16_t readI16(const uint8_t *data) {
  return static_cast<int16_t>(readU16(data));
}

LinearCalibration decodeLinear(const uint8_t *data) {
  LinearCalibration calibration;
  calibration.offset = readI16(data);
  calibration.gainNumerator = readU16(data + 2u);
  calibration.gainDenominator = readU16(data + 4u);
  return calibration;
}

CalibrationProfile decodeProfile(const uint8_t *payload) {
  CalibrationProfile profile;
  profile.adc[0] = decodeLinear(payload + 0u * kCalibrationBytes);
  profile.adc[1] = decodeLinear(payload + 1u * kCalibrationBytes);
  profile.dac[0] = decodeLinear(payload + 2u * kCalibrationBytes);
  profile.dac[1] = decodeLinear(payload + 3u * kCalibrationBytes);
  return profile;
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
  if (!CalibrationReader::profileFitsPersistentFormat(decoded)) return false;

  profile = decoded;
  generation = static_cast<uint8_t>(meta & kGenerationMask);
  return true;
}

}  // namespace

bool CalibrationReader::profileFitsPersistentFormat(
    const CalibrationProfile &profile) {
  if (!isCalibrationProfileValid(profile)) return false;
  for (uint8_t channel = 0u; channel < 2u; ++channel) {
    if (profile.adc[channel].offset < INT16_MIN ||
        profile.adc[channel].offset > INT16_MAX ||
        profile.dac[channel].offset < INT16_MIN ||
        profile.dac[channel].offset > INT16_MAX) {
      return false;
    }
  }
  return true;
}

CalibrationJournalStatus CalibrationReader::inspect() const {
  CalibrationJournalStatus status;
  bool found = false;
  uint8_t bestGeneration = 0u;
  for (uint8_t index = 0u; index < kCalibrationRecordCount; ++index) {
    CalibrationProfile candidate;
    uint8_t generation = 0u;
    const bool valid = readRecord(eeprom_, recordAddress(index), candidate,
                                  generation);
    status.records[index].valid = valid;
    status.records[index].generation = generation;
    if (valid && (!found || generationIsNewer(generation, bestGeneration))) {
      found = true;
      bestGeneration = generation;
      status.activeIndex = static_cast<int8_t>(index);
    }
  }
  return status;
}

bool CalibrationReader::load(CalibrationProfile &out) {
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

}  // namespace fmq
