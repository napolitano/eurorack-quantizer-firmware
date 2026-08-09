/**
 * @file StoredConfiguration.h
 * Persistable musical working state shared by live restore and full presets.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_APPLICATION_STORED_CONFIGURATION_H
#define FMQ_APPLICATION_STORED_CONFIGURATION_H

#include <stdint.h>

#include "fmq/application/Arpeggiator.h"
#include "fmq/application/UiLayer.h"
#include "fmq/domain/Quantizer.h"

namespace fmq {

struct StoredConfiguration {
  QuantizerState quantizer;
  ArpeggiatorConfig arpeggiators[kChannelCount];
  uint8_t selectedChannelIndex;
  UiLayer uiLayer;

  StoredConfiguration()
      : quantizer(),
        arpeggiators{ArpeggiatorConfig::makeDefault(),
                    ArpeggiatorConfig::makeDefault()},
        selectedChannelIndex(kChannelAIndex),
        uiLayer(UiLayer::Quantizer) {}
};

}  // namespace fmq

#endif
