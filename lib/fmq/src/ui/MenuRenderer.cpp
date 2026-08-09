/**
 * @file MenuRenderer.cpp
 * Renders Quantizer/Arpeggiator UI state into logical ring colours.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/ui/Menu.h"

#include "fmq/config/ProductConfig.h"
#include "fmq/config/UiConfig.h"

namespace fmq {
namespace {

void renderUnsignedScalar(LedFrame &frame, uint8_t value) {
  if (value > kNoteCount - 1u) value = kNoteCount - 1u;
  frame[0] = LedColor::Amber;
  for (uint8_t i = 1u; i <= value; ++i) frame[i] = LedColor::Green;
}

void renderSignedScalar(LedFrame &frame, int8_t value) {
  frame[0] = LedColor::Amber;
  if (value >= 0) {
    uint8_t v = static_cast<uint8_t>(value);
    if (v > kNoteCount - 1u) v = kNoteCount - 1u;
    for (uint8_t i = 1u; i <= v; ++i) frame[i] = LedColor::Green;
  } else {
    int start = static_cast<int>(kNoteCount) + value;
    if (start < 0) start = 0;
    for (uint8_t i = static_cast<uint8_t>(start); i < kNoteCount; ++i) {
      frame[i] = LedColor::Red;
    }
  }
}

LedColor channelColor(uint8_t channel) {
  return channel == kChannelAIndex ? LedColor::Green : LedColor::Red;
}

void renderSelectedIndex(LedFrame &frame, uint8_t index, uint8_t channel) {
  if (index < kNoteCount) frame[index] = channelColor(channel);
}

}  // namespace

LedFrame Menu::renderNotesDisplay(const QuantizerState &quantizer,
                                  const QuantizationResult &result) const {
  LedFrame frame;
  if (quantizer.channelsLinked) {
    const ChannelConfig &cfg = quantizer.channels[kChannelAIndex].config();
    for (uint8_t i = 0u; i < kNoteCount; ++i) {
      frame[i] = cfg.notes[i] ? LedColor::Amber : LedColor::Off;
    }
    frame[static_cast<uint8_t>(result.channelB.nominalSemitones) % kNoteCount] =
        LedColor::Red;
    frame[static_cast<uint8_t>(result.channelA.nominalSemitones) % kNoteCount] =
        LedColor::Green;
    return frame;
  }

  const uint8_t channel = channelIndex();
  const ChannelConfig &cfg = quantizer.channels[channel].config();
  const LedColor selected = channelColor(channel);
  for (uint8_t i = 0u; i < kNoteCount; ++i) {
    frame[i] = cfg.notes[i] ? selected : LedColor::Off;
  }
  const int8_t nominal = selectedChannel_ == UiChannel::A
                             ? result.channelA.nominalSemitones
                             : result.channelB.nominalSemitones;
  frame[static_cast<uint8_t>(nominal) % kNoteCount] = LedColor::Amber;
  return frame;
}

LedFrame Menu::renderCurrentPage(
    const QuantizerState &quantizer,
    const ArpeggiatorBank &arpeggiators,
    const QuantizationResult &result,
    uint32_t currentTimeMs) const {
  LedFrame frame;
  switch (page_) {
    case Page::MainMenu:
      // The active layer changes only the SHIFT function map. The normal ring
      // always remains the scale/quantized-note display.
      return renderNotesDisplay(quantizer, result);

    case Page::ScalarSubMenu: {
      const ChannelConfig &cfg = quantizer.channels[channelIndex()].config();
      const ArpeggiatorConfig &arp = arpeggiators.config(channelIndex());
      switch (subMenuWhich_) {
        case ScalarSubMenu::Glide: renderUnsignedScalar(frame, cfg.glideAmount); break;
        case ScalarSubMenu::Delay: renderUnsignedScalar(frame, cfg.triggerDelayAmount); break;
        case ScalarSubMenu::PreShift: renderSignedScalar(frame, cfg.preShift); break;
        case ScalarSubMenu::ScaleShift: renderSignedScalar(frame, cfg.scaleShift); break;
        case ScalarSubMenu::PostShift: renderSignedScalar(frame, cfg.postShift); break;
        case ScalarSubMenu::ArpRate:
          renderSelectedIndex(frame, arp.rateIndex, channelIndex());
          break;
        case ScalarSubMenu::ArpPattern:
          renderSelectedIndex(frame, static_cast<uint8_t>(arp.pattern), channelIndex());
          break;
        case ScalarSubMenu::ArpShape:
          renderSelectedIndex(frame, static_cast<uint8_t>(arp.shape), channelIndex());
          break;
        case ScalarSubMenu::ArpLength:
          renderSelectedIndex(frame, static_cast<uint8_t>(arp.length - 1u), channelIndex());
          break;
        case ScalarSubMenu::ArpRange:
          renderSelectedIndex(frame, static_cast<uint8_t>(arp.range - 1u), channelIndex());
          break;
        case ScalarSubMenu::ArpSync:
          renderSelectedIndex(frame, static_cast<uint8_t>(arp.syncMode), channelIndex());
          break;
        case ScalarSubMenu::ArpSwing:
          renderSelectedIndex(frame, arp.swing, channelIndex());
          break;
      }
      return frame;
    }

    case Page::ShowChangedBoolOption:
      switch (boolOptionWhich_) {
        case BoolOption::SampleMode:
          switch (quantizer.channels[channelIndex()].config().sampleMode) {
            case SampleMode::TrackAndHold:
              frame[config::kSampleModeButtonIndex] = LedColor::Green;
              break;
            case SampleMode::SampleAndHold:
              frame[config::kSampleModeButtonIndex] = LedColor::Red;
              break;
            case SampleMode::Continuous:
              frame[config::kSampleModeButtonIndex] =
                  config::kEnableContinuousSampleModeInUi ? LedColor::Amber
                                                          : LedColor::Green;
              break;
          }
          break;
        case BoolOption::RelativePitch:
          frame[config::kChannelBModeButtonIndex] =
              quantizer.channelBMode == PitchMode::Relative ? LedColor::Green
                                                             : LedColor::Red;
          break;
        case BoolOption::ChannelsLinked:
          frame[config::kChannelLinkButtonIndex] =
              quantizer.channelsLinked ? LedColor::Green : LedColor::Red;
          break;
        case BoolOption::ArpEnabled: {
          const uint32_t elapsed = currentTimeMs - pageStartTime_;
          const uint32_t phase =
              elapsed / config::kArpToggleBlinkHalfPeriodMs;
          const bool illuminated = (phase % 2u) == 0u;
          if (illuminated) {
            const LedColor color = arpeggiators.enabled(channelIndex())
                                       ? LedColor::Green
                                       : LedColor::Red;
            for (uint8_t i = 0u; i < kNoteCount; ++i) {
              frame[i] = color;
            }
          }
          break;
        }
        case BoolOption::ArpStepTrigger:
          frame[config::kArpStepTriggerButtonIndex] =
              arpeggiators.config(channelIndex()).stepTrigger ? LedColor::Green
                                                              : LedColor::Red;
          break;
      }
      return frame;

    case Page::SelectSaveSlot:
    case Page::SelectLoadSlot: {
      const LedColor color = slotColor();
      if (page_ == Page::SelectLoadSlot && slotType_ == SaveSlotType::FullConfig) {
        for (uint8_t i = 0u; i < kNoteCount; ++i) frame[i] = color;
        return frame;
      }
      const SlotOccupancy &occupancy = slotType_ == SaveSlotType::Scale
                                           ? scaleSlotsInUse_
                                           : configSlotsInUse_;
      for (uint8_t i = 0u; i < kNoteCount; ++i) {
        if (occupancy.get(i)) frame[i] = color;
      }
      return frame;
    }

    case Page::ConfirmSaveSlot: {
      const uint32_t elapsed = currentTimeMs - pageStartTime_;
      if (((elapsed / config::kSaveConfirmationBlinkHalfPeriodMs) % 2u) == 0u) {
        frame[confirmSlot_] = slotColor();
      }
      return frame;
    }

    case Page::ConfirmErase: {
      const uint32_t elapsed = currentTimeMs - pageStartTime_;
      uint32_t index = elapsed / config::kEraseSweepStepMs;
      index %= kNoteCount;
      frame[static_cast<uint8_t>(index)] = LedColor::Amber;
      return frame;
    }

    default:
      return frame;
  }
}

LedColor Menu::slotColor() const {
  if (slotType_ == SaveSlotType::FullConfig) return LedColor::Amber;
  return selectedChannel_ == UiChannel::A ? LedColor::Green : LedColor::Red;
}

}  // namespace fmq
