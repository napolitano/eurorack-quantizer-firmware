/**
 * @file Menu.cpp
 * Implements menu input handling and quantizer configuration changes.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/ui/Menu.h"

#include <string.h>

#include "fmq/persistence/Serialization.h"
#include "fmq/config/FactoryPresets.h"
#include "fmq/config/UiConfig.h"
#include "fmq/config/ProductConfig.h"

namespace fmq {

namespace {

/// @return true while the button is physically down in any of its states.
bool isButtonDown(LongPressButtonState s) {
  return s == LongPressButtonState::ButtonJustDown ||
         s == LongPressButtonState::ButtonHeldDownShort ||
         s == LongPressButtonState::ButtonHeldDownLong ||
         s == LongPressButtonState::ButtonJustClickedLong;
}

/// Map a note-button index (0..11) to the signed value used by shift menus.
int8_t buttonIndexToSigned(uint8_t index) {
  return index <= config::kSignedShiftPositiveMaxButtonIndex
             ? static_cast<int8_t>(index)
             : static_cast<int8_t>(static_cast<int>(index) - kNoteCount);
}

/// Rotate the scale one pitch class "left" (element 0 moves to the end),
/// matching the original firmware's `rotate_left(1)`.
void rotateNotesLeft(bool notes[kNoteCount]) {
  const bool first = notes[0];
  for (uint8_t i = 0; i + 1 < kNoteCount; ++i) {
    notes[i] = notes[i + 1];
  }
  notes[kNoteCount - 1] = first;
}

/// Rotate the scale one pitch class "right" (last element moves to the front),
/// matching the original firmware's `rotate_right(1)`.
void rotateNotesRight(bool notes[kNoteCount]) {
  const bool last = notes[kNoteCount - 1];
  for (uint8_t i = kNoteCount - 1; i > 0; --i) {
    notes[i] = notes[i - 1];
  }
  notes[0] = last;
}

/// Apply a scale rotation to the selected channel, or to both when linked.
void rotateSelectedScale(QuantizerState &quantizer, uint8_t selectedIndex,
                         bool left) {
  for (uint8_t c = 0; c < 2; ++c) {
    if (!quantizer.channelsLinked && c != selectedIndex) {
      continue;
    }
    bool *notes = quantizer.channels[c].config().notes;
    if (left) {
      rotateNotesLeft(notes);
    } else {
      rotateNotesRight(notes);
    }
  }
}


}  // namespace

Menu::Menu()
    : selectedChannel_(UiChannel::A),
      page_(Page::MainMenu),
      shiftWasPressed_(false),
      subMenuStatus_(ScalarSubMenuStatus::AwaitingFirstInput),
      subMenuWhich_(ScalarSubMenu::Glide),
      boolOptionWhich_(BoolOption::SampleMode),
      slotType_(SaveSlotType::Scale),
      confirmSlot_(0),
      pageStartTime_(0),
      committedBrightness_(BrightnessCalibration::makeDefault()),
      workingBrightness_(BrightnessCalibration::makeDefault()),
      selectedCalColor_(CalColor::Green),
      comboEntryStart_(kNoTime),
      shiftAloneStart_(kNoTime) {}

void Menu::begin(const SaveSlotStore &store) {
  store.scan(scaleSlotsInUse_, configSlotsInUse_);
}

// ---------------------------------------------------------------------------
// Shift + note-button actions (the "main menu" command set).
// ---------------------------------------------------------------------------
void Menu::handleShiftButtonPress(QuantizerState &quantizer,
                                  uint8_t buttonIndex) {
  const uint8_t ch = channelIndex();
  ChannelConfig &config = quantizer.channels[ch].config();

  switch (buttonIndex) {
    case config::kRotateScaleLeftButtonIndex:
      // Rotate the current scale left (element 0 wraps to the top).
      rotateSelectedScale(quantizer, ch, /*left=*/true);
      break;
    case config::kRotateScaleRightButtonIndex:
      // Rotate the current scale right (top wraps back to element 0).
      rotateSelectedScale(quantizer, ch, /*left=*/false);
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
      // Match the original firmware by default: SHIFT+4 toggles only between
      // Track-and-Hold and Sample-and-Hold. Continuous is an optional C++
      // extension and participates in the UI cycle only when explicitly enabled.
      if (config::kEnableContinuousSampleModeInUi) {
        if (config.sampleMode == SampleMode::TrackAndHold) {
          config.sampleMode = SampleMode::SampleAndHold;
        } else if (config.sampleMode == SampleMode::SampleAndHold) {
          config.sampleMode = SampleMode::Continuous;
        } else {
          config.sampleMode = SampleMode::TrackAndHold;
        }
      } else {
        config.sampleMode =
            (config.sampleMode == SampleMode::TrackAndHold)
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
      // Toggle channel B between relative and absolute pitch.
      quantizer.channelBMode = (quantizer.channelBMode == PitchMode::Relative)
                                   ? PitchMode::Absolute
                                   : PitchMode::Relative;
      page_ = Page::ShowChangedBoolOption;
      boolOptionWhich_ = BoolOption::RelativePitch;
      break;
    case config::kChannelLinkButtonIndex:
      // Toggle link. On linking, copy channel A's config to channel B so both
      // behave identically (config only; ephemeral processing state untouched).
      quantizer.channelsLinked = !quantizer.channelsLinked;
      if (quantizer.channelsLinked) {
        quantizer.channels[kChannelBIndex].config() = quantizer.channels[kChannelAIndex].config();
        selectedChannel_ = UiChannel::A;
      }
      page_ = Page::ShowChangedBoolOption;
      boolOptionWhich_ = BoolOption::ChannelsLinked;
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

// ---------------------------------------------------------------------------
// Main update entry point.
// ---------------------------------------------------------------------------
MenuOutput Menu::update(QuantizerState &quantizer, const MenuInput &input,
                        const QuantizationResult &result, uint32_t currentTimeMs,
                        SaveSlotStore &store) {
  // While any calibration page is active, calibration owns all input.
  if (inCalibration()) {
    MenuOutput out = handleCalibration(input, currentTimeMs);
    shiftWasPressed_ = input.shiftPressed;
    return out;
  }

  // Snapshot persistent state so we can report whether this cycle changed it.
  uint8_t before[kStateBytes];
  encodeState(quantizer, before);

  // --- Calibration entry combo: SHIFT + LOAD + SAVE held for five seconds ---
  const bool saveDown = isButtonDown(input.saveButton);
  const bool loadDown = isButtonDown(input.loadButton);
  const bool comboActive = input.shiftPressed && saveDown && loadDown;
  if (comboActive) {
    if (comboEntryStart_ == kNoTime) {
      comboEntryStart_ = currentTimeMs;
      page_ = Page::MainMenu;  // keep the display clean during the hold
    } else if (currentTimeMs - comboEntryStart_ >= config::kCalibrationEnterHoldMs) {
      enterCalibration(currentTimeMs);
      comboEntryStart_ = kNoTime;
      shiftWasPressed_ = input.shiftPressed;
      MenuOutput out;
      out.frame = handleCalibration(input, currentTimeMs).frame;
      out.persistentStateChanged = false;
      return out;
    }
    shiftWasPressed_ = input.shiftPressed;
    MenuOutput out;
    out.frame = renderNotesDisplay(quantizer, result);
    out.persistentStateChanged = false;
    return out;
  }
  comboEntryStart_ = kNoTime;

  // --- ScalarSubMenu: exit transitions on SHIFT release --------------------
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

  // --- SAVE / LOAD / erase handling ----------------------------------------
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
  } else if (!input.shiftPressed &&
             isButtonDown(input.loadButton) && isButtonDown(input.saveButton) &&
             (input.loadButton == LongPressButtonState::ButtonHeldDownLong ||
              input.loadButton == LongPressButtonState::ButtonJustClickedLong) &&
             (input.saveButton == LongPressButtonState::ButtonHeldDownLong ||
              input.saveButton == LongPressButtonState::ButtonJustClickedLong)) {
    // Erase-all: both long-pressed together, without SHIFT (SHIFT is reserved
    // for entering calibration).
    if (store.eraseAll()) {
      scaleSlotsInUse_ = SlotOccupancy();
      configSlotsInUse_ = SlotOccupancy();
      page_ = Page::ConfirmErase;
      pageStartTime_ = currentTimeMs;
    }
  }

  // --- Note-button events ---------------------------------------------------
  if (input.keyEvent.type == ButtonEventType::JustPressed) {
    const uint8_t n = input.keyEvent.index;
    switch (page_) {
      case Page::ScalarSubMenu: {
        // Apply the scalar change to the selected (or both, if linked) channels.
        const int8_t signedValue = buttonIndexToSigned(n);
        for (uint8_t c = 0; c < 2; ++c) {
          if (!quantizer.channelsLinked && c != channelIndex()) {
            continue;
          }
          ChannelConfig &cfg = quantizer.channels[c].config();
          switch (subMenuWhich_) {
            case ScalarSubMenu::Glide:
              cfg.glideAmount = n;
              break;
            case ScalarSubMenu::Delay:
              cfg.triggerDelayAmount = n;
              break;
            case ScalarSubMenu::PreShift:
              cfg.preShift = signedValue;
              break;
            case ScalarSubMenu::ScaleShift:
              cfg.scaleShift = signedValue;
              break;
            case ScalarSubMenu::PostShift:
              cfg.postShift = signedValue;
              break;
          }
        }
        // Decide whether to stay in the sub-menu or return to the main menu.
        switch (subMenuStatus_) {
          case ScalarSubMenuStatus::AwaitingFirstInput:
            subMenuStatus_ = ScalarSubMenuStatus::ExitOnShiftRelease;
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
      }

      case Page::MainMenu:
        if (input.shiftPressed) {
          handleShiftButtonPress(quantizer, n);
          if (page_ == Page::ShowChangedBoolOption) {
            pageStartTime_ = currentTimeMs;
          }
        } else {
          // Toggle the note in the selected (or both, if linked) channels.
          for (uint8_t c = 0; c < 2; ++c) {
            if (!quantizer.channelsLinked && c != channelIndex()) {
              continue;
            }
            ChannelConfig &cfg = quantizer.channels[c].config();
            if (cfg.notes[n]) {
              uint8_t enabled = 0;
              for (uint8_t i = 0; i < kNoteCount; ++i) enabled = static_cast<uint8_t>(enabled + (cfg.notes[i] ? 1u : 0u));
              if (enabled > 1) cfg.notes[n] = false;
            } else {
              cfg.notes[n] = true;
            }
          }
        }
        break;

      case Page::SelectSaveSlot: {
        bool queued = false;
        if (slotType_ == SaveSlotType::Scale) {
          queued = store.writeScale(
              n, quantizer.channels[channelIndex()].config().notes);
          if (queued) scaleSlotsInUse_.set(n, true);
        } else {
          queued = store.writeConfig(n, quantizer);
          if (queued) configSlotsInUse_.set(n, true);
        }
        if (queued) {
          page_ = Page::ConfirmSaveSlot;
          confirmSlot_ = n;
          pageStartTime_ = currentTimeMs;
        }
        break;
      }

      case Page::SelectLoadSlot:
        if (store.busy()) break;
        if (slotType_ == SaveSlotType::Scale) {
          bool notes[kNoteCount];
          if (store.readScale(n, notes)) {
            for (uint8_t c = 0; c < 2; ++c) {
              if (!quantizer.channelsLinked && c != channelIndex()) continue;
              ChannelConfig &cfg = quantizer.channels[c].config();
              for (uint8_t i = 0; i < kNoteCount; ++i) cfg.notes[i] = notes[i];
            }
          }
        } else {
          QuantizerState loaded;
          if (!store.readConfig(n, loaded)) {
            // Empty/erased full-config slots expose a built-in factory preset.
            // A valid user save always wins because it is attempted first.
            loaded = config::makeFactoryConfigPreset(n);
          }
          quantizer = loaded;
          selectedChannel_ = UiChannel::A;
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

  // --- Auto-dismiss the confirmation splashes ------------------------------
  if (page_ == Page::ConfirmSaveSlot &&
      currentTimeMs - pageStartTime_ >= config::kSaveConfirmationMs) {
    page_ = Page::MainMenu;
  }
  if (page_ == Page::ConfirmErase &&
      currentTimeMs - pageStartTime_ >= config::kEraseConfirmationMs) {
    page_ = Page::MainMenu;
  }
  if (page_ == Page::ShowChangedBoolOption &&
      currentTimeMs - pageStartTime_ >= config::kBoolOptionFeedbackMs) {
    page_ = Page::MainMenu;
  }

  // Deliberately do not call QuantizerChannel::setConfig() for ordinary UI
  // edits. The Rust original mutates ChannelConfig in place and preserves the
  // channel's ephemeral trigger/glide state. Calling setConfig() here reset
  // the input-trigger UI state, which made the A/B trigger LEDs flash on every
  // note-button press (both channels when linked). The quantizer's hysteresis
  // already invalidates itself when the previously selected note disappears,
  // so in-place config changes are safe and match the original behaviour.

  // --- Render and report -----------------------------------------------------
  MenuOutput out;
  out.frame = renderCurrentPage(quantizer, result, currentTimeMs);

  uint8_t after[kStateBytes];
  encodeState(quantizer, after);
  out.persistentStateChanged = memcmp(before, after, kStateBytes) != 0;
  return out;
}

// ---------------------------------------------------------------------------
// Rendering of the non-calibration pages.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Brightness-calibration mode.
// ---------------------------------------------------------------------------
void Menu::enterCalibration(uint32_t currentTimeMs) {
  page_ = Page::CalibrationEntering;
  pageStartTime_ = currentTimeMs;
  workingBrightness_ = committedBrightness_;
  selectedCalColor_ = CalColor::Green;
  shiftAloneStart_ = kNoTime;
}

MenuOutput Menu::handleCalibration(const MenuInput &input,
                                   uint32_t currentTimeMs) {
  MenuOutput out;
  out.persistentStateChanged = false;

  const LedColor color = (selectedCalColor_ == CalColor::Red) ? LedColor::Red
                                                              : LedColor::Green;

  switch (page_) {
    case Page::CalibrationEntering: {
      // Blink all LEDs to confirm entry, then switch to the active editor.
      if (currentTimeMs - pageStartTime_ >= config::kCalibrationEnterBlinkMs) {
        page_ = Page::CalibrationActive;
      } else {
        const bool on =
            ((currentTimeMs - pageStartTime_) / config::kMenuBlinkHalfPeriodMs) % 2 == 0;
        if (on) {
          for (uint8_t i = 0; i < kNoteCount; ++i) {
            out.frame[i] = color;
          }
        }
      }
      return out;
    }

    case Page::CalibrationActive: {
      const bool notePressed =
          input.keyEvent.type == ButtonEventType::JustPressed ||
          input.keyEvent.type == ButtonEventType::Held;

      if (input.keyEvent.type == ButtonEventType::JustPressed) {
        const uint8_t n = input.keyEvent.index;
        if (input.shiftPressed) {
          // Keep calibration colour semantics consistent with the normal UI:
          // channel A is green and channel B is red.
          if (n == config::kChannelAButtonIndex) {
            selectedCalColor_ = CalColor::Green;
          } else if (n == config::kChannelBButtonIndex) {
            selectedCalColor_ = CalColor::Red;
          }
        } else {
          // A note key chooses the brightness step (0..11) for the active colour.
          if (selectedCalColor_ == CalColor::Red) {
            workingBrightness_.redStep = n;
          } else {
            workingBrightness_.greenStep = n;
          }
        }
      }

      // Save when SHIFT is held alone (no note) for five seconds.
      const bool shiftAlone = input.shiftPressed && !notePressed;
      if (shiftAlone) {
        if (shiftAloneStart_ == kNoTime) {
          shiftAloneStart_ = currentTimeMs;
        } else if (currentTimeMs - shiftAloneStart_ >= config::kCalibrationSaveHoldMs) {
          committedBrightness_ = workingBrightness_;
          page_ = Page::CalibrationSaving;
          pageStartTime_ = currentTimeMs;
          shiftAloneStart_ = kNoTime;
          out.persistentStateChanged = true;  // ask caller to persist
        }
      } else {
        shiftAloneStart_ = kNoTime;
      }

      // The twelve ring positions form a clockwise bar graph for calibration.
      // 12 o'clock is step 0 and the final position is step 11. Every LED from
      // 12 o'clock through the nearest/current step is illuminated, making the
      // selected level readable at a glance. The actual emitter intensity is
      // still driven by the selected PWM level, so the bar is also a live
      // brightness preview.
      const LedColor previewColor =
          (selectedCalColor_ == CalColor::Red) ? LedColor::Red : LedColor::Green;
      const uint8_t displayStep =
          (selectedCalColor_ == CalColor::Red)
              ? workingBrightness_.redDisplayStep()
              : workingBrightness_.greenDisplayStep();

      for (uint8_t ringIndex = 0; ringIndex <= displayStep; ++ringIndex) {
        out.frame[ringIndex] = previewColor;
      }
      return out;
    }

    case Page::CalibrationSaving: {
      // Three confirmation blinks, then return to normal operation.
      if (currentTimeMs - pageStartTime_ >= config::kCalibrationSaveBlinkMs) {
        page_ = Page::MainMenu;
      } else {
        const bool on =
            ((currentTimeMs - pageStartTime_) / config::kMenuBlinkHalfPeriodMs) % 2 == 0;
        if (on) {
          for (uint8_t i = 0; i < kNoteCount; ++i) {
            out.frame[i] = color;
          }
        }
      }
      return out;
    }

    default:
      return out;
  }
}

}  // namespace fmq
