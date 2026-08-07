/**
 * @file Quantizer.h
 * Declares quantizer state, channel processing, hysteresis and sample modes.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_QUANTIZER_H
#define FM_QUANTIZER_CORE_QUANTIZER_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"

/**
 * The pitch-quantization engine (two channels).
 *
 * This module rounds an incoming control voltage (already converted to
 * semitones) to the nearest note of a user-defined scale, then applies optional
 * transposition, scale-degree shifting, trigger-delayed sample-and-hold and
 * portamento (glide). It is entirely hardware-independent and deterministic,
 * which makes it fully unit-testable on the host.
 *
 * The design mirrors the original Rust firmware's behaviour but corrects two
 * problems: a release-mode assertion in the glide integrator that could hang the
 * device, and reliance on undefined behaviour to keep the sub-menu code from
 * miscompiling.
 */
namespace fmq {

constexpr uint8_t kChannelCount = 2;
constexpr uint8_t kChannelAIndex = 0;
constexpr uint8_t kChannelBIndex = 1;

/// How a channel decides when to sample a new input pitch.
enum class SampleMode : uint8_t {
  /// Follow the input only while the trigger/gate is high.
  TrackAndHold = 0,
  /// Sample the input only on a rising trigger edge.
  SampleAndHold = 1,
  /// Optional extension: follow the input continuously. Not part of the original
  /// Rust UI cycle and not the factory default.
  Continuous = 2,
};

/// How channel B interprets its input relative to channel A.
enum class PitchMode : uint8_t {
  /// Channel B's input is added to channel A's input (clamped to the range).
  Relative = 0,
  /// Channel B's input is used on its own; this is the power-on default.
  Absolute = 1,
};

/// Persistent, user-editable configuration of a single quantizer channel.
struct ChannelConfig {
  /// Scale membership per pitch class (index 0 = C … 11 = B).
  bool notes[kNoteCount];
  /// Track-and-hold vs. sample-and-hold behaviour.
  SampleMode sampleMode;
  /// Portamento amount; larger is slower (0 = instant). Valid range 0..11.
  uint8_t glideAmount;
  /// Number of samples the output waits after a trigger. Valid range 0..11.
  uint8_t triggerDelayAmount;
  /// Whole-semitone transposition applied *before* quantization.
  int8_t preShift;
  /// Transposition in *scale degrees* applied after quantization.
  int8_t scaleShift;
  /// Whole-semitone transposition applied to the CV output *after* the scale
  /// shift (does not affect which scale degree is shown on the LEDs).
  int8_t postShift;

  /// @return Factory configuration (chromatic scale, Track-and-Hold, no shifts).
  static ChannelConfig makeDefault();
};

/// One sample of a channel's output, consumed by the DAC and the UI.
struct ChannelOutput {
  /// The quantized note before post-shift, used for LED display and change
  /// detection. Range 0..120.
  int8_t nominalSemitones;
  /// The actual pitch sent to the DAC after glide and post-shift (Q8.8).
  SemitoneQ8_8 actualSemitones;
  /// True for a few samples after a note change: drives the CV trigger output.
  bool outputTrigger;
  /// True for a longer window after a note change: drives the output LED.
  bool outputTriggerUi;
  /// True shortly after an input trigger was received: drives the input LED.
  bool inputTriggerUi;
};

/// Combined output of both channels for one processing step.
struct QuantizationResult {
  ChannelOutput channelA;
  ChannelOutput channelB;

  /// @return A zeroed result (both channels at note 0, no triggers).
  static QuantizationResult makeZero();
};

/**
 * Hysteresis state that stabilises which note is selected.
 *
 * Without hysteresis, an input voltage sitting exactly between two scale notes
 * would flicker rapidly between them due to ADC noise. This tracks the last
 * emitted note and holds it as long as the input stays within a widened band
 * around it.
 */
class Hysteresis {
 public:
  Hysteresis() : lastOutput_(0) {}

  /**
   * @brief Quantize a pre-shifted input pitch to a selected scale note.
   * @param inputQ8_8 Non-negative input pitch in Q8.8.
   * @param notes     Scale membership per pitch class.
   * @return Selected absolute semitone in 0..120 (0 if the scale is empty).
   */
  int8_t quantize(SemitoneQ8_8 inputQ8_8, const bool notes[kNoteCount]);

  /// Reset the remembered note (used when a channel's state is reinitialised).
  void reset() { lastOutput_ = 0; }

