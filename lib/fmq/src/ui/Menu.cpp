/**
 * @file Menu.cpp
 * Implements Quantizer and Arpeggiator UI layers.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/ui/Menu.h"

#include <string.h>

#include "fmq/config/FactoryPresets.h"
#include "fmq/config/ProductConfig.h"
#include "fmq/config/UiConfig.h"
#include "fmq/persistence/Serialization.h"

namespace fmq {
namespace {

bool isButtonDown(LongPressButtonState state) {
  return state == LongPressButtonState::ButtonJustDown ||
         state == LongPressButtonState::ButtonHeldDownShort ||
         state == LongPressButtonState::ButtonHeldDownLong ||
         state == LongPressButtonState::ButtonJustClickedLong;
}

int8_t buttonIndexToSigned(uint8_t index) {
  return index <= config::kSignedShiftPositiveMaxButtonIndex
             ? static_cast<int8_t>(index)
             : static_cast<int8_t>(static_cast<int>(index) - kNoteCount);
}

void rotateNotesLeft(bool notes[kNoteCount]) {
  const bool first = notes[0];
  for (uint8_t i = 0; i + 1u < kNoteCount; ++i) notes[i] = notes[i + 1u];
  notes[kNoteCount - 1u] = first;
}

void rotateNotesRight(bool notes[kNoteCount]) {
  const bool last = notes[kNoteCount - 1u];
  for (uint8_t i = kNoteCount - 1u; i > 0u; --i) notes[i] = notes[i - 1u];
  notes[0] = last;
}

void rotateSelectedScale(QuantizerState &quantizer, uint8_t selectedIndex,
                         bool left) {
  for (uint8_t channel = 0u; channel < kChannelCount; ++channel) {
    if (!quantizer.channelsLinked && channel != selectedIndex) continue;
    bool *notes = quantizer.channels[channel].config().notes;
    if (left) rotateNotesLeft(notes); else rotateNotesRight(notes);
  }
}

LedColor calibrationPreviewColor(uint8_t ringIndex) {
  switch (ringIndex % 3u) {
    case 0u: return LedColor::Green;
    case 1u: return LedColor::Red;
    default: return LedColor::Amber;
  }
}

}  // namespace

Menu::Menu()
    : selectedChannel_(UiChannel::A),
      layer_(UiLayer::Quantizer),
      page_(Page::MainMenu),
      shiftWasPressed_(false),
      subMenuStatus_(ScalarSubMenuStatus::AwaitingFirstInput),
      subMenuWhich_(ScalarSubMenu::Glide),
      boolOptionWhich_(BoolOption::SampleMode),
      slotType_(SaveSlotType::Scale),
      confirmSlot_(0u),
      pageStartTime_(0u),
      committedBrightness_(BrightnessCalibration::makeDefault()),
      workingBrightness_(BrightnessCalibration::makeDefault()),
      selectedCalColor_(CalColor::Green),
      calibrationMarkerStep_(0u),
      calibrationMarkerStart_(kNoTime),
      comboEntryStart_(kNoTime),
      shiftAloneStart_(kNoTime) {}

void Menu::begin(const SaveSlotStore &store) {
  store.scan(scaleSlotsInUse_, configSlotsInUse_);
}

bool Menu::toggleLayer(QuantizerState &quantizer,
                       ArpeggiatorBank &arpeggiators,
                       uint32_t nowMs) {
  const bool enteringArpeggiator = layer_ == UiLayer::Quantizer;
  layer_ = enteringArpeggiator ? UiLayer::Arpeggiator : UiLayer::Quantizer;
  subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;

  bool musicalStateChanged = false;
  if (enteringArpeggiator) {
    if (quantizer.channelsLinked) {
      const bool bothAlreadyEnabled =
          arpeggiators.enabled(kChannelAIndex) &&
          arpeggiators.enabled(kChannelBIndex);
      ArpeggiatorConfig config = arpeggiators.config(kChannelAIndex);
      config.enabled = true;
      arpeggiators.applySelectedConfig(kChannelAIndex, true, config, nowMs);
      selectedChannel_ = UiChannel::A;
      musicalStateChanged = !bothAlreadyEnabled;
    } else {
      const uint8_t channel = channelIndex();
      musicalStateChanged = !arpeggiators.enabled(channel);
      arpeggiators.setEnabled(channel, true, nowMs);
    }
  } else {
    // Leaving the Arpeggiator layer is the explicit ARP-off gesture. Stop both
    // channels so the Quantizer layer cannot unexpectedly retain an arpeggio
    // that is no longer represented by the active SHIFT menu.
    musicalStateChanged = arpeggiators.enabled(kChannelAIndex) ||
                          arpeggiators.enabled(kChannelBIndex);
    arpeggiators.setEnabled(kChannelAIndex, false, nowMs);
    arpeggiators.setEnabled(kChannelBIndex, false, nowMs);
  }

  // Layer switching and SHIFT+C use the exact same visual language: two green
  // full-ring flashes for ON, two red full-ring flashes for OFF. Keeping one
  // renderer for both paths prevents the layer transition from masking or
  // contradicting the Arpeggiator enable state.
  page_ = Page::ShowChangedBoolOption;
  boolOptionWhich_ = BoolOption::ArpEnabled;
  pageStartTime_ = nowMs;
  return musicalStateChanged;
}

StoredConfiguration Menu::captureStoredConfiguration(
    const QuantizerState &quantizer,
    const ArpeggiatorBank &arpeggiators) const {
  StoredConfiguration stored;
  stored.quantizer = quantizer;
  stored.arpeggiators[kChannelAIndex] = arpeggiators.config(kChannelAIndex);
  stored.arpeggiators[kChannelBIndex] = arpeggiators.config(kChannelBIndex);
  stored.selectedChannelIndex = selectedChannelIndex();
  stored.uiLayer = layer_;
  return stored;
}

void Menu::applyStoredConfiguration(const StoredConfiguration &stored,
                                    QuantizerState &quantizer,
                                    ArpeggiatorBank &arpeggiators,
                                    uint32_t nowMs) {
  quantizer = stored.quantizer;
  arpeggiators.setConfig(kChannelAIndex,
                         stored.arpeggiators[kChannelAIndex], nowMs);
  arpeggiators.setConfig(kChannelBIndex,
                         stored.arpeggiators[kChannelBIndex], nowMs);
  if (quantizer.channelsLinked) {
    arpeggiators.linkFromA(nowMs);
    selectedChannel_ = UiChannel::A;
  } else {
    setSelectedChannelIndex(stored.selectedChannelIndex);
  }

  // A full configuration includes the front-panel function map. Restore it
  // side-effect free: loading a preset must not emulate the three-second
  // gesture, retrigger feedback or alter the already-restored ARP enable bits.
  UiLayer restoredLayer = stored.uiLayer;
  if (restoredLayer == UiLayer::Quantizer &&
      (arpeggiators.enabled(kChannelAIndex) ||
       arpeggiators.enabled(kChannelBIndex))) {
    // Defensive migration for callers that provide a pre-layer in-memory
    // StoredConfiguration instead of a record decoded by Serialization.cpp.
    restoredLayer = UiLayer::Arpeggiator;
  }
  restoreLayer(restoredLayer);
}

void Menu::toggleLink(QuantizerState &quantizer,
                      ArpeggiatorBank &arpeggiators,
                      uint32_t nowMs) {
  quantizer.channelsLinked = !quantizer.channelsLinked;
  if (quantizer.channelsLinked) {
    quantizer.channels[kChannelBIndex].config() =
        quantizer.channels[kChannelAIndex].config();
    arpeggiators.linkFromA(nowMs);
    selectedChannel_ = UiChannel::A;
  }
  page_ = Page::ShowChangedBoolOption;
  boolOptionWhich_ = BoolOption::ChannelsLinked;
}

void Menu::handleQuantizerShiftButtonPress(
    QuantizerState &quantizer, ArpeggiatorBank &arpeggiators,
    uint8_t buttonIndex, uint32_t nowMs) {
  const uint8_t channel = channelIndex();
  ChannelConfig &channelConfig = quantizer.channels[channel].config();

  switch (buttonIndex) {
    case config::kRotateScaleLeftButtonIndex:
      rotateSelectedScale(quantizer, channel, true);
      break;
    case config::kRotateScaleRightButtonIndex:
      rotateSelectedScale(quantizer, channel, false);
      break;
    case config::kGlideButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::Glide;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kTriggerDelayButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::Delay;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kSampleModeButtonIndex:
      if (config::kEnableContinuousSampleModeInUi) {
        if (channelConfig.sampleMode == SampleMode::TrackAndHold) {
          channelConfig.sampleMode = SampleMode::SampleAndHold;
        } else if (channelConfig.sampleMode == SampleMode::SampleAndHold) {
          channelConfig.sampleMode = SampleMode::Continuous;
        } else {
          channelConfig.sampleMode = SampleMode::TrackAndHold;
        }
      } else {
        channelConfig.sampleMode =
            channelConfig.sampleMode == SampleMode::TrackAndHold
                ? SampleMode::SampleAndHold
                : SampleMode::TrackAndHold;
      }
      page_ = Page::ShowChangedBoolOption;
      boolOptionWhich_ = BoolOption::SampleMode;
      break;
    case config::kPostShiftButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::PostShift;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kScaleShiftButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::ScaleShift;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kPreShiftButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::PreShift;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kChannelBModeButtonIndex:
      quantizer.channelBMode = quantizer.channelBMode == PitchMode::Relative
                                   ? PitchMode::Absolute
                                   : PitchMode::Relative;
      page_ = Page::ShowChangedBoolOption;
      boolOptionWhich_ = BoolOption::RelativePitch;
      break;
    case config::kChannelLinkButtonIndex:
      toggleLink(quantizer, arpeggiators, nowMs);
      break;
    case config::kChannelAButtonIndex:
      if (!quantizer.channelsLinked) selectedChannel_ = UiChannel::A;
      break;
    case config::kChannelBButtonIndex:
      if (!quantizer.channelsLinked) selectedChannel_ = UiChannel::B;
      break;
    default:
      break;
  }
}

void Menu::handleArpeggiatorShiftButtonPress(
    QuantizerState &quantizer, ArpeggiatorBank &arpeggiators,
    uint8_t buttonIndex, uint32_t nowMs) {
  switch (buttonIndex) {
    case config::kArpEnableButtonIndex:
      (void)arpeggiators.toggleSelected(channelIndex(), quantizer.channelsLinked,
                                        nowMs);
      page_ = Page::ShowChangedBoolOption;
      boolOptionWhich_ = BoolOption::ArpEnabled;
      break;
    case config::kArpRateButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::ArpRate;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kArpPatternButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::ArpPattern;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kArpShapeButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::ArpShape;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kArpLengthButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::ArpLength;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kArpRangeButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::ArpRange;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kArpStepTriggerButtonIndex: {
      ArpeggiatorConfig arp = arpeggiators.config(channelIndex());
      arp.stepTrigger = !arp.stepTrigger;
      arpeggiators.applySelectedConfig(channelIndex(), quantizer.channelsLinked,
                                       arp, nowMs);
      page_ = Page::ShowChangedBoolOption;
      boolOptionWhich_ = BoolOption::ArpStepTrigger;
      break;
    }
    case config::kArpSyncButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::ArpSync;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kArpSwingButtonIndex:
      page_ = Page::ScalarSubMenu;
      subMenuWhich_ = ScalarSubMenu::ArpSwing;
      subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
      break;
    case config::kChannelLinkButtonIndex:
      toggleLink(quantizer, arpeggiators, nowMs);
      break;
    case config::kChannelAButtonIndex:
      if (!quantizer.channelsLinked) selectedChannel_ = UiChannel::A;
      break;
    case config::kChannelBButtonIndex:
      if (!quantizer.channelsLinked) selectedChannel_ = UiChannel::B;
      break;
    default:
      break;
  }
}

void Menu::applyScalarSelection(QuantizerState &quantizer,
                                ArpeggiatorBank &arpeggiators,
                                uint8_t buttonIndex, uint32_t nowMs) {
  const int8_t signedValue = buttonIndexToSigned(buttonIndex);
  switch (subMenuWhich_) {
    case ScalarSubMenu::Glide:
    case ScalarSubMenu::Delay:
    case ScalarSubMenu::PreShift:
    case ScalarSubMenu::ScaleShift:
    case ScalarSubMenu::PostShift:
      for (uint8_t channel = 0u; channel < kChannelCount; ++channel) {
        if (!quantizer.channelsLinked && channel != channelIndex()) continue;
        ChannelConfig &cfg = quantizer.channels[channel].config();
        switch (subMenuWhich_) {
          case ScalarSubMenu::Glide: cfg.glideAmount = buttonIndex; break;
          case ScalarSubMenu::Delay: cfg.triggerDelayAmount = buttonIndex; break;
          case ScalarSubMenu::PreShift: cfg.preShift = signedValue; break;
          case ScalarSubMenu::ScaleShift: cfg.scaleShift = signedValue; break;
          case ScalarSubMenu::PostShift: cfg.postShift = signedValue; break;
          default: break;
        }
      }
      return;

    case ScalarSubMenu::ArpRate:
    case ScalarSubMenu::ArpPattern:
    case ScalarSubMenu::ArpShape:
    case ScalarSubMenu::ArpLength:
    case ScalarSubMenu::ArpRange:
    case ScalarSubMenu::ArpSync:
    case ScalarSubMenu::ArpSwing: {
      ArpeggiatorConfig arp = arpeggiators.config(channelIndex());
      bool valid = true;
      switch (subMenuWhich_) {
        case ScalarSubMenu::ArpRate:
          arp.rateIndex = buttonIndex;
          break;
        case ScalarSubMenu::ArpPattern:
          if (buttonIndex >= Arpeggiator::patternCount()) valid = false;
          else arp.pattern = static_cast<ArpeggiatorPattern>(buttonIndex);
          break;
        case ScalarSubMenu::ArpShape:
          if (buttonIndex >= Arpeggiator::shapeCount()) valid = false;
          else arp.shape = static_cast<ArpeggiatorShape>(buttonIndex);
          break;
        case ScalarSubMenu::ArpLength:
          arp.length = static_cast<uint8_t>(buttonIndex + 1u);
          break;
        case ScalarSubMenu::ArpRange:
          if (buttonIndex >= config::kArpMaximumRange) valid = false;
          else arp.range = static_cast<uint8_t>(buttonIndex + 1u);
          break;
        case ScalarSubMenu::ArpSync:
          if (buttonIndex >= Arpeggiator::syncModeCount()) valid = false;
          else arp.syncMode = static_cast<ArpeggiatorSyncMode>(buttonIndex);
          break;
        case ScalarSubMenu::ArpSwing:
          arp.swing = buttonIndex;
          break;
        default:
          valid = false;
          break;
      }
      if (valid) {
        arpeggiators.applySelectedConfig(channelIndex(), quantizer.channelsLinked,
                                         arp, nowMs);
      }
      return;
    }
  }
}

MenuOutput Menu::update(QuantizerState &quantizer,
                        ArpeggiatorBank &arpeggiators,
                        const MenuInput &input,
                        const QuantizationResult &result,
                        uint32_t currentTimeMs,
                        SaveSlotStore &store) {
  if (inCalibration()) {
    MenuOutput out = handleCalibration(input, currentTimeMs);
    shiftWasPressed_ = input.shiftPressed;
    return out;
  }

  uint8_t before[kStoredConfigurationBytes];
  encodeStoredConfiguration(captureStoredConfiguration(quantizer, arpeggiators),
                            before);

  const bool saveDown = isButtonDown(input.saveButton);
  const bool loadDown = isButtonDown(input.loadButton);
  const bool comboActive = input.shiftPressed && saveDown && loadDown;
  if (comboActive) {
    if (comboEntryStart_ == kNoTime) {
      comboEntryStart_ = currentTimeMs;
      page_ = Page::MainMenu;
    } else if (currentTimeMs - comboEntryStart_ >=
               config::kCalibrationEnterHoldMs) {
      enterCalibration(currentTimeMs);
      comboEntryStart_ = kNoTime;
      shiftWasPressed_ = input.shiftPressed;
      MenuOutput out = handleCalibration(input, currentTimeMs);
      out.persistentStateChanged = false;
      return out;
    }
    shiftWasPressed_ = input.shiftPressed;
    MenuOutput out;
    // Layer selection changes the SHIFT menu, not the primary scale display.
    // Keep the musical scale visible in both UI layers.
    out.frame = renderNotesDisplay(quantizer, result);
    out.persistentStateChanged = false;
    return out;
  }
  comboEntryStart_ = kNoTime;

  if (shiftWasPressed_ && !input.shiftPressed && page_ == Page::ScalarSubMenu) {
    switch (subMenuStatus_) {
      case ScalarSubMenuStatus::AwaitingFirstInput:
        subMenuStatus_ = ScalarSubMenuStatus::ExitOnShiftRelease;
        break;
      case ScalarSubMenuStatus::ExitOnShiftRelease:
        page_ = Page::MainMenu;
        break;
      case ScalarSubMenuStatus::ExitOnButtonRelease:
        break;
    }
  }
  shiftWasPressed_ = input.shiftPressed;

  if (input.saveButton == LongPressButtonState::ButtonJustDown) {
    if (page_ == Page::SelectSaveSlot || page_ == Page::SelectLoadSlot) {
      page_ = Page::MainMenu;
    } else if (page_ != Page::ConfirmSaveSlot) {
      page_ = Page::SelectSaveSlot;
      slotType_ = input.shiftPressed ? SaveSlotType::FullConfig
                                     : SaveSlotType::Scale;
    }
  } else if (input.loadButton == LongPressButtonState::ButtonJustDown) {
    if (page_ == Page::SelectSaveSlot || page_ == Page::SelectLoadSlot) {
      page_ = Page::MainMenu;
    } else if (page_ != Page::ConfirmSaveSlot) {
      page_ = Page::SelectLoadSlot;
      slotType_ = input.shiftPressed ? SaveSlotType::FullConfig
                                     : SaveSlotType::Scale;
    }
  } else if (!input.shiftPressed && loadDown && saveDown &&
             (input.loadButton == LongPressButtonState::ButtonHeldDownLong ||
              input.loadButton == LongPressButtonState::ButtonJustClickedLong) &&
             (input.saveButton == LongPressButtonState::ButtonHeldDownLong ||
              input.saveButton == LongPressButtonState::ButtonJustClickedLong)) {
    if (store.eraseAll()) {
      scaleSlotsInUse_ = SlotOccupancy();
      configSlotsInUse_ = SlotOccupancy();
      page_ = Page::ConfirmErase;
      pageStartTime_ = currentTimeMs;
    }
  }

  if (input.keyEvent.type == ButtonEventType::JustPressed) {
    const uint8_t button = input.keyEvent.index;
    switch (page_) {
      case Page::ScalarSubMenu:
        applyScalarSelection(quantizer, arpeggiators, button, currentTimeMs);
        switch (subMenuStatus_) {
          case ScalarSubMenuStatus::AwaitingFirstInput:
            subMenuStatus_ = input.shiftPressed
                                 ? ScalarSubMenuStatus::ExitOnShiftRelease
                                 : ScalarSubMenuStatus::ExitOnButtonRelease;
            break;
          case ScalarSubMenuStatus::ExitOnShiftRelease:
            subMenuStatus_ = input.shiftPressed
                                 ? ScalarSubMenuStatus::ExitOnShiftRelease
                                 : ScalarSubMenuStatus::ExitOnButtonRelease;
            break;
          case ScalarSubMenuStatus::ExitOnButtonRelease:
            page_ = Page::MainMenu;
            break;
        }
        break;

      case Page::MainMenu:
        if (input.shiftPressed) {
          // Both UI layers use the same interaction grammar. SHIFT selects a
          // function; only the function map changes with the active layer.
          if (layer_ == UiLayer::Arpeggiator) {
            handleArpeggiatorShiftButtonPress(quantizer, arpeggiators, button,
                                               currentTimeMs);
          } else {
            handleQuantizerShiftButtonPress(quantizer, arpeggiators, button,
                                             currentTimeMs);
          }
          if (page_ == Page::ShowChangedBoolOption) {
            pageStartTime_ = currentTimeMs;
          }
        } else {
          // The unmodified note keys always edit the selected scale, regardless
          // of UI layer. This keeps the front panel visually and behaviorally
          // consistent while the Arpeggiator is active.
          for (uint8_t channel = 0u; channel < kChannelCount; ++channel) {
            if (!quantizer.channelsLinked && channel != channelIndex()) continue;
            ChannelConfig &cfg = quantizer.channels[channel].config();
            if (cfg.notes[button]) {
              uint8_t enabled = 0u;
              for (uint8_t i = 0u; i < kNoteCount; ++i) {
                if (cfg.notes[i]) ++enabled;
              }
              if (enabled > 1u) cfg.notes[button] = false;
            } else {
              cfg.notes[button] = true;
            }
          }
        }
        break;

      case Page::SelectSaveSlot: {
        bool queued = false;
        if (slotType_ == SaveSlotType::Scale) {
          queued = store.writeScale(
              button, quantizer.channels[channelIndex()].config().notes);
          if (queued) scaleSlotsInUse_.set(button, true);
        } else {
          const StoredConfiguration stored =
              captureStoredConfiguration(quantizer, arpeggiators);
          queued = store.writeConfig(button, stored);
          if (queued) configSlotsInUse_.set(button, true);
        }
        if (queued) {
          page_ = Page::ConfirmSaveSlot;
          confirmSlot_ = button;
          pageStartTime_ = currentTimeMs;
        }
        break;
      }

      case Page::SelectLoadSlot:
        if (store.busy()) break;
        if (slotType_ == SaveSlotType::Scale) {
          bool notes[kNoteCount];
          if (store.readScale(button, notes)) {
            for (uint8_t channel = 0u; channel < kChannelCount; ++channel) {
              if (!quantizer.channelsLinked && channel != channelIndex()) continue;
              ChannelConfig &cfg = quantizer.channels[channel].config();
              for (uint8_t i = 0u; i < kNoteCount; ++i) cfg.notes[i] = notes[i];
            }
          }
        } else {
          StoredConfiguration loaded;
          if (!store.readConfig(button, loaded)) {
            loaded.quantizer = config::makeFactoryConfigPreset(button);
            loaded.arpeggiators[kChannelAIndex] = ArpeggiatorConfig::makeDefault();
            loaded.arpeggiators[kChannelBIndex] = ArpeggiatorConfig::makeDefault();
            loaded.selectedChannelIndex = kChannelAIndex;
          }
          applyStoredConfiguration(loaded, quantizer, arpeggiators,
                                   currentTimeMs);
        }
        page_ = Page::MainMenu;
        break;

      default:
        break;
    }
  } else if (input.keyEvent.type == ButtonEventType::JustReleased) {
    if (page_ == Page::ScalarSubMenu &&
        subMenuStatus_ == ScalarSubMenuStatus::ExitOnButtonRelease) {
      page_ = Page::MainMenu;
    }
  }

  if (page_ == Page::ConfirmSaveSlot &&
      currentTimeMs - pageStartTime_ >= config::kSaveConfirmationMs) {
    page_ = Page::MainMenu;
  }
  if (page_ == Page::ConfirmErase &&
      currentTimeMs - pageStartTime_ >= config::kEraseConfirmationMs) {
    page_ = Page::MainMenu;
  }
  if (page_ == Page::ShowChangedBoolOption) {
    const uint32_t feedbackDurationMs =
        boolOptionWhich_ == BoolOption::ArpEnabled
            ? config::kArpToggleFeedbackMs
            : config::kBoolOptionFeedbackMs;
    if (currentTimeMs - pageStartTime_ >= feedbackDurationMs) {
      page_ = Page::MainMenu;
    }
  }

  MenuOutput out;
  out.frame = renderCurrentPage(quantizer, arpeggiators, result, currentTimeMs);
  uint8_t after[kStoredConfigurationBytes];
  encodeStoredConfiguration(captureStoredConfiguration(quantizer, arpeggiators),
                            after);
  out.persistentStateChanged =
      memcmp(before, after, kStoredConfigurationBytes) != 0;
  return out;
}

void Menu::enterCalibration(uint32_t currentTimeMs) {
  page_ = Page::CalibrationEntering;
  pageStartTime_ = currentTimeMs;
  workingBrightness_ = committedBrightness_;
  selectedCalColor_ = CalColor::Green;
  calibrationMarkerStep_ = 0u;
  calibrationMarkerStart_ = kNoTime;
  shiftAloneStart_ = kNoTime;
}

MenuOutput Menu::handleCalibration(const MenuInput &input,
                                   uint32_t currentTimeMs) {
  MenuOutput out;
  out.persistentStateChanged = false;
  const LedColor color = selectedCalColor_ == CalColor::Red
                             ? LedColor::Red
                             : LedColor::Green;

  switch (page_) {
    case Page::CalibrationEntering: {
      if (currentTimeMs - pageStartTime_ >= config::kCalibrationEnterBlinkMs) {
        page_ = Page::CalibrationActive;
      } else {
        const bool on = ((currentTimeMs - pageStartTime_) /
                         config::kMenuBlinkHalfPeriodMs) % 2u == 0u;
        if (on) {
          for (uint8_t i = 0u; i < kNoteCount; ++i) out.frame[i] = color;
        }
      }
      return out;
    }

    case Page::CalibrationActive: {
      const bool notePressed = input.keyEvent.type == ButtonEventType::JustPressed ||
                               input.keyEvent.type == ButtonEventType::Held;
      if (input.keyEvent.type == ButtonEventType::JustPressed) {
        const uint8_t button = input.keyEvent.index;
        if (input.shiftPressed) {
          if (button == config::kChannelAButtonIndex) selectedCalColor_ = CalColor::Green;
          else if (button == config::kChannelBButtonIndex) selectedCalColor_ = CalColor::Red;
        } else {
          if (selectedCalColor_ == CalColor::Red) workingBrightness_.redStep = button;
          else workingBrightness_.greenStep = button;
          calibrationMarkerStep_ = button;
          calibrationMarkerStart_ = currentTimeMs;
        }
      }

      const bool shiftAlone = input.shiftPressed && !notePressed;
      if (shiftAlone) {
        if (shiftAloneStart_ == kNoTime) {
          shiftAloneStart_ = currentTimeMs;
        } else if (currentTimeMs - shiftAloneStart_ >=
                   config::kCalibrationSaveHoldMs) {
          committedBrightness_ = workingBrightness_;
          page_ = Page::CalibrationSaving;
          pageStartTime_ = currentTimeMs;
          shiftAloneStart_ = kNoTime;
          out.persistentStateChanged = true;
        }
      } else {
        shiftAloneStart_ = kNoTime;
      }

      for (uint8_t ringIndex = 0u; ringIndex < kNoteCount; ++ringIndex) {
        out.frame[ringIndex] = calibrationPreviewColor(ringIndex);
      }
      if (calibrationMarkerStart_ != kNoTime) {
        const uint32_t elapsed = currentTimeMs - calibrationMarkerStart_;
        if (elapsed < config::kCalibrationStepMarkerMs) {
          out.frame[calibrationMarkerStep_] = LedColor::Off;
        } else {
          calibrationMarkerStart_ = kNoTime;
        }
      }
      return out;
    }

    case Page::CalibrationSaving: {
      if (currentTimeMs - pageStartTime_ >= config::kCalibrationSaveBlinkMs) {
        page_ = Page::MainMenu;
      } else {
        const bool on = ((currentTimeMs - pageStartTime_) /
                         config::kMenuBlinkHalfPeriodMs) % 2u == 0u;
        if (on) {
          for (uint8_t i = 0u; i < kNoteCount; ++i) out.frame[i] = color;
        }
      }
      return out;
    }

    default:
      return out;
  }
}

}  // namespace fmq
