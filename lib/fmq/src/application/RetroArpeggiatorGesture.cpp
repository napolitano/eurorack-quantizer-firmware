/**
 * @file RetroArpeggiatorGesture.cpp
 * Implements the hardware-independent Retro Arpeggiator SHIFT-hold gesture.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/RetroArpeggiatorGesture.h"

#include "fmq/config/UiConfig.h"

namespace fmq {

RetroArpeggiatorGesture::RetroArpeggiatorGesture()
    : holdStartMs_(kNoHold), consumed_(false) {}

void RetroArpeggiatorGesture::reset() {
  holdStartMs_ = kNoHold;
  consumed_ = false;
}

RetroArpeggiatorGestureAction RetroArpeggiatorGesture::update(
    bool shiftPressed, bool companionControlActive, bool blocked,
    uint32_t nowMs) {
  if (blocked) {
    reset();
    return RetroArpeggiatorGestureAction::None;
  }

  if (!shiftPressed) {
    reset();
    return RetroArpeggiatorGestureAction::None;
  }

  if (companionControlActive) {
    holdStartMs_ = kNoHold;
    consumed_ = true;
    return RetroArpeggiatorGestureAction::None;
  }

  if (consumed_) {
    return RetroArpeggiatorGestureAction::None;
  }

  if (holdStartMs_ == kNoHold) {
    holdStartMs_ = nowMs;
    return RetroArpeggiatorGestureAction::None;
  }

  if (nowMs - holdStartMs_ < config::kRetroArpToggleHoldMs) {
    return RetroArpeggiatorGestureAction::None;
  }

  consumed_ = true;
  return RetroArpeggiatorGestureAction::Toggle;
}

}  // namespace fmq
