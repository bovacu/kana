#ifndef KANA_BROWSE
#define KANA_BROWSE

#include "rde.h"
#include "kanji.h"
#include "catalog.h"
#include "glyph.h"
#include "ink.h"
#include "match.h"
#include "scroll.h"
#include "recognize.h"
#include "select.h"

// ===========================================================================
// Browse: every kana and kanji as a grid to scroll and tap — filtered, sorted,
// searched by typing (catalog.h) or by DRAWING the character (match.h). A tap
// opens the viewer on that character, walking the grid's list.
//
// This is the start of the "Register" section of docs/design.md.
//
// The grid and the drawing pad are drawn here, in 2D; the bar of chips and the
// search field above them are UI, built by the toolbar (they use its font and
// styles) and calling the setters below.
//
// Input: one pointer at a time. A finger or the pen drags the grid (with
// inertia) and taps a character; on the pad, the PEN writes (a finger still
// scrolls). Every stroke finished on the pad re-ranks the grid by resemblance.
//
// PARTS (the design's "search by component"): instead of the pad, a panel of
// the parts characters are built from (kanji.h, 'PART'), by stroke count. Tap
// parts to keep only the characters containing ALL of them; parts no listed
// character has are dimmed, so each pick narrows what is left to pick.
// ===========================================================================

#define KANA_BROWSE_MATCHES   60        // how many draw-search results the grid shows
#define KANA_BROWSE_CELL_MIN  88.0f     // screen units: the grid fits as many columns as this allows
#define KANA_BROWSE_PAD       230.0f    // the drawing pad's side
#define KANA_BROWSE_MAX_PICKED 6        // parts picked at once
#define KANA_BROWSE_PART_USES  5        // a part in fewer characters than this is not offered

// A part the panel offers.
RDE_STRUCT {
    u32 codepoint;
    u16 uses;          // characters containing it
    u8  strokes;
    b8  usable;        // some listed character contains it (or it is picked)
} kana_browse_part;

// A part's cell on the panel, as laid out for the last frame (content space: y
// down from the top of the panel's content).
RDE_STRUCT {
    f32 x, y, size;
    u32 part;          // into parts
} kana_browse_part_hit;

RDE_STRUCT {
    const kana_kanji_db* db;
    kana_catalog         catalog;
    kana_glyph           glyph;
    b8                   open;

    KANA_FILTER_         filter;
    KANA_SORT_           sort;
    c8                   search[128];
    b8                   drawing;       // the pad is shown and searches
    kana_ink             pad;           // what was drawn on it (pad-local units, Y up)
    kana_recognition     recognition;   // ML Kit's reading of the pad (recognize.h), when there is one
    b8                   _recognize_wanted;   // a stroke ended: ask ML Kit (on a later frame if it is busy)

    b8                   picking;       // the parts panel is shown
    rde_arr TYPE(kana_browse_part)     parts;       // offered, by stroke count then uses
    u32                  picked[KANA_BROWSE_MAX_PICKED];
    u32                  picked_count;
    kana_scroller        parts_scroller;
    b8                   on_parts;      // the pointer is on the panel
    rde_arr TYPE(kana_browse_part_hit) part_hits;
    f32                  parts_height;  // the panel's content height
    u8*                  _marks;        // scratch: a flag per code point (see kana_browse_recompute)

    rde_arr TYPE(u32)    list;          // what the grid shows: record indices, in order
    b8                   dirty;         // list must be recomputed

    kana_scroller        scroller;      // the grid's scrolling and taps
    b8                   on_pad;        // the pointer is a pen stroke on the pad

    // Layout of the last frame, for hit testing (screen space).
    rde_vec_2F           grid_min;
    rde_vec_2F           grid_max;
    rde_vec_2F           pad_min;
    rde_vec_2F           pad_max;
    rde_vec_2F           panel_min;
    rde_vec_2F           panel_max;
    f32                  cell;
    u32                  columns;

    i32                  tapped;        // a tapped position in list, or -1 (see kana_browse_take_tap)

    kana_selection*      selection;     // the app's (select.h), set by its owner: in Select mode the grid shows ticks
    u32                  _marks_seen;   // kana_marks_revision the list was made at (the Studying and Known filters)
} kana_browse;

void kana_browse_init(kana_browse* _browse, const kana_kanji_db* _db);
void kana_browse_destroy(kana_browse* _browse);

// The meanings are in another language now (kanji.h): searched and sorted in it.
void kana_browse_language_changed(kana_browse* _browse);
b8   kana_browse_available(const kana_browse* _browse);
void kana_browse_open(kana_browse* _browse);
void kana_browse_close(kana_browse* _browse);

void kana_browse_set_filter(kana_browse* _browse, KANA_FILTER_ _filter);
void kana_browse_set_sort(kana_browse* _browse, KANA_SORT_ _sort);
void kana_browse_set_search(kana_browse* _browse, const c8* _text);
void kana_browse_set_drawing(kana_browse* _browse, b8 _drawing);
void kana_browse_clear_pad(kana_browse* _browse);
// The parts panel: shown or not (the pad goes when it comes), and no parts picked.
void kana_browse_set_picking(kana_browse* _browse, b8 _picking);
void kana_browse_clear_parts(kana_browse* _browse);
// The data has parts to pick from.
b8   kana_browse_parts_available(const kana_browse* _browse);

// Pointer input, screen space. _pen: the pen (writes on the pad) or a finger.
void kana_browse_pointer_down(kana_browse* _browse, rde_vec_2F _screen, b8 _pen, f64 _time);
void kana_browse_pointer_moved(kana_browse* _browse, rde_vec_2F _screen, f64 _time);
void kana_browse_pointer_up(kana_browse* _browse, f64 _time);

// A character was tapped: its position in the list. Once per tap.
b8   kana_browse_take_tap(kana_browse* _browse, u32* _position);
const u32* kana_browse_list(const kana_browse* _browse);
u32        kana_browse_count(const kana_browse* _browse);

void kana_browse_update(kana_browse* _browse, f32 _dt);

// Draws the pad and the grid below _top (screen space: everything above it is
// the UI bar). Inside a 2D drawing block; the grid clips to its own area.
void kana_browse_render(kana_browse* _browse, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

#endif