 private:
  /// Compute the hold band [lower, upper] (Q8.8) around the last note, if the
  /// last note is still selected. Returns false when no band applies.
  bool computeThresholds(const bool notes[kNoteCount], SemitoneQ8_8 &lowerQ8_8,
                         SemitoneQ8_8 &upperQ8_8) const;

  int8_t lastOutput_;  ///< Last note emitted by quantize().
};

/**
 * A single quantizer channel: configuration plus live processing state.
 */
class QuantizerChannel {
 public:
  QuantizerChannel();

  /// Replace the channel's configuration and reset its live processing state.
  void setConfig(const ChannelConfig &config);

  /// @return Mutable access to the channel configuration.
  ChannelConfig &config() { return config_; }
  /// @return Read-only access to the channel configuration.
  const ChannelConfig &config() const { return config_; }

  /**
   * @brief Advance the channel by one sample.
   * @param inputSemitones Input pitch for this channel in Q8.8.
   * @param sampleTrigger  Current level of this channel's trigger input.
   * @return The channel output for this sample.
   */
  ChannelOutput step(SemitoneQ8_8 inputSemitones, bool sampleTrigger);

  /// Suppress the output-trigger pulse for the next pitch change.
  /// Used after front-panel configuration edits: changing the scale or another
  /// setting may legitimately move the DAC target, but that UI action is not an
  /// external musical trigger and should not flash/pulse the channel outputs.
  void suppressNextOutputTrigger() { suppressNextOutputTrigger_ = true; }

 private:
  /// Apply pre-shift, quantization and scale/post shifts.
  /// @param inputSemitones Input pitch in Q8.8.
  /// @param nominalOut     Receives the scale-shifted note (pre post-shift).
  /// @param glideTargetOut Receives the post-shifted CV target in Q8.24.
  void calculateQuantizationWithTransposition(SemitoneQ8_8 inputSemitones,
                                              int8_t &nominalOut,
                                              GlideQ8_24 &glideTargetOut);

  /// One step of the portamento integrator toward @p target (Q8.24).
  GlideQ8_24 calculateGlide(GlideQ8_24 current, GlideQ8_24 target) const;

  ChannelConfig config_;
  Hysteresis hysteresis_;

  // Live (ephemeral) processing state.
  bool hasLastOutput_;              ///< Whether a previous output exists.
  int8_t lastNominalSemitones_;     ///< Previous nominal note.
  GlideQ8_24 lastGlideTarget_;      ///< Previous glide target (Q8.24).
  GlideQ8_24 lastGlideCurrent_;     ///< Previous glide integrator value (Q8.24).
  bool lastTriggerInput_;           ///< Previous trigger level (for edge detect).
  uint8_t outputTriggerCountdown_;  ///< Remaining samples of output-trigger.
  uint8_t inputTriggerUiTimer_;     ///< Samples since trigger activity for the input LED.
  uint8_t triggerDelayTimer_;       ///< Samples elapsed while a delayed sample is pending.
  bool triggerDelayPending_;        ///< A delayed sample is waiting to fire.
  bool suppressNextOutputTrigger_;   ///< One-shot suppression after UI edits.
};

/**
 * The full two-channel quantizer.
 */
class QuantizerState {
 public:
  QuantizerState();

  /// Whether the two channels share a single scale/configuration.
  bool channelsLinked;
  /// How channel B interprets its input.
  PitchMode channelBMode;
  /// The two channels (index 0 = A, 1 = B).
  QuantizerChannel channels[kChannelCount];

  /**
   * @brief Advance both channels by one sample.
   * @param inputSemitonesA Channel A input pitch (Q8.8).
   * @param inputSemitonesB Channel B input pitch (Q8.8).
   * @param triggerA        Channel A trigger input level.
   * @param triggerB        Channel B trigger input level.
   * @return Combined result for this sample.
   */
  QuantizationResult step(SemitoneQ8_8 inputSemitonesA,
                          SemitoneQ8_8 inputSemitonesB, bool triggerA,
                          bool triggerB);

  /// Prevent the next configuration-induced pitch change from generating
  /// channel trigger pulses/LED flashes.
  void suppressNextOutputTriggers() {
    channels[kChannelAIndex].suppressNextOutputTrigger();
    channels[kChannelBIndex].suppressNextOutputTrigger();
  }
};

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_QUANTIZER_H
