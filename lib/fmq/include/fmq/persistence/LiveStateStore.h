/**
 * @file LiveStateStore.h
 * Declares optional wear-levelled persistence of the running state.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_LIVE_STATE_STORE_H
#define FM_QUANTIZER_CORE_LIVE_STATE_STORE_H

#include <stdint.h>

#include "fmq/ui/BrightnessCalibration.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/ports/Eeprom.h"
#include "fmq/persistence/AsyncEepromWriter.h"

/**
 * Wear-levelled persistence of the working configuration.
 *
 * To make the module power on exactly as it was left, the current quantizer
 * state and LED calibration are mirrored into EEPROM. Writing the same handful
 * of cells on every change would wear them out, so records are written round
 * robin across a ring of slots, each tagged with a monotonically increasing
 * 32-bit sequence number. On boot, the newest valid slot wins.
 *
 * The 32-bit sequence number cannot realistically wrap (4 billion commits far
 * exceeds the EEPROM's endurance), which keeps the "newest = largest sequence"
 * rule simple and always correct.
 */
namespace fmq {

/// The complete set of values mirrored to the live-state ring.
struct LiveState {
  QuantizerState state;
  BrightnessCalibration brightness;
};

class LiveStateStore {
 public:
  LiveStateStore(IEeprom &eeprom, AsyncEepromWriter &writer) : eeprom_(eeprom), writer_(writer), head_(0), seq_(0), pendingHead_(0), pendingSeq_(0), pendingCommit_(false) {}
  bool busy() const { return writer_.busy(); }
  void observeWriter();
#if !defined(ARDUINO)
  void flush() { writer_.flush(); observeWriter(); }
#endif

  /**
   * @brief Find and decode the most recently committed live state.
   *
   * Also primes the internal ring cursor so the next commit writes to the slot
   * after the newest one. If no valid record exists (blank or fully corrupt
   * EEPROM), @p out receives defaults and this returns false.
   *
   * @param out Receives the loaded (or default) live state.
   * @return true if a valid stored record was found.
   */
  bool load(LiveState &out);

  /**
   * @brief Write a new live-state record to the next ring slot.
   * @param state      Quantizer state to persist.
   * @param brightness LED calibration to persist.
   */
  bool commit(const QuantizerState &state,
              const BrightnessCalibration &brightness);

  /// @return The sequence number of the most recent committed record.
  uint32_t currentSequence() const { return seq_; }

 private:
  uint16_t slotAddress(uint16_t index) const {
    return static_cast<uint16_t>(kLiveRingBase + index * kLiveSlotSize);
  }

  IEeprom &eeprom_;
  AsyncEepromWriter &writer_;
  uint16_t head_;  ///< Index of the slot holding the newest record.
  uint32_t seq_;   ///< Sequence number of the newest record (0 if none).
  uint16_t pendingHead_;
  uint32_t pendingSeq_;
  bool pendingCommit_;
};

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_LIVE_STATE_STORE_H
