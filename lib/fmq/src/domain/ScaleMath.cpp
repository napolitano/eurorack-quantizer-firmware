/**
 * @file ScaleMath.cpp
 * Implements scale membership and bounded scale-degree movement.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fmq/domain/ScaleMath.h"

namespace fmq {

bool scaleHasAnyNote(const bool notes[kNoteCount]) {
  for (uint8_t i = 0; i < kNoteCount; ++i) {
    if (notes[i]) {
      return true;
    }
  }
  return false;
}

int8_t getNextSelectedNote(const bool notes[kNoteCount], int8_t startingNote,
                           ScaleDirection direction) {
  int8_t note = startingNote;

  // Walk semitone by semitone until a selected pitch class is reached or a
  // boundary is hit. Because the caller guarantees at least one selected note
  // when this is used for hysteresis, the loop is bounded in practice; the
  // explicit boundary checks make it total (always terminating) regardless.
  for (;;) {
    if (direction == ScaleDirection::Up) {
      if (note >= kMaxSemitone) {
        return startingNote;
      }
      note = static_cast<int8_t>(note + 1);
    } else {
      if (note <= 0) {
        return startingNote;
      }
      note = static_cast<int8_t>(note - 1);
    }

    if (notes[note % kNoteCount]) {
      return note;
    }
  }
}

int8_t stepInScale(const bool notes[kNoteCount], int8_t startingNote,
                   int8_t numSteps) {
  if (!scaleHasAnyNote(notes)) {
    return startingNote;
  }

  const ScaleDirection direction =
      numSteps < 0 ? ScaleDirection::Down : ScaleDirection::Up;

  int8_t note = startingNote;
  const int8_t stepCount = numSteps < 0 ? static_cast<int8_t>(-numSteps)
                                        : numSteps;
  for (int8_t i = 0; i < stepCount; ++i) {
    note = getNextSelectedNote(notes, note, direction);
  }
  return note;
}

}  // namespace fmq
