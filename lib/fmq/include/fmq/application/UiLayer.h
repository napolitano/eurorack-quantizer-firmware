/**
 * @file UiLayer.h
 * Defines the persistent front-panel function layer.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_APPLICATION_UI_LAYER_H
#define FMQ_APPLICATION_UI_LAYER_H

#include <stdint.h>

namespace fmq {

/** Front-panel function map selected by the three-second SHIFT gesture. */
enum class UiLayer : uint8_t { Quantizer = 0, Arpeggiator = 1 };

}  // namespace fmq

#endif  // FMQ_APPLICATION_UI_LAYER_H
