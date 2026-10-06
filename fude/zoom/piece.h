// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PIECE_H
#define FUDE_ZOOM_PIECE_H

#include "rde.h"
#include "drawing/base/kfile.h"
#include "zoom/scene.h"
#include "zoom/select.h"

// ===========================================================================
// Pieces: things kept to use again, on any canvas (Sketching's My pieces) — a
// jig's outline, a joint, a diagram's corner, a sign. Kept from the lasso (what
// it holds, an area with what is in it), each with a name; put back from
// Insert, in the middle of the view, at their TRUE SIZE: they are kept in the
// canvas's own units at ×1 (the home frame's), which are the same length on
// every canvas (with the screen calibrated, a millimetre stays a millimetre).
//
// Each thing is kept as the clipboard keeps it (select.h's fude_zoom_clip: its
// record's look, its points' bytes), its place a similarity from its own units
// to the piece's — round the piece's middle.
//
// On disk, beside the canvases: <saves>/pieces/index.kana (their order: ids)
// and <id>.piece each (kfile.h's format, kind 'PIEC': its name, its box, a
// chunk a thing).
// ===========================================================================

#define FUDE_ZOOM_PIECES      64u    // kept at most
#define FUDE_ZOOM_PIECE_NAME  64u    // a name's bytes, its NUL included
#define FUDE_ZOOM_PIECE_ITEMS 4096u  // things in one at most

typedef struct {
    u32           id;                          // its file: <id>.piece
    c8            name[FUDE_ZOOM_PIECE_NAME];
    fude_zoom_box box;                         // what it covers, round its middle (true size: home units)
    rde_arr TYPE(fude_zoom_clip) items;        // each thing; its on_screen: its own units → the piece's
} fude_zoom_piece;

typedef struct {
    fude_zoom_piece list[FUDE_ZOOM_PIECES];
    u32             count;
    u32             next_id;
    b8              loaded;
} fude_zoom_pieces;

// Read from _dir (once: again only after fude_zoom_pieces_free); a piece that cannot be read is left out.
void fude_zoom_pieces_load(fude_zoom_pieces* _p, const c8* _dir);
void fude_zoom_pieces_free(fude_zoom_pieces* _p);
// A new one kept (its things copied: _items' bytes stay the caller's), first in the list, written to _dir. Its
// place in the list; FUDE_ZOOM_NONE: full, empty or not written.
u32  fude_zoom_pieces_add(fude_zoom_pieces* _p, const c8* _dir, const c8* _name, const fude_zoom_clip* _items, u32 _n, fude_zoom_box _box);
// Piece _i let go: off the list, its file deleted.
void fude_zoom_pieces_remove(fude_zoom_pieces* _p, const c8* _dir, u32 _i);
// Renamed (written again).
void fude_zoom_pieces_rename(fude_zoom_pieces* _p, const c8* _dir, u32 _i, const c8* _name);

// One piece into bytes, and back (a piece made empty first; false: not one).
void fude_zoom_piece_write(const fude_zoom_piece* _piece, fude_bytes* _out);
b8   fude_zoom_piece_read(fude_zoom_piece* _piece, const u8* _data, u32 _size);
void fude_zoom_piece_clear(fude_zoom_piece* _piece);

// Its lines (the piece's units): each stroke's middle line, each fill's rings, each shape's outline, and —
// _boxes — a text's or a picture's box. Their points into _points (fude_zoom_v2), a line each into _lines
// (fude_zoom_piece_line). How many lines.
typedef struct {
    u32 first, count;
    b8  closed;
} fude_zoom_piece_line;
u32  fude_zoom_piece_lines(const fude_zoom_piece* _piece, b8 _boxes, rde_arr* _points, rde_arr* _lines);

#endif
