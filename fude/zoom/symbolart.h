// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SYMBOLART_H
#define FUDE_ZOOM_SYMBOLART_H

#include "rde.h"

// ===========================================================================
// SYMBOLS' ART: a library symbol drawn from an SVG file instead of its own
// lines (symbol.h's fude_zoom_symbol_set_art) — parsed once by RDE's SVG
// documents (rde.h: NanoSVG), drawn at any size from its curves, exported as
// vectors.
//
// A folder of them (the app's assets/parts/): parts.txt, a symbol's id a line;
// for each, its SVG, the id with its spaces as underscores ("drive motor":
// drive_motor.svg). An SVG is drawn into the symbol's box as its viewBox is
// (its own proportions: the symbol's as it comes; Y down in it, up on the
// canvas), so a pin at u, v (circuit.h) is at ((u + 1) / 2 · W, (1 − v) / 2 · H).
// Its FIRST shape is the symbol's outline (what it is snapped and joined by).
//
// Colours are roles where they are black or white, and themselves otherwise:
//   fill  black: the ink (the symbol's line colour)   white: its paper (or its
//         fill)   none: nothing   any other: that colour
//   stroke black: the ink   none: no line   any other: that colour
// Stroke widths are the SVG's (in its units); a dashed stroke, dashed. A
// shape's paths after its first that lie inside it are its holes.
// ===========================================================================

// Every symbol in _folder's parts.txt given its art (once; again: nothing). How many.
u32  fude_zoom_symbol_art_load(const c8* _folder);
// All let go (no art: each symbol its own lines again).
void fude_zoom_symbol_art_free(void);

#endif
