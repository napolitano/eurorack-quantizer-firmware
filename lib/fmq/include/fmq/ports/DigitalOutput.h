/**
 * @file DigitalOutput.h
 * Defines the platform-independent digital-output port.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_DIGITAL_OUTPUT_H
#define FM_QUANTIZER_HAL_DIGITAL_OUTPUT_H

/**
 * A single push-pull digital output pin.
 */
namespace fmq {

/// Abstract digital output (e.g. a chip-select, BLANK, or trigger line).
class IDigitalOutput {
 public:
  virtual ~IDigitalOutput() {}
  /// Drive the pin high (true) or low (false).
  virtual void set(bool high) = 0;
};

}  // namespace fmq
#endif  // FM_QUANTIZER_HAL_DIGITAL_OUTPUT_H
