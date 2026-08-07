/**
 * @file BoardProfileOriginal.h
 * Defines pins and electrical assumptions for the original FM Quantizer PCB.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_BOARD_PROFILE_ORIGINAL_H
#define FMQ_PLATFORM_NANO_BOARD_PROFILE_ORIGINAL_H
#include <Arduino.h>
namespace fmq::board {
struct Original {
  static constexpr const char *name = "FM Quantizer original PCB";
  static constexpr uint8_t dacCs = 10;
  static constexpr uint8_t ledCs = 9;
  static constexpr uint8_t ledBlank = A0;

  static constexpr uint8_t inputLedA = A1;
  static constexpr uint8_t outputLedA = A2;
  static constexpr uint8_t inputLedB = A3;
  static constexpr uint8_t outputLedB = A4;

  static constexpr uint8_t ladder = A5;
  static constexpr uint8_t cvA = A6;
  static constexpr uint8_t cvB = A7;

  static constexpr uint8_t shift = 8;
  static constexpr uint8_t save = 7;
  static constexpr uint8_t load = 6;

  static constexpr uint8_t trigInA = 2;
  static constexpr uint8_t trigInB = 3;
  static constexpr uint8_t trigOutA = 4;
  static constexpr uint8_t trigOutB = 5;
  // Important: main.rs initially constructs Adc with Aref, but fm-lib::init_async_adc()
  // subsequently writes ADMUX.REFS=AVCC. The running original firmware therefore
  // uses AVCC as its ADC reference. Keep this profile aligned with the effective
  // hardware behaviour, not the earlier constructor setting.
  static constexpr bool externalAref = false;
  static constexpr bool statusLedsActiveHigh = true;
  static constexpr bool triggerInputsActiveHigh = true;
  static constexpr bool triggerOutputsActiveHigh = true;
  static constexpr bool buttonsActiveLow = true;
  static constexpr bool buttonsUsePullups = true;
  static constexpr bool triggerInputsUsePullups = false;
};
}
#endif
