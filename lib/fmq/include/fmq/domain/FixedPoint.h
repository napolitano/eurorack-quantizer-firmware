/**
 * @file FixedPoint.h
 * Defines fixed-point pitch types and representation constants.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_FIXED_POINT_H
#define FM_QUANTIZER_CORE_FIXED_POINT_H

#include <stdint.h>

/**
 * Fixed-point numeric conventions used throughout the quantizer core.
 *
 * The original Rust firmware relied on the `fixed` crate (types such as `I8F8`,
 * `I8F24`). This reimplementation uses plain integers with an explicitly
 * documented scale factor ("Q-format"). This keeps the arithmetic cheap on the
 * 8-bit AVR (no template machinery) while remaining perfectly portable so the
 * exact same code can be unit-tested on the host.
 *
 * Two formats are used:
 *  - @ref SemitoneQ8_8  : a signed 16-bit value in units of 1/256 semitone.
 *  - @ref GlideQ8_24    : a signed 32-bit value in units of 1/2^24 semitone,
 *                         used only for the portamento (glide) integrator where
 *                         extremely small per-sample increments are required.
 *
 * A "semitone" here is one step of a 12-tone equal-tempered scale. The module
 * spans 0..120 semitones (ten octaves), which maps linearly to the DAC output.
 */
namespace fmq {

/// Signed fixed-point pitch in units of 1/256 semitone (Q8.8).
using SemitoneQ8_8 = int16_t;

/// Signed fixed-point pitch in units of 1/2^24 semitone (Q8.24) for glide.
using GlideQ8_24 = int32_t;

/// Number of raw units that represent exactly one semitone in Q8.8.
constexpr uint8_t kSemitoneFractionBits = 8;
constexpr uint8_t kGlideFractionBits = 24;
constexpr uint8_t kGlideToSemitoneShift =
    kGlideFractionBits - kSemitoneFractionBits;

constexpr SemitoneQ8_8 kSemitoneOneQ8_8 =
    static_cast<SemitoneQ8_8>(1u << kSemitoneFractionBits);
constexpr SemitoneQ8_8 kSemitoneFractionMask =
    static_cast<SemitoneQ8_8>(kSemitoneOneQ8_8 - 1);
constexpr SemitoneQ8_8 kHalfSemitoneQ8_8 =
    static_cast<SemitoneQ8_8>(kSemitoneOneQ8_8 / 2);

/// Number of raw units that represent exactly one semitone in Q8.24.
constexpr GlideQ8_24 kSemitoneOneQ8_24 =
    static_cast<GlideQ8_24>(1L << kGlideFractionBits);

/// Highest note the module can represent/emit (ten octaves above the lowest).
constexpr int8_t kMaxSemitone = 120;

/// Number of chromatic pitch classes / physical note buttons / note LEDs.
constexpr uint8_t kNoteCount = 12;

/**
 * Clamp an integer to the inclusive range [low, high].
 * @tparam T any integral type
 */
template <typename T>
constexpr T clampInt(T value, T low, T high) {
  return value < low ? low : (value > high ? high : value);
}

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_FIXED_POINT_H
