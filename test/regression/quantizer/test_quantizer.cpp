/**
 * @file test_quantizer.cpp
 * Host regression or unit tests for quantizer behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/Quantizer.h"
#include "TestScale.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

using fmqtest::semis;

static void setScale(ChannelConfig &c, const int *pcs, int count) {
  fmqtest::makeScale(c.notes, pcs, count);
}

// With a chromatic scale and instant glide, output tracks the rounded input.
static void test_chromatic_rounds_nearest(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  const int pcs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  setScale(c, pcs, 12);
  ch.setConfig(c);

  // 3.4 semitones -> rounds to 3.
  ChannelOutput o = ch.step(semis(3.4), true);
  TEST_ASSERT_EQUAL_INT8(3, o.nominalSemitones);
  TEST_ASSERT_EQUAL_INT16(3 * kSemitoneOneQ8_8, o.actualSemitones);
}

// An empty scale always emits note 0.
static void test_empty_scale_emits_zero(void) {
  QuantizerChannel ch;
  ChannelConfig empty = ChannelConfig::makeDefault();
  for (uint8_t i = 0; i < kNoteCount; ++i) empty.notes[i] = false;
  ch.setConfig(empty);
  ChannelOutput o = ch.step(semis(7.3), true);
  TEST_ASSERT_EQUAL_INT8(0, o.nominalSemitones);
}

// Hysteresis: once on a note, small input changes do not switch notes.
static void test_hysteresis_holds_note(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  const int pcs[] = {0, 2};  // C and D selected; wide gap
  setScale(c, pcs, 2);
  ch.setConfig(c);

  // Land firmly on C (0).
  ch.step(semis(0.0), true);
  // Move up to 0.9 (just below D at 2, inside the widened hold band of C).
  ChannelOutput o = ch.step(semis(0.9), true);
  TEST_ASSERT_EQUAL_INT8(0, o.nominalSemitones);
  // Move to 1.6 which is past the midpoint+hysteresis -> should snap to D (2).
  o = ch.step(semis(1.6), true);
  TEST_ASSERT_EQUAL_INT8(2, o.nominalSemitones);
}

// Glide: with a slow glide the output approaches the target monotonically and
// eventually reaches it without overshooting.
static void test_glide_converges_without_overshoot(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  const int pcs[] = {0, 12};
  setScale(c, pcs, 2);
  c.glideAmount = 4;  // moderate glide
  ch.setConfig(c);

  // Establish output at 0.
  ch.step(semis(0.0), true);
  // Now request the octave.
  SemitoneQ8_8 prev = 0;
  bool reached = false;
  for (int i = 0; i < 5000; ++i) {
    ChannelOutput o = ch.step(semis(12.0), true);
    TEST_ASSERT_TRUE(o.actualSemitones >= prev);            // monotonic up
    TEST_ASSERT_TRUE(o.actualSemitones <= 12 * kSemitoneOneQ8_8);  // no overshoot
    prev = o.actualSemitones;
    if (o.actualSemitones == 12 * kSemitoneOneQ8_8) {
      reached = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(reached);
}

// Glide amount 0 is an instant jump.
static void test_glide_zero_is_instant(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  const int pcs[] = {0, 12};
  setScale(c, pcs, 2);
  c.glideAmount = 0;
  ch.setConfig(c);
  ch.step(semis(0.0), true);
  ChannelOutput o = ch.step(semis(12.0), true);
  TEST_ASSERT_EQUAL_INT16(12 * kSemitoneOneQ8_8, o.actualSemitones);
}

// A note change asserts the output trigger for a bounded number of samples.
static void test_output_trigger_pulses_on_change(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  const int pcs[] = {0, 12};
  setScale(c, pcs, 2);
  ch.setConfig(c);

  ch.step(semis(0.0), true);              // establish note 0
  ChannelOutput o = ch.step(semis(12.0), true);  // change to 12 -> triggers
  TEST_ASSERT_TRUE(o.outputTrigger);
  TEST_ASSERT_TRUE(o.outputTriggerUi);

  // The CV trigger is short; after ~10 stable samples it should be gone while
  // the UI trigger (longer) may still be lit.
  for (int i = 0; i < 10; ++i) o = ch.step(semis(12.0), true);
  TEST_ASSERT_FALSE(o.outputTrigger);
}

// Sample-and-hold only samples on the rising edge of the trigger.
static void test_sample_and_hold_edge(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  const int pcs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  setScale(c, pcs, 12);
  c.sampleMode = SampleMode::SampleAndHold;
  ch.setConfig(c);

  // First sample initialises on note 3 regardless of trigger.
  ch.step(semis(3.0), false);
  // With trigger held low, changing input does not resample.
  ChannelOutput o = ch.step(semis(7.0), false);
  TEST_ASSERT_EQUAL_INT8(3, o.nominalSemitones);
  // Rising edge samples the new value.
  o = ch.step(semis(7.0), true);
  TEST_ASSERT_EQUAL_INT8(7, o.nominalSemitones);
}

// Relative mode adds channel A's input to channel B's.
static void test_relative_mode_adds_channels(void) {
  QuantizerState state;
  state.channelBMode = PitchMode::Relative;
  const int pcs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  ChannelConfig ca = ChannelConfig::makeDefault();
  setScale(ca, pcs, 12);
  ChannelConfig cb = ca;
  state.channels[0].setConfig(ca);
  state.channels[1].setConfig(cb);

  QuantizationResult r = state.step(semis(3.0), semis(4.0), true, true);
  TEST_ASSERT_EQUAL_INT8(3, r.channelA.nominalSemitones);
  TEST_ASSERT_EQUAL_INT8(7, r.channelB.nominalSemitones);  // 3 + 4
}

// A front-panel configuration edit may move the quantized pitch, but it must
// not synthesize a musical trigger pulse/status-LED flash.
static void test_ui_change_can_suppress_next_output_trigger(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  ch.setConfig(c);
  ch.step(semis(0.0), false);

  ch.suppressNextOutputTrigger();
  ChannelOutput o = ch.step(semis(4.0), false);
  TEST_ASSERT_EQUAL_INT8(4, o.nominalSemitones);
  TEST_ASSERT_FALSE(o.outputTrigger);
  TEST_ASSERT_FALSE(o.outputTriggerUi);

  // Suppression is one-shot; a subsequent real pitch change triggers normally.
  o = ch.step(semis(7.0), false);
  TEST_ASSERT_TRUE(o.outputTrigger);
  TEST_ASSERT_TRUE(o.outputTriggerUi);
}

// On the original PCB an unpatched trigger jack is normalised HIGH. In the
// factory-default Track-and-Hold mode that must keep the input activity LED lit
// and continuously follow the input CV.
static void test_track_and_hold_high_keeps_input_led_on_and_tracks(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  c.sampleMode = SampleMode::TrackAndHold;
  ch.setConfig(c);

  ChannelOutput o = ch.step(semis(2.0), true);
  TEST_ASSERT_TRUE(o.inputTriggerUi);
  TEST_ASSERT_EQUAL_INT8(2, o.nominalSemitones);

  for (int i = 0; i < 200; ++i) {
    o = ch.step(semis(7.0), true);
    TEST_ASSERT_TRUE(o.inputTriggerUi);
  }
  TEST_ASSERT_EQUAL_INT8(7, o.nominalSemitones);
}

// Sample-and-Hold uses the same physical input differently: a held HIGH level
// only produces one edge event, so the input LED must time out after the UI
// pulse interval instead of remaining continuously illuminated.
static void test_sample_and_hold_high_input_led_times_out(void) {
  QuantizerChannel ch;
  ChannelConfig c = ChannelConfig::makeDefault();
  c.sampleMode = SampleMode::SampleAndHold;
  ch.setConfig(c);

  ChannelOutput o = ch.step(semis(2.0), true);
  TEST_ASSERT_TRUE(o.inputTriggerUi);

  for (int i = 0; i < 100; ++i) {
    o = ch.step(semis(7.0), true);
  }
  TEST_ASSERT_FALSE(o.inputTriggerUi);
  TEST_ASSERT_EQUAL_INT8(2, o.nominalSemitones);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_chromatic_rounds_nearest);
  RUN_TEST(test_empty_scale_emits_zero);
  RUN_TEST(test_hysteresis_holds_note);
  RUN_TEST(test_glide_converges_without_overshoot);
  RUN_TEST(test_glide_zero_is_instant);
  RUN_TEST(test_output_trigger_pulses_on_change);
  RUN_TEST(test_sample_and_hold_edge);
  RUN_TEST(test_track_and_hold_high_keeps_input_led_on_and_tracks);
  RUN_TEST(test_sample_and_hold_high_input_led_times_out);
  RUN_TEST(test_relative_mode_adds_channels);
  RUN_TEST(test_ui_change_can_suppress_next_output_trigger);
  return UNITY_END();
}
