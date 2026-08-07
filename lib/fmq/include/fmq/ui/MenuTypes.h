/**
 * @file MenuTypes.h
 * Defines menu input/output events, pages and shared UI types.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_UI_MENU_TYPES_H
#define FMQ_UI_MENU_TYPES_H
#include "fmq/application/Button.h"
#include "fmq/application/ButtonLadder.h"
#include "fmq/domain/LedColor.h"
namespace fmq {
struct MenuInput {
  ButtonEvent keyEvent;
  LongPressButtonState loadButton;
  LongPressButtonState saveButton;
  bool shiftPressed;
};
struct MenuOutput {
  LedFrame frame;
  bool persistentStateChanged;
};
}
#endif
