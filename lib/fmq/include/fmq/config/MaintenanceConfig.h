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
constexpr uint16_t kMaintenanceConsoleStartupDelayMs = 50u;
constexpr uint16_t kMaintenancePersistenceTimeoutMs = 1500u;

constexpr uint16_t kSelfCalibrationSettleMs = 8u;
constexpr uint8_t kSelfCalibrationAverageSamples = 12u;
constexpr uint16_t kSelfCalibrationSampleTimeoutMs = 150u;
constexpr uint16_t kSelfCalibrationMaxFitResidualDacCodes = 20u;
constexpr uint16_t kSelfCalibrationMaxVerifyResidualDacCodes = 16u;

}  // namespace fmq::config

#endif
