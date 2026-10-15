// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SYMBOLART_H
#define FUDE_ZOOM_SYMBOLART_H

#include "rde.h"

// ===========================================================================
// Symbolart: a library symbol (symbol.h) drawn from an SVG file instead of its own lines. Each file is parsed once
// by RDE's SVG documents (rde.h, NanoSVG), drawn at any size from its curves, and exported as vectors.
//
// Where it sits: zoom, the canvas apps' layer. The page (page.c) loads the app's art once when it starts; from then
// on fude_zoom_symbol_parts draws a symbol from its art through the hook fude_zoom_symbol_set_art.
// Its main types and their lifecycle: the loaded SVG documents are module globals, one slot per symbol kind, created
// by fude_zoom_symbol_art_load and destroyed by fude_zoom_symbol_art_free.
// Threads: main thread only; the art is global state.
// Files: a folder of art (the app's assets/parts/). Its parts.txt lists one symbol id a line; empty lines and lines
//   starting with '#' are skipped. Each listed symbol's SVG is the id with its spaces as underscores ("drive motor":
//   drive_motor.svg). A missing or unreadable SVG is logged and skipped.
// Design:
//   An SVG is drawn into the symbol's box as its viewBox is. Its proportions should be the symbol's size as it comes;
//   y is down in the SVG and up on the canvas. So a pin at u, v (circuit.h) is at ((u + 1) / 2 · W, (1 − v) / 2 · H).
//   Its FIRST shape is the symbol's outline, which is what it is snapped and joined by.
//   Colours are roles where they are black or white, so the art follows the canvas's ink and paper, and themselves
//   otherwise:
//     fill:   black is the ink (the symbol's line colour); white is its paper (or its fill); none is nothing; any
//             other colour is that colour.
//     stroke: black is the ink; none is no line; any other colour is that colour.
//   Stroke widths are the SVG's (in its units), and a dashed stroke is drawn dashed. A filled shape's paths after its
//   first that lie inside it are its holes.
// ===========================================================================

// Gives every symbol listed in _folder's parts.txt its art and returns how many SVGs it loaded. It loads only once:
// called again, it does nothing and returns 0. Returns 0 too when parts.txt cannot be read.
u32  fude_zoom_symbol_art_load(const c8* _folder);
// Destroys all the art and removes the hook, so each symbol is drawn with its own lines again.
void fude_zoom_symbol_art_free(void);

#endif
