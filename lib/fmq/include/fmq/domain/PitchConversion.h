/**
 * @file PitchConversion.h
 * Declares calibrated conversion between ADC pitch and DAC codes.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_PITCH_CONVERSION_H
#define FM_QUANTIZER_CORE_PITCH_CONVERSION_H
#include <stdint.h>
#include "fmq/domain/FixedPoint.h"
namespace fmq {
SemitoneQ8_8 adcToSemitones(uint16_t adcValue,uint8_t channel=0);
uint16_t semitonesToDac(SemitoneQ8_8 semitones,uint8_t channel=0);
}
#endif
