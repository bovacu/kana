// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_HISTORY
#define FUDE_HISTORY

#include "rde.h"
#include "drawing/ink/ink.h"
#include "study/handwriting/score.h"

// ===========================================================================
// Practice history: every scored practice session, kept to show progress (the album, album.h). It is never
// overwritten, only added to.
//
// Where it sits: a study model (study/models). Practice saves a session each time it scores (practice.h) and shows
// a character's summary; the album lists, summarises and replays the sessions (album.h); the statistics read them
// all (stats.h). The exam log borrows its stroke and point records for read-back writing (examlog.h).
// Its main types and their lifecycle: the files are the state, so most calls take a code point and read or write its
// file then and there. fude_history holds one character's whole history in memory: its owner calls
// fude_history_init, fills it with fude_history_load as often as it likes, and frees it with fude_history_destroy.
// Threads: main thread only.
// Files: ONE FILE PER CHARACTER, <save dir>/practice/<code point in hex>.kana (at least four upper-case hex digits:
// 6728.kana), in the KANA format (kfile.h), kind 'PRAC', with one 'SESS' chunk per session, oldest first. A session
// is appended by rewriting that one small file atomically, the old bytes kept as they were.
//   'SESS'  u64 time (Unix seconds, UTC), u32 code point, f32 average score (over the squares that were scored),
//           u32 square count, u32 square record size (20), then per square:
//             the record: u8 scored (0: left empty), f32 score, f32 shape, u8 drawn, expected, missing, extra,
//             misplaced, reversed, first_reversed, swap_a, swap_b, worst (score.h), and one reserved byte;
//             then its drawing: u32 stroke count, and per stroke u32 point count, then per point u16 x, u16 y (a
//             fraction of the square's side times 65535, Y up) and f32 time (seconds since the stroke began).
// Every read, the one before a session is appended too, goes through fude_save_load (save.h): a file whose header does
// not read is set aside as .bad and its .bak (the file before the last session) is used instead, so a damaged file
// loses at most one session rather than all of them.
// Design: the drawing is stored RELATIVE TO ITS SQUARE, so it can be redrawn at any size, and with its timing, so it
// can be replayed stroke by stroke. One file per character keeps each save small, however long the history grows.
// ===========================================================================

// The version written in each file's header.
#define FUDE_HISTORY_VERSION 1u

// What a character's sessions come to.
RDE_STRUCT {
    u32 sessions;
    f32 first;        // The first session's average.
    f32 last;         // The latest session's average.
    f32 best;         // The best session average.
    u64 last_time;    // When the latest session was, in Unix seconds.
} fude_history_summary;

// The folder the history files are in, created if missing, ending in '/'. The string is static and stays valid for
// the whole run; it is found again whenever the save folder has changed.
const c8* fude_history_dir(void);

// Appends one session of _squares squares to _codepoint's file. _scores[i] and _drawings[i] are square i's; neither
// array nor any drawing may be NULL. A square whose score is .empty is stored as unscored. _square_size is the
// squares' side in the drawings' units. Returns false when the file could not be written.
b8 fude_history_save(u32 _codepoint, u64 _time, u32 _squares, const fude_score* _scores,
                     const fude_ink* const* _drawings, f32 _square_size);

// Reads a character's sessions down to a summary, into _out. Returns false, with _out zeroed, when it has none or its
// file cannot be read.
b8 fude_history_summarize(u32 _codepoint, fude_history_summary* _out);

// A number that goes up by one on every save, written or not: whatever shows the history reads it again when this
// has moved.
u32 fude_history_revision(void);

// Appends to _out (an rde_arr of u32 the caller owns) the code point of every character with a history file, in no
// particular order. The .bak, .tmp and .bad files beside them are not counted.
void fude_history_list(rde_arr* _out);

// --- a character's whole history, for the album ------------------------------------

// One point of a stroke.
RDE_STRUCT {
    u16 x, y;         // A fraction of the square's side, times 65535; Y up.
    f32 time;         // Seconds since the stroke began.
} fude_history_point;

// One stroke: a run of points.
RDE_STRUCT {
    u32 first_point;  // Its first point in fude_history.points.
    u32 point_count;
} fude_history_stroke;

// One square of a session: its score and its strokes.
RDE_STRUCT {
    fude_score score;        // As it was scored then; .empty when the square was left empty.
    u32        first_stroke; // Its first stroke in fude_history.strokes.
    u32        stroke_count;
} fude_history_square;

// One session: when, its average, and its squares.
RDE_STRUCT {
    u64 time;                // Unix seconds, UTC.
    f32 average;
    u32 first_square;        // Its first square in fude_history.squares.
    u32 square_count;
} fude_history_session;

// A character's whole history, in four flat arrays that index into each other.
RDE_STRUCT {
    u32 codepoint;
    rde_arr TYPE(fude_history_session) sessions;   // Oldest first.
    rde_arr TYPE(fude_history_square)  squares;
    rde_arr TYPE(fude_history_stroke)  strokes;
    rde_arr TYPE(fude_history_point)   points;
} fude_history;

// Makes _history empty and ready to load into; fude_history_destroy frees what it holds. Destroying a zeroed history
// is safe.
void fude_history_init(fude_history* _history);
void fude_history_destroy(fude_history* _history);
// Replaces _history's contents with _codepoint's sessions (_history must have been initialised). A damaged session is
// left out whole. Returns false, and leaves it empty, when the character has no sessions or its file cannot be read.
b8   fude_history_load(fude_history* _history, u32 _codepoint);

#endif
