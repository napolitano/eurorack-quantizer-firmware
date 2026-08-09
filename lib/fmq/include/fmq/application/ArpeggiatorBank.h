/**
 * @file ArpeggiatorBank.h
 * Owns independent Arpeggiator configuration/runtime for both channels.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_APPLICATION_ARPEGGIATOR_BANK_H
#define FMQ_APPLICATION_ARPEGGIATOR_BANK_H

#include <stdint.h>

#include "fmq/application/Arpeggiator.h"
#include "fmq/domain/Quantizer.h"

namespace fmq {

class ArpeggiatorBank {
 public:
  bool enabled(uint8_t channelIndex) const;
  const ArpeggiatorConfig &config(uint8_t channelIndex) const;
  void setConfig(uint8_t channelIndex, const ArpeggiatorConfig &config,
                 uint32_t nowMs);
  void setEnabled(uint8_t channelIndex, bool enabled, uint32_t nowMs);

  /** Copy A's complete Arpeggiator configuration to B and reset both phases. */
  void linkFromA(uint32_t nowMs);

  /** Toggle selected channel, or both channels to one deterministic state. */
  bool toggleSelected(uint8_t selectedChannelIndex, bool channelsLinked,
                      uint32_t nowMs);

  /** Apply a configuration edit to selected channel, or both when linked. */
  void applySelectedConfig(uint8_t selectedChannelIndex, bool channelsLinked,
                           const ArpeggiatorConfig &config, uint32_t nowMs);

  ArpeggiatorOutput process(uint8_t channelIndex, SemitoneQ8_8 basePitch,
                            int8_t nominalSemitones,
                            const bool notes[kNoteCount], bool syncEdge,
                            uint32_t nowMs);
  ArpeggiatorOutput processTimed(
      uint8_t channelIndex, SemitoneQ8_8 basePitch, int8_t nominalSemitones,
      const bool notes[kNoteCount], uint8_t syncEdgeCount,
      uint32_t latestSyncEdgeUs, uint32_t nowMs, uint32_t nowUs);

 private:
  static uint8_t validIndex(uint8_t channelIndex);
  Arpeggiator channels_[kChannelCount];
};

}  // namespace fmq

#endif  // FMQ_APPLICATION_ARPEGGIATOR_BANK_H
