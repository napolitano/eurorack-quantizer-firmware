/**
 * @file Arpeggiator.h
 * Scale-aware two-channel Arpeggiator core.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_APPLICATION_ARPEGGIATOR_H
#define FMQ_APPLICATION_ARPEGGIATOR_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"

namespace fmq {

enum class ArpeggiatorPattern : uint8_t {
  Up = 0,
  Down,
  UpDown,
  DownUp,
  Rotate,
  OutsideIn,
  InsideOut,
  Random,
  Count
};

enum class ArpeggiatorShape : uint8_t {
  Triad135 = 0,
  OneTwoFive,
  OneFourFive,
  OneThreeSix,
  OneFiveEight,
  Adjacent1234,
  Seventh1357,
  StackedThirds,
  Count
};

enum class ArpeggiatorSyncMode : uint8_t {
  Free = 0,
  Reset,
  Clock,
  Count
};

/** Persistent musical configuration of one Arpeggiator channel. */
struct ArpeggiatorConfig {
  bool enabled;
  uint8_t rateIndex;
  ArpeggiatorPattern pattern;
  ArpeggiatorShape shape;
  uint8_t length;
  uint8_t range;
  bool stepTrigger;
  ArpeggiatorSyncMode syncMode;
  uint8_t swing;

  static ArpeggiatorConfig makeDefault();
};

/** Result of one Arpeggiator processing tick. */
struct ArpeggiatorOutput {
  SemitoneQ8_8 pitch;
  bool stepAdvanced;
};

/**
 * @brief Small deterministic Arpeggiator suitable for the ATmega328P.
 *
 * The engine never buffers audio. It derives scale degrees from the already
 * quantized pitch and advances a compact step state from either its own clock,
 * a gate-reset event or an external clock edge.
 */
class Arpeggiator {
 public:
  Arpeggiator();

  const ArpeggiatorConfig &config() const { return config_; }
  void setConfig(const ArpeggiatorConfig &config, uint32_t nowMs);

  bool enabled() const { return config_.enabled; }
  void setEnabled(bool enabled, uint32_t nowMs);
  void toggle(uint32_t nowMs) { setEnabled(!config_.enabled, nowMs); }

  /** Reset the musical phase to the first pattern position. */
  void resetPhase(uint32_t nowMs);

  /**
   * @brief Process one 1-ms control-loop tick.
   * @param basePitch Already-quantized/glided pitch for the channel.
   * @param nominalSemitones Discrete quantized note used as the scale root.
   * @param notes Active pitch classes of the channel scale.
   * @param syncEdge Rising edge of the channel Sample/Gate input.
   * @param nowMs Monotonic millisecond clock; wrap-around is supported.
   */
  ArpeggiatorOutput process(SemitoneQ8_8 basePitch, int8_t nominalSemitones,
                            const bool notes[kNoteCount], bool syncEdge,
                            uint32_t nowMs);

  /**
   * @brief Process with an ISR-captured external-clock timestamp.
   * @param syncEdgeCount Number of rising edges since the previous control tick.
   * @param latestSyncEdgeUs Timestamp of the newest edge in microseconds.
   * @param nowUs Current microsecond time for multiplied substep scheduling.
   *
   * Free-running and Reset modes retain their millisecond timing. Clock mode
   * uses the microsecond data to measure external periods without 1 ms input
   * quantisation. Multiple edges are counted rather than collapsed.
   */
  ArpeggiatorOutput processTimed(
      SemitoneQ8_8 basePitch, int8_t nominalSemitones,
      const bool notes[kNoteCount], uint8_t syncEdgeCount,
      uint32_t latestSyncEdgeUs, uint32_t nowMs, uint32_t nowUs);

  static uint16_t freeRateMs(uint8_t rateIndex);
  static uint8_t clockMultiplier(uint8_t rateIndex);
  static uint8_t clockDivider(uint8_t rateIndex);
  static uint8_t patternCount() {
    return static_cast<uint8_t>(ArpeggiatorPattern::Count);
  }
  static uint8_t shapeCount() {
    return static_cast<uint8_t>(ArpeggiatorShape::Count);
  }
  static uint8_t syncModeCount() {
    return static_cast<uint8_t>(ArpeggiatorSyncMode::Count);
  }

 private:
  static ArpeggiatorConfig sanitize(const ArpeggiatorConfig &config);
  static uint8_t countEnabledNotes(const bool notes[kNoteCount]);
  static uint8_t semitoneOffsetForScaleDegree(const bool notes[kNoteCount],
                                               uint8_t rootPitchClass,
                                               uint8_t scaleDegreeOffset);
  static uint8_t patternPosition(ArpeggiatorPattern pattern,
                                 uint8_t stepCounter, uint8_t length,
                                 uint16_t randomState);
  static uint16_t nextRandomState(uint16_t state);
  static uint8_t shapeDegree(ArpeggiatorShape shape, uint8_t position,
                             uint8_t activeScaleNotes, uint8_t range);

  uint16_t stepDurationMs() const;
  bool updateFreeOrReset(bool syncEdge, uint32_t nowMs);
  bool updateClock(uint8_t syncEdgeCount, uint32_t latestSyncEdgeUs,
                   uint32_t nowUs);
  void advanceStep();

  ArpeggiatorConfig config_;
  uint8_t stepCounter_;
  uint32_t lastStepMs_;
  uint32_t lastClockEdgeUs_;
  uint32_t clockPeriodUs_;
  uint32_t lastClockStepUs_;
  uint8_t dividerEdgeCount_;
  uint8_t generatedSubsteps_;
  bool clockSeen_;
  uint16_t randomState_;
};

}  // namespace fmq

#endif  // FMQ_APPLICATION_ARPEGGIATOR_H
