/**
 * @file test_arpeggiator_channels.cpp
 * Independent A/B configuration, linking and runtime tests.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <unity.h>

#include "fmq/application/ArpeggiatorBank.h"

using namespace fmq;

void setUp(void) {}
void tearDown(void) {}

static void test_both_channels_start_disabled(void) {
  ArpeggiatorBank bank;
  TEST_ASSERT_FALSE(bank.enabled(kChannelAIndex));
  TEST_ASSERT_FALSE(bank.enabled(kChannelBIndex));
}

static void test_unlinked_toggles_are_independent(void) {
  ArpeggiatorBank bank;
  TEST_ASSERT_TRUE(bank.toggleSelected(kChannelAIndex, false, 100u));
  TEST_ASSERT_TRUE(bank.enabled(kChannelAIndex));
  TEST_ASSERT_FALSE(bank.enabled(kChannelBIndex));
  TEST_ASSERT_TRUE(bank.toggleSelected(kChannelBIndex, false, 200u));
  TEST_ASSERT_TRUE(bank.enabled(kChannelAIndex));
  TEST_ASSERT_TRUE(bank.enabled(kChannelBIndex));
}

static void test_linked_toggle_normalises_mixed_state(void) {
  ArpeggiatorBank bank;
  bank.setEnabled(kChannelAIndex, true, 10u);
  bank.setEnabled(kChannelBIndex, false, 20u);
  TEST_ASSERT_TRUE(bank.toggleSelected(kChannelAIndex, true, 400u));
  TEST_ASSERT_TRUE(bank.enabled(kChannelAIndex));
  TEST_ASSERT_TRUE(bank.enabled(kChannelBIndex));
}

static void test_link_from_a_copies_every_musical_parameter(void) {
  ArpeggiatorBank bank;
  ArpeggiatorConfig a = ArpeggiatorConfig::makeDefault();
  a.enabled = true;
  a.rateIndex = 10u;
  a.pattern = ArpeggiatorPattern::OutsideIn;
  a.shape = ArpeggiatorShape::Seventh1357;
  a.length = 9u;
  a.range = 3u;
  a.stepTrigger = true;
  a.syncMode = ArpeggiatorSyncMode::Clock;
  a.swing = 7u;
  bank.setConfig(kChannelAIndex, a, 10u);
  bank.linkFromA(20u);
  const ArpeggiatorConfig &b = bank.config(kChannelBIndex);
  TEST_ASSERT_EQUAL(a.enabled, b.enabled);
  TEST_ASSERT_EQUAL_UINT8(a.rateIndex, b.rateIndex);
  TEST_ASSERT_EQUAL(a.pattern, b.pattern);
  TEST_ASSERT_EQUAL(a.shape, b.shape);
  TEST_ASSERT_EQUAL_UINT8(a.length, b.length);
  TEST_ASSERT_EQUAL_UINT8(a.range, b.range);
  TEST_ASSERT_EQUAL(a.stepTrigger, b.stepTrigger);
  TEST_ASSERT_EQUAL(a.syncMode, b.syncMode);
  TEST_ASSERT_EQUAL_UINT8(a.swing, b.swing);
}

static void test_apply_selected_config_does_not_leak_to_other_channel(void) {
  ArpeggiatorBank bank;
  ArpeggiatorConfig a = bank.config(kChannelAIndex);
  a.rateIndex = 11u;
  bank.applySelectedConfig(kChannelAIndex, false, a, 100u);
  TEST_ASSERT_EQUAL_UINT8(11u, bank.config(kChannelAIndex).rateIndex);
  TEST_ASSERT_EQUAL_UINT8(3u, bank.config(kChannelBIndex).rateIndex);
}

static void test_apply_selected_config_updates_both_when_linked(void) {
  ArpeggiatorBank bank;
  ArpeggiatorConfig cfg = bank.config(kChannelBIndex);
  cfg.swing = 9u;
  bank.applySelectedConfig(kChannelBIndex, true, cfg, 100u);
  TEST_ASSERT_EQUAL_UINT8(9u, bank.config(kChannelAIndex).swing);
  TEST_ASSERT_EQUAL_UINT8(9u, bank.config(kChannelBIndex).swing);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_both_channels_start_disabled);
  RUN_TEST(test_unlinked_toggles_are_independent);
  RUN_TEST(test_linked_toggle_normalises_mixed_state);
  RUN_TEST(test_link_from_a_copies_every_musical_parameter);
  RUN_TEST(test_apply_selected_config_does_not_leak_to_other_channel);
  RUN_TEST(test_apply_selected_config_updates_both_when_linked);
  return UNITY_END();
}
