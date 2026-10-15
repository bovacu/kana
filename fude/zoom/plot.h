// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PLOT_H
#define FUDE_ZOOM_PLOT_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Plot: draws the diagram library's Maths family (Sketching's Maths topic) from its text: a GRAPH (curves drawn on
// axes), AXES and a NUMBER LINE. Each line of the text says one thing, and numbers are written as maths writes them
// (calc.h: 2pi, pi/2, 1e3):
//   a function of x        "y = sin(x)", "f(x) = x^2 - 1", "x^2 - 1"
//   a curve of t           "(cos(t), sin(2t))"   (t runs from 0 to 2π, or as "t 0 4pi" says)
//   a polar curve          "r = 1 + cos(θ)"      (θ is read as t)
//   an equation of x, y    "x^2 + y^2 = 4", "y^2 = x^3 - x", "x = y^2"
//   an inequality          "y < x^2", "x^2 + y^2 >= 1" (≤ and ≥ too): its region is shaded, and its edge is dashed
//                          when the edge is not part of it
//   its view               "x -5 5", "y -2 2". Without one, x runs from −10 to 10 and y as far as the functions go;
//                          a graph of only curves of t and equations is framed round them, a unit as long across as up
//   what to mark           "roots", "extrema", "intersections" (of its functions)
//   an area                "area 0 2": the area under its first function is shaded, and its value is in the legend
// Its axes, its ticks (1, 2 or 5 times a power of ten apart, about ten of them), its grid and each curve are lines in
// the symbol's own units. A curve is followed more finely where it bends, broken where it has no value or leaps (an
// asymptote), and cut at the view's edge. The ticks' numbers and other words are labels, placed where they go. The
// flags of the parts: below 32 they are the symbol's own (FUDE_ZOOM_SYMBOL_DASHED its grid); 32 + k is curve k's
// (plus 8 when dashed); 64 + k is a region of curve k's (a polygon filled faintly in its colour).
//
// Where it sits: `fude/zoom`, the canvas layer. The renderer (render.c) and the exports (export.c) draw these symbols
// with it, and charts (chart.h) are drawn through it too.
// Threads: pure functions over the caller's data, safe on any thread; they are called on the app's main thread.
// Design: docs/infinite_canvas_design.md §30 (graphs drawn from their functions) and §64 (equations, curves of t,
// regions and marks).
// ===========================================================================

// A label: words placed by a point of the drawing.
typedef struct {
    fude_zoom_v2 at;          // where, in the symbol's own units
    c8           text[48];
    // How it is placed against `at`: 0 centred under it; 1 ending at it (a y tick's number, to the left of its tick);
    // 2 centred on it; 3 starting at it (a point's numbers); 4 a line of the legend, starting at it, on the legend's
    // paper.
    u8           align;       // 0 under, 1 ending at it, 2 centred, 3 starting at it, 4 a legend's line
    u8           curve;       // 0: in its ink; 1 + k: in function k's colour (that of parts flagged 32 + k)
    c8           latex[192];  // a legend line as maths (tex.h: x^{2} + y^{2} = 4); empty: its text is shown as it is
} fude_zoom_plot_label;

// Returns whether symbol id _id is one of the Maths family's (charts included). These symbols are drawn from their
// text, so the text is not written on them as words: a graph's functions are shown in its legend at its top left,
// each in its curve's colour.
b8   fude_zoom_plot_is(const c8* _id);
// Returns the height of its labels, in its own units, for half sizes _hw × _hh.
f64  fude_zoom_plot_label_height(const c8* _id, f64 _hw, f64 _hh);
// Draws symbol _id from _text for half sizes _hw × _hh, appending its points to _points (fude_zoom_v2), its parts to
// _parts (symbol.h's fude_zoom_symbol_part: DASHED for the grid, 32 + k for function k's curve and dots, SOLID for a
// point's dot), and its ticks' numbers and its legend to _labels (fude_zoom_plot_label; NULL for none). Returns how
// many curves it has.
u32  fude_zoom_plot_lines(const c8* _id, const c8* _text, f64 _hw, f64 _hh, rde_arr* _points, rde_arr* _parts, rde_arr* _labels);

// --- its variables ---------------------------------------------------------------------------------
//
// A graph's line "a = 2" gives the functions' variable a (calc.h) its value. "a = 0..5" (or "0 to 5") gives a range
// it goes over and back while the graph plays (three seconds each way); when it does not play, the variable has its
// first number. "x = 1.5" (or a range) marks where each curve is at that x: a dot on each, with its numbers by it.
// "(2, 3)" puts a point. The legend shows each variable's value as it is now.

// A variable as a graph's text sets it.
typedef struct {
    c8   letter;      // 'a' to 'z' ('x': where points are marked)
    f64  from, to;    // its value (from), or its range
    b8   range;
} fude_zoom_plot_var;

// How a graph is playing: `time` is how long it has played, in seconds (NaN: it is not playing), and `held` is the
// value a slider holds for each letter (NaN: none, so the variable is as its line says).
typedef struct fude_zoom_plot_play {
    f64 time;
    f64 held[26];
} fude_zoom_plot_play;

// Draws it as fude_zoom_plot_lines does, as it is at _play (NULL: not playing).
u32  fude_zoom_plot_lines_play(const c8* _id, const c8* _text, f64 _hw, f64 _hh, const fude_zoom_plot_play* _play, rde_arr* _points, rde_arr* _parts,
                               rde_arr* _labels);
// Writes a graph's variables (those its text sets, in letter order) into _out, at most _max. Returns how many; 0 for
// a symbol that is not a graph.
u32  fude_zoom_plot_vars(const c8* _id, const c8* _text, fude_zoom_plot_var* _out, u32 _max);
// Returns a variable's value now: what a slider holds; else, for a range while playing, a value going smoothly from
// one end to the other and back; else its first number.
f64  fude_zoom_plot_var_value(const fude_zoom_plot_var* _v, const fude_zoom_plot_play* _play);

#endif
