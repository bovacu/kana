// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HOMELIST
#define FUDE_ZOOM_HOMELIST

#include "rde.h"
#include "drawing/ink/notes.h"

// ===========================================================================
// Home list: what the home screen (home.h) lists. It gathers a folder's folders and canvases as cards, each showing
// how the canvas was left, and works out the grid the cards go in.
//
// Where it sits: fude/zoom, under home.c, which is its only user besides the tests.
// Its main types and their lifecycle: fude_zoom_home_card is a plain value; the caller owns the rde_arr the cards go
// into.
// Threads: main thread only.
// Files: it reads, but never writes, the small files the page keeps beside each canvas: <id>.words (words.h: when the
// canvas was last left) and <id>.pages (pages.h: whether it is a notebook).
// Design: the home screen reads those side files rather than the canvases themselves, so listing a folder stays quick
// however big its canvases are.
// ===========================================================================

// One card on the home screen: a folder or a canvas.
typedef struct {
    u32 note;       // its id in notes.h
    u8  kind;       // FUDE_NOTE_FOLDER or FUDE_NOTE_CANVAS
    b8  notebook;   // a canvas that was made a notebook of pages (pages.h)
    b8  pdf;        // a canvas over a PDF
    u64 edited;     // in Unix seconds: when it was last left (from its words.h file), else when it was made
    u32 inside;     // for a folder, how many canvases are inside it, at any depth
} fude_zoom_home_card;

// Fills _cards (an rde_arr of fude_zoom_home_card, cleared first) with what folder _folder holds: its folders in their
// own order, then its canvases, the most recently left first (and of those left at the same time, the newest made
// first). Returns how many cards there are.
u32  fude_zoom_home_gather(const fude_notes* _notes, u32 _folder, rde_arr* _cards);
// Returns how many columns of cards fit across _width, each at least _least wide and _gap apart. Always at least 1.
u32  fude_zoom_home_columns(f32 _width, f32 _least, f32 _gap);
// Writes into _out the path of a file kept beside a canvas's own file: <id>.<_ext> in place of <id>.kana. Its cover
// uses "cover.png", its words "words".
void fude_zoom_home_side_path(u32 _canvas, const c8* _ext, c8* _out, usize _size);

#endif
