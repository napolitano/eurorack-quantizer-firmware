/**
 * @file Serialization.h
 * Stable binary serialization for scales, quantizer and Arpeggiator state.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_SERIALIZATION_H
#define FM_QUANTIZER_CORE_SERIALIZATION_H

#include <stdint.h>

#include "fmq/application/StoredConfiguration.h"
#include "fmq/domain/FixedPoint.h"
#include "fmq/domain/Quantizer.h"

namespace fmq {

constexpr uint8_t kScaleBytes = 2;
constexpr uint8_t kChannelConfigBytes = 8;
constexpr uint8_t kStateBytes = 1 + 2 * kChannelConfigBytes;
constexpr uint8_t kArpeggiatorConfigBytes = 7;
constexpr uint8_t kStoredConfigurationBytes =
    1 + kStateBytes + 2 * kArpeggiatorConfigBytes;
/** Byte offset of selectedChannelIndex in StoredConfiguration encoding. */
constexpr uint8_t kStoredSelectedChannelOffset = 0;

void encodeNotes(const bool notes[kNoteCount], uint8_t out[kScaleBytes]);
void decodeNotes(const uint8_t bytes[kScaleBytes], bool notes[kNoteCount]);

void encodeChannelConfig(const ChannelConfig &config,
                         uint8_t out[kChannelConfigBytes]);
ChannelConfig decodeChannelConfig(const uint8_t bytes[kChannelConfigBytes]);

void encodeState(const QuantizerState &state, uint8_t out[kStateBytes]);
QuantizerState decodeState(const uint8_t bytes[kStateBytes]);

void encodeArpeggiatorConfig(
    const ArpeggiatorConfig &config,
    uint8_t out[kArpeggiatorConfigBytes]);
ArpeggiatorConfig decodeArpeggiatorConfig(
    const uint8_t bytes[kArpeggiatorConfigBytes]);

void encodeStoredConfiguration(
    const StoredConfiguration &state,
    uint8_t out[kStoredConfigurationBytes]);
StoredConfiguration decodeStoredConfiguration(
    const uint8_t bytes[kStoredConfigurationBytes]);

}  // namespace fmq

#endif
