/**
 * @file ControlInputProcessor.cpp
 * Implements control-input sampling, modifier handling and event generation.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/ControlInputProcessor.h"

namespace fmq {

MenuInput ControlInputProcessor::sample(uint32_t currentTimeMs,
                                        const RawControlInput &raw) {
  MenuInput input;
  // Keep an immediate physical-ladder indication alongside the debounced event.
  // The SHIFT double-click layer gesture must be cancelled as soon as a note
  // key is physically pressed. Waiting for the 64 ms ladder debounce could let
  // a normal SHIFT shortcut finish the double-click sequence first. This flag
  // is cancellation-only and never executes a command.
  input.noteButtonDown =
      buttonIndexForAdc(raw.ladderAdc, ladder_.restAdc()) != kButtonLadderNoButton;
  input.keyEvent = ladder_.sample(currentTimeMs, raw.ladderAdc);
  // SHIFT deliberately bypasses debounce. It is a modifier and must already be
  // visible when a simultaneously pressed ladder key completes its much longer
  // debounce interval. This matches the original Rust firmware.
  input.shiftPressed = raw.shiftPressed;
  input.saveButton = save_.sample(raw.savePressed, currentTimeMs);
  input.loadButton = load_.sample(raw.loadPressed, currentTimeMs);
  return input;
}

}  // namespace fmq
