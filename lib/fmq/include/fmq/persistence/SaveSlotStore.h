/**
 * @file SaveSlotStore.h
 * Declares versioned scale/config save-slot persistence.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_SAVE_SLOT_STORE_H
#define FM_QUANTIZER_CORE_SAVE_SLOT_STORE_H

#include <stdint.h>

#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/ports/Eeprom.h"
#include "fmq/persistence/AsyncEepromWriter.h"

/**
 * User-facing "save/load slot" storage for scales and full configs.
 *
 * Provides the twelve scale slots and twelve full-config slots that the user
 * saves to and loads from with the SAVE/LOAD buttons. Each slot is individually
 * CRC-protected, so a corrupt slot reads back as simply "empty" instead of
 * loading garbage.
 */
namespace fmq {

/// A 12-bit set indicating which of the twelve slots currently hold data.
struct SlotOccupancy {
  uint16_t bits;  ///< Bit i set => slot i is occupied.

  SlotOccupancy() : bits(0) {}
  bool get(uint8_t slot) const { return (bits >> slot) & 1u; }
  void set(uint8_t slot, bool value) {
    if (value) {
      bits |= static_cast<uint16_t>(1u << slot);
    } else {
      bits &= static_cast<uint16_t>(~(1u << slot));  // correctly clears the bit
    }
  }
};

class SaveSlotStore {
 public:
  SaveSlotStore(IEeprom &eeprom, AsyncEepromWriter &writer) : eeprom_(eeprom), writer_(writer) {}

  bool busy() const { return writer_.busy(); }
#if !defined(ARDUINO)
  void flush() { writer_.flush(); }
#endif

  /// Scan both slot regions and report which slots are occupied.
  void scan(SlotOccupancy &scaleSlots, SlotOccupancy &configSlots) const;

  /// Save the given scale into scale slot @p slot (0..11).
  bool writeScale(uint8_t slot, const bool notes[kNoteCount]);
  /// Load a scale from scale slot @p slot into @p notes.
  /// @return true if the slot held a valid scale (else @p notes is untouched).
  bool readScale(uint8_t slot, bool notes[kNoteCount]) const;

  /// Save the full quantizer state into config slot @p slot (0..11).
  bool writeConfig(uint8_t slot, const QuantizerState &state);
  /// Load a full quantizer state from config slot @p slot into @p state.
  /// @return true if the slot held a valid config (else @p state is untouched).
  bool readConfig(uint8_t slot, QuantizerState &state) const;

  /// Invalidate every scale and config slot (the "erase all" feature).
  bool eraseAll();

 private:
  uint16_t scaleSlotAddress(uint8_t slot) const {
    return static_cast<uint16_t>(kScaleRegionBase + static_cast<uint16_t>(slot) * kScaleSlotSize);
  }
  uint16_t configSlotAddress(uint8_t slot) const {
    return static_cast<uint16_t>(kConfigRegionBase + static_cast<uint16_t>(slot) * kConfigSlotSize);
  }

  IEeprom &eeprom_;
  AsyncEepromWriter &writer_;
};

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_SAVE_SLOT_STORE_H
