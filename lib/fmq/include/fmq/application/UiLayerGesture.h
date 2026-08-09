/**
 * @file UiLayerGesture.h
 * SHIFT-only long-hold gesture used to switch UI layers.
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

class UiLayerGesture {
 public:
  UiLayerGesture();
  UiLayerGestureAction update(bool shiftPressed, bool companionControlActive,
                              bool blocked, uint32_t nowMs);
  void reset();

 private:
  static constexpr uint32_t kNoHold = 0xFFFFFFFFu;
  uint32_t holdStartMs_;
  bool consumed_;
};

}  // namespace fmq

#endif
