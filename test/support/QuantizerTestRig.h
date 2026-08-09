/**
 * @file QuantizerTestRig.h
 * Deterministic host-side signal harness for end-to-end Quantizer/ARP tests.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_TEST_SUPPORT_QUANTIZER_TEST_RIG_H
#define FM_QUANTIZER_TEST_SUPPORT_QUANTIZER_TEST_RIG_H

#include <stdint.h>
#include <vector>

#include "fmq/application/ArpeggiatorBank.h"
#include "fmq/config/AnalogConfig.h"
#include "fmq/config/ProductConfig.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/Quantizer.h"

namespace fmqtest {

struct SignalSample {
  uint32_t timeMs;
  fmq::QuantizationResult quantization;
  fmq::SemitoneQ8_8 outputPitchA;
  fmq::SemitoneQ8_8 outputPitchB;
  uint16_t dacCodeA;
  uint16_t dacCodeB;
  bool triggerA;
  bool triggerB;
  bool inputLedA;
  bool inputLedB;
  bool outputLedA;
  bool outputLedB;
};

class QuantizerTestRig {
 public:
  QuantizerTestRig()
      : cvRawA_(0),
        cvRawB_(0),
        gateA_(true),
        gateB_(true),
        previousGateA_(true),
        previousGateB_(true),
        arpTriggerTicksA_(0),
        arpTriggerTicksB_(0),
        arpOutputLedTicksA_(0),
        arpOutputLedTicksB_(0),
        nowMs_(0) {}

  fmq::QuantizerState &state() { return state_; }
  const fmq::QuantizerState &state() const { return state_; }
  fmq::ArpeggiatorBank &arpeggiators() { return arpeggiators_; }
  const fmq::ArpeggiatorBank &arpeggiators() const { return arpeggiators_; }

  void setArpeggiatorsEnabled(bool enabled) {
    arpeggiators_.setEnabled(fmq::kChannelAIndex, enabled, nowMs_);
    arpeggiators_.setEnabled(fmq::kChannelBIndex, enabled, nowMs_);
  }

  void setCvRawA(uint16_t value) { cvRawA_ = clampAdc(value); }
  void setCvRawB(uint16_t value) { cvRawB_ = clampAdc(value); }
  void setGateA(bool high) { gateA_ = high; }
  void setGateB(bool high) { gateB_ = high; }
  void setCvVoltsA(double volts) { cvRawA_ = voltsToAdc(volts); }
  void setCvVoltsB(double volts) { cvRawB_ = voltsToAdc(volts); }

  uint32_t nowMs() const { return nowMs_; }
  const std::vector<SignalSample> &history() const { return history_; }
  const SignalSample &last() const { return history_.back(); }

  const SignalSample &tick() {
    const bool edgeA = !previousGateA_ && gateA_;
    const bool edgeB = !previousGateB_ && gateB_;
    previousGateA_ = gateA_;
    previousGateB_ = gateB_;

    const fmq::SemitoneQ8_8 inputA = fmq::adcToSemitones(cvRawA_, 0);
    const fmq::SemitoneQ8_8 inputB = fmq::adcToSemitones(cvRawB_, 1);
    const fmq::ArpeggiatorConfig &cfgA =
        arpeggiators_.config(fmq::kChannelAIndex);
    const fmq::ArpeggiatorConfig &cfgB =
        arpeggiators_.config(fmq::kChannelBIndex);
    const bool clockA = cfgA.enabled &&
        cfgA.syncMode == fmq::ArpeggiatorSyncMode::Clock;
    const bool clockB = cfgB.enabled &&
        cfgB.syncMode == fmq::ArpeggiatorSyncMode::Clock;

    const fmq::QuantizationResult result =
        state_.step(inputA, inputB, gateA_, gateB_, clockA, clockB);

    const fmq::ArpeggiatorOutput arpA = arpeggiators_.process(
        fmq::kChannelAIndex, result.channelA.actualSemitones,
        result.channelA.nominalSemitones,
        state_.channels[fmq::kChannelAIndex].config().notes, edgeA, nowMs_);
    const fmq::ArpeggiatorOutput arpB = arpeggiators_.process(
        fmq::kChannelBIndex, result.channelB.actualSemitones,
        result.channelB.nominalSemitones,
        state_.channels[fmq::kChannelBIndex].config().notes, edgeB, nowMs_);

    if (arpA.stepAdvanced && arpeggiators_.config(fmq::kChannelAIndex).stepTrigger) {
      arpTriggerTicksA_ = fmq::config::kOutputTriggerCvSamples;
      arpOutputLedTicksA_ = fmq::config::kOutputTriggerLedSamples;
    }
    if (arpB.stepAdvanced && arpeggiators_.config(fmq::kChannelBIndex).stepTrigger) {
      arpTriggerTicksB_ = fmq::config::kOutputTriggerCvSamples;
      arpOutputLedTicksB_ = fmq::config::kOutputTriggerLedSamples;
    }

    SignalSample sample = {
        nowMs_,
        result,
        arpA.pitch,
        arpB.pitch,
        fmq::semitonesToDac(arpA.pitch, fmq::kChannelAIndex),
        fmq::semitonesToDac(arpB.pitch, fmq::kChannelBIndex),
        result.channelA.outputTrigger || arpTriggerTicksA_ != 0u,
        result.channelB.outputTrigger || arpTriggerTicksB_ != 0u,
        result.channelA.inputTriggerUi,
        result.channelB.inputTriggerUi,
        result.channelA.outputTriggerUi || arpOutputLedTicksA_ != 0u,
        result.channelB.outputTriggerUi || arpOutputLedTicksB_ != 0u,
    };
    history_.push_back(sample);
    decrement(arpTriggerTicksA_);
    decrement(arpTriggerTicksB_);
    decrement(arpOutputLedTicksA_);
    decrement(arpOutputLedTicksB_);
    ++nowMs_;
    return history_.back();
  }

  void runFor(uint32_t milliseconds) {
    for (uint32_t i = 0; i < milliseconds; ++i) tick();
  }
  void clearHistory() { history_.clear(); }

 private:
  static void decrement(uint8_t &value) { if (value != 0u) --value; }
  static uint16_t clampAdc(uint16_t value) {
    return value > fmq::config::kAdcMaximumCode
               ? fmq::config::kAdcMaximumCode
               : value;
  }
  static uint16_t voltsToAdc(double volts) {
    if (volts <= 0.0) return 0;
    if (volts >= 10.0) return fmq::config::kAdcMaximumCode;
    const double scaled =
        volts * static_cast<double>(fmq::config::kAdcMaximumCode) / 10.0;
    return static_cast<uint16_t>(scaled + 0.5);
  }

  fmq::QuantizerState state_;
  fmq::ArpeggiatorBank arpeggiators_;
  uint16_t cvRawA_;
  uint16_t cvRawB_;
  bool gateA_;
  bool gateB_;
  bool previousGateA_;
  bool previousGateB_;
  uint8_t arpTriggerTicksA_;
  uint8_t arpTriggerTicksB_;
  uint8_t arpOutputLedTicksA_;
  uint8_t arpOutputLedTicksB_;
  uint32_t nowMs_;
  std::vector<SignalSample> history_;
};

}  // namespace fmqtest

#endif
