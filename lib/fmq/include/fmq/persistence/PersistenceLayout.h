/**
 * @file PersistenceLayout.h
 * Defines the EEPROM record layout and persistence constants.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_PERSISTENCE_LAYOUT_H
#define FM_QUANTIZER_CORE_PERSISTENCE_LAYOUT_H

#include <stdint.h>

#include "fmq/persistence/Serialization.h"

/**
 * Address map and record framing for everything stored in EEPROM.
 *
 * The 1 KiB EEPROM contains three record regions plus one metadata byte:
 *
 *  1. Scale save slots   : 12 user slots holding a single channel's scale.
 *  2. Config save slots  : 12 user slots holding the full quantizer state.
 *  3. Live-state ring     : an optional wear-levelled working-state snapshot.
 *  4. Startup selector    : one byte storing which startup animation plays next.
 *
 * Every record is framed as `[marker][payload][crc16]`. The marker (0xA5)
 * distinguishes a written record from erased flash (0xFF); the CRC detects
 * corruption. This replaces the original firmware's single, unchecked sentinel
 * byte, which could silently mis-read a slot after a single bit flip.
 */
namespace fmq {

/// Marker byte identifying a written (non-erased) record.
constexpr uint8_t kRecordMarker = 0xA5;
/// Value of an erased EEPROM byte.
constexpr uint8_t kErasedByte = 0xFF;

/// Assumed EEPROM size (ATmega328P). The AVR layer asserts the real capacity.
constexpr uint16_t kEepromSize = 1024;

// --- Region 1: scale save slots ------------------------------------------
constexpr uint8_t kScaleSlotCount = 12;
constexpr uint8_t kSaveFormatVersion = 3;
/// marker(1) + version(1) + notes(2) + crc(2)
constexpr uint16_t kScaleSlotSize = 1 + 1 + kScaleBytes + 2;
constexpr uint16_t kScaleRegionBase = 0;
constexpr uint16_t kScaleRegionSize = kScaleSlotCount * kScaleSlotSize;

// --- Region 2: full-config save slots ------------------------------------
constexpr uint8_t kConfigSlotCount = 12;
/// marker(1) + version(1) + state(17) + crc(2)
constexpr uint16_t kConfigSlotSize = 1 + 1 + kStateBytes + 2;
constexpr uint16_t kConfigRegionBase = kScaleRegionBase + kScaleRegionSize;
constexpr uint16_t kConfigRegionSize = kConfigSlotCount * kConfigSlotSize;

// --- Region 3: wear-levelled live-state ring -----------------------------
/// Bytes of LED calibration stored alongside the live state (red + green step).
constexpr uint8_t kBrightnessBytes = 2;
/// Format-version byte carried in each live record for forward compatibility.
constexpr uint8_t kLiveFormatVersion = 4;
/// marker(1) + seq(4) + version(1) + state(17) + brightness(2) + crc(2)
constexpr uint16_t kLiveSlotSize = 1 + 4 + 1 + kStateBytes + kBrightnessBytes + 2;
constexpr uint16_t kLiveRingBase = kConfigRegionBase + kConfigRegionSize;

// --- Tail metadata ---------------------------------------------------------
// The final EEPROM byte stores the startup sequence that should play next.
// Erased (0xFF) or otherwise invalid values are interpreted as sequence 0.
constexpr uint16_t kStartupSequenceAddress = kEepromSize - 1u;

/// Number of live-state slots that fit before the reserved metadata byte.
constexpr uint16_t kLiveSlotCount =
    (kStartupSequenceAddress - kLiveRingBase) / kLiveSlotSize;

// Compile-time guarantees that the layout fits and is well-formed.
static_assert(kStateBytes == 17, "state layout changed unexpectedly");
static_assert(kLiveSlotCount >= 8, "live ring too small for effective wear-levelling");
static_assert(kLiveRingBase + kLiveSlotCount * kLiveSlotSize <=
                  kStartupSequenceAddress,
              "live-state ring overlaps startup metadata");
static_assert(kStartupSequenceAddress < kEepromSize,
              "startup metadata address is outside EEPROM");

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_PERSISTENCE_LAYOUT_H
