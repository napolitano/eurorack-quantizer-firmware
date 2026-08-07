/**
 * @file BoardConfig.h
 * Selects and exposes the active hardware board profile.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_BOARD_CONFIG_H
#define FMQ_PLATFORM_NANO_BOARD_CONFIG_H
#include "platform/nano_atmega328p/BoardProfileOriginal.h"
#include "platform/nano_atmega328p/BoardProfilePrototypeAvcc.h"
namespace fmq::hwconfig {
// Select exactly one verified board profile here.
using ActiveBoard = fmq::board::Original;
constexpr uint8_t kPinDacChipSelect = ActiveBoard::dacCs;
constexpr uint8_t kPinLedChipSelect = ActiveBoard::ledCs;
constexpr uint8_t kPinLedBlank = ActiveBoard::ledBlank;
constexpr uint8_t kPinInputLedA = ActiveBoard::inputLedA;
constexpr uint8_t kPinOutputLedA = ActiveBoard::outputLedA;
constexpr uint8_t kPinInputLedB = ActiveBoard::inputLedB;
constexpr uint8_t kPinOutputLedB = ActiveBoard::outputLedB;
constexpr uint8_t kPinButtonLadder = ActiveBoard::ladder;
constexpr uint8_t kPinCvInputA = ActiveBoard::cvA;
constexpr uint8_t kPinCvInputB = ActiveBoard::cvB;
constexpr uint8_t kPinShift = ActiveBoard::shift;
constexpr uint8_t kPinSave = ActiveBoard::save;
constexpr uint8_t kPinLoad = ActiveBoard::load;
constexpr uint8_t kPinTrigInputA = ActiveBoard::trigInA;
constexpr uint8_t kPinTrigInputB = ActiveBoard::trigInB;
constexpr uint8_t kPinTrigOutputA = ActiveBoard::trigOutA;
constexpr uint8_t kPinTrigOutputB = ActiveBoard::trigOutB;
constexpr bool kUseExternalAref = ActiveBoard::externalAref;
constexpr bool kStatusLedsActiveHigh = ActiveBoard::statusLedsActiveHigh;
constexpr bool kTriggerInputsActiveHigh = ActiveBoard::triggerInputsActiveHigh;
constexpr bool kTriggerOutputsActiveHigh = ActiveBoard::triggerOutputsActiveHigh;
constexpr bool kButtonsActiveLow = ActiveBoard::buttonsActiveLow;
constexpr bool kButtonsUsePullups = ActiveBoard::buttonsUsePullups;
constexpr bool kTriggerInputsUsePullups = ActiveBoard::triggerInputsUsePullups;
}
#endif
