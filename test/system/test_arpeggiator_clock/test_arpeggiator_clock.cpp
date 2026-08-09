/**
 * @file test_arpeggiator_clock.cpp
 * End-to-end timing, clock and trigger tests for the Arpeggiator signal path.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "QuantizerTestRig.h"
#include "fmq/application/Arpeggiator.h"
#include "fmq/domain/FixedPoint.h"

using namespace fmq;
using fmqtest::QuantizerTestRig;

void setUp(void) {}
void tearDown(void) {}

namespace {
ArpeggiatorConfig clockConfig(uint8_t rateIndex, bool stepTrigger = false) {
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.rateIndex = rateIndex;
  cfg.syncMode = ArpeggiatorSyncMode::Clock;
  cfg.stepTrigger = stepTrigger;
  return cfg;
}

void armClockInputA(QuantizerTestRig &rig) {
  rig.setGateA(false);
  rig.tick();
}

void risingClockA(QuantizerTestRig &rig) {
  rig.setGateA(true);
  rig.tick();
  rig.setGateA(false);
  rig.tick();
}
}

static void test_clock_mode_keeps_cv_tracking_even_if_sample_mode_is_sample_and_hold(void) {
  QuantizerTestRig rig;
  ChannelConfig q = ChannelConfig::makeDefault();
  q.sampleMode = SampleMode::SampleAndHold;
  rig.state().channels[kChannelAIndex].setConfig(q);
  rig.arpeggiators().setConfig(kChannelAIndex, clockConfig(5u), 0u);  // x1
  armClockInputA(rig);

  rig.setCvVoltsA(1.0);
  rig.tick();
  const int8_t first = rig.last().quantization.channelA.nominalSemitones;
  rig.setCvVoltsA(3.0);
  rig.tick();
  const int8_t second = rig.last().quantization.channelA.nominalSemitones;

  TEST_ASSERT_NOT_EQUAL(first, second);
  TEST_ASSERT_EQUAL_INT8(36, second);
}

static void test_clock_x1_advances_only_on_external_rising_edges(void) {
  QuantizerTestRig rig;
  rig.setCvVoltsA(1.0);
  rig.arpeggiators().setConfig(kChannelAIndex, clockConfig(5u), 0u);  // x1
  armClockInputA(rig);

  const SemitoneQ8_8 before = rig.tick().outputPitchA;
  rig.runFor(100u);
  TEST_ASSERT_EQUAL_INT32(before, rig.last().outputPitchA);

  rig.setGateA(true);
  const SemitoneQ8_8 firstEdge = rig.tick().outputPitchA;
  TEST_ASSERT_EQUAL_INT32(static_cast<SemitoneQ8_8>(12 * kSemitoneOneQ8_8), firstEdge);
  rig.setGateA(false);
  rig.tick();
  rig.runFor(100u);
  TEST_ASSERT_EQUAL_INT32(firstEdge, rig.last().outputPitchA);

  rig.setGateA(true);
  const SemitoneQ8_8 secondEdge = rig.tick().outputPitchA;
  TEST_ASSERT_TRUE(secondEdge > firstEdge);
}

static void test_clock_multiplier_x4_generates_three_intermediate_steps(void) {
  QuantizerTestRig rig;
  rig.setCvVoltsA(1.0);
  rig.arpeggiators().setConfig(kChannelAIndex, clockConfig(8u), 0u);  // x4
  armClockInputA(rig);

  // Two real edges 100 ms apart establish the external period.
  rig.setGateA(true); rig.tick();
  rig.setGateA(false); rig.tick();
  rig.runFor(98u);
  rig.setGateA(true); rig.tick();
  rig.setGateA(false); rig.tick();

  const SemitoneQ8_8 atEdge = rig.last().outputPitchA;
  rig.runFor(23u);
  TEST_ASSERT_EQUAL_INT32(atEdge, rig.last().outputPitchA);
  rig.tick();  // 25 ms from the edge, first generated substep
  TEST_ASSERT_NOT_EQUAL(atEdge, rig.last().outputPitchA);
}

static void test_clock_divide_by_two_requires_two_external_intervals_per_step(void) {
  QuantizerTestRig rig;
  rig.setCvVoltsA(1.0);
  rig.arpeggiators().setConfig(kChannelAIndex, clockConfig(4u), 0u);  // /2
  armClockInputA(rig);

  rig.setGateA(true);
  const SemitoneQ8_8 first = rig.tick().outputPitchA;
  rig.setGateA(false); rig.tick();
  rig.runFor(20u);
  rig.setGateA(true);
  const SemitoneQ8_8 second = rig.tick().outputPitchA;
  TEST_ASSERT_EQUAL_INT32(first, second);
  rig.setGateA(false); rig.tick();
  rig.runFor(20u);
  rig.setGateA(true);
  const SemitoneQ8_8 third = rig.tick().outputPitchA;
  TEST_ASSERT_NOT_EQUAL(second, third);
}

static void test_step_trigger_off_never_creates_trigger_from_arp_step(void) {
  QuantizerTestRig rig;
  rig.setCvVoltsA(1.0);
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.rateIndex = 0u;
  cfg.stepTrigger = false;
  rig.arpeggiators().setConfig(kChannelAIndex, cfg, 0u);
  rig.tick();
  rig.state().channels[kChannelAIndex].suppressNextOutputTrigger();
  rig.clearHistory();

  rig.runFor(Arpeggiator::freeRateMs(0u) + 2u);
  for (const auto &sample : rig.history()) {
    TEST_ASSERT_FALSE(sample.triggerA);
  }
}

static void test_step_trigger_on_is_exactly_five_ticks_and_led_is_approximately_65_ticks(void) {
  QuantizerTestRig rig;
  rig.setCvVoltsA(1.0);
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.rateIndex = 11u;  // keep next ARP step far away after the observed step
  cfg.stepTrigger = true;
  rig.arpeggiators().setConfig(kChannelAIndex, cfg, 0u);
  rig.tick();
  rig.state().channels[kChannelAIndex].suppressNextOutputTrigger();
  rig.clearHistory();

  rig.runFor(Arpeggiator::freeRateMs(11u));
  // The last sample is the first generated ARP step and therefore counts as
  // tick one of both pulse windows. Count it explicitly so the assertions are
  // stated in physical 1-ms ticks rather than as an off-by-one test artifact.
  uint8_t triggerTicks = rig.last().triggerA ? 1u : 0u;
  uint8_t ledTicks = rig.last().outputLedA ? 1u : 0u;
  for (uint16_t i = 0u; i < 70u; ++i) {
    const auto &sample = rig.tick();
    if (sample.triggerA) ++triggerTicks;
    if (sample.outputLedA) ++ledTicks;
  }
  TEST_ASSERT_EQUAL_UINT8(config::kOutputTriggerCvSamples, triggerTicks);
  TEST_ASSERT_EQUAL_UINT8(config::kOutputTriggerLedSamples, ledTicks);
}

static void test_channel_a_clock_does_not_advance_channel_b(void) {
  QuantizerTestRig rig;
  rig.setCvVoltsA(1.0);
  rig.setCvVoltsB(1.0);
  rig.arpeggiators().setConfig(kChannelAIndex, clockConfig(5u), 0u);
  rig.arpeggiators().setConfig(kChannelBIndex, clockConfig(5u), 0u);
  rig.setGateA(false);
  rig.setGateB(false);
  rig.tick();
  const SemitoneQ8_8 bBefore = rig.last().outputPitchB;

  risingClockA(rig);
  risingClockA(rig);
  TEST_ASSERT_EQUAL_INT32(bBefore, rig.last().outputPitchB);
}

static void test_reset_mode_restarts_internal_pattern_without_becoming_clock_driven(void) {
  QuantizerTestRig rig;
  rig.setCvVoltsA(1.0);
  ArpeggiatorConfig cfg = ArpeggiatorConfig::makeDefault();
  cfg.enabled = true;
  cfg.rateIndex = 3u;  // 24 ms
  cfg.syncMode = ArpeggiatorSyncMode::Reset;
  rig.arpeggiators().setConfig(kChannelAIndex, cfg, 0u);
  rig.setGateA(false);
  rig.tick();
  rig.runFor(25u);
  TEST_ASSERT_TRUE(rig.last().outputPitchA > static_cast<SemitoneQ8_8>(12 * kSemitoneOneQ8_8));

  rig.setGateA(true);
  rig.tick();
  TEST_ASSERT_EQUAL_INT32(static_cast<SemitoneQ8_8>(12 * kSemitoneOneQ8_8), rig.last().outputPitchA);
  rig.setGateA(false);
  rig.tick();
  rig.runFor(24u);
  TEST_ASSERT_TRUE(rig.last().outputPitchA > static_cast<SemitoneQ8_8>(12 * kSemitoneOneQ8_8));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_clock_mode_keeps_cv_tracking_even_if_sample_mode_is_sample_and_hold);
  RUN_TEST(test_clock_x1_advances_only_on_external_rising_edges);
  RUN_TEST(test_clock_multiplier_x4_generates_three_intermediate_steps);
  RUN_TEST(test_clock_divide_by_two_requires_two_external_intervals_per_step);
  RUN_TEST(test_step_trigger_off_never_creates_trigger_from_arp_step);
  RUN_TEST(test_step_trigger_on_is_exactly_five_ticks_and_led_is_approximately_65_ticks);
  RUN_TEST(test_channel_a_clock_does_not_advance_channel_b);
  RUN_TEST(test_reset_mode_restarts_internal_pattern_without_becoming_clock_driven);
  return UNITY_END();
}
