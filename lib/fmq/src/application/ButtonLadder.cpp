/**
 * @file ButtonLadder.cpp
 * Implements resistor-ladder decoding and stable key-event generation.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/application/ButtonLadder.h"

namespace fmq {
namespace {
uint16_t normalise(uint16_t value, uint16_t rest) {
  if (!config::kLadderUseRestNormalization || rest == 0u) return value;
  const uint32_t scaled =
      (static_cast<uint32_t>(value) * config::kLadderNominalRest + rest / 2u) /
      rest;
  return static_cast<uint16_t>(scaled > config::kAdcMaximumCode
                                   ? config::kAdcMaximumCode
                                   : scaled);
}
}  // namespace

uint8_t closestButtonIndex(uint16_t adcValue) {
  // Exact behavioural port of get_closest_button_index() from the Rust
  // firmware. Start with the unpressed/rest value as the best candidate and
  // only replace it when a key is strictly closer. The reverse scan preserves
  // the original tie-breaking at half-way points.
  uint8_t closest = config::kLadderNoButton;
  uint16_t bestDelta = static_cast<uint16_t>(
      adcValue > config::kLadderExpectedValues[config::kLadderNoButton]
          ? adcValue - config::kLadderExpectedValues[config::kLadderNoButton]
          : config::kLadderExpectedValues[config::kLadderNoButton] - adcValue);

  for (int8_t i = static_cast<int8_t>(config::kLadderButtonCount - 1); i >= 0;
       --i) {
    const uint16_t expected = config::kLadderExpectedValues[static_cast<uint8_t>(i)];
    const uint16_t delta = static_cast<uint16_t>(adcValue > expected
                                                     ? adcValue - expected
                                                     : expected - adcValue);
    if (delta < bestDelta) {
      closest = static_cast<uint8_t>(i);
      bestDelta = delta;
    }
  }
  return closest;
}

uint8_t buttonIndexForAdc(uint16_t adcValue, uint16_t restAdc) {
  const uint16_t normalized = normalise(adcValue, restAdc);

  // The unpressed node is intentionally accepted as a broad high range: once
  // the ladder approaches its measured rest voltage there is no valid key to
  // decode. This also makes open-circuit/high ADC faults fail safe as "none".
  if (normalized >= config::kLadderMinimumValidRest) {
    return config::kLadderNoButton;
  }

  const uint8_t candidate = closestButtonIndex(normalized);
  if (candidate >= config::kLadderButtonCount) {
    return config::kLadderNoButton;
  }

  const uint16_t expected = config::kLadderExpectedValues[candidate];
  const uint16_t delta = normalized > expected
                             ? static_cast<uint16_t>(normalized - expected)
                             : static_cast<uint16_t>(expected - normalized);
  return delta <= config::kLadderButtonAcceptanceDelta
             ? candidate
             : config::kLadderNoButton;
}

void ButtonLadder::calibrateRest(uint16_t adcValue) {
  // Keep the measured value for diagnostics/optional normalisation. On the
  // original board kLadderUseRestNormalization is false, so decoding remains
  // pinned to the proven absolute values from the Rust firmware.
  if (adcValue >= config::kLadderMinimumValidRest &&
      adcValue <= config::kLadderMaximumValidRest) {
    restAdc_ = adcValue;
  }
}

ButtonEvent ButtonLadder::sample(uint32_t now, uint16_t adc) {
  const uint8_t newValue = buttonIndexForAdc(adc, restAdc_);

  if (newValue != candidate_) {
    candidate_ = newValue;
    lastChangeTime_ = now;
    debouncing_ = true;
  } else if (debouncing_ && now - lastChangeTime_ > kDebounceTimeMs) {
    debouncing_ = false;
    debounced_ = candidate_;
    return debounced_ == kButtonLadderNoButton
               ? ButtonEvent::released()
               : ButtonEvent::pressed(debounced_);
  }

  return debounced_ == kButtonLadderNoButton
             ? ButtonEvent::none()
             : ButtonEvent::held(debounced_);
}
}  // namespace fmq
