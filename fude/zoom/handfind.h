// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HANDFIND
#define FUDE_ZOOM_HANDFIND

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// Handwriting for Find: a canvas's pen lines grouped into LINES OF WRITING, each
// to be read once as text (ML Kit's reading, as the lasso's To text reads: page.c
// asks, one line at a time, and keeps what each says by its key) — so Find finds
// words written by hand as it finds typed ones.
//
// A line of writing: pen strokes (not a marker's) in one frame, each about as
// tall as the frame's writing is (a long line of a drawing is no letter: more
// than 8 times the frame's usual stroke height, it is left out), next to each
// other along a line — their boxes overlapping up and down by a quarter of the
// smaller at least (or their middles within 0.6 of a usual height), and no more
// than a usual height apart across. Its KEY is its strokes' ids hashed (FNV-1a,
// in id order): the same strokes, the same key — what was read stays read while
// nothing in it changes, and a line written on is read again.
// ===========================================================================

#define FUDE_ZOOM_HAND_TALL 8.0    // a stroke longer than this many usual heights: a drawing's, not writing

typedef struct {
    u32           frame;
    fude_zoom_box box;           // its strokes', its frame's units
    u64           key;
    u32           first, count;  // its strokes in the strokes list (objects), in the order drawn
    f64           height;        // its frame's usual stroke height (to read it at a size)
} fude_zoom_hand_line;

// The lines of writing of everything shown (into _lines, cleared first; their strokes into
// _strokes, u32 objects). How many.
u32 fude_zoom_hand_lines(const fude_zoom_scene* _s, rde_arr* _lines, rde_arr* _strokes);

#endif
