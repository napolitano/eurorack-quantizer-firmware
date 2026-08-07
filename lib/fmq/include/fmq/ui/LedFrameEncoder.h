/**
 * @file LedFrameEncoder.h
 * Declares conversion of logical LED colours to TLC5947 frame bytes.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_LED_FRAME_ENCODER_H
#define FM_QUANTIZER_CORE_LED_FRAME_ENCODER_H

#include <stdint.h>

#include "fmq/domain/LedColor.h"

namespace fmq {

constexpr uint8_t kTlc5947FrameBytes = 36;

/** Encode a logical frame using one brightness per colour channel. */
void encodeLedFrame(const LedFrame &frame, uint16_t redLevel,
                    uint16_t greenLevel, uint8_t out[kTlc5947FrameBytes]);

/**
 * Encode a logical frame with an additional per-LED 0..4095 intensity.
 *
 * This is used by startup effects such as Glowworm and Sparkles. Normal UI
 * rendering continues to use @ref encodeLedFrame so it pays no extra cost.
 */
void encodeLedFrameScaled(
    const LedFrame &frame, uint16_t redLevel, uint16_t greenLevel,
    const uint16_t intensityQ12[kNoteCount],
    uint8_t out[kTlc5947FrameBytes]);

}  // namespace fmq

#endif
