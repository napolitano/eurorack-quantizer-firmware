/**
 * @file PersistenceLayout.h
 * EEPROM address map for presets, live restore and startup metadata.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_PERSISTENCE_LAYOUT_H
#define FM_QUANTIZER_CORE_PERSISTENCE_LAYOUT_H

#include <stdint.h>

#include "fmq/persistence/Serialization.h"

namespace fmq {

constexpr uint8_t kRecordMarker = 0xA5;
constexpr uint8_t kErasedByte = 0xFF;
constexpr uint16_t kEepromSize = 1024;

// Twelve user scale slots. Kept deliberately compact.
constexpr uint8_t kScaleSlotCount = 12;
constexpr uint8_t kPreviousSaveFormatVersion = 4;
constexpr uint8_t kSaveFormatVersion = 5;
constexpr uint16_t kScaleSlotSize = 1 + 1 + kScaleBytes + 2;
constexpr uint16_t kScaleRegionBase = 0;
constexpr uint16_t kScaleRegionSize = kScaleSlotCount * kScaleSlotSize;

// Twelve full musical configurations. A full config now includes Quantizer
// state, both Arpeggiator configurations, selected channel and UI layer.
constexpr uint8_t kConfigSlotCount = 12;
constexpr uint16_t kConfigSlotSize = 1 + 1 + kStoredConfigurationBytes + 2;
constexpr uint16_t kConfigRegionBase = kScaleRegionBase + kScaleRegionSize;
constexpr uint16_t kConfigRegionSize = kConfigSlotCount * kConfigSlotSize;

// Wear-levelled live state. LED brightness is a hardware calibration value and
// is stored with live state, but not in musical full-configuration presets.
constexpr uint8_t kBrightnessBytes = 2;
// StoredConfiguration v5 uses a spare bit of the selected-channel byte for the
// UI layer. Live-state v6 adopts the same encoding; its physical record size
// remains unchanged, allowing v5 live records to be migrated in place.
constexpr uint8_t kPreviousLiveFormatVersion = 5;
constexpr uint8_t kLiveFormatVersion = 6;
constexpr uint16_t kLiveSlotSize =
    1 + 4 + 1 + kStoredConfigurationBytes + kBrightnessBytes + 2;
constexpr uint16_t kLiveRingBase = kConfigRegionBase + kConfigRegionSize;

constexpr uint16_t kStartupSequenceAddress = kEepromSize - 1u;
constexpr uint16_t kLiveSlotCount =
    (kStartupSequenceAddress - kLiveRingBase) / kLiveSlotSize;

static_assert(kStateBytes == 17, "quantizer state layout changed unexpectedly");
static_assert(kStoredConfigurationBytes == 32,
              "stored musical configuration layout changed unexpectedly");
static_assert(kLiveSlotCount >= 8,
              "live ring too small for effective wear levelling");
static_assert(kLiveRingBase + kLiveSlotCount * kLiveSlotSize <=
                  kStartupSequenceAddress,
              "live-state ring overlaps startup metadata");
static_assert(kStartupSequenceAddress < kEepromSize,
              "startup metadata address is outside EEPROM");

}  // namespace fmq

#endif
