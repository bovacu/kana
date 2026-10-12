// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HOMELIST
#define FUDE_ZOOM_HOMELIST

#include "rde.h"
#include "drawing/ink/notes.h"

// ===========================================================================
// What the home screen (home.h) lists: a folder's folders and canvases as cards,
// how each was left — read from the files the page keeps beside each canvas
// (words.h: when it was left; pages.h: a notebook) — and the grid they go in.
// ===========================================================================

typedef struct {
    u32 note;       // notes.h id
    u8  kind;       // FUDE_NOTE_
    b8  notebook;   // a canvas made a notebook (pages.h)
    b8  pdf;        // a canvas over a PDF
    u64 edited;     // Unix seconds: when it was last left (its words.h file), else when it was made
    u32 inside;     // a folder's: what is in it
} fude_zoom_home_card;

// What folder _folder holds, as cards into _cards (cleared first): its folders in their order, then its canvases most
// recently left first (ties: the newest made). How many.
u32  fude_zoom_home_gather(const fude_notes* _notes, u32 _folder, rde_arr* _cards);
// A grid's columns _width wide, each at least _least, _gap apart (1 at least).
u32  fude_zoom_home_columns(f32 _width, f32 _least, f32 _gap);
// <canvas>.<_ext> beside a canvas's own file (its cover: "cover.png"; its words: "words").
void fude_zoom_home_side_path(u32 _canvas, const c8* _ext, c8* _out, usize _size);

#endif
