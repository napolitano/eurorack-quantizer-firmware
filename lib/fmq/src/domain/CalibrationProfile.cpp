/**
 * @file CalibrationProfile.cpp
 * Owns the active analogue calibration profile used at runtime.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/domain/CalibrationProfile.h"

#include "fmq/config/AnalogConfig.h"

namespace fmq {
namespace {

CalibrationProfile makeFactoryProfile() {
  CalibrationProfile profile;
  profile.adc[0] = {config::kAdcOffsetA, config::kAdcGainNumeratorA,
                    config::kAdcGainDenominatorA};
  profile.adc[1] = {config::kAdcOffsetB, config::kAdcGainNumeratorB,
                    config::kAdcGainDenominatorB};
  profile.dac[0] = {config::kDacOffsetA, config::kDacGainNumeratorA,
                    config::kDacGainDenominatorA};
  profile.dac[1] = {config::kDacOffsetB, config::kDacGainNumeratorB,
                    config::kDacGainDenominatorB};
  return profile;
}

CalibrationProfile g_activeProfile = makeFactoryProfile();

}  // namespace

CalibrationProfile factoryCalibrationProfile() { return makeFactoryProfile(); }

const CalibrationProfile &activeCalibrationProfile() { return g_activeProfile; }

bool isLinearCalibrationValid(const LinearCalibration &calibration) {
  return calibration.gainNumerator != 0u &&
         calibration.gainDenominator != 0u;
}

bool isCalibrationProfileValid(const CalibrationProfile &profile) {
  for (uint8_t channel = 0u; channel < 2u; ++channel) {
    if (!isLinearCalibrationValid(profile.adc[channel]) ||
        !isLinearCalibrationValid(profile.dac[channel])) {
      return false;
    }
  }
  return true;
}

bool setActiveCalibrationProfile(const CalibrationProfile &profile) {
  if (!isCalibrationProfileValid(profile)) return false;
  g_activeProfile = profile;
  return true;
}

void resetActiveCalibrationProfile() { g_activeProfile = makeFactoryProfile(); }

}  // namespace fmq
