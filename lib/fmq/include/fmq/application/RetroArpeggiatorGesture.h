/**
 * @file RetroArpeggiatorGesture.h
 * Declares the hardware-independent SHIFT-hold gesture for the Retro Arpeggiator.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_APPLICATION_RETRO_ARPEGGIATOR_GESTURE_H
#define FMQ_APPLICATION_RETRO_ARPEGGIATOR_GESTURE_H

#include <stdint.h>

namespace fmq {

/** Result produced by one gesture update. */
enum class RetroArpeggiatorGestureAction : uint8_t {
  None = 0,
  Toggle = 1,
};

/**
 * Recognises a long SHIFT-only hold without depending on Arduino GPIO or time.
 *
 * A normal SHIFT shortcut cancels the pending hold and requires SHIFT to be
 * released before another arpeggiator gesture may start. The caller can also
 * block recognition while another SHIFT-only workflow (for example LED
 * calibration) owns the control.
 */
class RetroArpeggiatorGesture {
 public:
  RetroArpeggiatorGesture();

  /**
   * Advance the recogniser.
   * @param shiftPressed True while SHIFT is physically held.
   * @param companionControlActive True when a note, SAVE or LOAD participates.
   * @param blocked True while another mode owns the SHIFT-only gesture.
   * @param nowMs Monotonic millisecond clock; unsigned wrap-around is supported.
   * @return Toggle exactly once after a valid hold reaches its configured time.
   */
  RetroArpeggiatorGestureAction update(bool shiftPressed,
                                       bool companionControlActive,
                                       bool blocked, uint32_t nowMs);

  /// Reset to the idle state.
  void reset();

 private:
  static constexpr uint32_t kNoHold = 0xFFFFFFFFu;

  uint32_t holdStartMs_;
  bool consumed_;
};

}  // namespace fmq

#endif  // FMQ_APPLICATION_RETRO_ARPEGGIATOR_GESTURE_H
