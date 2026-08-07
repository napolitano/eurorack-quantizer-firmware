/**
 * @file RetroArpeggiator.h
 * Scale-aware Retro Arpeggiator performance feature.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_APPLICATION_RETRO_ARPEGGIATOR_H
#define FMQ_APPLICATION_RETRO_ARPEGGIATOR_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"

namespace fmq {

class RetroArpeggiator {
 public:
  RetroArpeggiator() : enabled_(false), enabledAtMs_(0) {}

  bool enabled() const { return enabled_; }
  void setEnabled(bool enabled, uint32_t nowMs) {
    enabled_ = enabled;
    enabledAtMs_ = nowMs;
  }
  void toggle(uint32_t nowMs) { setEnabled(!enabled_, nowMs); }

  SemitoneQ8_8 process(SemitoneQ8_8 basePitch, int8_t nominalSemitones,
                       const bool notes[kNoteCount], uint32_t nowMs) const;

 private:
  static uint8_t semitoneOffsetForScaleDegree(const bool notes[kNoteCount],
                                               uint8_t rootPitchClass,
                                               uint8_t scaleDegreeOffset);

  bool enabled_;
  uint32_t enabledAtMs_;
};

}  // namespace fmq

#endif  // FMQ_APPLICATION_RETRO_ARPEGGIATOR_H
