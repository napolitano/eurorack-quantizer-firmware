/**
 * @file RuntimeConfig.h
 * Central scheduler, diagnostics and calibration-console configuration.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_RUNTIME_CONFIG_H
#define FMQ_CONFIG_RUNTIME_CONFIG_H
#include <stdint.h>
namespace fmq::config {
constexpr uint16_t kControlLoopFrequencyHz = 1000;

// Match the original UI sampling cadence. The menu itself is inexpensive and
// this also preserves the timing assumptions of the proven Rust input path.
constexpr uint8_t kUiDivider = 1;           // 1 kHz UI/input processing
constexpr uint8_t kLedDivider = 10;         // 100 Hz maximum LED refresh
constexpr bool kDiagnosticsEnabled = false;
constexpr uint32_t kDiagnosticsPeriodMs = 1000;
constexpr uint32_t kStartupInputSettleMs = 5;
constexpr uint32_t kLadderCalibrationTimeoutMs = 1000;
constexpr uint16_t kCalibrationConsoleStartupDelayMs = 50;
constexpr uint16_t kCalibrationStreamPeriodMs = 100;
constexpr bool kCalibrationConsoleEnabled = true;
// Compatibility baseline: the Rust original does not restore or autosave the
// running quantizer state. Keep the optional C++ live-state extension disabled
// by default so stale EEPROM data cannot alter startup behaviour or brightness.
constexpr bool kRestoreLiveStateOnBoot = false;
constexpr bool kAutosaveLiveState = false;
constexpr uint32_t kCalibrationBaud = 115200;
constexpr uint8_t kDiagnosticsLineBufferBytes = 64;
static_assert(kUiDivider >= 1 && kLedDivider >= 1, "invalid task divider");
}
#endif
