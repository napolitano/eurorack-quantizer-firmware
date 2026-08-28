/**
 * @file CalibrationPanelController.cpp
 * Implements panel-driven ADC/DAC calibration and verification.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/nano_atmega328p/CalibrationPanelController.h"

#include <Arduino.h>

#include "fmq/config/AnalogConfig.h"
#include "fmq/config/MaintenanceConfig.h"
#include "fmq/domain/AdcCalibration.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/SelfCalibration.h"

namespace fmq::platform::nano {
namespace {

constexpr uint8_t kChannelA = 0u;
constexpr uint8_t kChannelB = 1u;
constexpr uint8_t kZeroVoltNoteIndex = 0u;  // C
constexpr uint8_t kNineVoltNoteIndex = 9u;  // A
constexpr uint8_t kSelfFitPointCount = 5u;
constexpr uint8_t kSelfVerifyPointCount = 4u;
constexpr uint16_t kSelfFitDacCodes[kSelfFitPointCount] = {
    384u, 1024u, 2048u, 3072u, 3712u};
constexpr uint16_t kSelfVerifyDacCodes[kSelfVerifyPointCount] = {
    640u, 1536u, 2560u, 3456u};

Mcp4922Dac::Channel dacChannel(uint8_t channel) {
  return channel == kChannelA ? Mcp4922Dac::Channel::A
                              : Mcp4922Dac::Channel::B;
}

uint16_t absoluteDifference(uint16_t a, uint16_t b) {
  return a >= b ? static_cast<uint16_t>(a - b)
                : static_cast<uint16_t>(b - a);
}

}  // namespace

bool CalibrationPanelController::pressed(const IDigitalInput &input) const {
  const bool high = input.isHigh();
  return buttonsActiveLow_ ? !high : high;
}

void CalibrationPanelController::service() {
  calibrationWriter_.service();
  calibrationStore_.observeWriter();
  qualificationStore_.service();
  if (adc_.sampleReady()) adc_.beginCycle();
}

bool CalibrationPanelController::waitForPersistence() {
  const uint32_t start = static_cast<uint32_t>(millis());
  while (calibrationWriter_.busy() || qualificationStore_.busy() ||
         calibrationStore_.busy()) {
    service();
    if (millis() - start > config::kMaintenancePersistenceTimeoutMs) {
      return false;
    }
  }
  calibrationStore_.observeWriter();
  return true;
}

void CalibrationPanelController::waitForButtonsReleased() {
  uint32_t stableSince = static_cast<uint32_t>(millis());
  for (;;) {
    service();
    const uint32_t now = static_cast<uint32_t>(millis());
    const bool save = pressed(saveButton_);
    const bool load = pressed(loadButton_);
    (void)saveDebouncer_.sample(save, now);
    (void)loadDebouncer_.sample(load, now);
    if (save || load) {
      stableSince = now;
    } else if (now - stableSince > config::kDigitalDebounceMs) {
      return;
    }
    delay(config::kMaintenanceUiPollMs);
  }
}

bool CalibrationPanelController::waitForSaveOrCancel(PromptKind kind,
                                                      uint8_t channel,
                                                      uint8_t noteIndex) {
  waitForButtonsReleased();
  const uint32_t period = static_cast<uint32_t>(config::kMaintenancePromptPulseMs) +
                          config::kMaintenancePromptGapMs;
  for (;;) {
    service();
    const uint32_t now = static_cast<uint32_t>(millis());
    const bool lit = (now % period) < config::kMaintenancePromptPulseMs;
    switch (kind) {
      case PromptKind::Voltage:
        feedback_.showVoltagePrompt(channel, noteIndex, lit);
        break;
      case PromptKind::Loop:
        feedback_.showLoopPrompt(channel, lit);
        break;
      case PromptKind::DualLoop:
        feedback_.showDualLoopPrompt(lit);
        break;
    }

    const ButtonState saveState =
        saveDebouncer_.sample(pressed(saveButton_), now);
    const ButtonState loadState =
        loadDebouncer_.sample(pressed(loadButton_), now);
    if (loadState == ButtonState::JustPressed) return false;
    if (saveState == ButtonState::JustPressed) return true;
    delay(config::kMaintenanceUiPollMs);
  }
}

void CalibrationPanelController::waitForErrorAcknowledge() {
  waitForButtonsReleased();
  for (;;) {
    service();
    const uint32_t now = static_cast<uint32_t>(millis());
    (void)saveDebouncer_.sample(pressed(saveButton_), now);
    if (loadDebouncer_.sample(pressed(loadButton_), now) ==
        ButtonState::JustPressed) {
      waitForButtonsReleased();
      feedback_.showIdle();
      return;
    }
    delay(config::kMaintenanceUiPollMs);
  }
}

bool CalibrationPanelController::captureAdcStats(uint8_t channel,
                                                 uint16_t count,
                                                 bool calibrated,
                                                 AdcStats &stats) {
  uint32_t sum = 0u;
  uint16_t minimum = 0xFFFFu;
  uint16_t maximum = 0u;
  uint16_t samples = 0u;
  const uint32_t start = static_cast<uint32_t>(millis());
  const uint32_t timeout = static_cast<uint32_t>(count) * 4u + 250u;

  while (samples < count) {
    calibrationWriter_.service();
    calibrationStore_.observeWriter();
    qualificationStore_.service();
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

CalibrationPanelError CalibrationPanelController::calibrateAdcChannel(
    uint8_t channel, CalibrationProfile &working) {
  if (!waitForSaveOrCancel(PromptKind::Voltage, channel,
                           kZeroVoltNoteIndex)) {
    return CalibrationPanelError::None;
  }
  feedback_.beginAdcMeasurement(channel);
  AdcStats zero;
  if (!captureAdcStats(channel, config::kMaintenanceAdcCaptureSamples, false,
                       zero) ||
      zero.maximum - zero.minimum >
          config::kMaintenanceAdcMaximumCaptureSpan) {
    return CalibrationPanelError::InputUnstable;
  }

  if (!waitForSaveOrCancel(PromptKind::Voltage, channel,
                           kNineVoltNoteIndex)) {
    return CalibrationPanelError::None;
  }
  feedback_.beginAdcMeasurement(channel);
  AdcStats nine;
  if (!captureAdcStats(channel, config::kMaintenanceAdcCaptureSamples, false,
                       nine) ||
      nine.maximum - nine.minimum >
          config::kMaintenanceAdcMaximumCaptureSpan) {
    return CalibrationPanelError::InputUnstable;
  }

  const AdcCalibrationResult fit =
      solveAdcTwoPointCalibration(zero.mean, nine.mean);
  if (fit.status != AdcCalibrationStatus::Ok) {
    return CalibrationPanelError::AdcSpanInvalid;
  }

  working.adc[channel] = fit.calibration;
  if (!setActiveCalibrationProfile(working)) {
    return CalibrationPanelError::PersistenceOrProfileUnavailable;
  }
  return CalibrationPanelError::None;
}

CalibrationPanelError CalibrationPanelController::verifyDacChannel(
    uint8_t channel, uint16_t &maxResidual, bool updateFeedback) {
  maxResidual = 0u;
  if (updateFeedback) feedback_.beginLoopMeasurement(channel);

  for (uint8_t i = 0u; i < kSelfVerifyPointCount; ++i) {
    const uint16_t nominal = kSelfVerifyDacCodes[i];
    dac_.write(dacChannel(channel), calibratedDacCode(nominal, channel));
    delay(config::kSelfCalibrationSettleMs);
    AdcStats measured;
    if (!captureAdcStats(channel, config::kSelfCalibrationAverageSamples,
                         true, measured) ||
        measured.maximum - measured.minimum >
            config::kMaintenanceAdcMaximumCaptureSpan) {
      zeroOutputs();
      return CalibrationPanelError::InputUnstable;
    }

    const uint16_t equivalent = adcCodeToEquivalentDac(measured.mean);
    const uint16_t residual = absoluteDifference(equivalent, nominal);
    if (residual > maxResidual) maxResidual = residual;
    if (residual > config::kSelfCalibrationMaxVerifyResidualDacCodes) {
      zeroOutputs();
      return CalibrationPanelError::VerificationResidualTooLarge;
    }
  }
  zeroOutputs();
  return CalibrationPanelError::None;
}

CalibrationPanelError CalibrationPanelController::selfCalibrateDacChannel(
    uint8_t channel, CalibrationProfile &working) {
  if (!waitForSaveOrCancel(PromptKind::Loop, channel)) {
    return CalibrationPanelError::None;
  }
  feedback_.beginLoopMeasurement(channel);

  SelfCalibrationSample samples[kSelfFitPointCount];
  for (uint8_t i = 0u; i < kSelfFitPointCount; ++i) {
    const uint16_t rawDac = kSelfFitDacCodes[i];
    dac_.write(dacChannel(channel), rawDac);
    delay(config::kSelfCalibrationSettleMs);
    AdcStats measured;
    if (!captureAdcStats(channel, config::kSelfCalibrationAverageSamples,
                         true, measured) ||
        measured.maximum - measured.minimum >
            config::kMaintenanceAdcMaximumCaptureSpan) {
      zeroOutputs();
      return CalibrationPanelError::InputUnstable;
    }
    samples[i] = {rawDac, measured.mean};
  }
  zeroOutputs();

  const SelfCalibrationResult fit =
      solveDacSelfCalibration(samples, kSelfFitPointCount);
  if (fit.status == SelfCalibrationStatus::NonMonotonic) {
    return CalibrationPanelError::LoopbackMissingOrNonMonotonic;
  }
  if (fit.status != SelfCalibrationStatus::Ok ||
      fit.maxFitResidualDacCodes >
          config::kSelfCalibrationMaxFitResidualDacCodes) {
    return CalibrationPanelError::LoopbackRangeImplausible;
  }

  working.dac[channel] = fit.calibration;
  if (!setActiveCalibrationProfile(working)) {
    return CalibrationPanelError::PersistenceOrProfileUnavailable;
  }

  uint16_t residual = 0u;
  return verifyDacChannel(channel, residual, true);
}

CalibrationPanelError CalibrationPanelController::verifyBoth(
    uint16_t &residualA, uint16_t &residualB) {
  if (!waitForSaveOrCancel(PromptKind::DualLoop)) {
    return CalibrationPanelError::None;
  }
  feedback_.beginDualLoopMeasurement();
  CalibrationPanelError error = verifyDacChannel(kChannelA, residualA, false);
  if (error != CalibrationPanelError::None) return error;
  feedback_.beginDualLoopMeasurement();
  return verifyDacChannel(kChannelB, residualB, false);
}

bool CalibrationPanelController::saveProfileOnly(
    const CalibrationProfile &profile) {
  if (!waitForPersistence() || !calibrationStore_.save(profile) ||
      !waitForPersistence()) {
    return false;
  }
  return true;
}

bool CalibrationPanelController::saveProfileWithQualification(
    const CalibrationProfile &profile, uint8_t residualA, uint8_t residualB) {
  if (!waitForPersistence()) return false;
  const uint8_t nextGeneration =
      static_cast<uint8_t>((calibrationStore_.activeGeneration() + 1u) & 0x0Fu);
  if (!qualificationStore_.save(nextGeneration, residualA, residualB) ||
      !waitForPersistence()) {
    return false;
  }
  if (!calibrationStore_.save(profile) || !waitForPersistence()) return false;
  return calibrationStore_.activeGeneration() == nextGeneration;
}

bool CalibrationPanelController::saveQualificationForCurrentGeneration(
    uint8_t residualA, uint8_t residualB) {
  if (!waitForPersistence()) return false;
  if (!qualificationStore_.save(calibrationStore_.activeGeneration(),
                                residualA, residualB)) {
    return false;
  }
  return waitForPersistence();
}

void CalibrationPanelController::failAndRestore(
    CalibrationPanelError error, const CalibrationProfile &previous) {
  zeroOutputs();
  (void)setActiveCalibrationProfile(previous);
  if (error == CalibrationPanelError::None) {
    feedback_.showIdle();
    return;
  }
  feedback_.showErrorCode(static_cast<uint8_t>(error));
  waitForErrorAcknowledge();
}

void CalibrationPanelController::runFullCalibration() {
  const CalibrationProfile previous = activeCalibrationProfile();
  CalibrationProfile working = previous;
  waitForButtonsReleased();

  CalibrationPanelError error = calibrateAdcChannel(kChannelA, working);
  if (error != CalibrationPanelError::None ||
      pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  error = calibrateAdcChannel(kChannelB, working);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  error = selfCalibrateDacChannel(kChannelA, working);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  error = selfCalibrateDacChannel(kChannelB, working);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }

  uint16_t residualA = 0u;
  uint16_t residualB = 0u;
  error = verifyBoth(residualA, residualB);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  if (!saveProfileWithQualification(
          working, static_cast<uint8_t>(residualA),
          static_cast<uint8_t>(residualB))) {
    failAndRestore(CalibrationPanelError::PersistenceOrProfileUnavailable,
                   previous);
    return;
  }
  feedback_.showQualificationPass();
}

void CalibrationPanelController::runAdcOnlyCalibration() {
  const CalibrationProfile previous = activeCalibrationProfile();
  CalibrationProfile working = previous;
  waitForButtonsReleased();
  CalibrationPanelError error = calibrateAdcChannel(kChannelA, working);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  error = calibrateAdcChannel(kChannelB, working);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  if (!saveProfileOnly(working)) {
    failAndRestore(CalibrationPanelError::PersistenceOrProfileUnavailable,
                   previous);
    return;
  }
  feedback_.showPass();
}

void CalibrationPanelController::runDacOnlyCalibration() {
  CalibrationProfile stored;
  if (!waitForPersistence() || !calibrationStore_.load(stored)) {
    const CalibrationProfile previous = activeCalibrationProfile();
    failAndRestore(CalibrationPanelError::PersistenceOrProfileUnavailable,
                   previous);
    return;
  }
  const CalibrationProfile previous = stored;
  CalibrationProfile working = stored;
  (void)setActiveCalibrationProfile(stored);
  waitForButtonsReleased();

  CalibrationPanelError error = selfCalibrateDacChannel(kChannelA, working);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  error = selfCalibrateDacChannel(kChannelB, working);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  uint16_t residualA = 0u;
  uint16_t residualB = 0u;
  error = verifyBoth(residualA, residualB);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  if (!saveProfileWithQualification(
          working, static_cast<uint8_t>(residualA),
          static_cast<uint8_t>(residualB))) {
    failAndRestore(CalibrationPanelError::PersistenceOrProfileUnavailable,
                   previous);
    return;
  }
  feedback_.showQualificationPass();
}

void CalibrationPanelController::runVerification() {
  CalibrationProfile stored;
  const CalibrationProfile previous = activeCalibrationProfile();
  if (!waitForPersistence() || !calibrationStore_.load(stored) ||
      !setActiveCalibrationProfile(stored)) {
    failAndRestore(CalibrationPanelError::PersistenceOrProfileUnavailable,
                   previous);
    return;
  }
  waitForButtonsReleased();
  uint16_t residualA = 0u;
  uint16_t residualB = 0u;
  const CalibrationPanelError error = verifyBoth(residualA, residualB);
  if (error != CalibrationPanelError::None || pressed(loadButton_)) {
    failAndRestore(error, previous);
    return;
  }
  if (!saveQualificationForCurrentGeneration(
          static_cast<uint8_t>(residualA),
          static_cast<uint8_t>(residualB))) {
    failAndRestore(CalibrationPanelError::PersistenceOrProfileUnavailable,
                   previous);
    return;
  }
  feedback_.showQualificationPass();
}

void CalibrationPanelController::zeroOutputs() {
  dac_.write(Mcp4922Dac::Channel::A, calibratedDacCode(0, kChannelA));
  dac_.write(Mcp4922Dac::Channel::B, calibratedDacCode(0, kChannelB));
}

[[noreturn]] void CalibrationPanelController::run() {
  feedback_.showIdle();
  for (;;) {
    service();
    const uint32_t now = static_cast<uint32_t>(millis());
    const ButtonState saveState =
        saveDebouncer_.sample(pressed(saveButton_), now);
    const ButtonState loadState =
        loadDebouncer_.sample(pressed(loadButton_), now);

    if (saveState == ButtonState::JustPressed &&
        loadState != ButtonState::JustPressed) {
      if (pressed(shiftButton_)) {
        runAdcOnlyCalibration();
      } else {
        runFullCalibration();
      }
    } else if (loadState == ButtonState::JustPressed &&
               saveState != ButtonState::JustPressed) {
      if (pressed(shiftButton_)) {
        runDacOnlyCalibration();
      } else {
        runVerification();
      }
    }
    delay(config::kMaintenanceUiPollMs);
  }
}

}  // namespace fmq::platform::nano
