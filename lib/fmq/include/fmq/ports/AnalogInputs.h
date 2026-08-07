/**
 * @file AnalogInputs.h
 * Defines the platform-independent analog-input port.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_ANALOG_INPUTS_H
#define FM_QUANTIZER_HAL_ANALOG_INPUTS_H

#include <stdint.h>

/**
 * Access to the three analog channels sampled by the ADC.
 *
 * Channel indices: 0 = button ladder (A5), 1 = CV input A (ADC6),
 * 2 = CV input B (ADC7). The AVR implementation samples these continuously in
 * the background via free-running ADC + interrupt with configurable moving-average filtering.
 */
namespace fmq {

class IAnalogInputs {
 public:
  virtual ~IAnalogInputs() {}
  /// @return true once a fresh set of samples is ready (1 kHz on hardware).
  virtual bool sampleReady() = 0;
  /// Latch the most recent samples into an internal snapshot for this cycle.
  virtual void beginCycle() = 0;
  /**
   * @brief Read a latched channel value from the current cycle's snapshot.
   * @param channel 0..2 as documented above.
   * @return 10-bit ADC value 0..1023.
   */
  virtual uint16_t read(uint8_t channel) const = 0;
};

}  // namespace fmq
#endif  // FM_QUANTIZER_HAL_ANALOG_INPUTS_H
