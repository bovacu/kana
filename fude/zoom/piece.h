// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PIECE_H
#define FUDE_ZOOM_PIECE_H

#include "rde.h"
#include "drawing/base/kfile.h"
#include "zoom/scene.h"
#include "zoom/select.h"

// ===========================================================================
// Pieces: things kept to use again on any canvas (My pieces): a jig's outline, a joint, a diagram's
// corner, a sign. A piece is kept from the lasso (what it holds, or an area with what is in it), with a name, and put
// back from Insert in the middle of the view at its TRUE SIZE: it is kept in the canvas's own units at ×1 (the home
// frame's), which are the same length on every canvas, so with the screen calibrated a millimetre stays a millimetre.
//
// Where it sits: zoom, the canvas apps' layer, over select.h's clipboard records. The page owns the list
// (fude_zoom_pieces), loading it the first time it is wanted (page_pieces.c); custom parts (My parts) keep their
// drawing as a piece of their own beside them.
// Its main types and their lifecycle: fude_zoom_pieces holds up to FUDE_ZOOM_PIECES pieces; fude_zoom_pieces_load fills
// it and fude_zoom_pieces_free lets everything go. A fude_zoom_piece owns its items and their bytes;
// fude_zoom_piece_clear frees them.
// Threads: main thread only.
// Files: in the pieces folder beside the canvases (<saves>/pieces/): index.kana, their order (kfile.h's format, kind
// 'PIDX': the next id, then the ids), and <id>.piece for each (kind 'PIEC': a NAME chunk, a BOX chunk, and an ITEM
// chunk for each thing). All are written atomically (kfile.h).
// Each thing is kept as the clipboard keeps it (select.h's fude_zoom_clip: its record's look and its points' bytes),
// with its place: a similarity from its own units to the piece's, round the piece's middle.
// Design: see docs/infinite_canvas_design.md §23.
// ===========================================================================

#define FUDE_ZOOM_PIECES      64u    // The most pieces kept.
#define FUDE_ZOOM_PIECE_NAME  64u    // A name's size in bytes, its NUL included.
#define FUDE_ZOOM_PIECE_ITEMS 4096u  // The most things in one piece.

// One piece: its things, at true size round its middle.
typedef struct {
    u32           id;                          // Its number, which names its file: <id>.piece.
    c8            name[FUDE_ZOOM_PIECE_NAME];
    fude_zoom_box box;                         // What it covers, round its middle, at true size (home frame units).
    // Each thing, with its bytes.
    rde_arr TYPE(fude_zoom_clip) items;        // A thing's on_screen: from its own units to the piece's.
    // The families of its symbols (fude_zoom_piece_families), worked out when it was read or made.
    u32           families;                    // Bits: 1 << symbol.h's family.
} fude_zoom_piece;

// The list of pieces, the newest first.
typedef struct {
    fude_zoom_piece list[FUDE_ZOOM_PIECES];
    u32             count;
    u32             next_id;
    b8              loaded;
} fude_zoom_pieces;

// Reads the list from the folder _dir (ending in '/'), only once: again only after fude_zoom_pieces_free. A piece
// that cannot be read is left out.
void fude_zoom_pieces_load(fude_zoom_pieces* _p, const c8* _dir);
// Frees every piece and clears the list, so the next fude_zoom_pieces_load reads the folder again.
void fude_zoom_pieces_free(fude_zoom_pieces* _p);
// Keeps a new piece named _name: _n things copied from _items (their bytes stay the caller's) and its box _box. It goes
// first in the list and is written to _dir, with the list's order. Returns its place in the list (0), or
// FUDE_ZOOM_NONE when the list is full, _n is 0, or it could not be written.
u32  fude_zoom_pieces_add(fude_zoom_pieces* _p, const c8* _dir, const c8* _name, const fude_zoom_clip* _items, u32 _n, fude_zoom_box _box);
// Lets piece _i go: takes it off the list, writes the list's order, and deletes its file.
void fude_zoom_pieces_remove(fude_zoom_pieces* _p, const c8* _dir, u32 _i);
// Renames piece _i and writes it again. Returns false when there is no piece _i, or when its file could not be written
// (logged): it then has its new name until the app closes, and its old one in its file; the caller says so.
b8   fude_zoom_pieces_rename(fude_zoom_pieces* _p, const c8* _dir, u32 _i, const c8* _name);
// Replaces piece _i's things with _n copied from _items (their bytes stay the caller's) and its box with _box,
// keeping its name, its id and its place in the list, and writes it again. Returns false when there is no such
// piece, _n is 0, or it could not be written.
b8   fude_zoom_pieces_replace(fude_zoom_pieces* _p, const c8* _dir, u32 _i, const fude_zoom_clip* _items, u32 _n, fude_zoom_box _box);
// Write one piece as a file of its own at _path, and read one back (a custom part's drawing, kept beside it: My
// parts). Both return false when the file could not be written or read; a piece read is cleared first.
b8   fude_zoom_piece_save(const fude_zoom_piece* _piece, const c8* _path);
b8   fude_zoom_piece_load(fude_zoom_piece* _piece, const c8* _path);

// Returns the families of the symbols in it, as bits (1 << symbol.h's family); 0 when it has none: a drawing.
u32  fude_zoom_piece_families(const fude_zoom_piece* _piece);

// fude_zoom_piece_write appends one piece's file bytes to _out. fude_zoom_piece_read reads them back into _piece
// (cleared first); false when they are not a piece or hold no things. fude_zoom_piece_clear frees a piece's things
// and zeroes it.
void fude_zoom_piece_write(const fude_zoom_piece* _piece, fude_bytes* _out);
b8   fude_zoom_piece_read(fude_zoom_piece* _piece, const u8* _data, u32 _size);
void fude_zoom_piece_clear(fude_zoom_piece* _piece);

// One line of a piece's drawing (fude_zoom_piece_lines): its points' first index and count, and whether it closes.
typedef struct {
    u32 first, count;
    b8  closed;
} fude_zoom_piece_line;
// Collects a piece's lines in the piece's units, as its picture is drawn: each stroke's centreline, each fill's rings,
// each shape's outline and, with _boxes, each text's and picture's box. The points go into _points (fude_zoom_v2) and
// a fude_zoom_piece_line for each line into _lines; both are cleared first. Returns how many lines.
u32  fude_zoom_piece_lines(const fude_zoom_piece* _piece, b8 _boxes, rde_arr* _points, rde_arr* _lines);

#endif
