/**
 * @file FactoryPresets.h
 * Declares the twelve built-in fallback presets for full-configuration slots.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_FACTORY_PRESETS_H
#define FMQ_CONFIG_FACTORY_PRESETS_H

#include <stdint.h>

#include "fmq/domain/Quantizer.h"

namespace fmq::config {

constexpr uint8_t kFactoryPresetCount = 12;

/// Pitch-class masks use bit 0 = C, bit 1 = C#/Db, ... bit 11 = B.
struct FactoryScalePreset {
  uint16_t noteMask;
};

/// Factory fallback for the twelve full-config slots opened with SHIFT+LOAD.
/// A valid user-saved config in the same slot always takes precedence.
constexpr FactoryScalePreset kFactoryScalePresets[kFactoryPresetCount] = {
    {0x0FFF},  // 1: Chromatic
    {0x0AB5},  // 2: Major / Ionian       C D E F G A B
    {0x05AD},  // 3: Natural Minor        C D Eb F G Ab Bb
    {0x09AD},  // 4: Harmonic Minor       C D Eb F G Ab B
    {0x0AAD},  // 5: Melodic Minor        C D Eb F G A B
    {0x06AD},  // 6: Dorian               C D Eb F G A Bb
    {0x05AB},  // 7: Phrygian             C Db Eb F G Ab Bb
    {0x0AD5},  // 8: Lydian               C D E F# G A B
    {0x06B5},  // 9: Mixolydian           C D E F G A Bb
    {0x0295},  // 10: Major Pentatonic    C D E G A
    {0x04A9},  // 11: Minor Pentatonic    C Eb F G Bb
    {0x04E9},  // 12: Blues               C Eb F F# G Bb
};

static_assert(kFactoryPresetCount == kNoteCount,
              "factory preset count must match the twelve front-panel slots");

inline QuantizerState makeFactoryConfigPreset(uint8_t slot) {
  QuantizerState state;
  if (slot >= kFactoryPresetCount) {
    return state;
  }

  const uint16_t mask = kFactoryScalePresets[slot].noteMask;
  for (uint8_t channel = 0; channel < kChannelCount; ++channel) {
    ChannelConfig channelConfig = ChannelConfig::makeDefault();
    for (uint8_t note = 0; note < kNoteCount; ++note) {
      channelConfig.notes[note] = (mask & (1u << note)) != 0u;
    }
    state.channels[channel].setConfig(channelConfig);
  }
  return state;
}

}  // namespace fmq::config

#endif  // FMQ_CONFIG_FACTORY_PRESETS_H
