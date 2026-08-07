/**
 * @file MenuRenderer.cpp
 * Renders menu and quantizer state into logical note-ring colours.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/ui/Menu.h"
#include "fmq/config/ProductConfig.h"
#include "fmq/config/UiConfig.h"

namespace fmq {
namespace {
/// Render an unsigned scalar sub-menu: an amber anchor plus @p value green LEDs.
void renderUnsignedScalar(LedFrame &frame, uint8_t value) {
  if (value > kNoteCount - 1) {
    value = kNoteCount - 1;
  }
  frame[0] = LedColor::Amber;
  for (uint8_t i = 1; i <= value; ++i) {
    frame[i] = LedColor::Green;
  }
}

/// Render a signed scalar sub-menu: green for positive, red wrapping for
/// negative, anchored by an amber LED at index 0.
void renderSignedScalar(LedFrame &frame, int8_t value) {
  frame[0] = LedColor::Amber;
  if (value >= 0) {
    uint8_t v = static_cast<uint8_t>(value);
    if (v > kNoteCount - 1) {
      v = kNoteCount - 1;
    }
    for (uint8_t i = 1; i <= v; ++i) {
      frame[i] = LedColor::Green;
    }
  } else {
    // Negative values light the top of the ring red, matching the original's
    // `for i in (12 + n)..12` (n negative), which never touches index 0.
    int start = static_cast<int>(kNoteCount) + value;
    if (start < 0) {
      start = 0;
    }
    for (uint8_t i = static_cast<uint8_t>(start); i < kNoteCount; ++i) {
      frame[i] = LedColor::Red;
    }
  }
}

}  // namespace

LedFrame Menu::renderNotesDisplay(const QuantizerState &quantizer,
                                  const QuantizationResult &result) const {
  LedFrame frame;
  if (quantizer.channelsLinked) {
    // Show the shared scale; highlight both channels' current notes.
    const ChannelConfig &cfg = quantizer.channels[kChannelAIndex].config();
    for (uint8_t i = 0; i < kNoteCount; ++i) {
      frame[i] = cfg.notes[i] ? LedColor::Amber : LedColor::Off;
    }
    frame[static_cast<uint8_t>(result.channelB.nominalSemitones) % kNoteCount] =
        LedColor::Red;
    frame[static_cast<uint8_t>(result.channelA.nominalSemitones) % kNoteCount] =
        LedColor::Green;
    return frame;
  }

  const uint8_t ch = channelIndex();
  const ChannelConfig &cfg = quantizer.channels[ch].config();
  const LedColor selectedColor =
      (selectedChannel_ == UiChannel::A) ? LedColor::Green : LedColor::Red;
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    frame[i] = cfg.notes[i] ? selectedColor : LedColor::Off;
  }
  // The currently sounding note of the selected channel is shown amber.
  const int8_t nominal = (selectedChannel_ == UiChannel::A)
                             ? result.channelA.nominalSemitones
                             : result.channelB.nominalSemitones;
  frame[static_cast<uint8_t>(nominal) % kNoteCount] = LedColor::Amber;
  return frame;
}

LedFrame Menu::renderCurrentPage(const QuantizerState &quantizer,
                                 const QuantizationResult &result,
                                 uint32_t currentTimeMs) const {
  LedFrame frame;
  switch (page_) {
    case Page::MainMenu:
      return renderNotesDisplay(quantizer, result);

    case Page::ScalarSubMenu: {
      const ChannelConfig &cfg = quantizer.channels[channelIndex()].config();
      switch (subMenuWhich_) {
        case ScalarSubMenu::Glide:
          renderUnsignedScalar(frame, cfg.glideAmount);
          break;
        case ScalarSubMenu::Delay:
          renderUnsignedScalar(frame, cfg.triggerDelayAmount);
          break;
        case ScalarSubMenu::PreShift:
          renderSignedScalar(frame, cfg.preShift);
          break;
        case ScalarSubMenu::ScaleShift:
          renderSignedScalar(frame, cfg.scaleShift);
          break;
        case ScalarSubMenu::PostShift:
          renderSignedScalar(frame, cfg.postShift);
          break;
      }
      return frame;
    }

    case Page::ShowChangedBoolOption: {
      // A single indicator LED at the option's own button position shows the
      // resulting state after the toggle. For sample mode this deliberately
      // matches the Rust original: green = Track-and-Hold, red = Sample-and-Hold.
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
              frame[config::kSampleModeButtonIndex] = config::kEnableContinuousSampleModeInUi
                             ? LedColor::Amber
                             : LedColor::Green;
              break;
          }
          break;
        case BoolOption::RelativePitch:
          frame[config::kChannelBModeButtonIndex] =
              (quantizer.channelBMode == PitchMode::Relative)
                         ? LedColor::Green
                         : LedColor::Red;
          break;
        case BoolOption::ChannelsLinked:
          frame[config::kChannelLinkButtonIndex] =
              quantizer.channelsLinked ? LedColor::Green : LedColor::Red;
          break;
      }
      return frame;
    }

    case Page::SelectSaveSlot:
    case Page::SelectLoadSlot: {
      const LedColor color = slotColor();

      // Full-config LOAD always has something useful in all twelve positions:
      // user data when present, otherwise the corresponding factory preset.
      if (page_ == Page::SelectLoadSlot &&
          slotType_ == SaveSlotType::FullConfig) {
        for (uint8_t i = 0; i < kNoteCount; ++i) {
          frame[i] = color;
        }
        return frame;
      }

      // Scale slots and full-config SAVE keep showing actual user occupancy.
      const SlotOccupancy &occupancy =
          (slotType_ == SaveSlotType::Scale) ? scaleSlotsInUse_
                                             : configSlotsInUse_;
      for (uint8_t i = 0; i < kNoteCount; ++i) {
        if (occupancy.get(i)) {
          frame[i] = color;
        }
      }
      return frame;
    }

    case Page::ConfirmSaveSlot: {
      // Blink the saved slot's LED (~256 ms period) for the splash duration.
      const uint32_t elapsed = currentTimeMs - pageStartTime_;
      if (((elapsed / config::kSaveConfirmationBlinkHalfPeriodMs) % 2u) == 0u) {
        frame[confirmSlot_] = slotColor();
      }
      return frame;
    }

    case Page::ConfirmErase: {
      // Sweep a single amber LED across the ring as the "erased" animation.
      const uint32_t elapsed = currentTimeMs - pageStartTime_;
      uint32_t index = elapsed / config::kEraseSweepStepMs;
      if (index > kNoteCount) {
        index = kNoteCount;
      }
      index %= kNoteCount;
      frame[static_cast<uint8_t>(index)] = LedColor::Amber;
      return frame;
    }

    default:
      return frame;
  }
}

LedColor Menu::slotColor() const {
  if (slotType_ == SaveSlotType::FullConfig) {
    return LedColor::Amber;
  }
  return (selectedChannel_ == UiChannel::A) ? LedColor::Green : LedColor::Red;
}


}  // namespace fmq
