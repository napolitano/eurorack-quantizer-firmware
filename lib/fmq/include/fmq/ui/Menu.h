/**
 * @file Menu.h
 * Hardware-independent two-layer front-panel state machine.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_MENU_H
#define FM_QUANTIZER_CORE_MENU_H

#include <stdint.h>

#include "fmq/application/ArpeggiatorBank.h"
#include "fmq/application/StoredConfiguration.h"
#include "fmq/domain/LedColor.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/persistence/SaveSlotStore.h"
#include "fmq/ui/BrightnessCalibration.h"
#include "fmq/ui/MenuTypes.h"

namespace fmq {

enum class UiChannel : uint8_t { A = 0, B = 1 };

/**
 * @brief Complete user-interface state machine.
 *
 * The normal Quantizer layer preserves the established note/SHIFT workflow.
 * A SHIFT double-click switches to a complete Arpeggiator layer.
 * Both layers intentionally share the same interaction grammar: unmodified note
 * keys edit the selected scale, while SHIFT + note selects a layer-specific menu
 * function. Scalar functions then use the twelve note keys as value selectors.
 * A, A# and B retain Link / Channel A / Channel B in both SHIFT menus.
 */
class Menu {
 public:
  Menu();

  void begin(const SaveSlotStore &store);

  void setCommittedBrightness(const BrightnessCalibration &b) {
    committedBrightness_ = b;
  }
  const BrightnessCalibration &committedBrightness() const {
    return committedBrightness_;
  }
  const BrightnessCalibration &activeBrightness() const {
    return inCalibration() ? workingBrightness_ : committedBrightness_;
  }

  bool inCalibration() const {
    return page_ == Page::CalibrationEntering ||
           page_ == Page::CalibrationActive ||
           page_ == Page::CalibrationSaving;
  }

  uint8_t selectedChannelIndex() const {
    return static_cast<uint8_t>(selectedChannel_);
  }
  void setSelectedChannelIndex(uint8_t index) {
    selectedChannel_ = index == kChannelBIndex ? UiChannel::B : UiChannel::A;
  }

  UiLayer layer() const { return layer_; }

  /**
   * @brief Restore the persisted UI layer without changing musical state.
   *
   * Boot-time restore must not call toggleLayer(): that operation intentionally
   * enables/disables the Arpeggiator and starts visual feedback. This setter
   * restores only the front-panel function map and returns the UI to its stable
   * main page.
   */
  void restoreLayer(UiLayer layer) {
    layer_ = layer;
    page_ = Page::MainMenu;
    subMenuStatus_ = ScalarSubMenuStatus::AwaitingFirstInput;
  }

  /**
   * @brief Switch Quantizer/Arpeggiator UI layers.
   *
   * Entering the Arpeggiator layer enables the selected Arpeggiator (or both
   * linked channels) and starts the standard two-flash green confirmation.
   * Returning to the Quantizer layer disables all Arpeggiator channels and
   * starts the standard two-flash red confirmation. In both directions the
   * normal scale/quantized-note display is restored after the feedback.
   *
   * @return true when the persistent musical Arpeggiator enable state changed.
   */
  bool toggleLayer(QuantizerState &quantizer,
                   ArpeggiatorBank &arpeggiators, uint32_t nowMs);

  /** Double-click layer switching is valid only from the stable main page. */
  bool layerToggleAllowed() const { return page_ == Page::MainMenu; }

  uint8_t calibrationEditorChannelIndex() const {
    return selectedCalColor_ == CalColor::Green ? kChannelAIndex
                                                : kChannelBIndex;
  }

  StoredConfiguration captureStoredConfiguration(
      const QuantizerState &quantizer,
      const ArpeggiatorBank &arpeggiators) const;
  void applyStoredConfiguration(const StoredConfiguration &stored,
                                QuantizerState &quantizer,
                                ArpeggiatorBank &arpeggiators,
                                uint32_t nowMs);

  MenuOutput update(QuantizerState &quantizer,
                    ArpeggiatorBank &arpeggiators,
                    const MenuInput &input,
                    const QuantizationResult &result,
                    uint32_t currentTimeMs,
                    SaveSlotStore &store);

 private:
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
  enum class ScalarSubMenu : uint8_t {
    Glide,
    Delay,
    PreShift,
    ScaleShift,
    PostShift,
    ArpRate,
    ArpPattern,
    ArpShape,
    ArpLength,
    ArpRange,
    ArpSync,
    ArpSwing,
  };
  enum class BoolOption : uint8_t {
    SampleMode,
    RelativePitch,
    ChannelsLinked,
    ArpEnabled,
    ArpStepTrigger,
  };
  enum class SaveSlotType : uint8_t { Scale, FullConfig };
  enum class CalColor : uint8_t { Red, Green };

  uint8_t channelIndex() const { return static_cast<uint8_t>(selectedChannel_); }
  void handleQuantizerShiftButtonPress(QuantizerState &quantizer,
                                       ArpeggiatorBank &arpeggiators,
                                       uint8_t buttonIndex,
                                       uint32_t nowMs);
  void handleArpeggiatorShiftButtonPress(QuantizerState &quantizer,
                                    ArpeggiatorBank &arpeggiators,
                                    uint8_t buttonIndex,
                                    uint32_t nowMs);
  void toggleLink(QuantizerState &quantizer,
                  ArpeggiatorBank &arpeggiators,
                  uint32_t nowMs);
  void applyScalarSelection(QuantizerState &quantizer,
                            ArpeggiatorBank &arpeggiators,
                            uint8_t buttonIndex, uint32_t nowMs);

  LedFrame renderCurrentPage(const QuantizerState &quantizer,
                             const ArpeggiatorBank &arpeggiators,
                             const QuantizationResult &result,
                             uint32_t currentTimeMs) const;
  LedFrame renderNotesDisplay(const QuantizerState &quantizer,
                              const QuantizationResult &result) const;
  LedColor slotColor() const;

  MenuOutput handleCalibration(const MenuInput &input, uint32_t currentTimeMs);
  void enterCalibration(uint32_t currentTimeMs);

  UiChannel selectedChannel_;
  UiLayer layer_;
  Page page_;
  bool shiftWasPressed_;
  SlotOccupancy scaleSlotsInUse_;
  SlotOccupancy configSlotsInUse_;

  ScalarSubMenuStatus subMenuStatus_;
  ScalarSubMenu subMenuWhich_;
  BoolOption boolOptionWhich_;
  SaveSlotType slotType_;
  uint8_t confirmSlot_;
  uint32_t pageStartTime_;

  BrightnessCalibration committedBrightness_;
  BrightnessCalibration workingBrightness_;
  CalColor selectedCalColor_;
  uint8_t calibrationMarkerStep_;
  uint32_t calibrationMarkerStart_;

  static constexpr uint32_t kNoTime = 0xFFFFFFFFu;
  uint32_t comboEntryStart_;
  uint32_t shiftAloneStart_;
};

}  // namespace fmq

#endif
