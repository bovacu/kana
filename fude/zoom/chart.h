// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CHART_H
#define FUDE_ZOOM_CHART_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// CHARTS from data (the diagram library's Charts, 0.1.86): a BAR CHART, a LINE
// CHART and a PIE CHART drawn from their text, a line each:
//
//   Sales 2026            its title: a first line with no number in it
//   Jan 120               a category and its value (a colon, a comma or a tab
//   Feb 95 60             between them too); more values: more series
//   series: Shop, Online  the series' names (its legend)
//
// As plot.h's graphs are: lines and regions in the symbol's own units (round
// its middle, Y up), and labels — drawn by the renderer and the exports as a
// graph's (plot.c hands its ids here). Parts' flags: below 32 the symbol's own
// (its axes; DASHED its grid), 32 + k series k's line, 128 + k a region filled
// solid in series k's colour (a bar, a slice, a dot). Labels: a category's name
// under its place, a value's tick to the left of its axis, a slice's share by
// it, the title over it, the legend's lines (plot.h' align 4).
// ===========================================================================

#define FUDE_ZOOM_CHART_SOLID 128u   // a part's flags: 128 + k a region filled in series k's colour

// Is _id a chart's ("bar chart", "line chart", "pie chart")?
b8  fude_zoom_chart_is(const c8* _id);

// Its lines for half sizes _hw × _hh from _text (as plot.h' fude_zoom_plot_lines): appended to _points
// (fude_zoom_v2), _parts (fude_zoom_symbol_part), _labels (fude_zoom_plot_label; NULL: none). How many series.
u32 fude_zoom_chart_lines(const c8* _id, const c8* _text, f64 _hw, f64 _hh, rde_arr* _points, rde_arr* _parts, rde_arr* _labels);

// An example of its text, to start from.
const c8* fude_zoom_chart_example(const c8* _id);

#endif
