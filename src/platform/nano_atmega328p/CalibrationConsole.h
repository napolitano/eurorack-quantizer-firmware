/**
 * @file CalibrationConsole.h
 * Implements the serial hardware calibration and loopback self-calibration console.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_CALIBRATION_CONSOLE_H
#define FMQ_PLATFORM_NANO_CALIBRATION_CONSOLE_H

#include <Arduino.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "fmq/config/AnalogConfig.h"
#include "fmq/config/RuntimeConfig.h"
#include "fmq/domain/CalibrationProfile.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/SelfCalibration.h"
#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/CalibrationStore.h"
#include "fmq/ports/AnalogInputs.h"
#include "platform/nano_atmega328p/Mcp4922Dac.h"

namespace fmq::platform::nano {

/**
 * Boot-time hardware calibration aid. Hold SHIFT while powering on.
 *
 * RAW commands bypass output correction and remain useful for electrical
 * characterization. CAL commands and normal firmware operation use the active
 * runtime profile, which is loaded from EEPROM when valid and otherwise falls
 * back to AnalogConfig.h. Colon commands edit, persist and self-calibrate that
 * same profile without recompiling the firmware.
 */
class CalibrationConsole {
 public:
  CalibrationConsole(IAnalogInputs &adc, Mcp4922Dac &dac,
                     CalibrationStore &calibrationStore,
                     AsyncEepromWriter &writer)
      : adc_(adc), dac_(dac), calibrationStore_(calibrationStore),
        writer_(writer), commandLength_(0u), lineMode_(false) {
    command_[0] = '\0';
  }

