/**
 * @file MaintenanceConfig.h
 * Timing and limits used only by the dedicated calibration/maintenance image.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_MAINTENANCE_CONFIG_H
#define FMQ_CONFIG_MAINTENANCE_CONFIG_H

#include <stdint.h>

namespace fmq::config {

constexpr uint32_t kMaintenanceBaud = 115200u;
constexpr uint16_t kMaintenanceReportStartupDelayMs = 250u;
constexpr uint16_t kMaintenancePersistenceTimeoutMs = 1500u;
constexpr uint16_t kMaintenanceUiPollMs = 2u;
constexpr uint16_t kMaintenancePromptPulseMs = 350u;
constexpr uint16_t kMaintenancePromptGapMs = 250u;
constexpr uint16_t kMaintenanceAdcCaptureSamples = 128u;
constexpr uint16_t kMaintenanceAdcMaximumCaptureSpan = 8u;

// Visual maintenance-mode feedback. The ring is deliberately static and dim
// while idle, and completely off during analog measurements.
constexpr uint8_t kMaintenanceIdleBrightnessPercent = 8u;
constexpr uint8_t kMaintenanceBootBlinkCount = 3u;
constexpr uint8_t kMaintenanceResultBlinkCount = 2u;
constexpr uint8_t kMaintenanceReportBlinkCount = 3u;
constexpr uint16_t kMaintenanceFeedbackBlinkOnMs = 80u;
constexpr uint16_t kMaintenanceFeedbackBlinkOffMs = 70u;

static_assert(kMaintenanceIdleBrightnessPercent >= 5u &&
                  kMaintenanceIdleBrightnessPercent <= 10u,
              "maintenance idle brightness must remain deliberately dim");

constexpr uint16_t kSelfCalibrationSettleMs = 8u;
constexpr uint8_t kSelfCalibrationAverageSamples = 12u;
constexpr uint16_t kSelfCalibrationSampleTimeoutMs = 150u;
constexpr uint16_t kSelfCalibrationMaxFitResidualDacCodes = 20u;
constexpr uint16_t kSelfCalibrationMaxVerifyResidualDacCodes = 15u;

}  // namespace fmq::config

#endif
