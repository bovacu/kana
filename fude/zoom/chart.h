// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CHART_H
#define FUDE_ZOOM_CHART_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Charts: draws a BAR CHART, a LINE CHART or a PIE CHART from data typed as text, one line each (the diagram
// library's Charts, added in 0.1.86):
//
//   2026 sales (€)        its title: a first line that does not end in a number. A line that ends in one is a
//                         category, so "Sales 2026" would be Sales, worth 2026: put such a number earlier in it
//   Jan 120               a category and its value (a colon, a comma, a semicolon or a tab may come between them too)
//   Feb 95 60             more values on a line make more series (at most 6, and at most 64 categories)
//   series: Shop, Online  the series' names, shown in its legend
//
// A value may end in "%". A pie chart uses each category's first value, and leaves out values of 0 or less.
//
// Where it sits: `fude/zoom`, the canvas layer. plot.c hands it the ids of charts, so a chart is drawn as plot.h's
// graphs are: the renderer and the exports draw its lines and labels, and the page offers its example text
// (page_diagrams.c, page_pieces.c).
// Threads: pure functions over the caller's data, safe on any thread; they are called on the app's main thread.
// Design: docs/infinite_canvas_design.md §69.
//
// What it makes, as plot.h's graphs: lines and regions in the symbol's own units (round its middle, Y up), and
// labels. The flags of the parts: below 32 they are the symbol's own (its axes; FUDE_ZOOM_SYMBOL_DASHED its grid);
// 32 + k is series k's line; FUDE_ZOOM_CHART_SOLID + k is a region filled solid in series k's colour (a bar, a slice,
// a dot). The labels: a category's name under its place, each value's tick to the left of its axis, a slice's name
// and share beside it, the title over the chart, and the legend's lines (plot.h's align 4).
// ===========================================================================

#define FUDE_ZOOM_CHART_SOLID 128u   // a part's flags: 128 + k is a region filled in series k's colour

// Returns whether _id is a chart's ("bar chart", "line chart", "pie chart").
b8  fude_zoom_chart_is(const c8* _id);

// Draws chart _id from _text for half sizes _hw × _hh, as plot.h's fude_zoom_plot_lines does: appends its points to
// _points (fude_zoom_v2), its parts to _parts (fude_zoom_symbol_part) and its labels to _labels (fude_zoom_plot_label;
// NULL for none). Returns how many series it drew (a pie chart counts as one); 0 when there is nothing to draw.
u32 fude_zoom_chart_lines(const c8* _id, const c8* _text, f64 _hw, f64 _hh, rde_arr* _points, rde_arr* _parts, rde_arr* _labels);

// Returns an example of a chart's text to start from (a static string): a pie chart's for "pie chart", otherwise a
// bar or line chart's.
const c8* fude_zoom_chart_example(const c8* _id);

#endif
