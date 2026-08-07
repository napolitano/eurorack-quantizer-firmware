/**
 * @file BoardProfilePrototypeAvcc.h
 * Defines the alternate prototype board profile.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_BOARD_PROFILE_PROTOTYPE_AVCC_H
#define FMQ_PLATFORM_NANO_BOARD_PROFILE_PROTOTYPE_AVCC_H
#include "platform/nano_atmega328p/BoardProfileOriginal.h"
namespace fmq::board {
struct PrototypeAvcc : Original {
  static constexpr const char *name = "FM Quantizer prototype (AVCC reference)";
  static constexpr bool externalAref = false;
};
}
#endif
