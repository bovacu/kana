// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HANDFIND
#define FUDE_ZOOM_HANDFIND

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// Handfind: a canvas's pen strokes grouped into lines of writing, so that Find can find words written by hand as it
// finds typed ones.
//
// Where it sits: zoom, the canvas apps' layer, over scene.h. The page (page_find.c) calls it for Find: it hands each
// line to the handwriting reader (ML Kit, the same reader as the lasso's To text), one line at a time, and keeps what
// each line says by the line's key.
// Its main types and their lifecycle: fude_zoom_hand_line is plain data, written into an array the caller creates and
// frees.
// Threads: called on the main thread. It only reads the scene.
// Design:
//   A line of writing is a set of pen strokes (not a marker's) in one frame, each about as tall as the frame's
//   writing usually is. A stroke whose box is more than FUDE_ZOOM_HAND_TALL (8) times the frame's usual stroke height
//   is a line of a drawing, not a letter, and is left out. Strokes join a line when they sit next to each other along
//   it: their boxes overlap up and down by at least a quarter of the smaller one (or their middles are within 0.6 of a
//   usual height), and they are no more than a usual height apart across.
//   A line's KEY is a hash of its strokes' ids. The same strokes always give the same key, so what was read stays read
//   while nothing in the line changes, and a line that is written on, or has a letter rubbed out, is read again.
//   See docs/infinite_canvas_design.md §18 (Find reads handwriting).
// ===========================================================================

#define FUDE_ZOOM_HAND_TALL 8.0    // a stroke longer than this many usual heights is a drawing's, not writing

// One line of writing.
typedef struct {
    u32           frame;
    fude_zoom_box box;           // the box round its strokes, in its frame's units
    u64           key;   // rde_hash_64 of its strokes' ids, sorted; the page keeps what the line says under this key.
                           // Two lines sharing a key is a 1-in-2^64 chance, accepted rather than keeping every id list.
    u32           first, count;  // where its strokes are in the strokes array (object indices), in the order drawn
    f64           height;        // its frame's usual stroke height, so the reader can scale the line to a set size
} fude_zoom_hand_line;

// Finds the lines of writing among everything shown and returns how many there are. _lines (fude_zoom_hand_line) and
// _strokes (u32 scene object indices) are cleared first; each line's strokes are a run of _strokes. Only live, shown
// pen strokes count: markers, hidden objects and strokes in frames not shown are left out.
u32 fude_zoom_hand_lines(const fude_zoom_scene* _s, rde_arr* _lines, rde_arr* _strokes);

#endif
