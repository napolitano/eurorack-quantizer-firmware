/**
 * @file Button.cpp
 * Implements long-press button state and debounce processing.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/Button.h"

namespace fmq {

ButtonState ButtonDebouncer::sample(bool pressed, uint32_t currentTimeMs) {
  if (pressed != candidate_) {
    // Level changed; (re)start the debounce window.
    candidate_ = pressed;
    lastChangeTime_ = currentTimeMs;
    debouncing_ = true;
  } else if (debouncing_ &&
             currentTimeMs - lastChangeTime_ > debounceTimeMs_) {
    // Level held stable long enough; accept it and emit the edge.
    debouncing_ = false;
    debounced_ = candidate_;
    return debounced_ ? ButtonState::JustPressed : ButtonState::JustReleased;
  }

  return debounced_ ? ButtonState::HeldDown : ButtonState::IsUp;
}

LongPressButtonState ButtonWithLongPress::sample(bool pressed,
                                                 uint32_t currentTimeMs) {
  switch (base_.sample(pressed, currentTimeMs)) {
    case ButtonState::JustPressed:
      waiting_ = true;
      pressStartTime_ = currentTimeMs;
      return LongPressButtonState::ButtonJustDown;

    case ButtonState::JustReleased:
      // If we were still timing the press, it was a short click; otherwise the
      // long-press had already fired.
      if (waiting_) {
        return LongPressButtonState::ButtonJustClickedShort;
      }
      return LongPressButtonState::ButtonJustReleasedLong;

    case ButtonState::HeldDown:
      if (waiting_) {
        if (currentTimeMs - pressStartTime_ > longPressTimeMs_) {
          waiting_ = false;  // fire the long-press exactly once
          return LongPressButtonState::ButtonJustClickedLong;
        }
        return LongPressButtonState::ButtonHeldDownShort;
      }
      return LongPressButtonState::ButtonHeldDownLong;

    case ButtonState::IsUp:
    default:
      return LongPressButtonState::ButtonIsUp;
  }
}

}  // namespace fmq
