// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CHECK
#define FUDE_CHECK

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "study/chars/glyph.h"
#include "drawing/ink/ink.h"
#include "study/handwriting/match.h"
#include "study/handwriting/score.h"
#include "study/handwriting/segment.h"
#include "study/handwriting/recognize.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// Check: free writing, checked (section 4 of the Japanese study app's docs/design.md). It works without a trained
// recogniser.
//
// The lasso's selection on a canvas is READ (segment.h): split into the characters it was written as, and each one
// ranked against every character by the draw-to-search matcher (match.h), by what it looks like. Each character's
// best match is chosen and the writing scored against it (score.h) for order, direction and shape, with the same
// feedback line as Practice. A tap on another candidate scores the writing against that one instead ("I meant
// this").
//
// "I meant" for the whole selection: a typed text (romaji becomes kana, UPPER CASE romaji katakana; or text in the
// script from the keyboard) reads the selection AS that text. The strokes are split to fit it, and each character is
// scored against what was meant.
//
// It is a screen of its own, over the page. With one character it shows the writing in a square, with the chosen
// character faint behind it, the candidates beside it, and the score and its line below. With more it first shows
// the whole writing, a box around each character read, and the reading under it, each character coloured by its
// score; a tap on either looks closer at that character, in the square below.
//
// Where Google ML Kit is available (iOS, recognize.h) and nothing is typed, ML Kit reads first (a model trained on
// many writers), and the selection is split and scored AS its answer. Each character's candidates are then ML Kit's
// alternatives followed by the matcher's, as in Browse's draw-to-search. The app's own reading is what is used when
// ML Kit is not there.
//
// Where it sits: a study screen (study/screens), opened by Check in the page's selection menu
// (study/widgets/pagetext.c). The study shell (study/app/shell.c) owns the one Check and places FUDE_CHECK_SCREEN in
// the app's screen table.
// Its main types and their lifecycle: fude_check, made with fude_check_init over the character data and Browse's
// catalog (borrowed, not owned) and freed with fude_check_destroy when the app ends. fude_check_open copies the
// strokes it checks, so the page may change under it.
// Threads: main thread only. ML Kit's answer is asked for and then polled once a frame (recognize.h).
// Files: none.
// Design: reading takes a moment (tens of milliseconds a character in a debug build), so it happens on the frame
// after the screen first shows, which says "Reading…" until then.
// ===========================================================================

#define FUDE_CHECK_CANDIDATES 8
#define FUDE_CHECK_MEANT      128      // The most characters of a typed text.

// One character as read.
RDE_STRUCT {
    u32               first;           // Its strokes in the check's drawing: first .. first + count - 1.
    u32               count;
    rde_vec_2F        min;             // Their extent, in canvas units.
    rde_vec_2F        max;
    fude_match_result candidates[FUDE_CHECK_CANDIDATES];
    u32               candidate_count;
    i32               chosen;          // Its chosen candidate, or -1.
    fude_score        score;           // Against the chosen candidate.
} fude_check_char;

// Where a character is on screen, for taps (screen space).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u32        index;
} fude_check_hit;

RDE_STRUCT {
    const fude_kanji_db* db;
    const fude_catalog*  catalog;
    fude_glyph           glyph;
    b8                   open;

    fude_ink             drawing;       // The selected strokes, copied, in the order written.
    rde_vec_2F           bounds_min;    // Their extent, in canvas units.
    rde_vec_2F           bounds_max;

    fude_segment                  segment;
    rde_arr TYPE(fude_check_char) chars;       // The reading, in order.
    u32                           selected;    // The character looked at closely.
    b8                            pending;     // To be read on the next update (once the screen has shown).
    b8                            _drawn;

    // What was meant, typed: its characters as records. With none, the selection is read freely.
    u32                  meant[FUDE_CHECK_MEANT];
    u32                  meant_count;
    b8                   meant_failed;  // The selection could not be read as it (too few strokes).

    // ML Kit (mlkit.h), where there is one: with nothing typed, it is asked what the selection says, and the
    // selection is read as its answer.
    fude_recognition     recognition;       // The request and its answer (recognize.h).
    c8                   mlkit_text[256];   // Its best reading, "" for none.
    b8                   by_mlkit;          // The reading is ML Kit's.
    b8                   _asked_mlkit;

    // The layout of the last frame, for taps (screen space).
    rde_vec_2F           cell_tl[FUDE_CHECK_CANDIDATES];
    f32                  cell_size;
    rde_arr TYPE(fude_check_hit) hits;
    // A stroke's points on screen as it is drawn: scratch kept from frame to frame, so drawing allocates nothing once
    // it has grown to the longest stroke; every stroke is drawn whole, however many points it has.
    rde_arr TYPE(rde_vec_2F) _points;
    rde_ui_scroller      taps;          // Tells taps from drags the way the lists do (rde_ui_scroller).
} fude_check;

// Makes Check over the character data and Browse's catalog (both borrowed); destroy frees it.
void fude_check_init(fude_check* _check, const fude_kanji_db* _db, const fude_catalog* _catalog);
void fude_check_destroy(fude_check* _check);

// Opens on strokes _ids of _ink (the lasso's selection, in any order), copying them in the order they were written;
// the marker's strokes are not writing and are left out. Returns false, and does not open, when there is nothing to
// check (no strokes, or no character data). The reading follows on a later fude_check_update.
b8   fude_check_open(fude_check* _check, const fude_ink* _ink, const u32* _ids, u32 _count);
void fude_check_close(fude_check* _check);
// Once a frame while open: reads the selection when it is due, asking ML Kit first where it can.
void fude_check_update(fude_check* _check);

// Reads the selection as _text (UTF-8: text in the script, or romaji, lower case to hiragana and UPPER CASE to
// katakana). An empty text, or one with no characters the data has, reads it freely again. The reading follows on a
// later update.
void fude_check_read_as(fude_check* _check, const c8* _text);

// How many characters were read (0 while reading).
u32  fude_check_count(const fude_check* _check);
// Looks closer at character _index.
void fude_check_select(fude_check* _check, u32 _index);
// Scores the character looked at against its candidate _index instead.
void fude_check_choose(fude_check* _check, u32 _index);
// The record chosen for the character looked at; false when there is none (still reading, or no candidate).
b8   fude_check_chosen_record(const fude_check* _check, u32* _record);
// Every character's chosen record, in order, at most _max into _out (with _unique, each record once). Returns how
// many.
u32  fude_check_records(const fude_check* _check, u32* _out, u32 _max, b8 _unique);

// One pointer, in screen space: a tap on a candidate chooses it; a tap on a character (in the writing or the reading)
// looks closer at it.
void fude_check_pointer_down(fude_check* _check, rde_vec_2F _screen, f64 _time);
void fude_check_pointer_moved(fude_check* _check, rde_vec_2F _screen, f64 _time);
void fude_check_pointer_up(fude_check* _check, f64 _time);

// Draws the screen between _top and _bottom (screen y). Call it inside a 2D drawing block.
void fude_check_render(fude_check* _check, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// Check as a screen (screen.h). Its row: Back; Stroke order (the viewer over Check on the character looked at, Prev
// and Next going through the others that have a reading); Practice (one character alone, several as a set, each
// once). Its field, "I meant…", reads the selection as what is typed when Return is pressed (empty: freely again).
extern const struct fude_screen FUDE_CHECK_SCREEN;

#endif
