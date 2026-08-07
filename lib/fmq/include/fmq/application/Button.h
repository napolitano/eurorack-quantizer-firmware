/**
 * @file Button.h
 * Long-press button event types and debouncing interface.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_BUTTON_H
#define FM_QUANTIZER_CORE_BUTTON_H

#include <stdint.h>

/**
 * Debouncing and long-press detection for the individual push buttons
 *        (SHIFT, SAVE, LOAD).
 *
 * These classes take an already-read boolean "pressed" level plus the current
 * time, which keeps them free of any hardware dependency and fully testable.
 * Debouncing waits until the level has been stable for a configured window
 * before accepting it, so contact bounce does not register as extra presses.
 */
namespace fmq {

/// Debounced state of a simple button.
enum class ButtonState : uint8_t {
  JustPressed,
  JustReleased,
  HeldDown,
  IsUp,
};

/// Debounces a single boolean button input.
class ButtonDebouncer {
 public:
  /// @param debounceTimeMs Stable-time required before a change is accepted.
  explicit ButtonDebouncer(uint32_t debounceTimeMs)
      : debounceTimeMs_(debounceTimeMs),
        lastChangeTime_(0),
        candidate_(false),
        debounced_(false),
        debouncing_(false) {}

  /**
   * @brief Feed the current pressed level and time; get the debounced state.
   * @param pressed       true if the button currently reads as pressed.
   * @param currentTimeMs Current time in milliseconds.
   */
  ButtonState sample(bool pressed, uint32_t currentTimeMs);

 private:
  uint32_t debounceTimeMs_;
  uint32_t lastChangeTime_;
  bool candidate_;
  bool debounced_;
  bool debouncing_;
};

/// Extended button state distinguishing short clicks from long presses.
enum class LongPressButtonState : uint8_t {
  ButtonJustDown,          ///< Just pressed.
  ButtonJustClickedShort,  ///< Released before the long-press timer elapsed.
  ButtonJustClickedLong,   ///< Held long enough that the long-press just fired.
  ButtonJustReleasedLong,  ///< Released after the long-press had fired.
  ButtonHeldDownShort,     ///< Held, long-press timer not yet elapsed.
  ButtonHeldDownLong,      ///< Held, long-press already elapsed.
  ButtonIsUp,              ///< Not pressed.
};

/// Adds long-press detection on top of @ref ButtonDebouncer.
class ButtonWithLongPress {
 public:
  /**
   * @param debounceTimeMs  Debounce window.
   * @param longPressTimeMs Hold time after which a press counts as "long".
   */
  ButtonWithLongPress(uint32_t debounceTimeMs, uint32_t longPressTimeMs)
      : base_(debounceTimeMs),
        longPressTimeMs_(longPressTimeMs),
        waiting_(false),
        pressStartTime_(0) {}

  /// Feed the current pressed level and time; get the long-press-aware state.
  LongPressButtonState sample(bool pressed, uint32_t currentTimeMs);

 private:
  ButtonDebouncer base_;
  uint32_t longPressTimeMs_;
  bool waiting_;             ///< True while timing an in-progress press.
  uint32_t pressStartTime_;  ///< When the current press began.
};

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_BUTTON_H