  [[noreturn]] void run() {
    Serial.begin(config::kCalibrationBaud);
    delay(config::kCalibrationConsoleStartupDelayMs);
    printHelp();
    printCalibration();

    uint16_t dacA = 0u;
    uint16_t dacB = 0u;
    bool streamEnabled = false;
    uint32_t lastStreamMs = 0u;

    for (;;) {
      servicePersistence();
      if (adc_.sampleReady()) adc_.beginCycle();

      while (Serial.available()) {
        handleInput(static_cast<char>(Serial.read()), dacA, dacB,
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
  static constexpr uint8_t kSelfFitPointCount = 5u;
  static constexpr uint8_t kSelfVerifyPointCount = 4u;
  static constexpr uint16_t kSelfFitDacCodes[kSelfFitPointCount] = {
      384u, 1024u, 2048u, 3072u, 3712u};
  static constexpr uint16_t kSelfVerifyDacCodes[kSelfVerifyPointCount] = {
      640u, 1536u, 2560u, 3456u};

  void handleInput(char input, uint16_t &dacA, uint16_t &dacB,
                   bool &streamEnabled) {
    if (input == '\r') return;

    if (lineMode_) {
      if (input == '\n') {
        command_[commandLength_] = '\0';
        handleLineCommand();
        commandLength_ = 0u;
        lineMode_ = false;
        return;
      }
      if (commandLength_ + 1u < config::kCalibrationCommandBufferBytes) {
        command_[commandLength_++] = input;
      }
      return;
    }

    if (input == ':') {
      lineMode_ = true;
      commandLength_ = 0u;
      command_[commandLength_++] = ':';
      return;
    }
    if (input == '\n') return;
    handleImmediateCommand(input, dacA, dacB, streamEnabled);
  }

  void handleImmediateCommand(char command, uint16_t &dacA, uint16_t &dacB,
                              bool &streamEnabled) {
    switch (command) {
      case 'r': printAdc(); break;
      case 's':
        streamEnabled = !streamEnabled;
        Serial.println(streamEnabled ? F("ADC stream ON") : F("ADC stream OFF"));
        break;
      case 'c': printCalibration(); break;
      case 'z': dacA = 0u; dacB = 0u; writeRawDac(dacA, dacB); break;
      case 'm': dacA = kDacMidscale; dacB = kDacMidscale; writeRawDac(dacA, dacB); break;
      case 'f':
        dacA = config::kDacMaximumCode;
        dacB = config::kDacMaximumCode;
        writeRawDac(dacA, dacB);
        break;
      case 'Z': writeCalibratedDac(0u, 0u, dacA, dacB); break;
      case 'M': writeCalibratedDac(kDacMidscale, kDacMidscale, dacA, dacB); break;
      case 'F':
        writeCalibratedDac(config::kDacMaximumCode, config::kDacMaximumCode,
                           dacA, dacB);
        break;
      case '[':
        if (dacA > 0u) --dacA;
        writeRawDac(dacA, dacB);
        break;
      case ']':
        if (dacA < config::kDacMaximumCode) ++dacA;
        writeRawDac(dacA, dacB);
        break;
      case '{':
        if (dacB > 0u) --dacB;
        writeRawDac(dacA, dacB);
        break;
      case '}':
        if (dacB < config::kDacMaximumCode) ++dacB;
        writeRawDac(dacA, dacB);
        break;
      case '?': printHelp(); break;
      default: break;
    }
  }

  static char *nextToken() { return strtok(nullptr, " \t"); }

  static bool parseSigned(const char *text, int32_t &value) {
    if (text == nullptr || *text == '\0') return false;
    char *end = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0') return false;
    if (parsed < INT16_MIN || parsed > INT16_MAX) return false;
    value = static_cast<int32_t>(parsed);
    return true;
  }

  static bool parseUnsigned16(const char *text, uint16_t &value) {
    if (text == nullptr || *text == '\0' || *text == '-') return false;
    char *end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed == 0ul || parsed > 65535ul) {
      return false;
    }
    value = static_cast<uint16_t>(parsed);
    return true;
  }

  static bool parseChannel(const char *text, uint8_t &channel) {
    if (text == nullptr || text[0] == '\0' || text[1] != '\0') return false;
    if (text[0] == 'a' || text[0] == 'A') { channel = 0u; return true; }
    if (text[0] == 'b' || text[0] == 'B') { channel = 1u; return true; }
    return false;
  }

  void handleLineCommand() {
    char *verb = strtok(command_, " \t");
    if (verb == nullptr) return;

    if (strcmp(verb, ":show") == 0 || strcmp(verb, ":c") == 0) {
      printCalibration();
      return;
    }
    if (strcmp(verb, ":help") == 0 || strcmp(verb, ":?") == 0) {
      printHelp();
      return;
    }
    if (strcmp(verb, ":save") == 0) {
      saveCalibration();
      return;
    }
    if (strcmp(verb, ":load") == 0) {
      loadCalibration();
      return;
    }
    if (strcmp(verb, ":defaults") == 0) {
      resetActiveCalibrationProfile();
      Serial.println(F("Factory calibration active in RAM; use :save to persist."));
      printCalibration();
      return;
    }
    if (strcmp(verb, ":self") == 0) {
      uint8_t channel = 0u;
      if (!parseChannel(nextToken(), channel) || nextToken() != nullptr) {
        Serial.println(F("Usage: :self a|b"));
        return;
      }
      runSelfCalibration(channel);
      return;
    }
    if (strcmp(verb, ":adc") == 0 || strcmp(verb, ":dac") == 0) {
      const bool adcPath = strcmp(verb, ":adc") == 0;
      uint8_t channel = 0u;
      int32_t offset = 0;
      uint16_t numerator = 0u;
      uint16_t denominator = 0u;
      const char *channelToken = nextToken();
      const char *offsetToken = nextToken();
      const char *numeratorToken = nextToken();
      const char *denominatorToken = nextToken();
      if (!parseChannel(channelToken, channel) ||
          !parseSigned(offsetToken, offset) ||
          !parseUnsigned16(numeratorToken, numerator) ||
          !parseUnsigned16(denominatorToken, denominator) ||
          nextToken() != nullptr) {
        Serial.println(F("Usage: :adc|:dac a|b OFFSET NUM DEN"));
        return;
      }
      CalibrationProfile profile = activeCalibrationProfile();
      LinearCalibration &target =
          adcPath ? profile.adc[channel] : profile.dac[channel];
      target = {offset, numerator, denominator};
      if (!setActiveCalibrationProfile(profile)) {
        Serial.println(F("Rejected calibration."));
        return;
      }
      Serial.println(F("Calibration changed in RAM; use :save to persist."));
      printCalibration();
      return;
    }

    Serial.println(F("Unknown command; use :help."));
  }

  void printHelp() const {
    Serial.println(F("FMQ calibration console"));
    Serial.println(F("ADC: r=RAW+CAL read, s=stream, c=active profile"));
    Serial.println(F("RAW DAC: z/m/f=min/mid/max, [/]=A -/+, {/}=B -/+"));
    Serial.println(F("CAL DAC: Z/M/F=min/mid/max through active correction"));
    Serial.println(F(":adc a|b OFFSET NUM DEN   :dac a|b OFFSET NUM DEN"));
    Serial.println(F(":save  :load  :defaults  :show  :self a|b  :help"));
    Serial.println(F("Selfcal requires CV OUT A->CV IN A or B->B."));
  }

  void printCalibration() const {
    const CalibrationProfile &profile = activeCalibrationProfile();
    Serial.println(F("Active calibration"));
    printCalibrationLine(F("ADC A"), profile.adc[0]);
    printCalibrationLine(F("ADC B"), profile.adc[1]);
    printCalibrationLine(F("DAC A"), profile.dac[0]);
    printCalibrationLine(F("DAC B"), profile.dac[1]);
  }

  void printCalibrationLine(const __FlashStringHelper *label,
                            const LinearCalibration &calibration) const {
    Serial.print(label);
    Serial.print(F(" offset="));
    Serial.print(calibration.offset);
    Serial.print(F(" gain="));
    Serial.print(calibration.gainNumerator);
    Serial.print('/');
    Serial.println(calibration.gainDenominator);
  }

  void printAdc() const {
    const uint16_t ladderRaw = adc_.read(0u);
    const uint16_t adcARaw = adc_.read(1u);
    const uint16_t adcBRaw = adc_.read(2u);
    Serial.print(F("ADC RAW ladder="));
    Serial.print(ladderRaw);
    Serial.print(F(" A="));
    Serial.print(adcARaw);
    Serial.print(F(" B="));
    Serial.println(adcBRaw);
    Serial.print(F("ADC CAL A="));
    Serial.print(calibratedAdcCode(adcARaw, 0u));
    Serial.print(F(" B="));
    Serial.println(calibratedAdcCode(adcBRaw, 1u));
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
    dacA = calibratedDacCode(nominalA, 0u);
    dacB = calibratedDacCode(nominalB, 1u);
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

  void servicePersistence() {
    writer_.service();
    calibrationStore_.observeWriter();
  }

  bool waitForPersistence() {
    const uint32_t start = millis();
    while (writer_.busy()) {
      servicePersistence();
      if (millis() - start >= config::kCalibrationPersistenceTimeoutMs) {
        return false;
      }
    }
    calibrationStore_.observeWriter();
    return true;
  }

  void saveCalibration() {
    if (!waitForPersistence()) {
      Serial.println(F("Calibration save failed: EEPROM busy timeout."));
      return;
    }
    if (!calibrationStore_.save(activeCalibrationProfile())) {
      Serial.println(F("Calibration save rejected."));
      return;
    }
    if (!waitForPersistence()) {
      Serial.println(F("Calibration save failed: EEPROM completion timeout."));
      return;
    }
    Serial.println(F("Calibration stored."));
  }

  void loadCalibration() {
    if (!waitForPersistence()) {
      Serial.println(F("Calibration load failed: EEPROM busy timeout."));
      return;
    }
    CalibrationProfile profile;
    if (!calibrationStore_.load(profile)) {
      Serial.println(F("No valid stored calibration; active profile unchanged."));
      return;
    }
    if (!setActiveCalibrationProfile(profile)) {
      Serial.println(F("Stored calibration rejected; active profile unchanged."));
      return;
    }
    Serial.println(F("Stored calibration loaded."));
    printCalibration();
  }

  bool sampleCalibratedAdc(uint8_t channel, uint16_t &average) {
    uint32_t sum = 0u;
    uint8_t samples = 0u;
    const uint32_t start = millis();
    while (samples < config::kSelfCalibrationAverageSamples) {
      if (adc_.sampleReady()) {
        adc_.beginCycle();
        const uint16_t raw = adc_.read(static_cast<uint8_t>(channel + 1u));
        sum += calibratedAdcCode(raw, channel);
        ++samples;
      }
      if (millis() - start >= config::kSelfCalibrationSampleTimeoutMs) {
        return false;
      }
    }
    average = static_cast<uint16_t>(
        (sum + config::kSelfCalibrationAverageSamples / 2u) /
        config::kSelfCalibrationAverageSamples);
    return true;
  }

  Mcp4922Dac::Channel dacChannel(uint8_t channel) const {
    return channel == 0u ? Mcp4922Dac::Channel::A : Mcp4922Dac::Channel::B;
  }

  void zeroSelectedOutput(uint8_t channel) {
    dac_.write(dacChannel(channel), calibratedDacCode(0, channel));
  }

  void printSelfStatus(SelfCalibrationStatus status) const {
    switch (status) {
      case SelfCalibrationStatus::TooFewSamples: Serial.println(F("too few samples")); break;
      case SelfCalibrationStatus::NonMonotonic: Serial.println(F("non-monotonic loop")); break;
      case SelfCalibrationStatus::SpanTooSmall: Serial.println(F("loop span too small")); break;
      case SelfCalibrationStatus::ImplausibleGain: Serial.println(F("implausible loop gain")); break;
      case SelfCalibrationStatus::OffsetOutOfRange: Serial.println(F("offset out of range")); break;
      case SelfCalibrationStatus::Ok: Serial.println(F("OK")); break;
    }
  }

  void runSelfCalibration(uint8_t channel) {
    if (!waitForPersistence()) {
      Serial.println(F("Selfcal aborted: EEPROM busy timeout."));
      return;
    }

    Serial.print(F("Selfcal "));
    Serial.print(channel == 0u ? 'A' : 'B');
    Serial.println(F(": checking loop and measuring RAW output..."));

    const CalibrationProfile previous = activeCalibrationProfile();
    SelfCalibrationSample samples[kSelfFitPointCount];
    for (uint8_t i = 0u; i < kSelfFitPointCount; ++i) {
      const uint16_t rawDac = kSelfFitDacCodes[i];
      dac_.write(dacChannel(channel), rawDac);
      delay(config::kSelfCalibrationSettleMs);
      uint16_t adcCode = 0u;
      if (!sampleCalibratedAdc(channel, adcCode)) {
        zeroSelectedOutput(channel);
        Serial.println(F("Selfcal FAIL: ADC timeout; calibration unchanged."));
        return;
      }
      samples[i] = {rawDac, adcCode};
    }

    const SelfCalibrationResult fit =
        solveDacSelfCalibration(samples, kSelfFitPointCount);
    if (fit.status != SelfCalibrationStatus::Ok) {
      zeroSelectedOutput(channel);
      Serial.print(F("Selfcal FAIL: "));
      printSelfStatus(fit.status);
      Serial.println(F("Calibration unchanged."));
      return;
    }
    if (fit.maxFitResidualDacCodes >
        config::kSelfCalibrationMaxFitResidualDacCodes) {
      zeroSelectedOutput(channel);
      Serial.println(F("Selfcal FAIL: output is not sufficiently affine."));
      return;
    }

    CalibrationProfile candidate = previous;
    candidate.dac[channel] = fit.calibration;
    if (!setActiveCalibrationProfile(candidate)) {
      zeroSelectedOutput(channel);
      Serial.println(F("Selfcal FAIL: fitted calibration rejected."));
      return;
    }

    Serial.println(F("Fit OK; verifying independent points..."));
    uint16_t maxResidual = 0u;
    bool verificationOk = true;
    for (uint8_t i = 0u; i < kSelfVerifyPointCount; ++i) {
      const uint16_t nominal = kSelfVerifyDacCodes[i];
      dac_.write(dacChannel(channel), calibratedDacCode(nominal, channel));
      delay(config::kSelfCalibrationSettleMs);
      uint16_t adcCode = 0u;
      if (!sampleCalibratedAdc(channel, adcCode)) {
        verificationOk = false;
        break;
      }
      const uint16_t measured = adcCodeToEquivalentDac(adcCode);
      const uint16_t residual = measured >= nominal
                                    ? static_cast<uint16_t>(measured - nominal)
                                    : static_cast<uint16_t>(nominal - measured);
      if (residual > maxResidual) maxResidual = residual;
      if (residual > config::kSelfCalibrationMaxVerifyResidualDacCodes) {
        verificationOk = false;
        break;
      }
    }

    if (!verificationOk) {
      (void)setActiveCalibrationProfile(previous);
      zeroSelectedOutput(channel);
      Serial.println(F("Selfcal FAIL: verification residual too large; calibration unchanged."));
      return;
    }

    if (!calibrationStore_.save(activeCalibrationProfile()) ||
        !waitForPersistence()) {
      (void)setActiveCalibrationProfile(previous);
      zeroSelectedOutput(channel);
      Serial.println(F("Selfcal FAIL: could not persist result; calibration restored."));
      return;
    }

    zeroSelectedOutput(channel);
    Serial.print(F("Selfcal PASS, max verification residual="));
    Serial.print(maxResidual);
    Serial.println(F(" DAC codes. Calibration stored."));
    printCalibration();
  }

  IAnalogInputs &adc_;
  Mcp4922Dac &dac_;
  CalibrationStore &calibrationStore_;
  AsyncEepromWriter &writer_;
  char command_[config::kCalibrationCommandBufferBytes];
  uint8_t commandLength_;
  bool lineMode_;
};

constexpr uint16_t CalibrationConsole::kSelfFitDacCodes[kSelfFitPointCount];
constexpr uint16_t CalibrationConsole::kSelfVerifyDacCodes[kSelfVerifyPointCount];

}  // namespace fmq::platform::nano

#endif
