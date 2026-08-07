/**
 * @file main.cpp
 * Provides the minimal Arduino entry points for the firmware controller.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <Arduino.h>
#include "platform/nano_atmega328p/FirmwareController.h"

namespace {
fmq::platform::nano::FirmwareController firmware;
}

void setup() { firmware.begin(); }
void loop() { firmware.run(); }
