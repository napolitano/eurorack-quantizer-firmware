/**
 * @file UiConfig.h
 * Central control debounce, menu timing and UI mapping configuration.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_UI_CONFIG_H
#define FMQ_CONFIG_UI_CONFIG_H
#include <stdint.h>
namespace fmq::config {
// The original firmware samples the resistor ladder at 1 kHz and requires the
// selected key to remain stable for 64 ms. Keep that proven hardware behaviour
// as the default. SAVE/LOAD use the original 32 ms debounce.
constexpr uint32_t kDigitalDebounceMs = 32;
constexpr uint32_t kLadderDebounceMs = 64;
constexpr uint32_t kLongPressMs = 2000;

// Official runtime feature: hold SHIFT by itself for this long to toggle the
// scale-aware Retro Arpeggiator. Any other control activity cancels the
// pending hold so normal SHIFT shortcuts remain unaffected.
constexpr uint32_t kRetroArpToggleHoldMs = 3000;
constexpr uint32_t kRetroArpFeedbackMs = 500;

// SHIFT is a modifier, not an action button. It must be visible to the menu
// before a simultaneously pressed ladder key finishes its debounce interval.
// The Rust firmware reads SHIFT directly for exactly this reason.
constexpr bool kShiftIsImmediateModifier = true;

// Menu timing ---------------------------------------------------------------
constexpr uint32_t kCalibrationEnterHoldMs = 5000;
constexpr uint32_t kCalibrationEnterBlinkMs = 900;
constexpr uint32_t kCalibrationSaveHoldMs = 5000;
constexpr uint32_t kCalibrationSaveBlinkMs = 1200;
constexpr uint32_t kMenuBlinkHalfPeriodMs = 200;
constexpr uint32_t kBoolOptionFeedbackMs = 700;
constexpr uint32_t kSaveConfirmationMs = 1024;
constexpr uint32_t kEraseConfirmationMs = 832;

// Note-button assignments used while SHIFT is held. Keeping these names here
// makes the physical UI mapping explicit instead of scattering numeric indices
// through the menu state machine.
constexpr uint8_t kRotateScaleLeftButtonIndex = 0;
constexpr uint8_t kRotateScaleRightButtonIndex = 1;
constexpr uint8_t kGlideButtonIndex = 2;
constexpr uint8_t kTriggerDelayButtonIndex = 3;
constexpr uint8_t kSampleModeButtonIndex = 4;
constexpr uint8_t kPostShiftButtonIndex = 5;
constexpr uint8_t kScaleShiftButtonIndex = 6;
constexpr uint8_t kPreShiftButtonIndex = 7;
constexpr uint8_t kChannelBModeButtonIndex = 8;
constexpr uint8_t kChannelLinkButtonIndex = 9;
constexpr uint8_t kChannelAButtonIndex = 10;
constexpr uint8_t kChannelBButtonIndex = 11;
constexpr uint8_t kSignedShiftPositiveMaxButtonIndex = 6;

// Feedback-animation timing inside menu pages.
constexpr uint32_t kSaveConfirmationBlinkHalfPeriodMs = 128;
constexpr uint32_t kEraseSweepStepMs = 64;
}
#endif
