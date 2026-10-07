// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CUSTOM_H
#define FUDE_ZOOM_CUSTOM_H

#include "rde.h"

// ===========================================================================
// WHAT A PERSON KEEPS, AND WHERE THEY EDIT IT (Sketching's library, its
// Custom tab): a piece kept from the lasso (piece.h) and a custom part made
// of a circuit (logic.h) each have a canvas of their own, its TEMPLATE —
// what is drawn on it is what the piece is, or what is inside the part.
// Opened from the library (Edit) or from a part on any canvas (Inside), it
// is made the first time (the piece put on it, the part's inside drawn),
// and what it holds when it is left is kept as the piece, or made the part
// again: every one of it on every canvas follows.
//
// Which canvas is whose: a line each in a file beside the parts —
// "p <piece id> <canvas>", "u <part id> <canvas>".
// ===========================================================================

typedef enum {
    FUDE_ZOOM_CUSTOM_PIECE = 0,   // a kept piece: its key its id (piece.h)
    FUDE_ZOOM_CUSTOM_PART         // a custom part: its key its definition's id ("user/…")
} FUDE_ZOOM_CUSTOM_;

#define FUDE_ZOOM_CUSTOM_KEY 64u  // a key's bytes, its NUL included

typedef struct {
    u8  kind;                     // FUDE_ZOOM_CUSTOM_
    c8  key[FUDE_ZOOM_CUSTOM_KEY];
    u32 canvas;                   // its template's note (notes.h)
} fude_zoom_template;

typedef struct {
    rde_arr TYPE(fude_zoom_template) list;
} fude_zoom_templates;

void fude_zoom_templates_init(fude_zoom_templates* _t);
void fude_zoom_templates_free(fude_zoom_templates* _t);
// Read from _path (what was there forgotten; a missing file: none), written to it. False: not written.
void fude_zoom_templates_read(fude_zoom_templates* _t, const c8* _path);
b8   fude_zoom_templates_write(const fude_zoom_templates* _t, const c8* _path);
// The canvas of _kind's _key (0: none yet).
u32  fude_zoom_templates_canvas(const fude_zoom_templates* _t, u8 _kind, const c8* _key);
// Whose template canvas _canvas is (NULL: nobody's).
const fude_zoom_template* fude_zoom_templates_of(const fude_zoom_templates* _t, u32 _canvas);
// _kind's _key's canvas made _canvas (in place of one it had).
void fude_zoom_templates_set(fude_zoom_templates* _t, u8 _kind, const c8* _key, u32 _canvas);
// _kind's _key forgotten: its canvas (0: it had none).
u32  fude_zoom_templates_drop(fude_zoom_templates* _t, u8 _kind, const c8* _key);
// _kind's _key now _to (a piece's, a part's other id).
void fude_zoom_templates_rekey(fude_zoom_templates* _t, u8 _kind, const c8* _key, const c8* _to);

#endif
