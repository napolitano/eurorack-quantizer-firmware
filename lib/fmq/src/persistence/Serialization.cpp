/**
 * @file Serialization.cpp
 * Implements validation-aware binary serialization.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/persistence/Serialization.h"

#include "fmq/config/ProductConfig.h"

namespace fmq {
namespace {
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
constexpr uint8_t kChannelBStateOffset = 1 + kChannelConfigBytes;
constexpr uint8_t kChannelsLinkedFlag = 0x01;
constexpr uint8_t kChannelBAbsoluteFlag = 0x02;

constexpr uint8_t kArpFlagsOffset = 0;
constexpr uint8_t kArpRateOffset = 1;
constexpr uint8_t kArpPatternOffset = 2;
constexpr uint8_t kArpShapeOffset = 3;
constexpr uint8_t kArpLengthOffset = 4;
constexpr uint8_t kArpRangeOffset = 5;
constexpr uint8_t kArpSwingOffset = 6;
constexpr uint8_t kArpEnabledFlag = 0x01;
constexpr uint8_t kArpStepTriggerFlag = 0x02;
constexpr uint8_t kArpSyncShift = 2;
constexpr uint8_t kArpSyncMask = 0x0C;

constexpr uint8_t kStoredQuantizerOffset = 1;
constexpr uint8_t kStoredUiLayerFlag = 0x80u;
constexpr uint8_t kStoredSelectedChannelMask = 0x01u;
constexpr uint8_t kStoredArpAOffset = kStoredQuantizerOffset + kStateBytes;
constexpr uint8_t kStoredArpBOffset = kStoredArpAOffset + kArpeggiatorConfigBytes;

template <typename T>
T clampValue(T value, T minimum, T maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}
}  // namespace

void encodeNotes(const bool notes[kNoteCount], uint8_t out[kScaleBytes]) {
  out[0] = 0u;
  out[1] = 0u;
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    if (notes[i]) out[i / 8u] |= static_cast<uint8_t>(1u << (i % 8u));
  }
}

void decodeNotes(const uint8_t bytes[kScaleBytes], bool notes[kNoteCount]) {
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    notes[i] = ((bytes[i / 8u] >> (i % 8u)) & 1u) != 0u;
  }
}

void encodeChannelConfig(const ChannelConfig &config,
                         uint8_t out[kChannelConfigBytes]) {
  uint8_t notes[kScaleBytes];
  encodeNotes(config.notes, notes);
  out[kNotesLowOffset] = notes[0];
  out[kNotesHighOffset] = notes[1];
  out[kSampleModeOffset] = static_cast<uint8_t>(config.sampleMode);
  out[kGlideOffset] = config.glideAmount;
  out[kTriggerDelayOffset] = config.triggerDelayAmount;
  out[kPreShiftOffset] = static_cast<uint8_t>(config.preShift);
  out[kScaleShiftOffset] = static_cast<uint8_t>(config.scaleShift);
  out[kPostShiftOffset] = static_cast<uint8_t>(config.postShift);
}

ChannelConfig decodeChannelConfig(const uint8_t bytes[kChannelConfigBytes]) {
  ChannelConfig config = ChannelConfig::makeDefault();
  const uint8_t notes[kScaleBytes] = {bytes[kNotesLowOffset], bytes[kNotesHighOffset]};
  decodeNotes(notes, config.notes);
  switch (bytes[kSampleModeOffset]) {
    case 0: config.sampleMode = SampleMode::TrackAndHold; break;
    case 1: config.sampleMode = SampleMode::SampleAndHold; break;
    case 2: config.sampleMode = SampleMode::Continuous; break;
    default: config.sampleMode = config::kFactorySampleMode; break;
  }
  config.glideAmount = clampValue<uint8_t>(bytes[kGlideOffset], 0u,
                                           config::kMaxGlideAmount);
  config.triggerDelayAmount = clampValue<uint8_t>(bytes[kTriggerDelayOffset], 0u,
                                                  config::kMaxTriggerDelayAmount);
  config.preShift = clampValue<int8_t>(static_cast<int8_t>(bytes[kPreShiftOffset]),
                                       config::kMinimumShift, config::kMaximumShift);
  config.scaleShift = clampValue<int8_t>(static_cast<int8_t>(bytes[kScaleShiftOffset]),
                                         config::kMinimumShift, config::kMaximumShift);
  config.postShift = clampValue<int8_t>(static_cast<int8_t>(bytes[kPostShiftOffset]),
                                        config::kMinimumShift, config::kMaximumShift);
  bool any = false;
  for (uint8_t i = 0; i < kNoteCount; ++i) any = any || config.notes[i];
  if (!any) {
    for (uint8_t i = 0; i < kNoteCount; ++i) {
      config.notes[i] = (config::kFactoryScaleMask & (1u << i)) != 0u;
    }
  }
  return config;
}

void encodeState(const QuantizerState &state, uint8_t out[kStateBytes]) {
  uint8_t flags = 0u;
  if (state.channelsLinked) flags |= kChannelsLinkedFlag;
  if (state.channelBMode == PitchMode::Absolute) flags |= kChannelBAbsoluteFlag;
  out[kStateFlagsOffset] = flags;
  encodeChannelConfig(state.channels[kChannelAIndex].config(),
                      &out[kChannelAStateOffset]);
  encodeChannelConfig(state.channels[kChannelBIndex].config(),
                      &out[kChannelBStateOffset]);
}

QuantizerState decodeState(const uint8_t bytes[kStateBytes]) {
  QuantizerState state;
  state.channelsLinked = (bytes[kStateFlagsOffset] & kChannelsLinkedFlag) != 0u;
  state.channelBMode = (bytes[kStateFlagsOffset] & kChannelBAbsoluteFlag) != 0u
                           ? PitchMode::Absolute
                           : PitchMode::Relative;
  state.channels[kChannelAIndex].setConfig(
      decodeChannelConfig(&bytes[kChannelAStateOffset]));
  state.channels[kChannelBIndex].setConfig(
      decodeChannelConfig(&bytes[kChannelBStateOffset]));
  return state;
}

void encodeArpeggiatorConfig(const ArpeggiatorConfig &arp,
                             uint8_t out[kArpeggiatorConfigBytes]) {
  uint8_t flags = 0u;
  if (arp.enabled) flags |= kArpEnabledFlag;
  if (arp.stepTrigger) flags |= kArpStepTriggerFlag;
  flags |= static_cast<uint8_t>(
      (static_cast<uint8_t>(arp.syncMode) << kArpSyncShift) & kArpSyncMask);
  out[kArpFlagsOffset] = flags;
  out[kArpRateOffset] = arp.rateIndex;
  out[kArpPatternOffset] = static_cast<uint8_t>(arp.pattern);
  out[kArpShapeOffset] = static_cast<uint8_t>(arp.shape);
  out[kArpLengthOffset] = arp.length;
  out[kArpRangeOffset] = arp.range;
  out[kArpSwingOffset] = arp.swing;
}

ArpeggiatorConfig decodeArpeggiatorConfig(
    const uint8_t bytes[kArpeggiatorConfigBytes]) {
  ArpeggiatorConfig arp = ArpeggiatorConfig::makeDefault();
  arp.enabled = (bytes[kArpFlagsOffset] & kArpEnabledFlag) != 0u;
  arp.stepTrigger = (bytes[kArpFlagsOffset] & kArpStepTriggerFlag) != 0u;
  const uint8_t sync = static_cast<uint8_t>(
      (bytes[kArpFlagsOffset] & kArpSyncMask) >> kArpSyncShift);
  if (sync < Arpeggiator::syncModeCount()) {
    arp.syncMode = static_cast<ArpeggiatorSyncMode>(sync);
  }
  arp.rateIndex = clampValue<uint8_t>(bytes[kArpRateOffset], 0u,
                                      config::kArpRateCount - 1u);
  arp.pattern = bytes[kArpPatternOffset] < Arpeggiator::patternCount()
                    ? static_cast<ArpeggiatorPattern>(bytes[kArpPatternOffset])
                    : ArpeggiatorPattern::Up;
  arp.shape = bytes[kArpShapeOffset] < Arpeggiator::shapeCount()
                  ? static_cast<ArpeggiatorShape>(bytes[kArpShapeOffset])
                  : ArpeggiatorShape::Triad135;
  arp.length = clampValue<uint8_t>(bytes[kArpLengthOffset], 1u,
                                   config::kArpMaximumLength);
  arp.range = clampValue<uint8_t>(bytes[kArpRangeOffset], 1u,
                                  config::kArpMaximumRange);
  arp.swing = clampValue<uint8_t>(bytes[kArpSwingOffset], 0u,
                                  config::kArpMaximumSwingStep);
  return arp;
}

void encodeStoredConfiguration(const StoredConfiguration &state,
                               uint8_t out[kStoredConfigurationBytes]) {
  uint8_t selected = state.selectedChannelIndex < kChannelCount
                         ? state.selectedChannelIndex
                         : kChannelAIndex;
  if (state.uiLayer == UiLayer::Arpeggiator) selected |= kStoredUiLayerFlag;
  out[kStoredSelectedChannelOffset] = selected;
  encodeState(state.quantizer, &out[kStoredQuantizerOffset]);
  encodeArpeggiatorConfig(state.arpeggiators[kChannelAIndex],
                          &out[kStoredArpAOffset]);
  encodeArpeggiatorConfig(state.arpeggiators[kChannelBIndex],
                          &out[kStoredArpBOffset]);
}

StoredConfiguration decodeStoredConfiguration(
    const uint8_t bytes[kStoredConfigurationBytes]) {
  StoredConfiguration state;
  const uint8_t selectedByte = bytes[kStoredSelectedChannelOffset];
  const uint8_t selected =
      static_cast<uint8_t>(selectedByte & kStoredSelectedChannelMask);
  state.selectedChannelIndex = selected < kChannelCount ? selected
                                                        : kChannelAIndex;
  state.uiLayer = (selectedByte & kStoredUiLayerFlag) != 0u
                      ? UiLayer::Arpeggiator
                      : UiLayer::Quantizer;
  state.quantizer = decodeState(&bytes[kStoredQuantizerOffset]);
  state.arpeggiators[kChannelAIndex] =
      decodeArpeggiatorConfig(&bytes[kStoredArpAOffset]);
  state.arpeggiators[kChannelBIndex] =
      decodeArpeggiatorConfig(&bytes[kStoredArpBOffset]);

  // Full-config records written before the UI-layer bit existed used the same
  // payload size and left bit 7 clear. Under the current UI semantics a running
  // Arpeggiator cannot legitimately live behind the Quantizer layer, so enabled
  // ARP state is an unambiguous migration signal for those legacy records.
  if (state.uiLayer == UiLayer::Quantizer &&
      (state.arpeggiators[kChannelAIndex].enabled ||
       state.arpeggiators[kChannelBIndex].enabled)) {
    state.uiLayer = UiLayer::Arpeggiator;
  }

  if (state.quantizer.channelsLinked) {
    state.arpeggiators[kChannelBIndex] = state.arpeggiators[kChannelAIndex];
    state.selectedChannelIndex = kChannelAIndex;
  }
  return state;
}

}  // namespace fmq
