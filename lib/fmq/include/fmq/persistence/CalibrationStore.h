/**
 * @file CalibrationStore.h
 * Redundant EEPROM persistence for the module analogue calibration profile.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PERSISTENCE_CALIBRATION_STORE_H
#define FMQ_PERSISTENCE_CALIBRATION_STORE_H

#include <stdint.h>

#include "fmq/domain/CalibrationProfile.h"
#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/ports/Eeprom.h"

namespace fmq {

/**
 * Two-record journal for hardware-specific ADC/DAC affine calibration.
 *
 * A save invalidates the destination marker, writes metadata/payload/CRC, then
 * commits the marker last. The previous valid record therefore survives power
 * loss until the replacement is complete.
 */
class CalibrationStore {
 public:
  CalibrationStore(IEeprom &eeprom, AsyncEepromWriter &writer)
      : eeprom_(eeprom), writer_(writer), activeIndex_(1u), generation_(15u),
        pendingIndex_(0u), pendingGeneration_(0u), pendingCommit_(false) {}

  /** Load the newest valid record into @p out. */
  bool load(CalibrationProfile &out);

  /** Queue an atomic save of @p profile into the alternate journal record. */
  bool save(const CalibrationProfile &profile);

  /** Observe completion of a queued save and advance the in-memory journal head. */
  void observeWriter();

  bool busy() const { return writer_.busy() || pendingCommit_; }

#if !defined(ARDUINO)
  /** Host-test helper only; production code remains non-blocking. */
  void flush() { writer_.flush(); observeWriter(); }
#endif

 private:
  uint16_t recordAddress(uint8_t index) const {
    return static_cast<uint16_t>(kCalibrationRegionBase +
                                 static_cast<uint16_t>(index) *
                                     kCalibrationRecordSize);
  }

  IEeprom &eeprom_;
  AsyncEepromWriter &writer_;
  uint8_t activeIndex_;
  uint8_t generation_;
  uint8_t pendingIndex_;
  uint8_t pendingGeneration_;
  bool pendingCommit_;
};

}  // namespace fmq

#endif
