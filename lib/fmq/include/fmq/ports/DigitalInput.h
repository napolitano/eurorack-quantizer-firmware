/**
 * @file DigitalInput.h
 * Defines the platform-independent digital-input port.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_HAL_DIGITAL_INPUT_H
#define FM_QUANTIZER_HAL_DIGITAL_INPUT_H

/**
 * A single digital input pin.
 */
namespace fmq {

/// Abstract digital input (e.g. a push button or trigger jack).
class IDigitalInput {
 public:
  virtual ~IDigitalInput() {}
  /// @return true when the pin reads a logic high level.
  virtual bool isHigh() const = 0;
};

}  // namespace fmq
#endif  // FM_QUANTIZER_HAL_DIGITAL_INPUT_H
