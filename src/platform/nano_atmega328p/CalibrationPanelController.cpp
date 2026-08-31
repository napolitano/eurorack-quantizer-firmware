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
#include "fmq/domain/CalibrationMeasurement.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/SelfCalibration.h"

namespace fmq::platform::nano {
namespace {

constexpr uint8_t kChannelA = 0u;
constexpr uint8_t kChannelB = 1u;
constexpr uint8_t kAdcFitNoteIndices[kAdcCalibrationFitPointCount] = {
    1u, 3u, 5u, 7u, 9u};      // C#, D#, F, G, A -> 1/3/5/7/9 V
constexpr uint8_t
    kAdcVerifyNoteIndices[kAdcCalibrationVerificationPointCount] = {
        0u, 2u, 4u, 6u, 8u};  // C, D, E, F#, G# -> 0/2/4/6/8 V
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

bool CalibrationPanelController::waitForEraseConfirmation() {
  waitForButtonsReleased();
  const uint32_t period = static_cast<uint32_t>(config::kMaintenancePromptPulseMs) +
                          config::kMaintenancePromptGapMs;
  for (;;) {
    service();
    const uint32_t now = static_cast<uint32_t>(millis());
    feedback_.showErasePrompt((now % period) < config::kMaintenancePromptPulseMs);
    const ButtonState saveState =
        saveDebouncer_.sample(pressed(saveButton_), now);
    const ButtonState loadState =
        loadDebouncer_.sample(pressed(loadButton_), now);
    if (loadState == ButtonState::JustPressed) {
      waitForButtonsReleased();
      feedback_.showIdle();
      return false;
    }
    if (saveState == ButtonState::JustPressed) {
      waitForButtonsReleased();
      return true;
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
  // Fit one affine line from five points safely inside the ADC range. The
  // independent 0/2/4/6/8 V sequence then verifies the fitted line without
  // contributing any data to it. In particular, 0 V remains verification-only
  // because a negative input-path offset can disappear below ADC code zero.
  uint16_t rawFitPoints[kAdcCalibrationFitPointCount] = {};
  for (uint8_t i = 0u; i < kAdcCalibrationFitPointCount; ++i) {
    if (!waitForSaveOrCancel(PromptKind::Voltage, channel,
                             kAdcFitNoteIndices[i])) {
      return CalibrationPanelError::None;
    }
    feedback_.beginAdcMeasurement(channel);
    AdcStats stats;
    if (!captureAdcStats(channel, config::kMaintenanceAdcCaptureSamples, false,
                         stats) ||
        stats.maximum - stats.minimum >
            config::kMaintenanceAdcMaximumCaptureSpan) {
      return CalibrationPanelError::InputUnstable;
    }
    rawFitPoints[i] = stats.mean;
  }

  const AdcCalibrationResult fit =
      solveAdcMultiPointCalibration(rawFitPoints);
  if (fit.status != AdcCalibrationStatus::Ok ||
      fit.maxFitResidualCodes > config::kMaintenanceAdcFitToleranceCodes) {
    return CalibrationPanelError::AdcSpanInvalid;
  }

  // Verify five interleaved points that were not used by the fit. Code zero is
  // checked in the raw domain so a clipped negative analog offset cannot be
  // mistaken for a valid fitted endpoint. All remaining points are evaluated
  // through the newly fitted calibration.
  for (uint8_t i = 0u; i < kAdcCalibrationVerificationPointCount; ++i) {
    if (!waitForSaveOrCancel(PromptKind::Voltage, channel,
                             kAdcVerifyNoteIndices[i])) {
      return CalibrationPanelError::None;
    }
    feedback_.beginAdcMeasurement(channel);
    AdcStats stats;
    if (!captureAdcStats(channel, config::kMaintenanceAdcCaptureSamples, false,
                         stats) ||
        stats.maximum - stats.minimum >
            config::kMaintenanceAdcMaximumCaptureSpan) {
      return CalibrationPanelError::InputUnstable;
    }

    const uint8_t volts = kAdcCalibrationVerificationVolts[i];
    if (volts == 0u) {
      if (stats.mean > config::kMaintenanceAdcZeroRawMaximumCode) {
        return CalibrationPanelError::AdcSpanInvalid;
      }
      continue;
    }

    const uint16_t corrected =
        applyAdcCalibration(stats.mean, fit.calibration);
    const uint16_t target = nominalAdcCodeForVolts(volts);
    if (absoluteDifference(corrected, target) >
        config::kMaintenanceAdcVerificationToleranceCodes) {
      return CalibrationPanelError::AdcSpanInvalid;
    }
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

void CalibrationPanelController::applyMeasurementOutput(
    uint8_t channel, uint8_t octave, uint8_t pitchClass, bool rawMode) {
  const uint16_t zeroA = rawMode ? 0u : calibratedDacCode(0, kChannelA);
  const uint16_t zeroB = rawMode ? 0u : calibratedDacCode(0, kChannelB);
  dac_.write(Mcp4922Dac::Channel::A, zeroA);
  dac_.write(Mcp4922Dac::Channel::B, zeroB);
  if (!measurementSelectionValid(octave, pitchClass)) return;

  const SemitoneQ8_8 pitch = measurementPitchQ8_8(octave, pitchClass);
  const uint16_t code = rawMode ? semitonesToNominalDac(pitch)
                                : semitonesToDac(pitch, channel);
  dac_.write(dacChannel(channel), code);
}

void CalibrationPanelController::showMeasurementState(
    uint8_t channel, uint8_t octave, uint8_t pitchClass, bool showOctave,
    bool rawMode) {
  if (showOctave) {
    feedback_.showMeasurementOctave(channel, octave);
    delay(config::kMaintenanceMeasurementOctaveDisplayMs);
  }
  if (rawMode) {
    feedback_.showRawMeasurementSelection(channel, pitchClass);
  } else {
    feedback_.showMeasurementSelection(channel, pitchClass);
  }
}

void CalibrationPanelController::runOutputMeasurement() {
  CalibrationProfile stored;
  const CalibrationProfile previous = activeCalibrationProfile();
  const bool hasStoredProfile =
      waitForPersistence() && calibrationStore_.load(stored);
  if (hasStoredProfile) {
    (void)setActiveCalibrationProfile(stored);
  }

  waitForButtonsReleased();
  uint8_t channel = kChannelA;
  uint8_t octave = 0u;
  uint8_t pitchClass = 0u;
  bool rawMode = !hasStoredProfile;
  bool dualButtonLatched = false;
  ButtonWithLongPress measurementShift(config::kDigitalDebounceMs,
                                      config::kMaintenanceMeasurementModeHoldMs);

  applyMeasurementOutput(channel, octave, pitchClass, rawMode);
  showMeasurementState(channel, octave, pitchClass, true, rawMode);

  for (;;) {
    service();
    const uint32_t now = static_cast<uint32_t>(millis());
    const LongPressButtonState shiftState =
        measurementShift.sample(pressed(shiftButton_), now);
    const bool savePressed = pressed(saveButton_);
    const bool loadPressed = pressed(loadButton_);
    const ButtonState saveState =
        saveDebouncer_.sample(savePressed, now);
    const ButtonState loadState =
        loadDebouncer_.sample(loadPressed, now);
    const ButtonEvent noteEvent = ladder_.sample(now, adc_.read(0u));

    if (shiftState == LongPressButtonState::ButtonJustClickedLong) {
      zeroOutputs();
      (void)setActiveCalibrationProfile(previous);
      waitForButtonsReleased();
      feedback_.showIdle();
      return;
    }
    if (shiftState == LongPressButtonState::ButtonJustClickedShort) {
      channel = channel == kChannelA ? kChannelB : kChannelA;
      applyMeasurementOutput(channel, octave, pitchClass, rawMode);
      showMeasurementState(channel, octave, pitchClass, false, rawMode);
    }

    const bool saveDebouncedDown =
        saveState == ButtonState::JustPressed || saveState == ButtonState::HeldDown;
    const bool loadDebouncedDown =
        loadState == ButtonState::JustPressed || loadState == ButtonState::HeldDown;
    if (savePressed && loadPressed) {
      if (saveDebouncedDown && loadDebouncedDown && !dualButtonLatched) {
        dualButtonLatched = true;
        if (hasStoredProfile) rawMode = !rawMode;
        applyMeasurementOutput(channel, octave, pitchClass, rawMode);
        showMeasurementState(channel, octave, pitchClass, false, rawMode);
      }
    } else {
      dualButtonLatched = false;
      if (saveState == ButtonState::JustPressed) {
        if (octave < 9u) {
          ++octave;
        } else if (octave == 9u) {
          octave = kMeasurementMaximumOctave;
          pitchClass = 0u;
        }
        applyMeasurementOutput(channel, octave, pitchClass, rawMode);
        showMeasurementState(channel, octave, pitchClass, true, rawMode);
      } else if (loadState == ButtonState::JustPressed) {
        if (octave > 0u) --octave;
        applyMeasurementOutput(channel, octave, pitchClass, rawMode);
        showMeasurementState(channel, octave, pitchClass, true, rawMode);
      }
    }

    if (noteEvent.type == ButtonEventType::JustPressed) {
      const uint8_t requested = noteEvent.index;
      if (measurementSelectionValid(octave, requested)) {
        pitchClass = requested;
      } else {
        // Octave 10 contains only the top C (120 semitones / 10.000 V).
        pitchClass = 0u;
      }
      applyMeasurementOutput(channel, octave, pitchClass, rawMode);
      showMeasurementState(channel, octave, pitchClass, false, rawMode);
    }

    delay(config::kMaintenanceUiPollMs);
  }
}

void CalibrationPanelController::runEraseCalibration() {
  const CalibrationProfile previous = activeCalibrationProfile();
  zeroOutputs();
  if (!waitForEraseConfirmation()) return;

  // Invalidate qualification first. If power fails before the journal erase,
  // an old calibration may survive but it can no longer be certified as PASS.
  if (!qualificationStore_.erase() || !waitForPersistence() ||
      !calibrationStore_.erase() || !waitForPersistence()) {
    failAndRestore(CalibrationPanelError::PersistenceOrProfileUnavailable,
                   previous);
    return;
  }

  CalibrationProfile ignored;
  (void)calibrationStore_.load(ignored);  // resets journal head to empty state
  resetActiveCalibrationProfile();
  dac_.write(Mcp4922Dac::Channel::A, 0u);
  dac_.write(Mcp4922Dac::Channel::B, 0u);
  feedback_.showEraseComplete();
}

void CalibrationPanelController::zeroOutputs() {
  dac_.write(Mcp4922Dac::Channel::A, calibratedDacCode(0, kChannelA));
  dac_.write(Mcp4922Dac::Channel::B, calibratedDacCode(0, kChannelB));
}

[[noreturn]] void CalibrationPanelController::run() {
  feedback_.showIdle();
  uint32_t shiftPressedAt = 0u;
  uint32_t erasePressedAt = 0u;
  bool measurementModeEntered = false;
  bool eraseTriggered = false;

  for (;;) {
    service();
    const uint32_t now = static_cast<uint32_t>(millis());
    const bool shiftPressed = pressed(shiftButton_);
    const bool savePressed = pressed(saveButton_);
    const bool loadPressed = pressed(loadButton_);
    const ButtonState shiftState =
        shiftDebouncer_.sample(shiftPressed, now);
    const ButtonState saveState = saveDebouncer_.sample(savePressed, now);
    const ButtonState loadState = loadDebouncer_.sample(loadPressed, now);

    const bool eraseChord = shiftPressed && savePressed && loadPressed;
    if (eraseChord) {
      if (erasePressedAt == 0u) {
        erasePressedAt = now;
        eraseTriggered = false;
      } else if (!eraseTriggered &&
                 now - erasePressedAt >= config::kMaintenanceEraseHoldMs) {
        eraseTriggered = true;
        runEraseCalibration();
        erasePressedAt = 0u;
        shiftPressedAt = 0u;
        feedback_.showIdle();
        continue;
      }
    } else {
      erasePressedAt = 0u;
      eraseTriggered = false;
    }

    if (shiftState == ButtonState::JustPressed) {
      shiftPressedAt = now;
      measurementModeEntered = false;
    }
    if (!shiftPressed || savePressed || loadPressed) {
      shiftPressedAt = 0u;
    } else if (!measurementModeEntered && shiftPressedAt != 0u &&
               now - shiftPressedAt >=
                   config::kMaintenanceMeasurementModeHoldMs) {
      measurementModeEntered = true;
      runOutputMeasurement();
      shiftPressedAt = 0u;
      feedback_.showIdle();
      continue;
    }

    if (!eraseChord && saveState == ButtonState::JustPressed && !loadPressed) {
      if (shiftPressed) {
        runAdcOnlyCalibration();
      } else {
        runFullCalibration();
      }
    } else if (!eraseChord && loadState == ButtonState::JustPressed &&
               !savePressed) {
      if (shiftPressed) {
        runDacOnlyCalibration();
      } else {
        runVerification();
      }
    }
    delay(config::kMaintenanceUiPollMs);
  }
}

}  // namespace fmq::platform::nano
