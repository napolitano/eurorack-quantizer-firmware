/**
 * @file CalibrationQualificationStore.h
 * Stores the compact read-only calibration qualification certificate metadata.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PERSISTENCE_CALIBRATION_QUALIFICATION_STORE_H
#define FMQ_PERSISTENCE_CALIBRATION_QUALIFICATION_STORE_H

#include <stdint.h>

#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/ports/Eeprom.h"

namespace fmq {

/** Compact verification result bound to one calibration-journal generation. */
struct CalibrationQualification {
  bool valid = false;
  uint8_t generation = 0u;
  uint8_t maxResidualA = 0u;
  uint8_t maxResidualB = 0u;
};

/**
 * Two-byte qualification metadata store.
 *
 * The validity marker occupies the former <=0.2.1 twelfth live-slot marker at
 * EEPROM byte 966. The packed A/B residual byte occupies byte 1023. The marker
 * uses a value that can never be mistaken for a live-state marker. Writes are
 * ordered invalidate -> residuals -> commit marker, so an interrupted update
 * cannot certify a partial record.
 */
class CalibrationQualificationStore {
 public:
  CalibrationQualificationStore(const IEeprom &eeprom,
                                AsyncEepromWriter &writer)
      : eeprom_(eeprom), writer_(writer) {}

  /** Load a certificate only when it matches @p expectedGeneration. */
  CalibrationQualification load(uint8_t expectedGeneration) const;

  /** Queue an atomic certificate write. Residuals must fit 4 bits each. */
  bool save(uint8_t generation, uint8_t maxResidualA, uint8_t maxResidualB);

  /** Invalidate any existing certificate marker. */
  bool invalidate();

  bool busy() const { return writer_.busy(); }
  void service() { writer_.service(); }

#if !defined(ARDUINO)
  void flush() { writer_.flush(); }
#endif

 private:
  const IEeprom &eeprom_;
  AsyncEepromWriter &writer_;
};

}  // namespace fmq

#endif
