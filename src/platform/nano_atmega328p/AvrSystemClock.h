/**
 * @file AvrSystemClock.h
 * Implements the monotonic clock port using Arduino millis().
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_AVR_SYSTEM_CLOCK_H
#define FM_QUANTIZER_HAL_AVR_SYSTEM_CLOCK_H

#include <Arduino.h>

#include "fmq/ports/Clock.h"

/**
 * IClock backed by the Arduino millis() timer (Timer0).
 */
namespace fmq {

class AvrSystemClock : public IClock {
 public:
  uint32_t millis() const override { return ::millis(); }
};

}  // namespace fmq
#endif  // FM_QUANTIZER_HAL_AVR_SYSTEM_CLOCK_H
