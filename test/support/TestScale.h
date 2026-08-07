/**
 * @file TestScale.h
 * Provides scale-construction helpers shared by host tests.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_TEST_SUPPORT_TEST_SCALE_H
#define FM_QUANTIZER_TEST_SUPPORT_TEST_SCALE_H

#include "fmq/domain/FixedPoint.h"

/// Test helpers shared across test suites.
namespace fmqtest {

/**
 * Populate a 12-element scale array from a list of pitch classes.
 * @param notes Destination array (all entries reset to false first).
 * @param pcs   Pitch classes (0..11) to select; out-of-range entries ignored.
 * @param count Number of entries in @p pcs.
 */
inline void makeScale(bool notes[fmq::kNoteCount], const int *pcs, int count) {
  for (unsigned i = 0; i < fmq::kNoteCount; ++i) {
    notes[i] = false;
  }
  for (int i = 0; i < count; ++i) {
    const int pc = pcs[i];
    if (pc >= 0 && pc < static_cast<int>(fmq::kNoteCount)) {
      notes[pc] = true;
    }
  }
}

/// Convert a floating-point semitone value to Q8.8 for test readability.
inline fmq::SemitoneQ8_8 semis(double s) {
  return static_cast<fmq::SemitoneQ8_8>(s * fmq::kSemitoneOneQ8_8);
}

}  // namespace fmqtest

#endif  // FM_QUANTIZER_TEST_SUPPORT_TEST_SCALE_H
