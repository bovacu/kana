// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HAND_H
#define FUDE_ZOOM_HAND_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Hand: typed text written out as pen strokes in a joined-up script, so a text box can be turned into handwriting.
//
// Where it sits: zoom, the canvas apps' layer. The page uses it for the selection's Handwriting button, which replaces
// selected plain texts with strokes (page_text.c), and to write a speech transcript onto the canvas as handwriting
// (page_speech.c).
// Its main types and their lifecycle: none of its own. The caller creates the two arrays the strokes go into and frees
// them.
// Threads: called on the main thread. It keeps no state between calls.
// Files: none. The letter shapes are compiled in from zoom/hand_glyphs.h.
// Design:
//   The font is Dr. A. V. Hershey's Script Simplex (zoom/hand_glyphs.h), a single-line font: each letter is drawn as
//   one line rather than an outline, and the letters keep the font's own joins.
//   Each letter is a little uneven, as a hand's are: its slant, its size, where it sits on the line and how far after
//   it the next one starts are each varied a touch. Every stroke is smoothed through the font's points (the font's
//   strokes are straight between them) and wavers slightly along its length. The variation is seeded, so the same
//   text and seed are always written the same way.
//   A letter whose last stroke ends where the next letter's first stroke begins runs into it as one stroke, the way a
//   word is written without lifting the pen. Accents and dots are written when the joined run ends, so the next letter
//   joins on from its letter and not from a mark.
//   Letters: ASCII, and the accented letters of Spanish, Portuguese and French, written as the plain letter and its
//   mark (á é í ó ú à è ù â ê î ô û ã õ ä ë ï ö ü ÿ ñ ç å). ¿ and ¡ are ? and ! turned over. Typographic quotes and
//   dashes are written as their plain ones. Any other character leaves a space's width. A new line ('\n') moves to
//   the next line down.
//   See docs/infinite_canvas_design.md §17 (Handwriting) for the feature as the user sees it.
// ===========================================================================

// Writes _text (UTF-8) out by hand and returns how many strokes it added.
// The points are appended to _points (an rde_arr of fude_zoom_v2, x right and y up). A capital is _cap tall, the first
// line's baseline runs along y = 0 from x = 0, and each next line is _cap × 1.9 lower.
// For each stroke, _ends (an rde_arr of u32) gets the number of points in _points up to and including the stroke's last
// point. _seed chooses the unevenness: the same text and seed always give the same strokes.
u32 fude_zoom_hand_write(const c8* _text, f64 _cap, u32 _seed, rde_arr* _points, rde_arr* _ends);

#endif
