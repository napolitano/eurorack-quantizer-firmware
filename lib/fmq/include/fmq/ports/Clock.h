/**
 * @file Clock.h
 * Defines the platform-independent monotonic clock port.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_CLOCK_H
#define FM_QUANTIZER_HAL_CLOCK_H

#include <stdint.h>

/**
 * Monotonic millisecond time source.
 */
namespace fmq {

/// Abstract millisecond clock. The AVR implementation is backed by Timer0.
class IClock {
 public:
  virtual ~IClock() {}
  /// @return Milliseconds elapsed since power-on (monotonic, may wrap after
  ///         ~49 days; the firmware only ever compares small differences).
  virtual uint32_t millis() const = 0;
  /// @return Microseconds elapsed since power-on. Wrap-around after roughly
  ///         71 minutes is intentional; unsigned subtraction remains valid.
  virtual uint32_t micros() const = 0;
};

}  // namespace fmq
#endif  // FM_QUANTIZER_HAL_CLOCK_H
