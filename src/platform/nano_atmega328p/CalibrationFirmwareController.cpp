/**
 * @file CalibrationFirmwareController.cpp
 * Wires the panel-driven calibration/maintenance image to Nano hardware.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <Arduino.h>
#include <SPI.h>
#include <avr/interrupt.h>

#include "platform/nano_atmega328p/CalibrationFirmwareController.h"

#include "fmq/config/MaintenanceConfig.h"
#include "fmq/domain/CalibrationProfile.h"
#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/CalibrationQualificationStore.h"
#include "fmq/persistence/CalibrationStore.h"
#include "fmq/persistence/EepromPartition.h"
#include "fmq/persistence/LiveStateStore.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "platform/nano_atmega328p/AvrAnalogInputs.h"
#include "platform/nano_atmega328p/AvrDigitalInput.h"
#include "platform/nano_atmega328p/AvrDigitalOutput.h"
#include "platform/nano_atmega328p/AvrEeprom.h"
#include "platform/nano_atmega328p/AvrSpiBus.h"
#include "platform/nano_atmega328p/BoardConfig.h"
#include "platform/nano_atmega328p/CalibrationPanelController.h"
#include "platform/nano_atmega328p/MaintenanceFeedback.h"
#include "platform/nano_atmega328p/Mcp4922Dac.h"
#include "platform/nano_atmega328p/Tlc5947LedDriver.h"

namespace fmq::platform::nano {
namespace {

using namespace fmq::hwconfig;

AvrEeprom g_eeprom;
AsyncEepromWriter g_auxWriter(g_eeprom);
LiveStateStore g_liveStore(g_eeprom, g_auxWriter);
CalibrationQualificationStore g_qualificationStore(g_eeprom, g_auxWriter);

EepromPartition g_calibrationEeprom(
    g_eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
AsyncEepromWriter g_calibrationWriter(g_calibrationEeprom);
CalibrationStore g_calibrationStore(g_calibrationEeprom, g_calibrationWriter);

AvrSpiBus g_spi;
AvrAnalogInputs g_analog(kPinButtonLadder, kPinCvInputA, kPinCvInputB);
AvrDigitalInput g_shift(kPinShift, kButtonsUsePullups);
AvrDigitalInput g_save(kPinSave, kButtonsUsePullups);
AvrDigitalInput g_load(kPinLoad, kButtonsUsePullups);
AvrDigitalOutput g_dacCs(kPinDacChipSelect, /*initialHigh=*/true);
AvrDigitalOutput g_ledCs(kPinLedChipSelect, /*initialHigh=*/true);
AvrDigitalOutput g_ledBlank(kPinLedBlank, /*initialHigh=*/true);
AvrDigitalOutput g_trigOutA(kPinTrigOutputA, /*initialHigh=*/false);
AvrDigitalOutput g_trigOutB(kPinTrigOutputB, /*initialHigh=*/false);
AvrDigitalOutput g_inputLedA(kPinInputLedA, /*initialHigh=*/!kStatusLedsActiveHigh);
AvrDigitalOutput g_outputLedA(kPinOutputLedA, /*initialHigh=*/!kStatusLedsActiveHigh);
AvrDigitalOutput g_inputLedB(kPinInputLedB, /*initialHigh=*/!kStatusLedsActiveHigh);
AvrDigitalOutput g_outputLedB(kPinOutputLedB, /*initialHigh=*/!kStatusLedsActiveHigh);
Mcp4922Dac g_dac(g_spi, g_dacCs);
Tlc5947LedDriver g_ledDriver(g_spi, g_ledCs, g_ledBlank);
MaintenanceFeedback g_feedback(g_ledDriver, g_inputLedA, g_outputLedA,
                               g_inputLedB, g_outputLedB,
                               kStatusLedsActiveHigh);

void printLinear(const __FlashStringHelper *label,
                 const LinearCalibration &calibration) {
  Serial.print(label);
  Serial.print(F(": offset="));
  Serial.print(calibration.offset);
  Serial.print(F(" gain="));
  Serial.print(calibration.gainNumerator);
  Serial.print('/');
  Serial.println(calibration.gainDenominator);
}

