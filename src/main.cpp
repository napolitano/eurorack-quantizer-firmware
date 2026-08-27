/**
 * @file main.cpp
 * Selects either the normal production image or the calibration image.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <Arduino.h>

#if defined(FMQ_CALIBRATION_FIRMWARE)
#include "platform/nano_atmega328p/CalibrationFirmwareController.h"
namespace {
fmq::platform::nano::CalibrationFirmwareController firmware;
}
#else
#include "platform/nano_atmega328p/FirmwareController.h"
namespace {
fmq::platform::nano::FirmwareController firmware;
}
#endif

void setup() { firmware.begin(); }
void loop() { firmware.run(); }
