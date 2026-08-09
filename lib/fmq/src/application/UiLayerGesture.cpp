/**
 * @file UiLayerGesture.cpp
 * Implements the debounced SHIFT double-click UI-layer gesture.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/UiLayerGesture.h"

#include "fmq/config/UiConfig.h"

namespace fmq {

UiLayerGesture::UiLayerGesture()
    : rawShiftPressed_(false),
      debouncedShiftPressed_(false),
      waitingForSecondClick_(false),
      currentPressIsSecond_(false),
      currentPressInvalid_(false),
      suppressUntilRelease_(false),
      rawChangedMs_(0u),
      pressStartedMs_(0u),
      firstClickReleasedMs_(0u) {}

void UiLayerGesture::reset() {
  rawShiftPressed_ = false;
  debouncedShiftPressed_ = false;
  waitingForSecondClick_ = false;
  currentPressIsSecond_ = false;
  currentPressInvalid_ = false;
  suppressUntilRelease_ = false;
  rawChangedMs_ = 0u;
  pressStartedMs_ = 0u;
  firstClickReleasedMs_ = 0u;
}

UiLayerGestureAction UiLayerGesture::update(bool shiftPressed,
                                            bool companionControlActive,
                                            bool blocked, uint32_t nowMs) {
  if (shiftPressed != rawShiftPressed_) {
    rawShiftPressed_ = shiftPressed;
    rawChangedMs_ = nowMs;
  }

  // A layer change is only legal from the stable main page. Likewise, any
  // physical companion control makes the current/pending SHIFT sequence a
  // normal modifier interaction rather than a layer gesture.
  if (blocked) {
    waitingForSecondClick_ = false;
    currentPressIsSecond_ = false;
    if (shiftPressed || debouncedShiftPressed_) {
      currentPressInvalid_ = true;
      suppressUntilRelease_ = true;
    }
  }
  if (companionControlActive) {
    waitingForSecondClick_ = false;
    currentPressIsSecond_ = false;
    if (shiftPressed || debouncedShiftPressed_) {
      currentPressInvalid_ = true;
      suppressUntilRelease_ = true;
    }
  }

  // Expire a lone first click once the second raw press can no longer have
  // started inside the configured double-click interval.
  if (waitingForSecondClick_ && !rawShiftPressed_ &&
      nowMs - firstClickReleasedMs_ > config::kUiLayerDoubleClickGapMs) {
    waitingForSecondClick_ = false;
  }

  // Debounce only the gesture recogniser. Menu::update still receives the raw
  // SHIFT state immediately, preserving simultaneous SHIFT+note operation.
  if (rawShiftPressed_ == debouncedShiftPressed_ ||
      nowMs - rawChangedMs_ < config::kUiLayerDoubleClickDebounceMs) {
    return UiLayerGestureAction::None;
  }

  debouncedShiftPressed_ = rawShiftPressed_;
  if (debouncedShiftPressed_) {
    pressStartedMs_ = nowMs;
    currentPressIsSecond_ =
        waitingForSecondClick_ &&
        rawChangedMs_ - firstClickReleasedMs_ <=
            config::kUiLayerDoubleClickGapMs;
    if (!currentPressIsSecond_) {
      waitingForSecondClick_ = false;
    }
    return UiLayerGestureAction::None;
  }

  const uint32_t pressDurationMs = nowMs - pressStartedMs_;
  const bool validShortClick =
      !currentPressInvalid_ && !suppressUntilRelease_ &&
      pressDurationMs <= config::kUiLayerDoubleClickMaxPressMs;

  if (suppressUntilRelease_) {
    suppressUntilRelease_ = false;
    currentPressInvalid_ = false;
    currentPressIsSecond_ = false;
    waitingForSecondClick_ = false;
    return UiLayerGestureAction::None;
  }

  currentPressInvalid_ = false;
  if (!validShortClick) {
    currentPressIsSecond_ = false;
    waitingForSecondClick_ = false;
    return UiLayerGestureAction::None;
  }

  if (currentPressIsSecond_) {
    currentPressIsSecond_ = false;
    waitingForSecondClick_ = false;
    return UiLayerGestureAction::ToggleLayer;
  }

  waitingForSecondClick_ = true;
  firstClickReleasedMs_ = nowMs;
  return UiLayerGestureAction::None;
}

}  // namespace fmq