void printStoredQualificationReport() {
  delay(config::kMaintenanceReportStartupDelayMs);
  Serial.println();
  Serial.println(F("FM QUANTIZER STORED CALIBRATION REPORT"));
  Serial.println(F("Firmware target: 0.2.2 (Unreleased)"));

  CalibrationProfile stored;
  if (!g_calibrationStore.load(stored)) {
    Serial.println(F("Calibration profile: NOT STORED"));
    Serial.println(F("Qualification: NOT AVAILABLE"));
    Serial.println(F("No calibration label may be applied."));
    return;
  }

  const uint8_t generation = g_calibrationStore.activeGeneration();
  const CalibrationQualification qualification =
      g_qualificationStore.load(generation);
  Serial.print(F("Calibration format: "));
  Serial.println(kCalibrationFormatVersion);
  Serial.print(F("Generation: "));
  Serial.println(generation);
  printLinear(F("ADC A"), stored.adc[0]);
  printLinear(F("ADC B"), stored.adc[1]);
  printLinear(F("DAC A"), stored.dac[0]);
  printLinear(F("DAC B"), stored.dac[1]);

  if (!qualification.valid) {
    Serial.println(F("Qualification: NOT VERIFIED FOR THIS GENERATION"));
    Serial.println(F("No calibration label may be applied."));
    return;
  }

  Serial.print(F("DAC A verification: PASS / max residual "));
  Serial.print(qualification.maxResidualA);
  Serial.println(F(" codes"));
  Serial.print(F("DAC B verification: PASS / max residual "));
  Serial.print(qualification.maxResidualB);
  Serial.println(F(" codes"));
  Serial.println(F("OVERALL: PASS"));
  Serial.println(F("----- LABEL -----"));
  Serial.println(F("CALIBRATION VERIFIED"));
  Serial.println(F("ADC A/B: STORED PROFILE"));
  Serial.print(F("DAC A: PASS / max "));
  Serial.print(qualification.maxResidualA);
  Serial.println(F(" codes"));
  Serial.print(F("DAC B: PASS / max "));
  Serial.print(qualification.maxResidualB);
  Serial.println(F(" codes"));
  Serial.print(F("CAL fmt "));
  Serial.print(kCalibrationFormatVersion);
  Serial.print(F(" / gen "));
  Serial.println(generation);
  Serial.println(F("-----------------"));
  Serial.println(F("Date / serial number / tester are host or label metadata."));
}

}  // namespace

ISR(ADC_vect) { AvrAnalogInputs::handleInterrupt(); }

void CalibrationFirmwareController::begin() {
  Serial.begin(config::kMaintenanceBaud);

  g_dacCs.begin();
  g_ledCs.begin();
  g_ledBlank.begin();
  g_trigOutA.begin();
  g_trigOutB.begin();
  g_inputLedA.begin();
  g_outputLedA.begin();
  g_inputLedB.begin();
  g_outputLedB.begin();
  g_shift.begin();
  g_save.begin();
  g_load.begin();
  g_trigOutA.set(!kTriggerOutputsActiveHigh);
  g_trigOutB.set(!kTriggerOutputsActiveHigh);
  g_inputLedA.set(!kStatusLedsActiveHigh);
  g_outputLedA.set(!kStatusLedsActiveHigh);
  g_inputLedB.set(!kStatusLedsActiveHigh);
  g_outputLedB.set(!kStatusLedsActiveHigh);
  g_ledBlank.set(true);

  analogReference(kUseExternalAref ? EXTERNAL : DEFAULT);
  g_spi.begin();
  g_feedback.begin();
  g_dac.write(Mcp4922Dac::Channel::A, 0u);
  g_dac.write(Mcp4922Dac::Channel::B, 0u);

  // Preserve a newest <=0.2.1 live record before byte 966 can be reused as
  // qualification metadata. The legacy record remains untouched until the
  // migration commit has physically completed.
  LiveState ignored;
  (void)g_liveStore.load(ignored);
  while (g_auxWriter.busy()) g_auxWriter.service();
  g_liveStore.observeWriter();

  CalibrationProfile stored;
  if (g_calibrationStore.load(stored)) {
    (void)setActiveCalibrationProfile(stored);
  } else {
    resetActiveCalibrationProfile();
  }

  g_analog.begin(kUseExternalAref);
  g_feedback.playBootIndication();
  printStoredQualificationReport();
}

[[noreturn]] void CalibrationFirmwareController::run() {
  CalibrationPanelController panel(
      g_analog, g_dac, g_calibrationStore, g_calibrationWriter,
      g_qualificationStore, g_shift, g_save, g_load, g_feedback,
      kButtonsActiveLow);
  panel.run();
}

}  // namespace fmq::platform::nano
