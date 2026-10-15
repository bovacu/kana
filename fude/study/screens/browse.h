// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_BROWSE
#define FUDE_BROWSE

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "study/chars/glyph.h"
#include "drawing/ink/ink.h"
#include "study/handwriting/match.h"
#include "drawing/widgets/scroll.h"
#include "study/handwriting/recognize.h"
#include "study/models/select.h"

// ===========================================================================
// Browse: every character of the character data (in Kana, every kana and kanji) as a grid to scroll and tap. The grid
// is filtered, sorted and searched by typing (catalog.h), or searched by DRAWING the character (match.h). A tap opens
// the viewer on that character, walking the grid's list. This is where the "Register" section of
// apps/kana/docs/design.md starts.
//
// Input: one pointer at a time. A finger or the pen drags the grid (with inertia) and taps a character; on the
// drawing pad the PEN writes, while a finger still scrolls. Every stroke finished on the pad ranks the grid again by
// resemblance, with ML Kit's reading first where there is one (recognize.h).
//
// PARTS (the design's "search by component"): instead of the pad, a panel of the parts characters are built from
// (kanji.h, 'PART'), ordered by stroke count. Tapping parts keeps only the characters that contain ALL of them; parts
// that no listed character has are dimmed, so each pick narrows what is left to pick.
//
// Where it sits: a study screen (study/screens), opened from the side panel. The study shell (study/app/shell.c)
// owns the one Browse, places FUDE_BROWSE_SCREEN in the app's screen table, and lends Browse's catalog to the exams,
// the word exams, Check and the statistics.
// Its main types and their lifecycle: fude_browse, made by the shell with fude_browse_init over the character data
// and freed with fude_browse_destroy when the app ends. fude_browse_open and fude_browse_close show and hide it.
// Threads: main thread only; it runs in the app's frame (update, render, pointer).
// Files: none.
// Design: the grid and the drawing pad are drawn here, in 2D. The bar of chips and the search field above them are a
// filter bar (filterbar.h), declared at the end of browse.c, which calls the setters below.
// ===========================================================================

#define FUDE_BROWSE_MATCHES   60        // How many results a drawn search shows.
#define FUDE_BROWSE_CELL_MIN  88.0f     // In screen units: the grid fits as many columns as this allows.
#define FUDE_BROWSE_PAD       230.0f    // The drawing pad's side.
#define FUDE_BROWSE_MAX_PICKED 6        // The most parts picked at once.
#define FUDE_BROWSE_PART_USES  5        // A part in fewer characters than this is not offered.

// A part the panel offers.
RDE_STRUCT {
    u32 codepoint;
    u16 uses;          // How many characters contain it.
    u8  strokes;
    b8  usable;        // Some listed character contains it (or it is picked).
} fude_browse_part;

// A part's cell on the panel, as laid out for the last frame, in content space (y down from the top of the panel's
// content).
RDE_STRUCT {
    f32 x, y, size;
    u32 part;          // Its index in parts.
} fude_browse_part_hit;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_catalog         catalog;
    fude_glyph           glyph;
    b8                   open;

    FUDE_FILTER_         filter;
    FUDE_SORT_           sort;
    c8                   search[128];
    b8                   drawing;       // The pad is shown and searches.
    fude_ink             pad;           // What was drawn on it (in pad-local units, Y up).
    fude_recognition     recognition;   // ML Kit's reading of the pad (recognize.h), when there is one.
    b8                   _recognize_wanted;   // A stroke ended: ask ML Kit (on a later frame if it is busy).

    b8                   picking;       // The parts panel is shown.
    rde_arr TYPE(fude_browse_part)     parts;       // The parts offered, by stroke count, then by uses.
    u32                  picked[FUDE_BROWSE_MAX_PICKED];
    u32                  picked_count;
    fude_scroller        parts_scroller;
    b8                   on_parts;      // The pointer is on the panel.
    rde_arr TYPE(fude_browse_part_hit) part_hits;
    f32                  parts_height;  // The panel's content height.
    rde_arr TYPE(u8)     _marks;        // Scratch: a flag per code point under FUDE_BROWSE_MARKS.

    rde_arr TYPE(u32)    list;          // What the grid shows: record indices, in order.
    b8                   dirty;         // The list must be worked out again.

    fude_scroller        scroller;      // The grid's scrolling and taps.
    b8                   on_pad;        // The pointer is a pen stroke on the pad.

    // The layout of the last frame, for hit testing (screen space).
    rde_vec_2F           grid_min;
    rde_vec_2F           grid_max;
    rde_vec_2F           pad_min;
    rde_vec_2F           pad_max;
    rde_vec_2F           panel_min;
    rde_vec_2F           panel_max;
    f32                  cell;
    u32                  columns;

    i32                  tapped;        // A tapped position in list, or -1 (see fude_browse_take_tap).

    fude_selection*      selection;     // The app's (select.h), set by its owner: in Select mode the grid shows ticks.
    u32                  _marks_seen;   // The fude_marks_revision the list was made at (for Studying and Known).
} fude_browse;

