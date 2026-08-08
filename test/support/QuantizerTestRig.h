/**
 * @file QuantizerTestRig.h
 * Deterministic host-side signal harness for end-to-end quantizer tests.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_TEST_SUPPORT_QUANTIZER_TEST_RIG_H
#define FM_QUANTIZER_TEST_SUPPORT_QUANTIZER_TEST_RIG_H

#include <stdint.h>
#include <vector>

#include "fmq/application/RetroArpeggiator.h"
#include "fmq/config/AnalogConfig.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/Quantizer.h"

namespace fmqtest {

/** One externally observable 1-ms processing sample. */
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

/**
 * Virtual module boundary used by native system tests.
 *
 * Inputs are expressed as the real firmware sees them: 10-bit ADC codes and
 * digital gate levels. Outputs capture the production quantizer,
 * arpeggiator and calibrated DAC-conversion path. Time advances in the same
 * 1-ms increments as the Nano control loop.
 */
class QuantizerTestRig {
 public:
  QuantizerTestRig()
      : cvRawA_(0), cvRawB_(0), gateA_(true), gateB_(true), nowMs_(0) {}

  fmq::QuantizerState &state() { return state_; }
  const fmq::QuantizerState &state() const { return state_; }

  fmq::RetroArpeggiator &arpeggiator() { return arpeggiator_; }
  const fmq::RetroArpeggiator &arpeggiator() const { return arpeggiator_; }

  void setCvRawA(uint16_t value) { cvRawA_ = clampAdc(value); }
  void setCvRawB(uint16_t value) { cvRawB_ = clampAdc(value); }
  void setGateA(bool high) { gateA_ = high; }
  void setGateB(bool high) { gateB_ = high; }

  /** Set ideal 0..10 V input, converted to the corresponding 10-bit ADC code. */
  void setCvVoltsA(double volts) { cvRawA_ = voltsToAdc(volts); }
  void setCvVoltsB(double volts) { cvRawB_ = voltsToAdc(volts); }

  uint32_t nowMs() const { return nowMs_; }
  const std::vector<SignalSample> &history() const { return history_; }
  const SignalSample &last() const { return history_.back(); }

  /** Execute one complete production signal-path tick and advance time by 1 ms. */
  const SignalSample &tick() {
    const fmq::SemitoneQ8_8 inputA = fmq::adcToSemitones(cvRawA_, 0);
    const fmq::SemitoneQ8_8 inputB = fmq::adcToSemitones(cvRawB_, 1);
    const fmq::QuantizationResult result =
        state_.step(inputA, inputB, gateA_, gateB_);

    const fmq::SemitoneQ8_8 outputA = arpeggiator_.process(
        result.channelA.actualSemitones, result.channelA.nominalSemitones,
        state_.channels[fmq::kChannelAIndex].config().notes, nowMs_);
    const fmq::SemitoneQ8_8 outputB = arpeggiator_.process(
        result.channelB.actualSemitones, result.channelB.nominalSemitones,
        state_.channels[fmq::kChannelBIndex].config().notes, nowMs_);

    SignalSample sample = {
        nowMs_,
        result,
        outputA,
        outputB,
        fmq::semitonesToDac(outputA, fmq::kChannelAIndex),
        fmq::semitonesToDac(outputB, fmq::kChannelBIndex),
        result.channelA.outputTrigger,
        result.channelB.outputTrigger,
        result.channelA.inputTriggerUi,
        result.channelB.inputTriggerUi,
        result.channelA.outputTriggerUi,
        result.channelB.outputTriggerUi,
    };
    history_.push_back(sample);
    ++nowMs_;
    return history_.back();
  }

  void runFor(uint32_t milliseconds) {
    for (uint32_t i = 0; i < milliseconds; ++i) {
      tick();
    }
  }

  void clearHistory() { history_.clear(); }

 private:
  static uint16_t clampAdc(uint16_t value) {
    return value > fmq::config::kAdcMaximumCode
               ? fmq::config::kAdcMaximumCode
               : value;
  }

  static uint16_t voltsToAdc(double volts) {
    if (volts <= 0.0) {
      return 0;
    }
    if (volts >= 10.0) {
      return fmq::config::kAdcMaximumCode;
    }
    const double scaled =
        volts * static_cast<double>(fmq::config::kAdcMaximumCode) / 10.0;
    return static_cast<uint16_t>(scaled + 0.5);
  }

  fmq::QuantizerState state_;
  fmq::RetroArpeggiator arpeggiator_;
  uint16_t cvRawA_;
  uint16_t cvRawB_;
  bool gateA_;
  bool gateB_;
  uint32_t nowMs_;
  std::vector<SignalSample> history_;
};

}  // namespace fmqtest

#endif  // FM_QUANTIZER_TEST_SUPPORT_QUANTIZER_TEST_RIG_H
