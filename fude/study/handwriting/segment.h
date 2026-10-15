// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SEGMENT
#define FUDE_SEGMENT

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "drawing/ink/ink.h"
#include "study/handwriting/match.h"

// ===========================================================================
// Segment: reads free writing. It finds which strokes make which character, and what each character is.
//
// Strokes come in the order they were written, and a character's strokes are written together, so a reading is a
// split of that sequence into runs. It is found in four steps:
//
//   1. DIRECTION: across (lines top to bottom) or down (columns right to left), from the shape of the whole. A line
//      or column has an extent H across the writing ("line" below means either).
//   2. LINES: a stroke past the line's far side that jumps back to its start begins the next line.
//   3. PIECES: a stroke clear of everything since the last cut (in the writing direction) may begin a character: it
//      is a cut. This over-splits (明 becomes 日 and 月, 川 becomes three) but almost never misses a real boundary.
//   4. CHARACTERS: runs of consecutive pieces no longer than about a character (FUDE_SEGMENT_SPAN × H) are ranked by
//      the matcher (match.h), and the cheapest reading wins (dynamic programming over the pieces): each run's best
//      match, paying FUDE_SEGMENT_PER_CHAR per character, so that pieces that make one character stay together.
//
// A run's candidates are the matcher's, with a term for size and place added: in a line, a character sits in its
// cell the way the reference sits in KanjiVG's box (っ is small and low, つ is not), which the matcher alone, fitting
// every drawing to the same box, cannot tell apart. The line's frame (where a full cell's top is, and how tall it
// is) is first taken from its extent, then fitted to the characters read, and the candidates are weighed again.
// Last comes what the language says (lang.h: fude_lang_writing): in Japanese, a small ゃ only after an i-row kana, ヘ
// or へ chosen by the script around it, and a word kept in one script. A language with none of those rules (every
// other one taught today) is read by shape, size and place alone.
//
// Reading AS a text ("I meant: 今日は") is the same split with the characters known: every stroke boundary may be a
// cut, and each character gets the run of strokes that costs least against it, guided by its stroke count.
//
// Where it sits: the study layer's handwriting (study/handwriting). The handwriting reader (textink.h) and Check
// use it.
// Its main types and their lifecycle: the caller owns a fude_segment, made by fude_segment_init and freed by
// fude_segment_destroy; each read replaces the characters of the one before.
// Threads: call it on the app's main thread only (it uses the matcher, whose cache is a static, and static scratch).
// Files: none.
// ===========================================================================

#define FUDE_SEGMENT_CANDIDATES 8
#define FUDE_SEGMENT_MAX_CHARS  256

typedef enum {
    FUDE_SEGMENT_AUTO = 0,
    FUDE_SEGMENT_ACROSS,        // horizontal lines, top to bottom
    FUDE_SEGMENT_DOWN           // vertical columns, right to left
} FUDE_SEGMENT_DIRECTION_;

RDE_STRUCT {
    u32               first;           // its strokes are first .. first + count - 1, counted among
    u32               count;           // the drawing's alive strokes in writing order
    u32               line;
    rde_vec_2F        min;             // its extent, in the drawing's units (Y up)
    rde_vec_2F        max;
    fude_match_result candidates[FUDE_SEGMENT_CANDIDATES];   // best first (the cost is per stroke, place included)
    f32               shape[FUDE_SEGMENT_CANDIDATES];        // the same without the place: the matcher's own cost
    u32               candidate_count;
} fude_segment_char;

RDE_STRUCT {
    b8                            vertical;
    u32                           lines;
    rde_arr TYPE(fude_segment_char) chars;
} fude_segment;

// Sets up an empty reading; and frees it.
void fude_segment_init(fude_segment* _segment);
void fude_segment_destroy(fude_segment* _segment);

// Reads the alive strokes of _drawing (Y up, in the order written) as lines of characters, into _segment->chars.
// Returns how many characters: 0 when there is no data or nothing to read.
u32  fude_segment_read(fude_segment* _segment, const fude_kanji_db* _db, const fude_catalog* _catalog,
                       const fude_ink* _drawing, FUDE_SEGMENT_DIRECTION_ _direction);
// Reads them as the characters _records, in order: each gets the run of strokes that fits it best, as its only
// candidate. Returns false, with nothing read, when there are fewer strokes than characters.
b8   fude_segment_read_as(fude_segment* _segment, const fude_kanji_db* _db, const fude_ink* _drawing,
                          FUDE_SEGMENT_DIRECTION_ _direction, const u32* _records, u32 _count);

#endif
