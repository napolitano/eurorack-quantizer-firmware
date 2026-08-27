/**
 * @file CalibrationFirmwareController.cpp
 * Wires the dedicated calibration/maintenance image to Nano hardware.
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
#include "fmq/persistence/CalibrationStore.h"
#include "fmq/persistence/EepromPartition.h"
#include "fmq/persistence/LiveStateStore.h"
#include "fmq/persistence/PersistenceLayout.h"
#include "platform/nano_atmega328p/AvrAnalogInputs.h"
#include "platform/nano_atmega328p/AvrDigitalOutput.h"
#include "platform/nano_atmega328p/AvrEeprom.h"
#include "platform/nano_atmega328p/AvrSpiBus.h"
#include "platform/nano_atmega328p/BoardConfig.h"
#include "platform/nano_atmega328p/CalibrationConsole.h"
#include "platform/nano_atmega328p/Mcp4922Dac.h"
#include "platform/nano_atmega328p/MaintenanceFeedback.h"
#include "platform/nano_atmega328p/Tlc5947LedDriver.h"

namespace fmq::platform::nano {
namespace {

using namespace fmq::hwconfig;

AvrEeprom g_eeprom;
AsyncEepromWriter g_migrationWriter(g_eeprom);
LiveStateStore g_liveStore(g_eeprom, g_migrationWriter);

EepromPartition g_calibrationEeprom(
    g_eeprom, kCalibrationRegionBase, kCalibrationRegionSize);
AsyncEepromWriter g_calibrationWriter(g_calibrationEeprom);
CalibrationStore g_calibrationStore(g_calibrationEeprom, g_calibrationWriter);

AvrSpiBus g_spi;
AvrAnalogInputs g_analog(kPinButtonLadder, kPinCvInputA, kPinCvInputB);
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
MaintenanceFeedback g_feedback(g_ledDriver, g_inputLedA, g_inputLedB,
                               kStatusLedsActiveHigh);

}  // namespace

ISR(ADC_vect) { AvrAnalogInputs::handleInterrupt(); }

void CalibrationFirmwareController::begin() {
  Serial.begin(config::kMaintenanceBaud);

  // Keep normal module outputs quiet while the maintenance image owns the MCU.
  g_dacCs.begin();
  g_ledCs.begin();
  g_ledBlank.begin();
  g_trigOutA.begin();
  g_trigOutB.begin();
  g_inputLedA.begin();
  g_outputLedA.begin();
  g_inputLedB.begin();
  g_outputLedB.begin();
  g_trigOutA.set(!kTriggerOutputsActiveHigh);
  g_trigOutB.set(!kTriggerOutputsActiveHigh);
  g_inputLedA.set(!kStatusLedsActiveHigh);
  g_outputLedA.set(!kStatusLedsActiveHigh);
  g_inputLedB.set(!kStatusLedsActiveHigh);
  g_outputLedB.set(!kStatusLedsActiveHigh);
  g_ledBlank.set(true);  // TLC5947 BLANK is active high.

  analogReference(kUseExternalAref ? EXTERNAL : DEFAULT);
  g_spi.begin();
  g_feedback.begin();
  g_dac.write(Mcp4922Dac::Channel::A, 0u);
  g_dac.write(Mcp4922Dac::Channel::B, 0u);

  // A direct upgrade from <=0.2.1 can still have the newest live-state record
  // in the former twelfth slot beginning at byte 966. Let the existing
  // LiveStateStore migrate that record before the overlapping calibration tail
  // is ever written. No preset/config write API is exposed after this step.
  LiveState ignored;
  (void)g_liveStore.load(ignored);
  while (g_migrationWriter.busy()) g_migrationWriter.service();
  g_liveStore.observeWriter();

  CalibrationProfile stored;
  if (g_calibrationStore.load(stored)) {
    (void)setActiveCalibrationProfile(stored);
  } else {
    resetActiveCalibrationProfile();
  }

  g_analog.begin(kUseExternalAref);
  g_feedback.playBootIndication();
}

[[noreturn]] void CalibrationFirmwareController::run() {
  CalibrationConsole console(g_analog, g_dac, g_calibrationStore,
                             g_calibrationWriter, g_eeprom, g_feedback);
  console.run();
}

}  // namespace fmq::platform::nano
