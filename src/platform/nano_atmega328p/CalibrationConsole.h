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
#include "platform/nano_atmega328p/AvrAnalogInputs.h"
#include "platform/nano_atmega328p/Mcp4922Dac.h"

namespace fmq::platform::nano {

/**
 * Boot-time hardware calibration aid. Hold SHIFT while powering on.
 *
 * The console deliberately does not infer electrical calibration values. It
 * exposes stable ADC readings and raw DAC codes so measured values can be
 * copied into AnalogConfig after checking them with suitable test equipment.
 */
class CalibrationConsole {
 public:
  CalibrationConsole(AvrAnalogInputs &adc, Mcp4922Dac &dac)
      : adc_(adc), dac_(dac) {}

  [[noreturn]] void run() {
    Serial.begin(config::kCalibrationBaud);
    delay(config::kCalibrationConsoleStartupDelayMs);
    printHelp();

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
        break;
      case 'z':
        dacA = 0;
        dacB = 0;
        writeDac(dacA, dacB);
        break;
      case 'm':
        dacA = kDacMidscale;
        dacB = kDacMidscale;
        writeDac(dacA, dacB);
        break;
      case 'f':
        dacA = config::kDacMaximumCode;
        dacB = config::kDacMaximumCode;
        writeDac(dacA, dacB);
        break;
      case '[':
        if (dacA > 0) --dacA;
        writeDac(dacA, dacB);
        break;
      case ']':
        if (dacA < config::kDacMaximumCode) ++dacA;
        writeDac(dacA, dacB);
        break;
      case '{':
        if (dacB > 0) --dacB;
        writeDac(dacA, dacB);
        break;
      case '}':
        if (dacB < config::kDacMaximumCode) ++dacB;
        writeDac(dacA, dacB);
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
    Serial.println(F("r=read ADC, s=stream toggle, z/m/f=DAC min/mid/max"));
    Serial.println(F("[/]=DAC A -/+, {/}=DAC B -/+, ?=help"));
    Serial.println(F("Record ladder codes with r/stream; measure DAC A/B externally."));
  }

  void printAdc() const {
    Serial.print(F("ladder="));
    Serial.print(adc_.read(0));
    Serial.print(F(" cvA="));
    Serial.print(adc_.read(1));
    Serial.print(F(" cvB="));
    Serial.println(adc_.read(2));
  }

  void writeDac(uint16_t dacA, uint16_t dacB) {
    dac_.write(Mcp4922Dac::Channel::A, dacA);
    dac_.write(Mcp4922Dac::Channel::B, dacB);
    Serial.print(F("dacA="));
    Serial.print(dacA);
    Serial.print(F(" dacB="));
    Serial.println(dacB);
  }

  AvrAnalogInputs &adc_;
  Mcp4922Dac &dac_;
};

}  // namespace fmq::platform::nano

#endif
