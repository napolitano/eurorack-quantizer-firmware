/**
 * @file CalibrationStore.h
 * Writable maintenance interface for the redundant calibration journal.
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

#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/CalibrationReader.h"

namespace fmq {

/**
 * Writable calibration journal used only by the maintenance firmware.
 *
 * The supplied EEPROM is expected to be an EepromPartition covering exactly
 * the calibration region. Save addresses are therefore partition-relative and
 * cannot overlap presets/live state even when this class is misused.
 */
class CalibrationStore : public CalibrationReader {
 public:
  CalibrationStore(IEeprom &calibrationEeprom, AsyncEepromWriter &writer)
      : CalibrationReader(calibrationEeprom), writer_(writer),
        pendingIndex_(0u), pendingGeneration_(0u), pendingCommit_(false) {}

  /** Queue an atomic save of @p profile into the alternate journal record. */
  bool save(const CalibrationProfile &profile);

  /**
   * Invalidate both redundant calibration records.
   *
   * Only the record markers are erased; payload bytes become unreachable and
   * are deliberately left untouched to minimize EEPROM wear.
   */
  bool erase();

  /** Observe completion of a queued save and advance the in-memory journal head. */
  void observeWriter();

  bool busy() const { return writer_.busy() || pendingCommit_; }

#if !defined(ARDUINO)
  /** Host-test helper only; target code remains non-blocking. */
  void flush() { writer_.flush(); observeWriter(); }
#endif

 private:
  AsyncEepromWriter &writer_;
  uint8_t pendingIndex_;
  uint8_t pendingGeneration_;
  bool pendingCommit_;
};

}  // namespace fmq

#endif
