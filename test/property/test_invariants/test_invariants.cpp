/**
 * @file test_invariants.cpp
 * Deterministic property-style stress tests for firmware invariants.
 *
 * The generator is deliberately tiny and deterministic so CI failures can be
 * reproduced exactly on every supported host. These tests are not a claim of
 * formal verification; they complement example-, matrix- and regression tests
 * with broad state-space exploration.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdint.h>
#include <unity.h>

#include "fmq/application/Arpeggiator.h"
#include "fmq/application/ArpeggiatorBank.h"
#include "fmq/application/UiLayerGesture.h"
#include "fmq/config/AnalogConfig.h"
#include "fmq/config/ProductConfig.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/persistence/Serialization.h"

using namespace fmq;

namespace {
class DeterministicRng {
 public:
  explicit DeterministicRng(uint32_t seed) : state_(seed == 0u ? 1u : seed) {}

  uint32_t next() {
    uint32_t x = state_;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    state_ = x;
    return x;
  }

  uint8_t u8(uint8_t maximumInclusive) {
    return static_cast<uint8_t>(next() % (static_cast<uint32_t>(maximumInclusive) + 1u));
  }

  int8_t i8() {
    const int16_t value = static_cast<int16_t>(next() & 0xFFu) - 128;
    return static_cast<int8_t>(value);
  }

 private:
  uint32_t state_;
};

bool sameArp(const ArpeggiatorConfig &a, const ArpeggiatorConfig &b) {
  return a.enabled == b.enabled && a.rateIndex == b.rateIndex &&
         a.pattern == b.pattern && a.shape == b.shape && a.length == b.length &&
         a.range == b.range && a.stepTrigger == b.stepTrigger &&
         a.syncMode == b.syncMode && a.swing == b.swing;
}

bool sameChannelConfig(const ChannelConfig &a, const ChannelConfig &b) {
  for (uint8_t i = 0u; i < kNoteCount; ++i) {
    if (a.notes[i] != b.notes[i]) return false;
  }
  return a.sampleMode == b.sampleMode && a.glideAmount == b.glideAmount &&
         a.triggerDelayAmount == b.triggerDelayAmount &&
         a.preShift == b.preShift && a.scaleShift == b.scaleShift &&
         a.postShift == b.postShift;
}

bool validChannelConfig(const ChannelConfig &config) {
  bool anyNote = false;
  for (uint8_t i = 0u; i < kNoteCount; ++i) anyNote = anyNote || config.notes[i];
  const uint8_t sampleMode = static_cast<uint8_t>(config.sampleMode);
  return anyNote && sampleMode <= static_cast<uint8_t>(SampleMode::Continuous) &&
         config.glideAmount <= config::kMaxGlideAmount &&
         config.triggerDelayAmount <= config::kMaxTriggerDelayAmount &&
         config.preShift >= config::kMinimumShift &&
         config.preShift <= config::kMaximumShift &&
         config.scaleShift >= config::kMinimumShift &&
         config.scaleShift <= config::kMaximumShift &&
         config.postShift >= config::kMinimumShift &&
         config.postShift <= config::kMaximumShift;
}

bool validArp(const ArpeggiatorConfig &config) {
  return config.rateIndex < config::kArpRateCount &&
         static_cast<uint8_t>(config.pattern) < Arpeggiator::patternCount() &&
         static_cast<uint8_t>(config.shape) < Arpeggiator::shapeCount() &&
         config.length >= 1u && config.length <= config::kArpMaximumLength &&
         config.range >= 1u && config.range <= config::kArpMaximumRange &&
         static_cast<uint8_t>(config.syncMode) < Arpeggiator::syncModeCount() &&
         config.swing <= config::kArpMaximumSwingStep;
}

void randomNotes(DeterministicRng &rng, bool notes[kNoteCount], bool allowEmpty) {
  bool any = false;
  for (uint8_t i = 0u; i < kNoteCount; ++i) {
    notes[i] = (rng.next() & 1u) != 0u;
    any = any || notes[i];
  }
  if (!allowEmpty && !any) notes[rng.u8(kNoteCount - 1u)] = true;
}

ArpeggiatorConfig arbitraryArp(DeterministicRng &rng) {
  ArpeggiatorConfig config = ArpeggiatorConfig::makeDefault();
  config.enabled = (rng.next() & 1u) != 0u;
  config.rateIndex = rng.u8(255u);
  config.pattern = static_cast<ArpeggiatorPattern>(rng.u8(255u));
  config.shape = static_cast<ArpeggiatorShape>(rng.u8(255u));
  config.length = rng.u8(255u);
  config.range = rng.u8(255u);
  config.stepTrigger = (rng.next() & 1u) != 0u;
  config.syncMode = static_cast<ArpeggiatorSyncMode>(rng.u8(255u));
  config.swing = rng.u8(255u);
  return config;
}
}  // namespace

void setUp() {}
void tearDown() {}

static void test_random_serialized_bytes_always_decode_to_valid_state(void) {
  DeterministicRng rng(0x51A7E123u);
  for (uint32_t iteration = 0u; iteration < 50000u; ++iteration) {
    uint8_t bytes[kStoredConfigurationBytes];
    for (uint8_t i = 0u; i < kStoredConfigurationBytes; ++i) {
      bytes[i] = static_cast<uint8_t>(rng.next());
    }

    const StoredConfiguration decoded = decodeStoredConfiguration(bytes);
    TEST_ASSERT_TRUE(decoded.selectedChannelIndex < kChannelCount);
    TEST_ASSERT_TRUE(decoded.uiLayer == UiLayer::Quantizer ||
                     decoded.uiLayer == UiLayer::Arpeggiator);
    TEST_ASSERT_TRUE(validChannelConfig(decoded.quantizer.channels[kChannelAIndex].config()));
    TEST_ASSERT_TRUE(validChannelConfig(decoded.quantizer.channels[kChannelBIndex].config()));
    TEST_ASSERT_TRUE(validArp(decoded.arpeggiators[kChannelAIndex]));
    TEST_ASSERT_TRUE(validArp(decoded.arpeggiators[kChannelBIndex]));
    if (decoded.quantizer.channelsLinked) {
      TEST_ASSERT_TRUE(sameArp(decoded.arpeggiators[kChannelAIndex],
                               decoded.arpeggiators[kChannelBIndex]));
      TEST_ASSERT_EQUAL_UINT8(kChannelAIndex, decoded.selectedChannelIndex);
    }

    uint8_t canonical[kStoredConfigurationBytes];
    encodeStoredConfiguration(decoded, canonical);
    const StoredConfiguration roundTrip = decodeStoredConfiguration(canonical);
    TEST_ASSERT_TRUE(sameChannelConfig(
        decoded.quantizer.channels[kChannelAIndex].config(),
        roundTrip.quantizer.channels[kChannelAIndex].config()));
    TEST_ASSERT_TRUE(sameChannelConfig(
        decoded.quantizer.channels[kChannelBIndex].config(),
        roundTrip.quantizer.channels[kChannelBIndex].config()));
    TEST_ASSERT_TRUE(sameArp(decoded.arpeggiators[kChannelAIndex],
                             roundTrip.arpeggiators[kChannelAIndex]));
    TEST_ASSERT_TRUE(sameArp(decoded.arpeggiators[kChannelBIndex],
                             roundTrip.arpeggiators[kChannelBIndex]));
  }
}

static void test_random_quantizer_inputs_stay_inside_output_domain(void) {
  DeterministicRng rng(0xC0FFEE11u);
  for (uint32_t iteration = 0u; iteration < 30000u; ++iteration) {
    QuantizerChannel channel;
    ChannelConfig config = ChannelConfig::makeDefault();
    randomNotes(rng, config.notes, true);
    config.sampleMode = SampleMode::Continuous;
    config.glideAmount = rng.u8(config::kMaxGlideAmount);
    config.triggerDelayAmount = rng.u8(config::kMaxTriggerDelayAmount);
    config.preShift = static_cast<int8_t>(config::kMinimumShift +
        rng.u8(static_cast<uint8_t>(config::kMaximumShift - config::kMinimumShift)));
    config.scaleShift = static_cast<int8_t>(config::kMinimumShift +
        rng.u8(static_cast<uint8_t>(config::kMaximumShift - config::kMinimumShift)));
    config.postShift = static_cast<int8_t>(config::kMinimumShift +
        rng.u8(static_cast<uint8_t>(config::kMaximumShift - config::kMinimumShift)));
    channel.setConfig(config);

    const int16_t semitone = static_cast<int16_t>(rng.next() % 131u) - 5;
    const SemitoneQ8_8 input = static_cast<SemitoneQ8_8>(semitone * kSemitoneOneQ8_8);
    const ChannelOutput output = channel.step(input, true, true);
    TEST_ASSERT_TRUE(output.nominalSemitones >= 0);
    TEST_ASSERT_TRUE(output.nominalSemitones <= kMaxSemitone);
    TEST_ASSERT_TRUE(output.actualSemitones >= 0);
    TEST_ASSERT_TRUE(output.actualSemitones <=
                     static_cast<SemitoneQ8_8>(kMaxSemitone * kSemitoneOneQ8_8));
    TEST_ASSERT_TRUE(semitonesToDac(output.actualSemitones, 0u) <=
                     config::kDacMaximumCode);
  }
}

static void test_random_glide_progress_never_retriggers_after_pulse_window(void) {
  DeterministicRng rng(0x91D37A5Bu);
  for (uint32_t iteration = 0u; iteration < 2500u; ++iteration) {
    QuantizerChannel channel;
    ChannelConfig config = ChannelConfig::makeDefault();
    randomNotes(rng, config.notes, false);
    config.sampleMode = SampleMode::Continuous;
    config.glideAmount = static_cast<uint8_t>(1u + rng.u8(config::kMaxGlideAmount - 1u));
    channel.setConfig(config);

    const int16_t semitone = static_cast<int16_t>(rng.next() % 121u);
    const SemitoneQ8_8 input = static_cast<SemitoneQ8_8>(semitone * kSemitoneOneQ8_8);
    for (uint16_t tick = 0u; tick < 160u; ++tick) {
      const ChannelOutput output = channel.step(input, true, true);
      if (tick >= config::kOutputTriggerCvSamples) {
        TEST_ASSERT_FALSE(output.outputTrigger);
      }
    }
  }
}

static void test_arpeggiator_sanitizes_arbitrary_configs_and_pitch_stays_bounded(void) {
  DeterministicRng rng(0xA12E991Du);
  bool notes[kNoteCount];
  for (uint32_t iteration = 0u; iteration < 30000u; ++iteration) {
    randomNotes(rng, notes, true);
    Arpeggiator arp;
    arp.setConfig(arbitraryArp(rng), static_cast<uint32_t>(rng.next()));
    TEST_ASSERT_TRUE(validArp(arp.config()));

    const SemitoneQ8_8 base = static_cast<SemitoneQ8_8>(
        (rng.next() % 121u) * static_cast<uint32_t>(kSemitoneOneQ8_8));
    const int8_t nominal = static_cast<int8_t>(rng.next() % 121u);
    const uint32_t nowUs = rng.next();
    const uint32_t nowMs = nowUs / 1000u;
    const uint8_t edgeCount = rng.u8(4u);
    const ArpeggiatorOutput output = arp.processTimed(
        base, nominal, notes, edgeCount, nowUs - rng.u8(250u), nowMs, nowUs);
    TEST_ASSERT_TRUE(output.pitch >= 0);
    TEST_ASSERT_TRUE(output.pitch <=
                     static_cast<SemitoneQ8_8>(kMaxSemitone * kSemitoneOneQ8_8));
  }
}

static void test_linked_arpeggiator_edits_never_diverge(void) {
  DeterministicRng rng(0x1EAFB00Cu);
  ArpeggiatorBank bank;
  uint32_t now = 0xFFFFFF00u;
  for (uint32_t iteration = 0u; iteration < 30000u; ++iteration) {
    now += static_cast<uint32_t>(1u + rng.u8(30u));
    if ((rng.next() & 3u) == 0u) {
      (void)bank.toggleSelected(rng.u8(1u), true, now);
    } else {
      bank.applySelectedConfig(rng.u8(1u), true, arbitraryArp(rng), now);
    }
    TEST_ASSERT_TRUE(sameArp(bank.config(kChannelAIndex),
                             bank.config(kChannelBIndex)));
  }
}

static void test_random_ui_gesture_stream_never_toggles_while_blocked_or_modified(void) {
  DeterministicRng rng(0xD0B1EC11u);
  UiLayerGesture gesture;
  uint32_t now = 0xFFFFF000u;
  bool shift = false;

  for (uint32_t iteration = 0u; iteration < 100000u; ++iteration) {
    now += static_cast<uint32_t>(rng.u8(40u));
    if ((rng.next() & 7u) == 0u) shift = !shift;
    const bool companion = (rng.next() & 0x1Fu) == 0u;
    const bool blocked = (rng.next() & 0x3Fu) == 0u;
    const UiLayerGestureAction action =
        gesture.update(shift, companion, blocked, now);
    if (action == UiLayerGestureAction::ToggleLayer) {
      TEST_ASSERT_FALSE(shift);
      TEST_ASSERT_FALSE(companion);
      TEST_ASSERT_FALSE(blocked);
    }
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_random_serialized_bytes_always_decode_to_valid_state);
  RUN_TEST(test_random_quantizer_inputs_stay_inside_output_domain);
  RUN_TEST(test_random_glide_progress_never_retriggers_after_pulse_window);
  RUN_TEST(test_arpeggiator_sanitizes_arbitrary_configs_and_pitch_stays_bounded);
  RUN_TEST(test_linked_arpeggiator_edits_never_diverge);
  RUN_TEST(test_random_ui_gesture_stream_never_toggles_while_blocked_or_modified);
  return UNITY_END();
}
