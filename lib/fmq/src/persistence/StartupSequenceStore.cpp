/**
 * @file StartupSequenceStore.cpp
 * Implements persistence for cyclic startup-animation selection.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/StartupSequenceStore.h"

#include "fmq/persistence/PersistenceLayout.h"

namespace fmq {

uint8_t StartupSequenceStore::loadSequenceToPlay(uint8_t sequenceCount) const {
  if (sequenceCount == 0 || writer_.busy()) {
    return 0;
  }
  const uint8_t stored = eeprom_.readByte(kStartupSequenceAddress);
  return stored < sequenceCount ? stored : 0;
}

bool StartupSequenceStore::storeNextSequence(uint8_t sequenceIndex) {
  const uint16_t address = kStartupSequenceAddress;
  return writer_.begin(&address, &sequenceIndex, 1);
}

}  // namespace fmq
