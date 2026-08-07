/**
 * @file Serialization.h
 * Declares stable binary serialization of quantizer state.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_SERIALIZATION_H
#define FM_QUANTIZER_CORE_SERIALIZATION_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"
#include "fmq/domain/Quantizer.h"

/**
 * Fixed, versionable byte layouts for the persisted data structures.
 *
 * These functions define the on-EEPROM representation of a scale, a channel
 * configuration and the full quantizer state. Keeping the (de)serialisation in
 * one place—separate from the storage/wear-levelling logic—means the byte
 * layout is easy to audit and unit-test, and the enums are always decoded into
 * valid values regardless of what bytes are read back.
 */
namespace fmq {

/// Bytes used to store a 12-note scale (one bit per pitch class).
constexpr uint8_t kScaleBytes = 2;
/// Bytes used to store a single channel configuration.
constexpr uint8_t kChannelConfigBytes = 8;
/// Bytes used to store the full two-channel quantizer state.
constexpr uint8_t kStateBytes = 1 + 2 * kChannelConfigBytes;  // flags + A + B

/// Pack a 12-note scale into @ref kScaleBytes bytes (little-endian bit order).
void encodeNotes(const bool notes[kNoteCount], uint8_t out[kScaleBytes]);
/// Unpack @ref kScaleBytes bytes into a 12-note scale.
void decodeNotes(const uint8_t bytes[kScaleBytes], bool notes[kNoteCount]);

/// Serialise one channel configuration into @ref kChannelConfigBytes bytes.
void encodeChannelConfig(const ChannelConfig &config,
                         uint8_t out[kChannelConfigBytes]);
/// Deserialise @ref kChannelConfigBytes bytes into a channel configuration.
/// Enum-valued fields are always decoded to a defined value.
ChannelConfig decodeChannelConfig(const uint8_t bytes[kChannelConfigBytes]);

/// Serialise the full quantizer state into @ref kStateBytes bytes.
void encodeState(const QuantizerState &state, uint8_t out[kStateBytes]);
/// Deserialise @ref kStateBytes bytes into a quantizer state.
QuantizerState decodeState(const uint8_t bytes[kStateBytes]);

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_SERIALIZATION_H
