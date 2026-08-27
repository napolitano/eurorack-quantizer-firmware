/**
 * @file CalibrationReader.h
 * Read-only access to the redundant analogue-calibration EEPROM journal.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PERSISTENCE_CALIBRATION_READER_H
#define FMQ_PERSISTENCE_CALIBRATION_READER_H

#include <stdint.h>

#include "fmq/domain/CalibrationProfile.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/ports/Eeprom.h"

namespace fmq {

struct CalibrationRecordStatus {
  bool valid = false;
  uint8_t generation = 0u;
};

struct CalibrationJournalStatus {
  CalibrationRecordStatus records[kCalibrationRecordCount];
  int8_t activeIndex = -1;
};

/**
 * Read-only calibration journal used by normal production firmware.
 *
 * The reader has no write API. In production it is constructed on an
 * EepromPartition covering only the calibration region, making preset writes
 * impossible by construction from this component.
 */
class CalibrationReader {
 public:
  explicit CalibrationReader(const IEeprom &eeprom)
      : eeprom_(eeprom), activeIndex_(1u), generation_(15u) {}

  /** Load the newest valid calibration record into @p out. */
  bool load(CalibrationProfile &out);

  /** Inspect both records without changing the active runtime profile. */
  CalibrationJournalStatus inspect() const;

  /** Validate whether a profile can be encoded by journal format v1. */
  static bool profileFitsPersistentFormat(const CalibrationProfile &profile);

 protected:
  uint16_t recordAddress(uint8_t index) const {
    return static_cast<uint16_t>(static_cast<uint16_t>(index) *
                                 kCalibrationRecordSize);
  }

  const IEeprom &eeprom_;
  uint8_t activeIndex_;
  uint8_t generation_;
};

}  // namespace fmq

#endif
