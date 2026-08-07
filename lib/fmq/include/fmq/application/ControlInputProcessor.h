/**
 * @file ControlInputProcessor.h
 * Combines physical control inputs into menu-ready input events.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_CONTROL_INPUT_PROCESSOR_H
#define FM_QUANTIZER_CORE_CONTROL_INPUT_PROCESSOR_H

#include <stdint.h>

#include "fmq/application/Button.h"
#include "fmq/application/ButtonLadder.h"
#include "fmq/config/UiConfig.h"
#include "fmq/ui/MenuTypes.h"

namespace fmq {

struct RawControlInput {
  uint16_t ladderAdc;
  bool shiftPressed;
  bool savePressed;
  bool loadPressed;
};

class ControlInputProcessor {
 public:
  ControlInputProcessor(uint32_t digitalDebounceMs = config::kDigitalDebounceMs,
                        uint32_t longPressMs = config::kLongPressMs)
      : save_(digitalDebounceMs, longPressMs),
        load_(digitalDebounceMs, longPressMs) {}

  void calibrateLadderRest(uint16_t adcValue) {
    ladder_.calibrateRest(adcValue);
  }

  MenuInput sample(uint32_t currentTimeMs, const RawControlInput &raw);

 private:
  ButtonLadder ladder_;
  ButtonWithLongPress save_;
  ButtonWithLongPress load_;
};

}  // namespace fmq

#endif
