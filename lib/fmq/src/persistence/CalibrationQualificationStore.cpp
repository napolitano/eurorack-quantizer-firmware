/**
 * @file CalibrationQualificationStore.cpp
 * Implements compact calibration qualification persistence.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/CalibrationQualificationStore.h"

#include "fmq/persistence/PersistenceLayout.h"

namespace fmq {
namespace {

constexpr uint8_t kQualificationMarkerHighNibble = 0xD0u;
constexpr uint8_t kGenerationMask = 0x0Fu;
constexpr uint8_t kResidualMask = 0x0Fu;

uint8_t markerFor(uint8_t generation) {
  return static_cast<uint8_t>(kQualificationMarkerHighNibble |
                              (generation & kGenerationMask));
}

bool markerValid(uint8_t marker) {
  return (marker & 0xF0u) == kQualificationMarkerHighNibble;
}

}  // namespace

CalibrationQualification CalibrationQualificationStore::load(
    uint8_t expectedGeneration) const {
  CalibrationQualification result;
  const uint8_t marker = eeprom_.readByte(kQualificationMarkerAddress);
  if (!markerValid(marker)) return result;

  const uint8_t generation = static_cast<uint8_t>(marker & kGenerationMask);
  if (generation != (expectedGeneration & kGenerationMask)) return result;

  const uint8_t packed = eeprom_.readByte(kQualificationResidualAddress);
  result.valid = true;
  result.generation = generation;
  result.maxResidualA = static_cast<uint8_t>(packed >> 4u);
  result.maxResidualB = static_cast<uint8_t>(packed & kResidualMask);
  return result;
}

bool CalibrationQualificationStore::save(uint8_t generation,
                                         uint8_t maxResidualA,
                                         uint8_t maxResidualB) {
  if (writer_.busy() || maxResidualA > kResidualMask ||
      maxResidualB > kResidualMask) {
    return false;
  }

  const uint16_t addresses[3] = {kQualificationMarkerAddress,
                                 kQualificationResidualAddress,
                                 kQualificationMarkerAddress};
  const uint8_t packed = static_cast<uint8_t>(
      static_cast<uint8_t>(maxResidualA << 4u) |
      static_cast<uint8_t>(maxResidualB & kResidualMask));
  const uint8_t values[3] = {kErasedByte, packed, markerFor(generation)};
  return writer_.begin(addresses, values, 3u);
}

bool CalibrationQualificationStore::invalidate() {
  if (writer_.busy()) return false;
  const uint16_t address = kQualificationMarkerAddress;
  const uint8_t value = kErasedByte;
  return writer_.begin(&address, &value, 1u);
}


bool CalibrationQualificationStore::erase() {
  if (writer_.busy()) return false;
  const uint16_t addresses[2] = {kQualificationMarkerAddress,
                                 kQualificationResidualAddress};
  const uint8_t values[2] = {kErasedByte, kErasedByte};
  return writer_.begin(addresses, values, 2u);
}

}  // namespace fmq
