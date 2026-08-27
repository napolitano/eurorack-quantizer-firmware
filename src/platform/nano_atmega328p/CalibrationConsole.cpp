/**
 * @file CalibrationConsole.cpp
 * Implements the dedicated serial calibration/maintenance console.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/nano_atmega328p/CalibrationConsole.h"

#include <Arduino.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "fmq/config/AnalogConfig.h"
#include "fmq/config/MaintenanceConfig.h"
#include "fmq/domain/CalibrationProfile.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/SelfCalibration.h"
#include "fmq/persistence/PersistenceLayout.h"

namespace fmq::platform::nano {
namespace {

constexpr uint8_t kSelfFitPointCount = 5u;
constexpr uint8_t kSelfVerifyPointCount = 4u;
constexpr uint16_t kSelfFitDacCodes[kSelfFitPointCount] = {
    384u, 1024u, 2048u, 3072u, 3712u};
constexpr uint16_t kSelfVerifyDacCodes[kSelfVerifyPointCount] = {
    640u, 1536u, 2560u, 3456u};
constexpr uint16_t kMeasureSamples = 64u;
constexpr uint16_t kNoiseSamples = 1024u;
constexpr uint16_t kAdcWizardSamples = 128u;
constexpr uint16_t kDacSweepStep = 256u;
constexpr uint16_t kAdcWizardMinimumSpan = 700u;

Mcp4922Dac::Channel dacChannel(uint8_t channel) {
  return channel == 0u ? Mcp4922Dac::Channel::A : Mcp4922Dac::Channel::B;
}

uint16_t absoluteDifference(uint16_t a, uint16_t b) {
  return a >= b ? static_cast<uint16_t>(a - b)
                : static_cast<uint16_t>(b - a);
}

uint8_t countMarkers(const IEeprom &eeprom, uint16_t base, uint16_t size,
                     uint8_t count) {
  uint8_t found = 0u;
  for (uint8_t i = 0u; i < count; ++i) {
    const uint16_t address = static_cast<uint16_t>(
        base + static_cast<uint16_t>(i) * size);
    if (eeprom.readByte(address) == kRecordMarker) ++found;
  }
  return found;
}

}  // namespace

[[noreturn]] void CalibrationConsole::run() {
  delay(config::kMaintenanceConsoleStartupDelayMs);
  printBanner();
  printCalibration();
  Serial.println(F("Type 'help' for commands."));

  for (;;) {
    servicePersistence();
    if (adc_.sampleReady()) adc_.beginCycle();

    while (Serial.available() > 0) {
      const char input = static_cast<char>(Serial.read());
      if (input == '\r') continue;
      if (input == '\n') {
        command_[commandLength_] = '\0';
        if (commandLength_ != 0u) handleLine(command_);
        commandLength_ = 0u;
        continue;
      }
      if (commandLength_ + 1u < kCommandBufferBytes) {
        command_[commandLength_++] = input;
      }
    }
  }
}

void CalibrationConsole::printBanner() const {
  Serial.println();
  Serial.println(F("FM Quantizer Calibration & Maintenance"));
  Serial.println(F("target: 0.2.2 (Unreleased) / Arduino Nano ATmega328P"));
  Serial.println(F("production firmware is not running in this image"));
}

void CalibrationConsole::printHelp() const {
  Serial.println(F("Commands:"));
  Serial.println(F("  version | help"));
  Serial.println(F("  cal show | cal load | cal save | cal defaults"));
  Serial.println(F("  cal set adc|dac a|b OFFSET NUM DEN"));
  Serial.println(F("  cal adc a|b          guided 0V/10V ADC calibration"));
  Serial.println(F("  cal self a|b         verified OUT->IN DAC selfcal"));
  Serial.println(F("  cal verify a|b       verify active DAC calibration"));
  Serial.println(F("  measure adc a|b      RAW/CAL statistics"));
  Serial.println(F("  diag noise a|b       1024-sample ADC stability"));
  Serial.println(F("  diag sweep a|b       RAW DAC loopback CSV sweep"));
  Serial.println(F("  eeprom status        partition/journal overview"));
  Serial.println(F("  eeprom backup        read-only 1024-byte hex dump"));
  Serial.println(F("No command can restore/erase presets or write outside calibration."));
}

bool CalibrationConsole::parseChannel(const char *text, uint8_t &channel) {
  if (text == nullptr || text[0] == '\0' || text[1] != '\0') return false;
  if (text[0] == 'a' || text[0] == 'A') {
    channel = 0u;
    return true;
  }
  if (text[0] == 'b' || text[0] == 'B') {
    channel = 1u;
    return true;
  }
  return false;
}

bool CalibrationConsole::parseSigned(const char *text, int32_t &value) {
  if (text == nullptr || *text == '\0') return false;
  char *end = nullptr;
  const long parsed = strtol(text, &end, 10);
  if (end == text || *end != '\0' || parsed < INT16_MIN || parsed > INT16_MAX) {
    return false;
  }
  value = static_cast<int32_t>(parsed);
  return true;
}

bool CalibrationConsole::parseUnsigned16(const char *text, uint16_t &value,
                                         bool allowZero) {
  if (text == nullptr || *text == '\0' || *text == '-') return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (end == text || *end != '\0' || parsed > 65535ul ||
      (!allowZero && parsed == 0ul)) {
    return false;
  }
  value = static_cast<uint16_t>(parsed);
  return true;
}

uint16_t CalibrationConsole::gcd16(uint16_t a, uint16_t b) {
  while (b != 0u) {
    const uint16_t r = static_cast<uint16_t>(a % b);
    a = b;
    b = r;
  }
  return a == 0u ? 1u : a;
}

void CalibrationConsole::handleLine(char *line) {
  char *verb = strtok(line, " \t");
  if (verb == nullptr) return;

  if (strcmp(verb, "help") == 0 || strcmp(verb, "?") == 0) {
    printHelp();
    return;
  }
  if (strcmp(verb, "version") == 0) {
    printBanner();
    return;
  }

  if (strcmp(verb, "cal") == 0) {
    char *action = strtok(nullptr, " \t");
    if (action == nullptr) {
      printCalibration();
      return;
    }
    if (strcmp(action, "show") == 0) {
      printCalibration();
      return;
    }
    if (strcmp(action, "load") == 0) {
      loadCalibration();
      return;
    }
    if (strcmp(action, "save") == 0) {
      saveCalibration();
      return;
    }
    if (strcmp(action, "defaults") == 0) {
      resetActiveCalibrationProfile();
      Serial.println(F("Factory profile active in RAM; use 'cal save' to persist."));
      printCalibration();
      return;
    }
    if (strcmp(action, "adc") == 0 || strcmp(action, "self") == 0 ||
        strcmp(action, "verify") == 0) {
      uint8_t channel = 0u;
      if (!parseChannel(strtok(nullptr, " \t"), channel) ||
          strtok(nullptr, " \t") != nullptr) {
        Serial.println(F("Usage: cal adc|self|verify a|b"));
        return;
      }
      if (strcmp(action, "adc") == 0) runAdcWizard(channel);
      else if (strcmp(action, "self") == 0) runSelfCalibration(channel);
      else {
        uint16_t residual = 0u;
        const bool ok = verifyDacCalibration(channel, residual);
        Serial.print(ok ? F("PASS max residual=") : F("FAIL max residual="));
        Serial.print(residual);
        Serial.println(F(" DAC codes"));
      }
      return;
    }
    if (strcmp(action, "set") == 0) {
      const char *path = strtok(nullptr, " \t");
      uint8_t channel = 0u;
      int32_t offset = 0;
      uint16_t numerator = 0u;
      uint16_t denominator = 0u;
      if (path == nullptr ||
          (strcmp(path, "adc") != 0 && strcmp(path, "dac") != 0) ||
          !parseChannel(strtok(nullptr, " \t"), channel) ||
          !parseSigned(strtok(nullptr, " \t"), offset) ||
          !parseUnsigned16(strtok(nullptr, " \t"), numerator) ||
          !parseUnsigned16(strtok(nullptr, " \t"), denominator) ||
          strtok(nullptr, " \t") != nullptr) {
        Serial.println(F("Usage: cal set adc|dac a|b OFFSET NUM DEN"));
        return;
      }
      CalibrationProfile profile = activeCalibrationProfile();
      LinearCalibration &target = strcmp(path, "adc") == 0
                                      ? profile.adc[channel]
                                      : profile.dac[channel];
      target = {offset, numerator, denominator};
      if (!setActiveCalibrationProfile(profile)) {
        Serial.println(F("Rejected calibration."));
        return;
      }
      Serial.println(F("Changed in RAM; use 'cal save' after verification."));
      printCalibration();
      return;
    }
  }

  if (strcmp(verb, "measure") == 0) {
    const char *what = strtok(nullptr, " \t");
    uint8_t channel = 0u;
    if (what != nullptr && strcmp(what, "adc") == 0 &&
        parseChannel(strtok(nullptr, " \t"), channel) &&
        strtok(nullptr, " \t") == nullptr) {
      measureAdc(channel);
      return;
    }
  }

  if (strcmp(verb, "diag") == 0) {
    const char *action = strtok(nullptr, " \t");
    uint8_t channel = 0u;
    if (action != nullptr && parseChannel(strtok(nullptr, " \t"), channel) &&
        strtok(nullptr, " \t") == nullptr) {
      if (strcmp(action, "noise") == 0) {
        diagnoseNoise(channel);
        return;
      }
      if (strcmp(action, "sweep") == 0) {
        diagnoseSweep(channel);
        return;
      }
    }
  }

  if (strcmp(verb, "eeprom") == 0) {
    const char *action = strtok(nullptr, " \t");
    if (action != nullptr && strtok(nullptr, " \t") == nullptr) {
      if (strcmp(action, "status") == 0) {
        printEepromStatus();
        return;
      }
      if (strcmp(action, "backup") == 0) {
        printEepromBackup();
        return;
      }
    }
  }

  Serial.println(F("Unknown/invalid command. Type 'help'."));
}

void CalibrationConsole::printCalibrationLine(
    const __FlashStringHelper *label,
    const LinearCalibration &calibration) const {
  Serial.print(label);
  Serial.print(F(" offset="));
  Serial.print(calibration.offset);
  Serial.print(F(" gain="));
  Serial.print(calibration.gainNumerator);
  Serial.print('/');
  Serial.println(calibration.gainDenominator);
}

void CalibrationConsole::printCalibration() const {
  const CalibrationProfile &profile = activeCalibrationProfile();
  Serial.println(F("Active calibration:"));
  printCalibrationLine(F("  ADC A"), profile.adc[0]);
  printCalibrationLine(F("  ADC B"), profile.adc[1]);
  printCalibrationLine(F("  DAC A"), profile.dac[0]);
  printCalibrationLine(F("  DAC B"), profile.dac[1]);

  const CalibrationJournalStatus status = calibrationStore_.inspect();
  Serial.print(F("Journal: record0="));
  Serial.print(status.records[0].valid ? F("valid/g") : F("invalid/g"));
  Serial.print(status.records[0].generation);
  Serial.print(F(" record1="));
  Serial.print(status.records[1].valid ? F("valid/g") : F("invalid/g"));
  Serial.print(status.records[1].generation);
  Serial.print(F(" active="));
  if (status.activeIndex < 0) Serial.println(F("factory fallback"));
  else Serial.println(static_cast<int>(status.activeIndex));
}

void CalibrationConsole::servicePersistence() {
  calibrationWriter_.service();
  calibrationStore_.observeWriter();
}

bool CalibrationConsole::waitForPersistence() {
  const uint32_t start = millis();
  while (calibrationWriter_.busy()) {
    servicePersistence();
    if (millis() - start >= config::kMaintenancePersistenceTimeoutMs) {
      return false;
    }
  }
  calibrationStore_.observeWriter();
  return true;
}

void CalibrationConsole::saveCalibration() {
  if (!waitForPersistence()) {
    Serial.println(F("FAIL: calibration EEPROM busy timeout."));
    return;
  }
  if (!calibrationStore_.save(activeCalibrationProfile()) ||
      !waitForPersistence()) {
    Serial.println(F("FAIL: calibration was not persisted."));
    return;
  }
  Serial.println(F("Calibration stored atomically."));
}

void CalibrationConsole::loadCalibration() {
  if (!waitForPersistence()) {
    Serial.println(F("FAIL: calibration EEPROM busy timeout."));
    return;
  }
  CalibrationProfile profile;
  if (!calibrationStore_.load(profile) || !setActiveCalibrationProfile(profile)) {
    Serial.println(F("No valid stored calibration; active profile unchanged."));
    return;
  }
  Serial.println(F("Stored calibration loaded."));
  printCalibration();
}

bool CalibrationConsole::captureAdcStats(uint8_t channel, uint16_t count,
                                         bool calibrated, AdcStats &stats) {
  if (channel > 1u || count == 0u) return false;
  uint32_t sum = 0u;
  uint16_t minimum = 0xFFFFu;
  uint16_t maximum = 0u;
  uint16_t samples = 0u;
  const uint32_t start = millis();
  const uint32_t timeout = static_cast<uint32_t>(count) * 4u + 250u;

  while (samples < count) {
    servicePersistence();
    if (adc_.sampleReady()) {
      adc_.beginCycle();
      uint16_t value = adc_.read(static_cast<uint8_t>(channel + 1u));
      if (calibrated) value = calibratedAdcCode(value, channel);
      if (value < minimum) minimum = value;
      if (value > maximum) maximum = value;
      sum += value;
      ++samples;
    }
    if (millis() - start >= timeout) return false;
  }

  stats.minimum = minimum;
  stats.maximum = maximum;
  stats.mean = static_cast<uint16_t>((sum + count / 2u) / count);
  return true;
}

void CalibrationConsole::measureAdc(uint8_t channel) {
  AdcStats raw;
  AdcStats calibrated;
  if (!captureAdcStats(channel, kMeasureSamples, false, raw) ||
      !captureAdcStats(channel, kMeasureSamples, true, calibrated)) {
    Serial.println(F("ADC measurement timeout."));
    return;
  }
  Serial.print(F("ADC "));
  Serial.print(channel == 0u ? 'A' : 'B');
  Serial.print(F(" RAW min/mean/max="));
  Serial.print(raw.minimum); Serial.print('/'); Serial.print(raw.mean);
  Serial.print('/'); Serial.print(raw.maximum);
  Serial.print(F(" span=")); Serial.println(raw.maximum - raw.minimum);
  Serial.print(F("CAL min/mean/max="));
  Serial.print(calibrated.minimum); Serial.print('/'); Serial.print(calibrated.mean);
  Serial.print('/'); Serial.print(calibrated.maximum);
  Serial.print(F(" span=")); Serial.println(calibrated.maximum - calibrated.minimum);
}

void CalibrationConsole::diagnoseNoise(uint8_t channel) {
  AdcStats raw;
  if (!captureAdcStats(channel, kNoiseSamples, false, raw)) {
    Serial.println(F("ADC noise measurement timeout."));
    return;
  }
  Serial.print(F("ADC noise "));
  Serial.print(channel == 0u ? 'A' : 'B');
  Serial.print(F(": samples=1024 min=")); Serial.print(raw.minimum);
  Serial.print(F(" mean=")); Serial.print(raw.mean);
  Serial.print(F(" max=")); Serial.print(raw.maximum);
  Serial.print(F(" p-p=")); Serial.println(raw.maximum - raw.minimum);
}

bool CalibrationConsole::waitForEnter(const __FlashStringHelper *prompt) {
  Serial.println(prompt);
  while (Serial.available() > 0) (void)Serial.read();
  const uint32_t start = millis();
  for (;;) {
    servicePersistence();
    if (adc_.sampleReady()) adc_.beginCycle();
    if (Serial.available() > 0) {
      while (Serial.available() > 0) {
        if (static_cast<char>(Serial.read()) == '\n') return true;
      }
    }
    if (millis() - start > 120000ul) {
      Serial.println(F("Wizard timeout."));
      return false;
    }
  }
}

void CalibrationConsole::runAdcWizard(uint8_t channel) {
  Serial.print(F("ADC "));
  Serial.print(channel == 0u ? 'A' : 'B');
  Serial.println(F(" two-point calibration (RAM only until 'cal save')."));
  Serial.println(F("Use a trustworthy external voltage reference."));

  if (!waitForEnter(F("Connect exactly 0.000 V to CV IN, then press ENTER."))) return;
  AdcStats zero;
  if (!captureAdcStats(channel, kAdcWizardSamples, false, zero)) {
    Serial.println(F("FAIL: 0 V measurement timeout."));
    return;
  }

  if (!waitForEnter(F("Connect exactly 10.000 V to CV IN, then press ENTER."))) return;
  AdcStats full;
  if (!captureAdcStats(channel, kAdcWizardSamples, false, full)) {
    Serial.println(F("FAIL: 10 V measurement timeout."));
    return;
  }

  if (full.mean <= zero.mean ||
      static_cast<uint16_t>(full.mean - zero.mean) < kAdcWizardMinimumSpan) {
    Serial.println(F("FAIL: ADC span is implausible; profile unchanged."));
    return;
  }

  const uint16_t span = static_cast<uint16_t>(full.mean - zero.mean);
  const uint16_t divisor = gcd16(config::kAdcMaximumCode, span);
  LinearCalibration calibration;
  calibration.offset = -static_cast<int32_t>(zero.mean);
  calibration.gainNumerator = static_cast<uint16_t>(config::kAdcMaximumCode / divisor);
  calibration.gainDenominator = static_cast<uint16_t>(span / divisor);

  const uint16_t correctedZero = applyAdcCalibration(zero.mean, calibration);
  const uint16_t correctedFull = applyAdcCalibration(full.mean, calibration);
  if (correctedZero > 2u ||
      absoluteDifference(correctedFull, config::kAdcMaximumCode) > 2u) {
    Serial.println(F("FAIL: fitted ADC correction did not verify endpoints."));
    return;
  }

  CalibrationProfile candidate = activeCalibrationProfile();
  candidate.adc[channel] = calibration;
  if (!setActiveCalibrationProfile(candidate)) {
    Serial.println(F("FAIL: fitted ADC correction rejected."));
    return;
  }

  Serial.print(F("0V raw mean/span=")); Serial.print(zero.mean); Serial.print('/');
  Serial.println(zero.maximum - zero.minimum);
  Serial.print(F("10V raw mean/span=")); Serial.print(full.mean); Serial.print('/');
  Serial.println(full.maximum - full.minimum);
  Serial.println(F("ADC fit PASS; active in RAM. Verify intermediate voltages before saving."));
  printCalibrationLine(channel == 0u ? F("ADC A") : F("ADC B"), calibration);
}

void CalibrationConsole::zeroSelectedOutput(uint8_t channel) {
  dac_.write(dacChannel(channel), calibratedDacCode(0, channel));
}

bool CalibrationConsole::verifyDacCalibration(uint8_t channel,
                                              uint16_t &maxResidual) {
  maxResidual = 0u;
  for (uint8_t i = 0u; i < kSelfVerifyPointCount; ++i) {
    const uint16_t nominal = kSelfVerifyDacCodes[i];
    dac_.write(dacChannel(channel), calibratedDacCode(nominal, channel));
    delay(config::kSelfCalibrationSettleMs);
    AdcStats measuredStats;
    if (!captureAdcStats(channel, config::kSelfCalibrationAverageSamples,
                         true, measuredStats)) {
      zeroSelectedOutput(channel);
      return false;
    }
    const uint16_t measured = adcCodeToEquivalentDac(measuredStats.mean);
    const uint16_t residual = absoluteDifference(measured, nominal);
    if (residual > maxResidual) maxResidual = residual;
    Serial.print(nominal); Serial.print(','); Serial.print(measured);
    Serial.print(','); Serial.println(residual);
    if (residual > config::kSelfCalibrationMaxVerifyResidualDacCodes) {
      zeroSelectedOutput(channel);
      return false;
    }
  }
  zeroSelectedOutput(channel);
  return true;
}

void CalibrationConsole::runSelfCalibration(uint8_t channel) {
  if (!waitForPersistence()) {
    Serial.println(F("Selfcal aborted: calibration EEPROM busy."));
    return;
  }

  Serial.print(F("Patch CV OUT "));
  Serial.print(channel == 0u ? 'A' : 'B');
  Serial.print(F(" -> CV IN "));
  Serial.println(channel == 0u ? 'A' : 'B');
  Serial.println(F("Selfcal uses the active ADC calibration as reference."));

  const CalibrationProfile previous = activeCalibrationProfile();
  SelfCalibrationSample samples[kSelfFitPointCount];
  for (uint8_t i = 0u; i < kSelfFitPointCount; ++i) {
    const uint16_t rawDac = kSelfFitDacCodes[i];
    dac_.write(dacChannel(channel), rawDac);
    delay(config::kSelfCalibrationSettleMs);
    AdcStats stats;
    if (!captureAdcStats(channel, config::kSelfCalibrationAverageSamples,
                         true, stats)) {
      zeroSelectedOutput(channel);
      Serial.println(F("Selfcal FAIL: ADC timeout; calibration unchanged."));
      return;
    }
    samples[i] = {rawDac, stats.mean};
  }

  const SelfCalibrationResult fit =
      solveDacSelfCalibration(samples, kSelfFitPointCount);
  if (fit.status != SelfCalibrationStatus::Ok ||
      fit.maxFitResidualDacCodes > config::kSelfCalibrationMaxFitResidualDacCodes) {
    zeroSelectedOutput(channel);
    Serial.print(F("Selfcal FAIL: fit status="));
    Serial.print(static_cast<uint8_t>(fit.status));
    Serial.print(F(" residual="));
    Serial.println(fit.maxFitResidualDacCodes);
    return;
  }

  CalibrationProfile candidate = previous;
  candidate.dac[channel] = fit.calibration;
  if (!setActiveCalibrationProfile(candidate)) {
    zeroSelectedOutput(channel);
    Serial.println(F("Selfcal FAIL: fitted profile rejected."));
    return;
  }

  Serial.println(F("verify_nominal,measured,residual"));
  uint16_t maxResidual = 0u;
  if (!verifyDacCalibration(channel, maxResidual)) {
    (void)setActiveCalibrationProfile(previous);
    zeroSelectedOutput(channel);
    Serial.println(F("Selfcal FAIL: verification; previous calibration restored."));
    return;
  }

  if (!calibrationStore_.save(activeCalibrationProfile()) ||
      !waitForPersistence()) {
    (void)setActiveCalibrationProfile(previous);
    zeroSelectedOutput(channel);
    Serial.println(F("Selfcal FAIL: persistence; previous calibration restored."));
    return;
  }

  Serial.print(F("Selfcal PASS; max residual="));
  Serial.print(maxResidual);
  Serial.println(F(" DAC codes; calibration stored."));
  printCalibration();
}

void CalibrationConsole::diagnoseSweep(uint8_t channel) {
  Serial.println(F("raw_dac,adc_raw,adc_cal,equivalent_dac"));
  uint16_t code = 0u;
  for (;;) {
    dac_.write(dacChannel(channel), code);
    delay(config::kSelfCalibrationSettleMs);
    AdcStats raw;
    if (!captureAdcStats(channel, config::kSelfCalibrationAverageSamples,
                         false, raw)) {
      Serial.println(F("# sweep aborted: ADC timeout"));
      break;
    }
    const uint16_t cal = calibratedAdcCode(raw.mean, channel);
    Serial.print(code); Serial.print(','); Serial.print(raw.mean); Serial.print(',');
    Serial.print(cal); Serial.print(','); Serial.println(adcCodeToEquivalentDac(cal));
    if (code == config::kDacMaximumCode) break;
    const uint16_t remaining = static_cast<uint16_t>(config::kDacMaximumCode - code);
    code = remaining <= kDacSweepStep
               ? config::kDacMaximumCode
               : static_cast<uint16_t>(code + kDacSweepStep);
  }
  zeroSelectedOutput(channel);
}

void CalibrationConsole::printEepromStatus() const {
  Serial.print(F("EEPROM bytes: ")); Serial.println(fullEeprom_.capacity());
  Serial.print(F("scale presets 0.."));
  Serial.print(kScaleRegionBase + kScaleRegionSize - 1u);
  Serial.print(F(" committed markers="));
  Serial.println(countMarkers(fullEeprom_, kScaleRegionBase, kScaleSlotSize,
                              kScaleSlotCount));
  Serial.print(F("config presets "));
  Serial.print(kConfigRegionBase); Serial.print(F(".."));
  Serial.print(kConfigRegionBase + kConfigRegionSize - 1u);
  Serial.print(F(" committed markers="));
  Serial.println(countMarkers(fullEeprom_, kConfigRegionBase, kConfigSlotSize,
                              kConfigSlotCount));
  Serial.print(F("live ring ")); Serial.print(kLiveRingBase); Serial.print(F(".."));
  Serial.print(kLegacyExtraLiveSlotAddress - 1u);
  Serial.print(F(" committed markers="));
  Serial.println(countMarkers(fullEeprom_, kLiveRingBase, kLiveSlotSize,
                              static_cast<uint8_t>(kLiveSlotCount)));
  Serial.print(F("legacy slot marker @966=0x"));
  Serial.println(fullEeprom_.readByte(kLegacyExtraLiveSlotAddress), HEX);
  Serial.print(F("calibration 967..1022; reserved @1023=0x"));
  Serial.println(fullEeprom_.readByte(kReservedEepromTailAddress), HEX);
  printCalibration();
}

void CalibrationConsole::printEepromBackup() const {
  Serial.println(F("BEGIN EEPROM BACKUP v1 (read-only)"));
  for (uint16_t address = 0u; address < fullEeprom_.capacity(); address += 16u) {
    if (address < 0x100u) Serial.print('0');
    if (address < 0x010u) Serial.print('0');
    Serial.print(address, HEX);
    Serial.print(':');
    for (uint8_t offset = 0u; offset < 16u; ++offset) {
      const uint16_t current = static_cast<uint16_t>(address + offset);
      if (current >= fullEeprom_.capacity()) break;
      const uint8_t value = fullEeprom_.readByte(current);
      Serial.print(' ');
      if (value < 0x10u) Serial.print('0');
      Serial.print(value, HEX);
    }
    Serial.println();
  }
  Serial.println(F("END EEPROM BACKUP"));
}

}  // namespace fmq::platform::nano
