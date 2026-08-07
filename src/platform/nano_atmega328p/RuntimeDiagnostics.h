/**
 * @file RuntimeDiagnostics.h
 * Collects and reports optional real-time scheduler diagnostics.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_RUNTIME_DIAGNOSTICS_H
#define FMQ_PLATFORM_NANO_RUNTIME_DIAGNOSTICS_H

#include <Arduino.h>
#include <stdio.h>
#include "fmq/config/RuntimeConfig.h"

namespace fmq::platform::nano {

struct RuntimeDiagnostics {
  uint32_t ticks = 0;
  uint32_t missedDeadlines = 0;
  uint32_t adcStaleTicks = 0;
  uint32_t uiRuns = 0;
  uint32_t ledWrites = 0;
  uint8_t maxPending = 0;
  uint32_t lastReport = 0;

  /**
   * One control cycle is executed for the newest state. Older accumulated
   * ticks are counted as missed deadlines rather than replayed with current
   * ADC/trigger values, which would fabricate history that was never sampled.
   */
  void observeQueue(uint8_t pending) {
    if (pending > maxPending) maxPending = pending;
    if (pending > 1u) {
      missedDeadlines += static_cast<uint32_t>(pending - 1u);
    }
  }

  /** Non-blocking best-effort serial report. */
  void report(uint32_t now) {
    if (!config::kDiagnosticsEnabled ||
        now - lastReport < config::kDiagnosticsPeriodMs) {
      return;
    }

    char line[config::kDiagnosticsLineBufferBytes];
    const int length = snprintf(
        line, sizeof(line), "d t=%lu m=%lu q=%u a=%lu u=%lu l=%lu\n",
        static_cast<unsigned long>(ticks),
        static_cast<unsigned long>(missedDeadlines), maxPending,
        static_cast<unsigned long>(adcStaleTicks),
        static_cast<unsigned long>(uiRuns),
        static_cast<unsigned long>(ledWrites));
    if (length <= 0 || length >= static_cast<int>(sizeof(line))) return;

    // Never stall the real-time loop merely to print diagnostics.
    if (Serial.availableForWrite() < length) return;
    Serial.write(reinterpret_cast<const uint8_t *>(line),
                 static_cast<size_t>(length));
    lastReport = now;
  }
};

}  // namespace fmq::platform::nano
#endif
