/**
 * @file PersistenceConfig.h
 * Central configuration for EEPROM persistence timing and behaviour.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_CONFIG_PERSISTENCE_CONFIG_H
#define FMQ_CONFIG_PERSISTENCE_CONFIG_H
#include <stdint.h>
namespace fmq::config { constexpr uint32_t kLiveAutosaveQuiescenceMs = 3000; }
#endif
