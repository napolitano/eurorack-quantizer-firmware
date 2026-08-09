/**
 * @file LiveStateStore.h
 * Wear-levelled persistence of the complete running musical state.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_LIVE_STATE_STORE_H
#define FM_QUANTIZER_CORE_LIVE_STATE_STORE_H

#include <stdint.h>

#include "fmq/application/StoredConfiguration.h"
#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "fmq/ports/Eeprom.h"
#include "fmq/ui/BrightnessCalibration.h"
#include "fmq/application/UiLayer.h"

namespace fmq {

struct LiveState {
  StoredConfiguration configuration;
  BrightnessCalibration brightness;
  UiLayer uiLayer = UiLayer::Quantizer;
};

class LiveStateStore {
 public:
  LiveStateStore(IEeprom &eeprom, AsyncEepromWriter &writer)
      : eeprom_(eeprom),
        writer_(writer),
        head_(0),
        seq_(0),
        pendingHead_(0),
        pendingSeq_(0),
        pendingCommit_(false) {}

  bool busy() const { return writer_.busy(); }
  void observeWriter();
#if !defined(ARDUINO)
  void flush() { writer_.flush(); observeWriter(); }
#endif

  bool load(LiveState &out);
  bool commit(const StoredConfiguration &configuration,
              const BrightnessCalibration &brightness, UiLayer uiLayer);

  uint32_t currentSequence() const { return seq_; }

 private:
  uint16_t slotAddress(uint16_t index) const {
    return static_cast<uint16_t>(kLiveRingBase + index * kLiveSlotSize);
  }

  IEeprom &eeprom_;
  AsyncEepromWriter &writer_;
  uint16_t head_;
  uint32_t seq_;
  uint16_t pendingHead_;
  uint32_t pendingSeq_;
  bool pendingCommit_;
};

}  // namespace fmq

#endif
