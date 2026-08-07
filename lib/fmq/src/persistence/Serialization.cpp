/**
 * @file Serialization.cpp
 * Implements validated binary serialization of quantizer state.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/Serialization.h"
#include "fmq/config/ProductConfig.h"

namespace fmq {
namespace {
constexpr uint8_t kChannelsLinkedFlag = 0x01u;
constexpr uint8_t kChannelBAbsoluteFlag = 0x02u;

constexpr uint8_t kNotesLowOffset = 0;
constexpr uint8_t kNotesHighOffset = 1;
constexpr uint8_t kSampleModeOffset = 2;
constexpr uint8_t kGlideOffset = 3;
constexpr uint8_t kTriggerDelayOffset = 4;
constexpr uint8_t kPreShiftOffset = 5;
constexpr uint8_t kScaleShiftOffset = 6;
constexpr uint8_t kPostShiftOffset = 7;
constexpr uint8_t kStateFlagsOffset = 0;
constexpr uint8_t kChannelAStateOffset = 1;
constexpr uint8_t kChannelBStateOffset =
    kChannelAStateOffset + kChannelConfigBytes;
}

void encodeNotes(const bool notes[kNoteCount], uint8_t out[kScaleBytes]) {
  out[0] = 0;
  out[1] = 0;
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    if (notes[i]) {
      out[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
    }
  }
}

void decodeNotes(const uint8_t bytes[kScaleBytes], bool notes[kNoteCount]) {
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    const uint8_t shifted = static_cast<uint8_t>(bytes[i / 8u] >> (i % 8u));
    notes[i] = (shifted & static_cast<uint8_t>(1u)) != 0u;
  }
}

void encodeChannelConfig(const ChannelConfig &config,
                         uint8_t out[kChannelConfigBytes]) {
  uint8_t noteBytes[kScaleBytes];
  encodeNotes(config.notes, noteBytes);
  out[kNotesLowOffset] = noteBytes[0];
  out[kNotesHighOffset] = noteBytes[1];
  out[kSampleModeOffset] = static_cast<uint8_t>(config.sampleMode);
  out[kGlideOffset] = config.glideAmount;
  out[kTriggerDelayOffset] = config.triggerDelayAmount;
  // Signed values are stored as their two's-complement byte.
  out[kPreShiftOffset] = static_cast<uint8_t>(config.preShift);
  out[kScaleShiftOffset] = static_cast<uint8_t>(config.scaleShift);
  out[kPostShiftOffset] = static_cast<uint8_t>(config.postShift);
}

ChannelConfig decodeChannelConfig(const uint8_t bytes[kChannelConfigBytes]) {
  ChannelConfig config = ChannelConfig::makeDefault();
  uint8_t noteBytes[kScaleBytes] = {bytes[kNotesLowOffset], bytes[kNotesHighOffset]};
  decodeNotes(noteBytes, config.notes);
  switch (bytes[kSampleModeOffset]) {
    case 0: config.sampleMode = SampleMode::TrackAndHold; break;
    case 1: config.sampleMode = SampleMode::SampleAndHold; break;
    case 2: config.sampleMode = SampleMode::Continuous; break;
    default: config.sampleMode = fmq::config::kFactorySampleMode; break;
  }
  config.glideAmount =
      bytes[kGlideOffset] > fmq::config::kMaxGlideAmount
          ? fmq::config::kMaxGlideAmount
          : bytes[kGlideOffset];
  config.triggerDelayAmount =
      bytes[kTriggerDelayOffset] > fmq::config::kMaxTriggerDelayAmount
          ? fmq::config::kMaxTriggerDelayAmount
          : bytes[kTriggerDelayOffset];
  const auto clampShift = [](int8_t value) -> int8_t {
    return clampInt<int8_t>(value, fmq::config::kMinimumShift,
                            fmq::config::kMaximumShift);
  };
  config.preShift = clampShift(static_cast<int8_t>(bytes[kPreShiftOffset]));
  config.scaleShift = clampShift(static_cast<int8_t>(bytes[kScaleShiftOffset]));
  config.postShift = clampShift(static_cast<int8_t>(bytes[kPostShiftOffset]));
  // A blank/corrupt scale makes the module appear dead. Fall back to the
  // chromatic factory scale instead of accepting an unusable live state.
  bool any = false;
  for (uint8_t i = 0; i < kNoteCount; ++i) any = any || config.notes[i];
  if (!any) {
    for (uint8_t i = 0; i < kNoteCount; ++i) config.notes[i] = true;
  }
  return config;
}

void encodeState(const QuantizerState &state, uint8_t out[kStateBytes]) {
  uint8_t flags = 0;
  if (state.channelsLinked) {
    flags |= kChannelsLinkedFlag;
  }
  if (state.channelBMode == PitchMode::Absolute) {
    flags |= kChannelBAbsoluteFlag;
  }
  out[kStateFlagsOffset] = flags;
  encodeChannelConfig(state.channels[kChannelAIndex].config(), &out[kChannelAStateOffset]);
  encodeChannelConfig(state.channels[kChannelBIndex].config(), &out[kChannelBStateOffset]);
}

QuantizerState decodeState(const uint8_t bytes[kStateBytes]) {
  QuantizerState state;
  state.channelsLinked = (bytes[kStateFlagsOffset] & kChannelsLinkedFlag) != 0;
  state.channelBMode =
      (bytes[kStateFlagsOffset] & kChannelBAbsoluteFlag) ? PitchMode::Absolute : PitchMode::Relative;
  state.channels[kChannelAIndex].setConfig(decodeChannelConfig(&bytes[kChannelAStateOffset]));
  state.channels[kChannelBIndex].setConfig(
      decodeChannelConfig(&bytes[kChannelBStateOffset]));
  return state;
}

}  // namespace fmq
