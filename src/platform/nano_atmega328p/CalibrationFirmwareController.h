/**
 * @file CalibrationFirmwareController.h
 * Declares the dedicated Nano calibration/maintenance firmware image.
 *
 * @author Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_CALIBRATION_FIRMWARE_CONTROLLER_H
#define FMQ_PLATFORM_NANO_CALIBRATION_FIRMWARE_CONTROLLER_H

namespace fmq::platform::nano {

class CalibrationFirmwareController {
 public:
  void begin();
  [[noreturn]] void run();
};

}  // namespace fmq::platform::nano

#endif
