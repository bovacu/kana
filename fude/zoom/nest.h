// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_NEST_H
#define FUDE_ZOOM_NEST_H

#include "rde.h"
#include "zoom/units.h"
#include "zoom/shape.h"

// ===========================================================================
// Nest: fits parts into a board, the other half of the cut list (docs/research/infinite_canvas_research.md §2.10,
// item 9). Rectangles are placed on a rectangle with the saw's kerf between them, as many of them and as much of
// their area as will fit. It also reads the list of parts a person types for it, a line at a time.
//
// Where it sits: fude/zoom, pure geometry with no scene (units.h reads the sizes typed). The page's Fit parts
// (page_boards.c) uses it.
// Its main types and their lifecycle: fude_zoom_nest_part and fude_zoom_nest_place are plain values in arrays the
// caller owns.
// Threads: it keeps no state, so any thread may call it.
// Design (docs/infinite_canvas_design.md §17, Fit parts):
//   - MaxRects (Jylänki's): the board's free room is kept as the largest rectangles it holds. Each part goes into the
//     free rectangle it leaves least over on its shorter side (Best Short Side Fit), and every free rectangle it
//     overlaps is split round it.
//   - Each part is also tried turned a quarter, when it may be: a part whose grain must run along the board's may not.
//   - The parts are tried in several orders (by area, by their longer side, their shorter side, their perimeter,
//     their width and their height), and the order that puts down the most wood wins.
//   - The kerf is made room for by growing each part and the board by it, so neighbours end up a kerf apart while the
//     board's edges need none.
// ===========================================================================

// One part to place.
typedef struct {
    f64 w, h;       // its size across and up the board (a quarter turn swaps them)
    b8  can_turn;   // whether it may be turned a quarter (true when its grain does not matter)
} fude_zoom_nest_part;

// Where one part was placed. One that was not (placed false) is at 0, 0, its size as it was given, not turned.
typedef struct {
    f64 x, y;       // its lower left corner on the board (the board's own corner being 0, 0)
    f64 w, h;       // its size as placed (swapped when turned)
    b8  turned;     // turned a quarter from how it was given
    b8  placed;     // false when it did not fit
} fude_zoom_nest_place;

// Places _n parts on a board _board_w by _board_h with _kerf between them, writing each part's place into _out (_n
// entries, in the parts' order). A part with no area is never placed. Returns how many parts fit.
u32 fude_zoom_nest(f64 _board_w, f64 _board_h, f64 _kerf, const fude_zoom_nest_part* _parts, u32 _n, fude_zoom_nest_place* _out);

// The longest line of a list of parts that is read (bytes, once its "x"s are spaced out): a part's line is short, and
// the rest of a longer one is left out.
#define FUDE_ZOOM_NEST_LINE 512u

// One line of a list of parts, as fude_zoom_nest_read_line reads it.
typedef struct {
    c8  name[FUDE_ZOOM_BOARD_NAME];   // its name ("" when it has none), as a board's
    f64 length, width;                // its sizes in millimetres: the larger, then the smaller (its grain along the larger)
    u32 count;                        // how many of it (1 when the line does not say)
} fude_zoom_nest_line;

// Reads one line of a typed list of parts (_len bytes of _text, not NUL-ended) into *_out: a name and its two sizes,
// length x width, with how many either after them ("Side 600 x 300 x 2", "... x2") or first, before the name
// ("2x Side 600 x 300", "2 × Side 600 x 300"); "×" and "*" do for "x". A size is in _unit unless it says its own
// (600mm, 23 5/8", 60cm); how many is held between 1 and _most. Returns false when the line does not give two sizes
// above 0 (an empty line, a heading): then *_out is all zeros.
b8 fude_zoom_nest_read_line(const c8* _text, usize _len, FUDE_ZOOM_UNIT_ _unit, u32 _most, fude_zoom_nest_line* _out);

#endif
