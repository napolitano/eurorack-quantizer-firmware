/**
 * @file CalibrationStore.cpp
 * Implements atomic writes to the maintenance calibration partition.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/CalibrationStore.h"

#include "fmq/persistence/Crc16.h"
#include "fmq/persistence/PersistenceLayout.h"

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
constexpr uint8_t kCalibrationBytes = 6u;

static_assert(kOffCrc + 2u == kCalibrationRecordSize,
              "calibration record framing mismatch");
static_assert(kCalibrationRecordSize + 1u <= AsyncEepromWriter::kMaxOps,
              "EEPROM queue too small for calibration record");

void writeU16(uint8_t *data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value);
  data[1] = static_cast<uint8_t>(value >> 8u);
}

void writeI16(uint8_t *data, int16_t value) {
  writeU16(data, static_cast<uint16_t>(value));
}

void encodeLinear(const LinearCalibration &calibration, uint8_t *out) {
  writeI16(out, static_cast<int16_t>(calibration.offset));
  writeU16(out + 2u, calibration.gainNumerator);
  writeU16(out + 4u, calibration.gainDenominator);
}

void encodeProfile(const CalibrationProfile &profile, uint8_t *payload) {
  encodeLinear(profile.adc[0], payload + 0u * kCalibrationBytes);
  encodeLinear(profile.adc[1], payload + 1u * kCalibrationBytes);
  encodeLinear(profile.dac[0], payload + 2u * kCalibrationBytes);
  encodeLinear(profile.dac[1], payload + 3u * kCalibrationBytes);
}

uint8_t metaByte(uint8_t generation) {
  return static_cast<uint8_t>((kCalibrationFormatVersion << kVersionShift) |
                              (generation & kGenerationMask));
}

}  // namespace

bool CalibrationStore::save(const CalibrationProfile &profile) {
  if (writer_.busy() || pendingCommit_ ||
      !profileFitsPersistentFormat(profile)) {
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
