/**
 * @file CalibrationConsole.h
 * Implements the serial hardware calibration and diagnostic console.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_CALIBRATION_CONSOLE_H
#define FMQ_PLATFORM_NANO_CALIBRATION_CONSOLE_H

#include <Arduino.h>

#include "fmq/config/AnalogConfig.h"
#include "fmq/config/RuntimeConfig.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/ports/AnalogInputs.h"
#include "platform/nano_atmega328p/Mcp4922Dac.h"

namespace fmq::platform::nano {

/**
 * Boot-time hardware calibration aid. Hold SHIFT while powering on.
 *
 * The console exposes both views required for a safe calibration workflow:
 *
 * - RAW values are the uncorrected hardware ADC/DAC codes used to derive
 *   offsets and gain ratios.
 * - CAL values pass through the exact same calibration helpers used by normal
 *   quantizer operation and are therefore suitable for verifying the compiled
 *   configuration.
 *
 * Raw DAC stepping intentionally remains available for analogue diagnostics.
 */
class CalibrationConsole {
 public:
  CalibrationConsole(IAnalogInputs &adc, Mcp4922Dac &dac)
      : adc_(adc), dac_(dac) {}

  [[noreturn]] void run() {
    Serial.begin(config::kCalibrationBaud);
    delay(config::kCalibrationConsoleStartupDelayMs);
    printHelp();
    printCalibration();

    uint16_t dacA = 0;
    uint16_t dacB = 0;
    bool streamEnabled = false;
    uint32_t lastStreamMs = 0;

    for (;;) {
      if (adc_.sampleReady()) {
        adc_.beginCycle();
      }

      if (Serial.available()) {
        handleCommand(static_cast<char>(Serial.read()), dacA, dacB,
                      streamEnabled);
      }

      const uint32_t nowMs = millis();
      if (streamEnabled &&
          nowMs - lastStreamMs >= config::kCalibrationStreamPeriodMs) {
        lastStreamMs = nowMs;
        printAdc();
      }
    }
  }

 private:
  static constexpr uint16_t kDacMidscale =
      (config::kDacMaximumCode + 1u) / 2u;

  void handleCommand(char command, uint16_t &dacA, uint16_t &dacB,
                     bool &streamEnabled) {
    switch (command) {
      case 'r':
        printAdc();
        break;
      case 's':
        streamEnabled = !streamEnabled;
        Serial.println(streamEnabled ? F("ADC stream ON") : F("ADC stream OFF"));
        break;
      case 'c':
        printCalibration();
        break;
      case 'z':
        dacA = 0;
        dacB = 0;
        writeRawDac(dacA, dacB);
        break;
      case 'm':
        dacA = kDacMidscale;
        dacB = kDacMidscale;
        writeRawDac(dacA, dacB);
        break;
      case 'f':
        dacA = config::kDacMaximumCode;
        dacB = config::kDacMaximumCode;
        writeRawDac(dacA, dacB);
        break;
      case 'Z':
        writeCalibratedDac(0, 0, dacA, dacB);
        break;
      case 'M':
        writeCalibratedDac(kDacMidscale, kDacMidscale, dacA, dacB);
        break;
      case 'F':
        writeCalibratedDac(config::kDacMaximumCode,
                           config::kDacMaximumCode, dacA, dacB);
        break;
      case '[':
        if (dacA > 0) --dacA;
        writeRawDac(dacA, dacB);
        break;
      case ']':
        if (dacA < config::kDacMaximumCode) ++dacA;
        writeRawDac(dacA, dacB);
        break;
      case '{':
        if (dacB > 0) --dacB;
        writeRawDac(dacA, dacB);
        break;
      case '}':
        if (dacB < config::kDacMaximumCode) ++dacB;
        writeRawDac(dacA, dacB);
        break;
      case '?':
        printHelp();
        break;
      default:
        break;
    }
  }

  void printHelp() const {
    Serial.println(F("FMQ calibration console"));
    Serial.println(F("ADC: r=RAW+CAL read, s=stream, c=constants"));
    Serial.println(F("RAW DAC: z/m/f=min/mid/max, [/]=A -/+, {/}=B -/+"));
    Serial.println(F("CAL DAC: Z/M/F=min/mid/max through runtime correction"));
    Serial.println(F("RAW bypasses calibration; CAL uses runtime correction."));
    Serial.println(F("?=help"));
  }

  void printCalibration() const {
    Serial.println(F("Calibration constants"));
    printCalibrationLine(F("ADC A"), config::kAdcOffsetA,
                         config::kAdcGainNumeratorA,
                         config::kAdcGainDenominatorA);
    printCalibrationLine(F("ADC B"), config::kAdcOffsetB,
                         config::kAdcGainNumeratorB,
                         config::kAdcGainDenominatorB);
    printCalibrationLine(F("DAC A"), config::kDacOffsetA,
                         config::kDacGainNumeratorA,
                         config::kDacGainDenominatorA);
    printCalibrationLine(F("DAC B"), config::kDacOffsetB,
                         config::kDacGainNumeratorB,
                         config::kDacGainDenominatorB);
  }

  void printCalibrationLine(const __FlashStringHelper *label, int32_t offset,
                            uint16_t numerator, uint16_t denominator) const {
    Serial.print(label);
    Serial.print(F(" offset="));
    Serial.print(offset);
    Serial.print(F(" gain="));
    Serial.print(numerator);
    Serial.print('/');
    Serial.println(denominator);
  }

  void printAdc() const {
    const uint16_t ladderRaw = adc_.read(0);
    const uint16_t adcARaw = adc_.read(1);
    const uint16_t adcBRaw = adc_.read(2);

    Serial.print(F("ADC RAW ladder="));
    Serial.print(ladderRaw);
    Serial.print(F(" A="));
    Serial.print(adcARaw);
    Serial.print(F(" B="));
    Serial.println(adcBRaw);

    Serial.print(F("ADC CAL A="));
    Serial.print(calibratedAdcCode(adcARaw, 0));
    Serial.print(F(" B="));
    Serial.println(calibratedAdcCode(adcBRaw, 1));
  }

  void writeRawDac(uint16_t dacA, uint16_t dacB) {
    dac_.write(Mcp4922Dac::Channel::A, dacA);
    dac_.write(Mcp4922Dac::Channel::B, dacB);
    Serial.print(F("DAC RAW A="));
    Serial.print(dacA);
    Serial.print(F(" B="));
    Serial.println(dacB);
  }

  void writeCalibratedDac(uint16_t nominalA, uint16_t nominalB,
                          uint16_t &dacA, uint16_t &dacB) {
    dacA = calibratedDacCode(nominalA, 0);
    dacB = calibratedDacCode(nominalB, 1);

    dac_.write(Mcp4922Dac::Channel::A, dacA);
    dac_.write(Mcp4922Dac::Channel::B, dacB);

    Serial.print(F("DAC CAL A="));
    Serial.print(nominalA);
    Serial.print(F("->"));
    Serial.print(dacA);
    Serial.print(F(" B="));
    Serial.print(nominalB);
    Serial.print(F("->"));
    Serial.println(dacB);
  }

  IAnalogInputs &adc_;
  Mcp4922Dac &dac_;
};

}  // namespace fmq::platform::nano

#endif
