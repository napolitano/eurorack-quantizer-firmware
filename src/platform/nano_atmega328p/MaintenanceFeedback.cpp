/**
 * @file MaintenanceFeedback.cpp
 * Implements panel-driven calibration status feedback.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/nano_atmega328p/MaintenanceFeedback.h"

#include <Arduino.h>

#include "fmq/config/LedConfig.h"
#include "fmq/config/MaintenanceConfig.h"
#include "fmq/domain/LedColor.h"
#include "fmq/ui/BrightnessCalibration.h"
#include "fmq/ui/LedFrameEncoder.h"

namespace fmq::platform::nano {
namespace {

uint16_t percentOf(uint16_t level, uint8_t percent) {
  const uint32_t scaled = static_cast<uint32_t>(level) * percent + 50u;
  return static_cast<uint16_t>(scaled / 100u);
}

}  // namespace

void MaintenanceFeedback::begin() {
  setStatus(false, false, false, false);
  ring_.begin();
  showRingOff();
}

void MaintenanceFeedback::playBootIndication() {
  setStatus(false, false, false, false);
  blink(BrightnessCalibration::kLegacyRedLevel,
        BrightnessCalibration::kLegacyGreenLevel,
        config::kMaintenanceBootBlinkCount);
  showIdle();
}

void MaintenanceFeedback::showIdle() {
  setStatus(false, false, false, false);
  writeUniform(percentOf(BrightnessCalibration::kLegacyRedLevel,
                         config::kMaintenanceIdleBrightnessPercent),
               percentOf(BrightnessCalibration::kLegacyGreenLevel,
                         config::kMaintenanceIdleBrightnessPercent));
}

void MaintenanceFeedback::showVoltagePrompt(uint8_t channel,
                                            uint8_t noteIndex, bool lit) {
  setStatus(channel == 0u, false, channel == 1u, false);
  if (lit) {
    writeSingle(noteIndex, LedColor::Green, 0u,
                BrightnessCalibration::kLegacyGreenLevel);
  } else {
    showRingOff();
  }
}

void MaintenanceFeedback::showLoopPrompt(uint8_t channel, bool lit) {
  showRingOff();
  setStatus(lit && channel == 0u, lit && channel == 0u,
            lit && channel == 1u, lit && channel == 1u);
}

void MaintenanceFeedback::showDualLoopPrompt(bool lit) {
  showRingOff();
  setStatus(lit, lit, lit, lit);
}

void MaintenanceFeedback::beginAdcMeasurement(uint8_t channel) {
  showRingOff();
  setStatus(channel == 0u, false, channel == 1u, false);
}

void MaintenanceFeedback::beginLoopMeasurement(uint8_t channel) {
  showRingOff();
  setStatus(channel == 0u, channel == 0u, channel == 1u, channel == 1u);
}

void MaintenanceFeedback::beginDualLoopMeasurement() {
  showRingOff();
  setStatus(true, true, true, true);
}


void MaintenanceFeedback::showMeasurementSelection(uint8_t channel,
                                                   uint8_t noteIndex) {
  setStatus(false, channel == 0u, false, channel == 1u);
  const LedColor color = channel == 0u ? LedColor::Green : LedColor::Red;
  writeSingle(noteIndex, color, BrightnessCalibration::kLegacyRedLevel,
              BrightnessCalibration::kLegacyGreenLevel);
}

void MaintenanceFeedback::showRawMeasurementSelection(uint8_t channel,
                                                      uint8_t noteIndex) {
  setStatus(false, channel == 0u, false, channel == 1u);
  writeSingle(noteIndex, LedColor::Amber,
              BrightnessCalibration::kLegacyRedLevel,
              BrightnessCalibration::kLegacyGreenLevel);
}

void MaintenanceFeedback::showMeasurementOctave(uint8_t channel,
                                                uint8_t octave) {
  setStatus(false, channel == 0u, false, channel == 1u);
  writeSingle(octave, LedColor::Amber, BrightnessCalibration::kLegacyRedLevel,
              BrightnessCalibration::kLegacyGreenLevel);
}

void MaintenanceFeedback::showPass() {
  setStatus(false, false, false, false);
  blink(0u, BrightnessCalibration::kLegacyGreenLevel,
        config::kMaintenanceResultBlinkCount);
  showIdle();
}

void MaintenanceFeedback::showFail() {
  setStatus(false, false, false, false);
  blink(BrightnessCalibration::kLegacyRedLevel, 0u,
        config::kMaintenanceResultBlinkCount);
  showIdle();
}

void MaintenanceFeedback::showQualificationPass() {
  setStatus(false, false, false, false);
  blink(0u, BrightnessCalibration::kLegacyGreenLevel,
        config::kMaintenanceReportBlinkCount);
  showIdle();
}

void MaintenanceFeedback::showQualificationFail() {
  setStatus(false, false, false, false);
  blink(BrightnessCalibration::kLegacyRedLevel, 0u,
        config::kMaintenanceReportBlinkCount);
  showIdle();
}

void MaintenanceFeedback::showErasePrompt(bool lit) {
  setStatus(false, false, false, false);
  if (lit) {
    writeUniform(BrightnessCalibration::kLegacyRedLevel, 0u);
  } else {
    showRingOff();
  }
}

void MaintenanceFeedback::showEraseComplete() {
  setStatus(false, false, false, false);
  blink(BrightnessCalibration::kLegacyRedLevel,
        BrightnessCalibration::kLegacyGreenLevel,
        config::kMaintenanceReportBlinkCount);
  showIdle();
}

void MaintenanceFeedback::showErrorCode(uint8_t code) {
  setStatus(false, false, false, false);
  blink(BrightnessCalibration::kLegacyRedLevel, 0u,
        config::kMaintenanceResultBlinkCount);
  writeErrorCode(code);
}

void MaintenanceFeedback::setStatus(bool inputA, bool outputA, bool inputB,
                                    bool outputB) {
  const auto level = [this](bool on) {
    return statusLedsActiveHigh_ ? on : !on;
  };
  inputLedA_.set(level(inputA));
  outputLedA_.set(level(outputA));
  inputLedB_.set(level(inputB));
  outputLedB_.set(level(outputB));
}

void MaintenanceFeedback::writeUniform(uint16_t redLevel,
                                       uint16_t greenLevel) {
  LedFrame frame;
  for (uint8_t i = 0u; i < kNoteCount; ++i) frame[i] = LedColor::Amber;

  uint8_t bytes[kTlc5947FrameBytes];
  encodeLedFrame(frame, redLevel, greenLevel, bytes);
  ring_.writeFrame(bytes);
}

void MaintenanceFeedback::writeSingle(uint8_t noteIndex, LedColor color,
                                      uint16_t redLevel,
                                      uint16_t greenLevel) {
  LedFrame frame;
  for (uint8_t i = 0u; i < kNoteCount; ++i) frame[i] = LedColor::Off;
  if (noteIndex < kNoteCount) frame[noteIndex] = color;
  uint8_t bytes[kTlc5947FrameBytes];
  encodeLedFrame(frame, redLevel, greenLevel, bytes);
  ring_.writeFrame(bytes);
}

void MaintenanceFeedback::writeErrorCode(uint8_t code) {
  LedFrame frame;
  for (uint8_t i = 0u; i < kNoteCount; ++i) {
    frame[i] = i < code ? LedColor::Red : LedColor::Off;
  }
  uint8_t bytes[kTlc5947FrameBytes];
  encodeLedFrame(frame, BrightnessCalibration::kLegacyRedLevel, 0u, bytes);
  ring_.writeFrame(bytes);
}

void MaintenanceFeedback::showRingOff() { writeUniform(0u, 0u); }

void MaintenanceFeedback::blink(uint16_t redLevel, uint16_t greenLevel,
                                uint8_t count) {
  for (uint8_t i = 0u; i < count; ++i) {
    writeUniform(redLevel, greenLevel);
    delay(config::kMaintenanceFeedbackBlinkOnMs);
    showRingOff();
    if (i + 1u < count) delay(config::kMaintenanceFeedbackBlinkOffMs);
  }
}

}  // namespace fmq::platform::nano
