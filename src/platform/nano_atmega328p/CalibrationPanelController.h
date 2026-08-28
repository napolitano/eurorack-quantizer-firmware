/**
 * @file CalibrationPanelController.h
 * Panel-driven calibration state machine for the dedicated maintenance image.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_CALIBRATION_PANEL_CONTROLLER_H
#define FMQ_PLATFORM_NANO_CALIBRATION_PANEL_CONTROLLER_H

#include <stdint.h>

#include "fmq/application/Button.h"
#include "fmq/config/UiConfig.h"
#include "fmq/domain/CalibrationProfile.h"
#include "fmq/persistence/AsyncEepromWriter.h"
#include "fmq/persistence/CalibrationQualificationStore.h"
#include "fmq/persistence/CalibrationStore.h"
#include "fmq/ports/AnalogInputs.h"
#include "fmq/ports/DigitalInput.h"
#include "platform/nano_atmega328p/MaintenanceFeedback.h"
#include "platform/nano_atmega328p/Mcp4922Dac.h"

namespace fmq::platform::nano {

/** Error codes rendered as one to six red note LEDs after a failed operation. */
enum class CalibrationPanelError : uint8_t {
  None = 0u,
  InputUnstable = 1u,
  AdcSpanInvalid = 2u,
  LoopbackMissingOrNonMonotonic = 3u,
  LoopbackRangeImplausible = 4u,
  VerificationResidualTooLarge = 5u,
  PersistenceOrProfileUnavailable = 6u,
};

/**
 * Blocking maintenance-mode UI that uses only the module front panel.
 *
 * No serial input is accepted. SAVE/LOAD/SHIFT select workflows and confirm or
 * cancel wizard steps. All new coefficients remain in RAM until the requested
 * workflow has passed its final verification.
 */
class CalibrationPanelController {
 public:
  CalibrationPanelController(
      IAnalogInputs &adc, Mcp4922Dac &dac, CalibrationStore &calibrationStore,
      AsyncEepromWriter &calibrationWriter,
      CalibrationQualificationStore &qualificationStore,
      IDigitalInput &shiftButton, IDigitalInput &saveButton,
      IDigitalInput &loadButton, MaintenanceFeedback &feedback,
      bool buttonsActiveLow)
      : adc_(adc), dac_(dac), calibrationStore_(calibrationStore),
        calibrationWriter_(calibrationWriter),
        qualificationStore_(qualificationStore), shiftButton_(shiftButton),
        saveButton_(saveButton), loadButton_(loadButton), feedback_(feedback),
        buttonsActiveLow_(buttonsActiveLow),
        saveDebouncer_(config::kDigitalDebounceMs),
        loadDebouncer_(config::kDigitalDebounceMs) {}

  [[noreturn]] void run();

 private:
  struct AdcStats {
    uint16_t minimum = 0u;
    uint16_t maximum = 0u;
    uint16_t mean = 0u;
  };

  enum class PromptKind : uint8_t { Voltage, Loop, DualLoop };

  bool pressed(const IDigitalInput &input) const;
  void service();
  bool waitForPersistence();
  void waitForButtonsReleased();
  bool waitForSaveOrCancel(PromptKind kind, uint8_t channel = 0u,
                           uint8_t noteIndex = 0u);
  void waitForErrorAcknowledge();

  bool captureAdcStats(uint8_t channel, uint16_t count, bool calibrated,
                       AdcStats &stats);
  CalibrationPanelError calibrateAdcChannel(uint8_t channel,
                                            CalibrationProfile &working);
  CalibrationPanelError selfCalibrateDacChannel(
      uint8_t channel, CalibrationProfile &working);
  CalibrationPanelError verifyDacChannel(uint8_t channel,
                                         uint16_t &maxResidual,
                                         bool updateFeedback);
  CalibrationPanelError verifyBoth(uint16_t &residualA, uint16_t &residualB);

  bool saveProfileOnly(const CalibrationProfile &profile);
  bool saveProfileWithQualification(const CalibrationProfile &profile,
                                    uint8_t residualA, uint8_t residualB);
  bool saveQualificationForCurrentGeneration(uint8_t residualA,
                                             uint8_t residualB);

  void runFullCalibration();
  void runAdcOnlyCalibration();
  void runDacOnlyCalibration();
  void runVerification();
  void failAndRestore(CalibrationPanelError error,
                      const CalibrationProfile &previous);
  void zeroOutputs();

  IAnalogInputs &adc_;
  Mcp4922Dac &dac_;
  CalibrationStore &calibrationStore_;
  AsyncEepromWriter &calibrationWriter_;
  CalibrationQualificationStore &qualificationStore_;
  IDigitalInput &shiftButton_;
  IDigitalInput &saveButton_;
  IDigitalInput &loadButton_;
  MaintenanceFeedback &feedback_;
  bool buttonsActiveLow_;
  ButtonDebouncer saveDebouncer_;
  ButtonDebouncer loadDebouncer_;
};

}  // namespace fmq::platform::nano

#endif
