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

// Double-click SHIFT by itself to switch between the Quantizer and Arpeggiator
// UI layers. The gesture has its own debounce so SHIFT can remain an immediate
// modifier for normal shortcuts. Both clicks must be short and the second press
// must begin within the configured gap. Any note, SAVE or LOAD activity cancels
// the pending sequence.
constexpr uint32_t kUiLayerDoubleClickDebounceMs = kDigitalDebounceMs;
constexpr uint32_t kUiLayerDoubleClickMaxPressMs = 350;
constexpr uint32_t kUiLayerDoubleClickGapMs = 350;
static_assert(kUiLayerDoubleClickDebounceMs > 0u,
              "UI-layer double-click debounce must be non-zero");
static_assert(kUiLayerDoubleClickMaxPressMs >= kUiLayerDoubleClickDebounceMs,
              "UI-layer click window must exceed debounce");

// SHIFT is a modifier, not an action button. It must be visible to the menu
// before a simultaneously pressed ladder key finishes its debounce interval.
// The Rust firmware reads SHIFT directly for exactly this reason.
constexpr bool kShiftIsImmediateModifier = true;

// Menu timing ---------------------------------------------------------------
constexpr uint32_t kCalibrationEnterHoldMs = 5000;
constexpr uint32_t kCalibrationEnterBlinkMs = 900;
constexpr uint32_t kCalibrationSaveHoldMs = 5000;
constexpr uint32_t kCalibrationSaveBlinkMs = 1200;
// After selecting a brightness step, briefly mark its ring position with a
// dark gap before returning to the live green/red/amber balance preview.
constexpr uint32_t kCalibrationStepMarkerMs = 350;
constexpr uint32_t kMenuBlinkHalfPeriodMs = 200;
constexpr uint32_t kBoolOptionFeedbackMs = 700;
// Arpeggiator enable/bypass feedback uses the complete note ring rather than a
// single menu-position LED: two green flashes when enabled, two red flashes
// when disabled, then the normal scale display is restored.
constexpr uint32_t kArpToggleBlinkHalfPeriodMs = 150;
constexpr uint8_t kArpToggleBlinkCount = 2;
constexpr uint32_t kArpToggleFeedbackMs =
    kArpToggleBlinkHalfPeriodMs * 2u * kArpToggleBlinkCount;
static_assert(kArpToggleBlinkCount == 2u,
              "Arpeggiator toggle feedback must flash exactly twice");
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

// Arpeggiator-layer SHIFT mappings. The interaction grammar deliberately stays
// identical to the Quantizer layer: SHIFT + note selects a function, then an
// unmodified note selects the value where a scalar parameter is required.
// A/A#/B retain Link / Channel A / Channel B in both layers.
constexpr uint8_t kArpEnableButtonIndex = 0;
constexpr uint8_t kArpRateButtonIndex = 1;
constexpr uint8_t kArpPatternButtonIndex = 2;
constexpr uint8_t kArpShapeButtonIndex = 3;
constexpr uint8_t kArpLengthButtonIndex = 4;
constexpr uint8_t kArpRangeButtonIndex = 5;
constexpr uint8_t kArpStepTriggerButtonIndex = 6;
constexpr uint8_t kArpSyncButtonIndex = 7;
constexpr uint8_t kArpSwingButtonIndex = 8;

// Feedback-animation timing inside menu pages.
constexpr uint32_t kSaveConfirmationBlinkHalfPeriodMs = 128;
constexpr uint32_t kEraseSweepStepMs = 64;
}
#endif
