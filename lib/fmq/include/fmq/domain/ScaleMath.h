/**
 * @file ScaleMath.h
 * Declares scale-note lookup and scale-degree stepping helpers.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FM_QUANTIZER_CORE_SCALE_MATH_H
#define FM_QUANTIZER_CORE_SCALE_MATH_H

#include <stdint.h>

#include "fmq/domain/FixedPoint.h"

/**
 * Helpers for navigating a user-defined musical scale.
 *
 * A "scale" is represented as a 12-element boolean array indexed by pitch class
 * (0 = C, 1 = C#, … 11 = B). A note is "selected" (part of the scale) when its
 * entry is true. Absolute pitch is expressed in whole semitones 0..120, whose
 * pitch class is `pitch % 12`.
 */
namespace fmq {

/// Direction of travel when searching for scale notes.
enum class ScaleDirection : int8_t {
  Down = -1,  ///< Toward lower pitches.
  Up = 1,     ///< Toward higher pitches.
};

/**
 * Find the next selected note strictly beyond @p startingNote.
 *
 * Walks one semitone at a time in @p direction until a selected pitch class is
 * found, clamping at the module's pitch limits (0 and @ref kMaxSemitone). If no
 * note is selected in that direction, the corresponding limit is returned.
 *
 * @param notes        Scale membership per pitch class.
 * @param startingNote Absolute semitone to start from (search is exclusive).
 * @param direction    Search direction.
 * @return The next selected absolute semitone, clamped to [0, 120].
 */
int8_t getNextSelectedNote(const bool notes[kNoteCount], int8_t startingNote,
                           ScaleDirection direction);

/**
 * Step a given number of scale degrees from a starting note.
 *
 * Positive @p numSteps walks upward, negative downward, moving from one
 * selected note to the next. If the scale is empty, @p startingNote is returned
 * unchanged.
 *
 * @param notes        Scale membership per pitch class.
 * @param startingNote Absolute semitone to start from.
 * @param numSteps     Signed number of scale degrees to move.
 * @return The resulting absolute semitone, clamped to [0, 120].
 */
int8_t stepInScale(const bool notes[kNoteCount], int8_t startingNote,
                   int8_t numSteps);

/**
 * Test whether a scale contains at least one selected note.
 * @param notes Scale membership per pitch class.
 * @return true if any pitch class is selected.
 */
bool scaleHasAnyNote(const bool notes[kNoteCount]);

}  // namespace fmq

#endif  // FM_QUANTIZER_CORE_SCALE_MATH_H
