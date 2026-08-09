/**
 * @file ArpeggiatorBank.cpp
 * Implements independent two-channel Arpeggiator state.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/ArpeggiatorBank.h"

namespace fmq {

uint8_t ArpeggiatorBank::validIndex(uint8_t channelIndex) {
  return channelIndex < kChannelCount ? channelIndex : kChannelAIndex;
}

bool ArpeggiatorBank::enabled(uint8_t channelIndex) const {
  return channels_[validIndex(channelIndex)].enabled();
}

const ArpeggiatorConfig &ArpeggiatorBank::config(uint8_t channelIndex) const {
  return channels_[validIndex(channelIndex)].config();
}

void ArpeggiatorBank::setConfig(uint8_t channelIndex,
                               const ArpeggiatorConfig &config,
                               uint32_t nowMs) {
  channels_[validIndex(channelIndex)].setConfig(config, nowMs);
}

void ArpeggiatorBank::setEnabled(uint8_t channelIndex, bool enabled,
                                uint32_t nowMs) {
  channels_[validIndex(channelIndex)].setEnabled(enabled, nowMs);
}

void ArpeggiatorBank::linkFromA(uint32_t nowMs) {
  const ArpeggiatorConfig source = channels_[kChannelAIndex].config();
  channels_[kChannelAIndex].setConfig(source, nowMs);
  channels_[kChannelBIndex].setConfig(source, nowMs);
  channels_[kChannelAIndex].resetPhase(nowMs);
  channels_[kChannelBIndex].resetPhase(nowMs);
}

bool ArpeggiatorBank::toggleSelected(uint8_t selectedChannelIndex,
                                    bool channelsLinked, uint32_t nowMs) {
  if (channelsLinked) {
    const bool enableBoth =
        !(channels_[kChannelAIndex].enabled() &&
          channels_[kChannelBIndex].enabled());
    channels_[kChannelAIndex].setEnabled(enableBoth, nowMs);
    channels_[kChannelBIndex].setEnabled(enableBoth, nowMs);
    return enableBoth;
  }

  const uint8_t index = validIndex(selectedChannelIndex);
  channels_[index].toggle(nowMs);
  return channels_[index].enabled();
}

void ArpeggiatorBank::applySelectedConfig(
    uint8_t selectedChannelIndex, bool channelsLinked,
    const ArpeggiatorConfig &config, uint32_t nowMs) {
  if (channelsLinked) {
    channels_[kChannelAIndex].setConfig(config, nowMs);
    channels_[kChannelBIndex].setConfig(config, nowMs);
    return;
  }
  channels_[validIndex(selectedChannelIndex)].setConfig(config, nowMs);
}

ArpeggiatorOutput ArpeggiatorBank::process(
    uint8_t channelIndex, SemitoneQ8_8 basePitch, int8_t nominalSemitones,
    const bool notes[kNoteCount], bool syncEdge, uint32_t nowMs) {
  return channels_[validIndex(channelIndex)].process(
      basePitch, nominalSemitones, notes, syncEdge, nowMs);
}

ArpeggiatorOutput ArpeggiatorBank::processTimed(
    uint8_t channelIndex, SemitoneQ8_8 basePitch, int8_t nominalSemitones,
    const bool notes[kNoteCount], uint8_t syncEdgeCount,
    uint32_t latestSyncEdgeUs, uint32_t nowMs, uint32_t nowUs) {
  return channels_[validIndex(channelIndex)].processTimed(
      basePitch, nominalSemitones, notes, syncEdgeCount, latestSyncEdgeUs,
      nowMs, nowUs);
}

}  // namespace fmq
