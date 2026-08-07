/**
 * @file AvrAnalogInputs.h
 * Implements interrupt-driven AVR ADC scanning for ladder and CV inputs.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_AVR_ANALOG_INPUTS_H
#define FMQ_PLATFORM_NANO_AVR_ANALOG_INPUTS_H

#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "fmq/config/AnalogConfig.h"
#include "fmq/ports/AnalogInputs.h"

namespace fmq {

/**
 * Interrupt-driven ADC scanner for ladder, CV A and CV B.
 *
 * Conversions are started explicitly instead of using AVR free-running mode.
 * This matters because free-running mode starts the next conversion before the
 * ADC ISR executes, making a MUX change in the ISR ambiguous. Here each MUX
 * change is followed by a deliberately discarded conversion and then a kept
 * conversion on the same channel.
 *
 * With F_CPU=16 MHz and prescaler 128 the ADC clock is 125 kHz, matching the
 * effective fm-lib settings. Three-sample filtering uses the median, as the
 * Rust helper actually does. Six conversions (discard + keep for three
 * channels) produce roughly 1.6 k snapshots/s.
 */
class AvrAnalogInputs : public IAnalogInputs {
 public:
  AvrAnalogInputs(uint8_t ladderPin, uint8_t cvAPin, uint8_t cvBPin)
      : channel_(0), discard_(config::kDiscardFirstAdcReadAfterMuxChange),
        sequence_(0), consumed_(0), historyIndex_(0), primed_(false) {
    pins_[0] = ladderPin;
    pins_[1] = cvAPin;
    pins_[2] = cvBPin;
    for (uint8_t c = 0; c < config::kAdcChannelCount; ++c) {
      published_[c] = 0;
      snapshot_[c] = 0;
      for (uint8_t h = 0; h < config::kAdcMovingAverageSamples; ++h) {
        history_[c][h] = 0;
      }
    }
  }

  void begin(bool externalReference) {
    instance_ = this;
    const uint8_t refs = externalReference ? 0u : _BV(REFS0);
    ADMUX = static_cast<uint8_t>(refs | adcChannelForPin(pins_[0]));
    ADCSRB = 0;
    // ADC enabled, completion interrupt enabled, prescaler /128. No ADATE:
    // every conversion is explicitly started after the MUX is settled.
    ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
    startConversion();
  }

  void end() {
    ADCSRA = 0;
    instance_ = nullptr;
  }

  bool sampleReady() override {
    uint16_t sequence;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { sequence = sequence_; }
    return sequence != consumed_;
  }

  void beginCycle() override {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      for (uint8_t c = 0; c < config::kAdcChannelCount; ++c) {
        snapshot_[c] = published_[c];
      }
      consumed_ = sequence_;
    }
  }

  uint16_t read(uint8_t channel) const override {
    return channel < config::kAdcChannelCount ? snapshot_[channel] : 0u;
  }

  uint16_t sequence() const {
    uint16_t sequence;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { sequence = sequence_; }
    return sequence;
  }

  static void handleInterrupt() {
    if (instance_) instance_->onConversion(ADC);
  }

 private:
  static uint8_t adcChannelForPin(uint8_t pin) {
    return static_cast<uint8_t>(pin - A0);
  }

  static void startConversion() { ADCSRA |= _BV(ADSC); }

  void onConversion(uint16_t value) {
    if (discard_) {
      discard_ = false;
      startConversion();  // same channel; this second result will be kept
      return;
    }

    if (!primed_) {
      for (uint8_t h = 0; h < config::kAdcMovingAverageSamples; ++h) {
        history_[channel_][h] = value;
      }
    } else {
      history_[channel_][historyIndex_] = value;
    }

    if (config::kAdcUseMedianFilter) {
      uint16_t sorted[config::kAdcMovingAverageSamples];
      for (uint8_t h = 0; h < config::kAdcMovingAverageSamples; ++h) {
        sorted[h] = history_[channel_][h];
      }
      for (uint8_t i = 1; i < config::kAdcMovingAverageSamples; ++i) {
        uint8_t j = i;
        while (j > 0u && sorted[j - 1u] > sorted[j]) {
          const uint16_t tmp = sorted[j - 1u];
          sorted[j - 1u] = sorted[j];
          sorted[j] = tmp;
          --j;
        }
      }
      published_[channel_] = sorted[config::kAdcMovingAverageSamples / 2u];
    } else {
      uint32_t sum = 0;
      for (uint8_t h = 0; h < config::kAdcMovingAverageSamples; ++h) {
        sum += history_[channel_][h];
      }
      published_[channel_] =
          static_cast<uint16_t>(sum / config::kAdcMovingAverageSamples);
    }

    ++channel_;
    if (channel_ >= config::kAdcChannelCount) {
      channel_ = 0;
      primed_ = true;
      historyIndex_ = static_cast<uint8_t>(
          (historyIndex_ + 1u) % config::kAdcMovingAverageSamples);
      ++sequence_;
    }

    ADMUX = static_cast<uint8_t>((ADMUX & 0xF0u) | adcChannelForPin(pins_[channel_]));
    discard_ = config::kDiscardFirstAdcReadAfterMuxChange;
    startConversion();
  }

  uint8_t pins_[config::kAdcChannelCount];
  volatile uint16_t published_[config::kAdcChannelCount];
  uint16_t snapshot_[config::kAdcChannelCount];
  uint16_t history_[config::kAdcChannelCount][config::kAdcMovingAverageSamples];
  volatile uint8_t channel_;
  volatile bool discard_;
  volatile uint16_t sequence_;
  uint16_t consumed_;
  uint8_t historyIndex_;
  bool primed_;
  static AvrAnalogInputs *instance_;
};

inline AvrAnalogInputs *AvrAnalogInputs::instance_ = nullptr;
}  // namespace fmq
#endif
