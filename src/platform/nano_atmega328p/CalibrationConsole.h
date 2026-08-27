/**
 * @file CalibrationConsole.h
 * Declares the dedicated serial calibration/maintenance console.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_CALIBRATION_CONSOLE_H
#define FMQ_PLATFORM_NANO_CALIBRATION_CONSOLE_H

#include <stdint.h>

class __FlashStringHelper;

#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/CalibrationStore.h"
#include "fmq/ports/AnalogInputs.h"
#include "fmq/ports/Eeprom.h"
#include "platform/nano_atmega328p/Mcp4922Dac.h"

namespace fmq::platform::nano {

/** Dedicated maintenance UI. It is never linked into production firmware. */
class CalibrationConsole {
 public:
  CalibrationConsole(IAnalogInputs &adc, Mcp4922Dac &dac,
                     CalibrationStore &calibrationStore,
                     AsyncEepromWriter &calibrationWriter,
                     const IEeprom &fullEeprom)
      : adc_(adc), dac_(dac), calibrationStore_(calibrationStore),
        calibrationWriter_(calibrationWriter), fullEeprom_(fullEeprom),
        commandLength_(0u) {
    command_[0] = '\0';
  }

  [[noreturn]] void run();

 private:
  struct AdcStats {
    uint16_t minimum = 0u;
    uint16_t maximum = 0u;
    uint16_t mean = 0u;
  };

  static constexpr uint8_t kCommandBufferBytes = 72u;
  void handleLine(char *line);
  void printBanner() const;
  void printHelp() const;
  void printCalibration() const;
  void printCalibrationLine(const __FlashStringHelper *label,
                            const LinearCalibration &calibration) const;
  void printEepromStatus() const;
  void printEepromBackup() const;

  bool waitForPersistence();
  void servicePersistence();
  void saveCalibration();
  void loadCalibration();

  bool captureAdcStats(uint8_t channel, uint16_t count, bool calibrated,
                       AdcStats &stats);
  void measureAdc(uint8_t channel);
  void diagnoseNoise(uint8_t channel);
  void diagnoseSweep(uint8_t channel);

  void runAdcWizard(uint8_t channel);
  bool waitForEnter(const __FlashStringHelper *prompt);

  void runSelfCalibration(uint8_t channel);
  bool verifyDacCalibration(uint8_t channel, uint16_t &maxResidual);
  void zeroSelectedOutput(uint8_t channel);

  static bool parseChannel(const char *text, uint8_t &channel);
  static bool parseSigned(const char *text, int32_t &value);
  static bool parseUnsigned16(const char *text, uint16_t &value,
                              bool allowZero = false);
  static uint16_t gcd16(uint16_t a, uint16_t b);

  IAnalogInputs &adc_;
  Mcp4922Dac &dac_;
  CalibrationStore &calibrationStore_;
  AsyncEepromWriter &calibrationWriter_;
  const IEeprom &fullEeprom_;
  char command_[kCommandBufferBytes];
  uint8_t commandLength_;
};

}  // namespace fmq::platform::nano

#endif
