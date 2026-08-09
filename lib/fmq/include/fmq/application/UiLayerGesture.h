/**
 * @file UiLayerGesture.h
 * Debounced SHIFT double-click gesture used to switch UI layers.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_APPLICATION_UI_LAYER_GESTURE_H
#define FMQ_APPLICATION_UI_LAYER_GESTURE_H

#include <stdint.h>

namespace fmq {

enum class UiLayerGestureAction : uint8_t { None = 0, ToggleLayer = 1 };

/**
 * @brief Recognises an isolated, debounced double-click on SHIFT.
 *
 * SHIFT remains an immediate modifier for normal menu shortcuts. This class
 * observes the same raw state independently and only emits ToggleLayer after
 * two short, clean SHIFT clicks. Any note, SAVE or LOAD activity cancels the
 * pending sequence, preventing ordinary SHIFT shortcuts from changing layers.
 */
class UiLayerGesture {
 public:
  UiLayerGesture();
  UiLayerGestureAction update(bool shiftPressed, bool companionControlActive,
                              bool blocked, uint32_t nowMs);
  void reset();

 private:
  bool rawShiftPressed_;
  bool debouncedShiftPressed_;
  bool waitingForSecondClick_;
  bool currentPressIsSecond_;
  bool currentPressInvalid_;
  bool suppressUntilRelease_;
  uint32_t rawChangedMs_;
  uint32_t pressStartedMs_;
  uint32_t firstClickReleasedMs_;
};

}  // namespace fmq

#endif
