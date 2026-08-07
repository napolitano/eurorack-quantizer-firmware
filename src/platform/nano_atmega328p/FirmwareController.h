/**
 * @file FirmwareController.h
 * Declares board-level firmware setup and real-time execution.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_FIRMWARE_CONTROLLER_H
#define FMQ_PLATFORM_NANO_FIRMWARE_CONTROLLER_H
namespace fmq::platform::nano {
class FirmwareController {
 public:
  void begin();
  void run();
};
}
#endif
