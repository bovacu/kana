#ifndef KANA_SEGMENT
#define KANA_SEGMENT

#include "rde.h"
#include "chars/kanji.h"
#include "chars/catalog.h"
#include "ink/ink.h"
#include "handwriting/match.h"

// ===========================================================================
// Reading free writing: which strokes make which character, and what each is.
//
// Strokes come in the order they were written, and a character's strokes are
// written together — so a reading is a split of that sequence into runs. It is
// found in four steps:
//
//   1. DIRECTION: across (lines top to bottom) or down (columns right to
//      left), from the shape of the whole; a line or column is its extent H
//      across the writing ("line" below means either).
//   2. LINES: a stroke past the line's far side that jumps back to its start
//      begins the next line.
//   3. PIECES: a stroke clear of everything since the last cut (in the writing
//      direction) may begin a character — a cut. This over-splits (明 is 日 and
//      月, 川 is three) but almost never misses a real boundary.
//   4. CHARACTERS: runs of consecutive pieces no longer than about a character
//      (KANA_SEGMENT_SPAN × H) are ranked by the matcher (match.h), and the
//      cheapest reading — each run's best match, paying KANA_SEGMENT_PER_CHAR
//      per character so that pieces that make one character stay together —
//      wins (dynamic programming over the pieces).
//
// A run's candidates are the matcher's, with a size-and-place term: in a line,
// a character sits in its cell the way the reference sits in KanjiVG's box —
// っ is small and low, つ is not; the matcher alone, fitting every drawing to
// the same box, cannot tell them apart. The line's frame (where a full cell's
// top is, how tall it is) is first its extent, then fitted to the characters
// read, and the candidates weighed again. Last, what the language says: small
// ゃ only after an i-row kana, ヘ or へ by the script around it.
//
// Reading AS a text ("I meant: 今日は") is the same split with the characters
// known: every stroke boundary may be a cut, and each character's strokes are
// the run that costs least against it — the count of strokes guides it.
// ===========================================================================

#define KANA_SEGMENT_CANDIDATES 8
#define KANA_SEGMENT_MAX_CHARS  256

typedef enum {
    KANA_SEGMENT_AUTO = 0,
    KANA_SEGMENT_ACROSS,        // horizontal lines, top to bottom
    KANA_SEGMENT_DOWN           // vertical columns, right to left
} KANA_SEGMENT_DIRECTION_;

RDE_STRUCT {
    u32               first;           // its strokes: first .. first + count - 1, positions among
    u32               count;           // the drawing's alive strokes in writing order
    u32               line;
    rde_vec_2F        min;             // its extent, the drawing's units (Y up)
    rde_vec_2F        max;
    kana_match_result candidates[KANA_SEGMENT_CANDIDATES];   // best first (cost: per stroke, place included)
    f32               shape[KANA_SEGMENT_CANDIDATES];        // the same without the place: the matcher's
    u32               candidate_count;
} kana_segment_char;

RDE_STRUCT {
    b8                            vertical;
    u32                           lines;
    rde_arr TYPE(kana_segment_char) chars;
} kana_segment;

void kana_segment_init(kana_segment* _segment);
void kana_segment_destroy(kana_segment* _segment);

// Reads the alive strokes of _drawing (Y up, in the order written) as lines of
// characters; returns how many characters.
u32  kana_segment_read(kana_segment* _segment, const kana_kanji_db* _db, const kana_catalog* _catalog,
                       const kana_ink* _drawing, KANA_SEGMENT_DIRECTION_ _direction);
// Reads them as the characters _records, in order: each gets the run of strokes
// that fits it best (its only candidate). False, and nothing read, when there
// are fewer strokes than characters.
b8   kana_segment_read_as(kana_segment* _segment, const kana_kanji_db* _db, const kana_ink* _drawing,
                          KANA_SEGMENT_DIRECTION_ _direction, const u32* _records, u32 _count);

#endif
