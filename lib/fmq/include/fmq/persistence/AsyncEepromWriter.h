/**
 * @file AsyncEepromWriter.h
 * Provides a non-blocking queued EEPROM write service.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_ASYNC_EEPROM_WRITER_H
#define FMQ_ASYNC_EEPROM_WRITER_H

#include <stdint.h>
#include "fmq/ports/Eeprom.h"

namespace fmq {

/**
 * Shared, non-blocking EEPROM write queue.
 *
 * `busy()` remains true until the final physical EEPROM write has completed,
 * not merely until the final byte has been submitted to the AVR controller.
 * This makes it safe for clients to guard reads with the same writer.
 */
class AsyncEepromWriter {
 public:
  static constexpr uint8_t kMaxOps = 32;

  explicit AsyncEepromWriter(IEeprom &eeprom)
      : eeprom_(eeprom), count_(0), pos_(0), active_(false) {}

  bool busy() const { return active_; }

  bool begin(const uint16_t *addresses, const uint8_t *values, uint8_t count) {
    if (busy() || count == 0 || count > kMaxOps) return false;
    for (uint8_t i = 0; i < count; ++i) {
      addresses_[i] = addresses[i];
      values_[i] = values[i];
    }
    count_ = count;
    pos_ = 0;
    active_ = true;
    return true;
  }

  /** Advance by at most one physical byte write. */
  void service() {
    if (!active_) return;
    if (pos_ < count_) {
      if (!eeprom_.isReady()) return;
      eeprom_.writeByte(addresses_[pos_], values_[pos_]);
      ++pos_;
      return;
    }
    // The final byte was submitted on an earlier call. Do not advertise idle
    // until the EEPROM controller itself reports that the write has finished.
    if (eeprom_.isReady()) active_ = false;
  }

#if !defined(ARDUINO)
  // Host-test helper only. Production firmware deliberately has no blocking
  // EEPROM flush path.
  void flush() {
    while (busy()) service();
  }
#endif

 private:
  IEeprom &eeprom_;
  uint16_t addresses_[kMaxOps];
  uint8_t values_[kMaxOps];
  uint8_t count_;
  uint8_t pos_;
  bool active_;
};

}  // namespace fmq
#endif
