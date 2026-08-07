/**
 * @file Menu.h
 * Declares the menu state machine and user interaction model.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_MENU_H
#define FM_QUANTIZER_CORE_MENU_H

#include <stdint.h>

#include "fmq/ui/BrightnessCalibration.h"
#include "fmq/application/Button.h"
#include "fmq/ui/MenuTypes.h"
#include "fmq/application/ButtonLadder.h"
#include "fmq/domain/LedColor.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/persistence/SaveSlotStore.h"

/**
 * The complete user-interface state machine.
 *
 * Consumes debounced button input and the current quantization result, mutates
 * the quantizer configuration accordingly, and produces the twelve-LED display
 * frame. It also implements two additions over the original firmware:
 *
 *  - an @b LED @b brightness @b calibration mode (entered by holding
 *    SHIFT+LOAD+SAVE for five seconds), where the red and green emitter
 *    brightness can be set independently in twelve steps;
 *  - reporting when persistent state changed, so the caller can mirror the
 *    working configuration to the wear-levelled live-state store.
 *
 * The class is hardware-independent: it talks to storage only through
 * @ref SaveSlotStore (an @ref IEeprom) and returns a logical @ref LedFrame, so
 * the entire UI can be exercised in host unit tests.
 */
namespace fmq {

/// Which channel the UI currently edits.
enum class UiChannel : uint8_t { A = 0, B = 1 };

class Menu {
 public:
  Menu();

  /// Scan existing save slots so the save/load menus show occupancy.
  void begin(const SaveSlotStore &store);

  /// Set the committed LED calibration (called at boot with the restored value).
  void setCommittedBrightness(const BrightnessCalibration &b) {
    committedBrightness_ = b;
  }
  /// @return The committed LED calibration (persist this in the live store).
  const BrightnessCalibration &committedBrightness() const {
    return committedBrightness_;
  }
  /**
   * @brief The calibration to render with this cycle.
   *
   * While the brightness-calibration mode is active this is the (uncommitted)
   * working value so the user sees a live preview; otherwise it is the
   * committed value that governs normal operation.
   */
  const BrightnessCalibration &activeBrightness() const {
    return inCalibration() ? workingBrightness_ : committedBrightness_;
  }
  /// @return true while any brightness-calibration page is active.
  bool inCalibration() const {
    return page_ == Page::CalibrationEntering ||
           page_ == Page::CalibrationActive || page_ == Page::CalibrationSaving;
  }

  /**
   * @brief Process one UI cycle.
   * @param quantizer    Quantizer state to read and (for edits) mutate.
   * @param input        Debounced control input for this cycle.
   * @param result       Latest quantization result (for the note display).
   * @param currentTimeMs Current time in milliseconds.
   * @param store        Save-slot storage for save/load/erase operations.
   * @return The LED frame plus a "state changed" flag.
   */
  MenuOutput update(QuantizerState &quantizer, const MenuInput &input,
                    const QuantizationResult &result, uint32_t currentTimeMs,
                    SaveSlotStore &store);

 private:
  // ---- UI enumerations ----------------------------------------------------
  enum class Page : uint8_t {
    MainMenu,
    ScalarSubMenu,
    ShowChangedBoolOption,
    SelectSaveSlot,
    SelectLoadSlot,
    ConfirmSaveSlot,
    ConfirmErase,
    CalibrationEntering,
    CalibrationActive,
    CalibrationSaving,
  };
  enum class ScalarSubMenuStatus : uint8_t {
    AwaitingFirstInput,
    ExitOnShiftRelease,
    ExitOnButtonRelease,
  };
  enum class ScalarSubMenu : uint8_t { Glide, Delay, PreShift, ScaleShift, PostShift };
  enum class BoolOption : uint8_t { SampleMode, RelativePitch, ChannelsLinked };
  enum class SaveSlotType : uint8_t { Scale, FullConfig };
  enum class CalColor : uint8_t { Red, Green };

  // ---- helpers ------------------------------------------------------------
  uint8_t channelIndex() const { return static_cast<uint8_t>(selectedChannel_); }
  void handleShiftButtonPress(QuantizerState &quantizer, uint8_t buttonIndex);
  LedFrame renderCurrentPage(const QuantizerState &quantizer,
                             const QuantizationResult &result,
                             uint32_t currentTimeMs) const;
  LedFrame renderNotesDisplay(const QuantizerState &quantizer,
                              const QuantizationResult &result) const;
  /// Colour used for the save/load slot display and its confirmation blink.
  LedColor slotColor() const;

  // Brightness-calibration handling (separate from the main flow).
  MenuOutput handleCalibration(const MenuInput &input, uint32_t currentTimeMs);
  void enterCalibration(uint32_t currentTimeMs);

  // ---- state --------------------------------------------------------------
  UiChannel selectedChannel_;
  Page page_;
  bool shiftWasPressed_;
  SlotOccupancy scaleSlotsInUse_;
  SlotOccupancy configSlotsInUse_;

  // Page-associated parameters (only the active page's fields are meaningful).
  ScalarSubMenuStatus subMenuStatus_;
  ScalarSubMenu subMenuWhich_;
  BoolOption boolOptionWhich_;
  SaveSlotType slotType_;
  uint8_t confirmSlot_;
  uint32_t pageStartTime_;

  // Brightness calibration.
  BrightnessCalibration committedBrightness_;
  BrightnessCalibration workingBrightness_;
  CalColor selectedCalColor_;

  // Combo / long-hold timers (kNoTime = inactive).
  static constexpr uint32_t kNoTime = 0xFFFFFFFFu;
  uint32_t comboEntryStart_;    ///< SHIFT+LOAD+SAVE held-together timer.
  uint32_t shiftAloneStart_;    ///< SHIFT-alone timer inside calibration.
};

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_MENU_H