// Makes Browse over the character data (_db may be NULL: Browse is then never available); destroy frees it.
void fude_browse_init(fude_browse* _browse, const fude_kanji_db* _db);
void fude_browse_destroy(fude_browse* _browse);

// The meanings are in another language now (kanji.h): the catalog is made again, so it searches and sorts in it.
void fude_browse_language_changed(fude_browse* _browse);
// Whether there is character data to browse. fude_browse_open opens only when there is; fude_browse_close closes.
b8   fude_browse_available(const fude_browse* _browse);
void fude_browse_open(fude_browse* _browse);
void fude_browse_close(fude_browse* _browse);

// What the grid lists: its filter, its sort, the typed search (NULL is ""), the drawing pad on or off (off clears it;
// on hides the parts panel), and the pad emptied. Each starts the grid again from its top.
void fude_browse_set_filter(fude_browse* _browse, FUDE_FILTER_ _filter);
void fude_browse_set_sort(fude_browse* _browse, FUDE_SORT_ _sort);
void fude_browse_set_search(fude_browse* _browse, const c8* _text);
void fude_browse_set_drawing(fude_browse* _browse, b8 _drawing);
void fude_browse_clear_pad(fude_browse* _browse);
// Shows the parts panel or hides it (the pad goes when it comes). Either way the picks are dropped, because picks
// that cannot be seen would filter the grid unseen. fude_browse_clear_parts drops the picks alone.
void fude_browse_set_picking(fude_browse* _browse, b8 _picking);
void fude_browse_clear_parts(fude_browse* _browse);
// Whether the data has parts to pick from (data baked before parts has none, and so do Thai, Hindi and Arabic).
b8   fude_browse_parts_available(const fude_browse* _browse);

// Pointer input, in screen space. _pen is true for the pen, which writes on the pad, and false for a finger.
void fude_browse_pointer_down(fude_browse* _browse, rde_vec_2F _screen, b8 _pen, f64 _time);
void fude_browse_pointer_moved(fude_browse* _browse, rde_vec_2F _screen, f64 _time);
void fude_browse_pointer_up(fude_browse* _browse, f64 _time);

// Whether a character was tapped; if so its position in the list goes into *_position. Each tap is handed out once.
b8   fude_browse_take_tap(fude_browse* _browse, u32* _position);
// The grid's list, as record indices in order, and how many. The pointer is Browse's and stays valid until the list
// is next worked out (a setter, a stroke, the marks changing).
const u32* fude_browse_list(const fude_browse* _browse);
u32        fude_browse_count(const fude_browse* _browse);

// Once a frame while open: works the list out again when it changed (the Studying and Known filters follow the
// marks), asks ML Kit to read the pad and takes its answer, and scrolls.
void fude_browse_update(fude_browse* _browse, f32 _dt);

// Draws the pad and the grid between _top and _bottom (screen space; above _top is the UI's bar, and the grid stays
// above _bottom). Call it inside a 2D drawing block; the grid clips to its own area.
void fude_browse_render(fude_browse* _browse, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// Browse as a screen (screen.h), with its filter bar (the filters, the sorts, the search field, and Draw, Parts and
// Clear). Its row: Select, Practice (its list as a practice set) and Close; in Select mode, Select's row (study.h). A
// tapped character opens the viewer on its list, or in Select mode is ticked. What is in view is its whole list.
extern const struct fude_screen FUDE_BROWSE_SCREEN;

#endif
