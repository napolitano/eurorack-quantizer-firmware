/**
 * @file ProductConfig.h
 * Central product defaults and quantizer operating limits.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_PRODUCT_CONFIG_H
#define FMQ_CONFIG_PRODUCT_CONFIG_H

#include <stdint.h>

#include "fmq/domain/Quantizer.h"

namespace fmq::config {

// Product defaults ----------------------------------------------------------
constexpr uint16_t kFactoryScaleMask = 0x0FFF;  // chromatic

// The Rust original powers up in Track-and-Hold and SHIFT+4 toggles only
// between Track-and-Hold and Sample-and-Hold. Continuous remains available as
// an optional C++ extension, but is deliberately excluded from normal UI
// cycling unless this flag is enabled.
constexpr SampleMode kFactorySampleMode = SampleMode::TrackAndHold;
constexpr bool kEnableContinuousSampleModeInUi = false;

constexpr bool kFactoryChannelsLinked = false;
constexpr bool kFactoryChannelBAbsolute = true;

// Quantizer limits ----------------------------------------------------------
constexpr uint16_t kPitchRangeSemitones = 120;
constexpr uint8_t kMaxGlideAmount = 11;
constexpr uint8_t kMaxTriggerDelayAmount = 11;
constexpr int8_t kMinimumShift = -5;
constexpr int8_t kMaximumShift = 6;
constexpr uint8_t kOutputTriggerLedSamples = 65;
constexpr uint8_t kOutputTriggerCvSamples = 5;
constexpr int16_t kHysteresisQ8_8 = 102;

// Arpeggiator limits and defaults. Factory/default operation is intentionally
// internal and free-running: 24 ms, root/third/fifth, no external clock. CLOCK
// is an explicit additional sync mode selected by the user.
constexpr uint8_t kArpRateCount = 12;
constexpr uint8_t kArpDefaultRateIndex = 3;  // 24 ms in FREE/RESET
constexpr uint8_t kArpMaximumLength = 12;
constexpr uint8_t kArpMaximumRange = 4;
constexpr uint8_t kArpMaximumSwingStep = 11;

static_assert((kFactoryScaleMask & 0x0FFFu) != 0,
              "factory scale must not be empty");
static_assert(kPitchRangeSemitones > 0 && kPitchRangeSemitones <= 120,
              "invalid pitch range");
static_assert(kMaxGlideAmount <= 31, "glide amount is unexpectedly large");
static_assert(kMaxTriggerDelayAmount <= 31,
              "trigger delay amount is unexpectedly large");
static_assert(kOutputTriggerCvSamples <= kOutputTriggerLedSamples,
              "CV trigger must fit LED pulse");
static_assert(kArpDefaultRateIndex < kArpRateCount,
              "invalid Arpeggiator default rate");
static_assert(kArpMaximumLength <= 12,
              "Arpeggiator length must fit the note-button UI");

}  // namespace fmq::config

#endif
