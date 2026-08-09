/**
 * @file UiLayerGesture.cpp
 * Implements the SHIFT-only long-hold UI-layer gesture.
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

UiLayerGesture::UiLayerGesture() : holdStartMs_(kNoHold), consumed_(false) {}

void UiLayerGesture::reset() {
  holdStartMs_ = kNoHold;
  consumed_ = false;
}

UiLayerGestureAction UiLayerGesture::update(bool shiftPressed,
                                            bool companionControlActive,
                                            bool blocked, uint32_t nowMs) {
  if (blocked) {
    reset();
    return UiLayerGestureAction::None;
  }
  if (!shiftPressed) {
    reset();
    return UiLayerGestureAction::None;
  }
  if (companionControlActive) {
    holdStartMs_ = kNoHold;
    consumed_ = true;
    return UiLayerGestureAction::None;
  }
  if (consumed_) return UiLayerGestureAction::None;
  if (holdStartMs_ == kNoHold) {
    holdStartMs_ = nowMs;
    return UiLayerGestureAction::None;
  }
  if (nowMs - holdStartMs_ < config::kUiLayerToggleHoldMs) {
    return UiLayerGestureAction::None;
  }
  consumed_ = true;
  return UiLayerGestureAction::ToggleLayer;
}

}  // namespace fmq
