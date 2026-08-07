/**
 * @file StartupSequenceStore.h
 * Persists the index of the startup animation that should play next.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_STARTUP_SEQUENCE_STORE_H
#define FMQ_STARTUP_SEQUENCE_STORE_H

#include <stdint.h>

#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/ports/Eeprom.h"

namespace fmq {

class StartupSequenceStore {
 public:
  StartupSequenceStore(IEeprom &eeprom, AsyncEepromWriter &writer)
      : eeprom_(eeprom), writer_(writer) {}

  /** Return the sequence to play now; invalid/erased data falls back to zero. */
  uint8_t loadSequenceToPlay(uint8_t sequenceCount) const;

  /** Queue the sequence that should play on the next boot. */
  bool storeNextSequence(uint8_t sequenceIndex);

 private:
  IEeprom &eeprom_;
  AsyncEepromWriter &writer_;
};

}  // namespace fmq

#endif
