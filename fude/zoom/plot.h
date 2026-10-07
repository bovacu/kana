// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PLOT_H
#define FUDE_ZOOM_PLOT_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// MATHS (Sketching's Maths topic): the diagram library's Maths family — a GRAPH
// (functions of x drawn on axes), AXES, a NUMBER LINE — drawn from their text:
// each line a function ("y = sin(x)", "x^2 - 1": calc.h), or its view ("x -5 5",
// "y -2 2": from and to; without one, x from −10 to 10 and y as the functions
// go). Their axes, ticks (1, 2 or 5 of a power of ten apart, about ten of them)
// and grid, and each function's curve (broken where it is not, or leaps off the
// view: an asymptote), are lines in the symbol's own units; the ticks' numbers,
// labels where they go.
// ===========================================================================

typedef struct {
    fude_zoom_v2 at;          // its own units
    c8           text[48];
    u8           align;       // 0 centred under, 1 right of it (a y tick's: to its left), 2 centred, 3 from it on (a point's), 4 a legend's line (from it on, on its paper)
    u8           curve;       // 0: in its ink; 1 + k: its function k's colour (a curve's part: flags 32 + k)
} fude_zoom_plot_label;

// Is symbol id _id one of the Maths family's (drawn from its text: its text not written in it as it is — a graph's
// functions its legend, top left, each in its curve's colour)?
b8   fude_zoom_plot_is(const c8* _id);
// Its labels' height (its own units) for half sizes _hw × _hh.
f64  fude_zoom_plot_label_height(const c8* _id, f64 _hw, f64 _hh);
// Its lines for half sizes _hw × _hh from _text: points into _points (fude_zoom_v2), parts into _parts
// (symbol.h's fude_zoom_symbol_part: DASHED the grid's, 32 + k function k's curve and dots, SOLID a point's dot),
// its ticks' numbers and its legend into _labels (fude_zoom_plot_label; NULL: none). Appended. How many curves drawn.
u32  fude_zoom_plot_lines(const c8* _id, const c8* _text, f64 _hw, f64 _hh, rde_arr* _points, rde_arr* _parts, rde_arr* _labels);

// --- its variables ---------------------------------------------------------------------------------
//
// A graph's line "a = 2" gives the functions' variable a (calc.h) its value; "a = 0..5" (or "0 to 5") a range it
// goes over and back while it plays (three seconds each way), its first number when it does not. "x = 1.5" (or a
// range) marks where each curve is there: a dot on each, its numbers by it. "(2, 3)" puts a point. The legend shows
// each variable's value as it is.

typedef struct {
    c8   letter;      // 'a'… ('x': where points are marked)
    f64  from, to;    // its value (from), or its range
    b8   range;
} fude_zoom_plot_var;

// Playing: how long it has (NaN: it is not), and each variable a slider holds (NaN: none: as its line says).
typedef struct fude_zoom_plot_play {
    f64 time;
    f64 held[26];
} fude_zoom_plot_play;

// ...drawn as it is _play'ing (NULL: not).
u32  fude_zoom_plot_lines_play(const c8* _id, const c8* _text, f64 _hw, f64 _hh, const fude_zoom_plot_play* _play, rde_arr* _points, rde_arr* _parts,
                               rde_arr* _labels);
// A graph's variables (its text's, in letter order) into _out (_max at most). How many.
u32  fude_zoom_plot_vars(const c8* _id, const c8* _text, fude_zoom_plot_var* _out, u32 _max);
// ...one's value now.
f64  fude_zoom_plot_var_value(const fude_zoom_plot_var* _v, const fude_zoom_plot_play* _play);

#endif
