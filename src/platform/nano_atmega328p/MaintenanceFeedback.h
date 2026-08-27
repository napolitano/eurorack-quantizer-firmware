/**
 * @file MaintenanceFeedback.h
 * Declares visual status feedback for the calibration/maintenance firmware.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FMQ_PLATFORM_NANO_MAINTENANCE_FEEDBACK_H
#define FMQ_PLATFORM_NANO_MAINTENANCE_FEEDBACK_H

#include <stdint.h>

#include "fmq/ports/DigitalOutput.h"
#include "platform/nano_atmega328p/Tlc5947LedDriver.h"

namespace fmq::platform::nano {

/**
 * @brief Drives the maintenance image's deliberately simple visual UI.
 *
 * The note ring remains dim amber while the console is idle. During analog
 * measurements the ring is switched fully off to avoid unnecessary PWM load;
 * one discrete channel indicator identifies the channel under test. Commands
 * finish with green/red confirmation flashes and then return to dim amber.
 */
class MaintenanceFeedback {
 public:
  MaintenanceFeedback(Tlc5947LedDriver &ring, IDigitalOutput &channelLedA,
                      IDigitalOutput &channelLedB, bool statusLedsActiveHigh)
      : ring_(ring), channelLedA_(channelLedA), channelLedB_(channelLedB),
        statusLedsActiveHigh_(statusLedsActiveHigh) {}

  /** Initialize the ring and discrete channel indicators in a known state. */
  void begin();

  /** Three amber flashes followed by the steady dim-amber idle indication. */
  void playBootIndication();

  /** Restore the steady dim-amber maintenance idle indication. */
  void showIdle();

  /** Turn the ring off and identify channel A or B during a measurement. */
  void beginChannelMeasurement(uint8_t channel);

  /** Turn the ring off and identify both channels during a dual-channel test. */
  void beginDualChannelMeasurement();

  /** Two green flashes, then return to the dim-amber idle state. */
  void showPass();

  /** Two red flashes, then return to the dim-amber idle state. */
  void showFail();

  /** Three green flashes for a complete passing calibration report. */
  void showReportPass();

  /** Three red flashes for a failed calibration report. */
  void showReportFail();

 private:
  void setChannelIndicators(bool channelA, bool channelB);
  void writeUniform(uint16_t redLevel, uint16_t greenLevel);
  void showRingOff();
  void blink(uint16_t redLevel, uint16_t greenLevel, uint8_t count);

  Tlc5947LedDriver &ring_;
  IDigitalOutput &channelLedA_;
  IDigitalOutput &channelLedB_;
  bool statusLedsActiveHigh_;
};

}  // namespace fmq::platform::nano

#endif
