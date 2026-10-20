// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CUSTOM_H
#define FUDE_ZOOM_CUSTOM_H

#include "rde.h"

// ===========================================================================
// Custom: what a person keeps and where they edit it (the library's Custom tab). A piece kept from the
// lasso (piece.h) and a custom part made from a circuit (logic.h) each have a canvas of their own, its TEMPLATE: what
// is drawn on that canvas is what the piece is, or what is inside the part. This module keeps the map from each piece
// or part to its template canvas.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_custom.c) owns one fude_zoom_templates, opens a template
// from the library (Edit) or from a part on any canvas (Inside), and keeps what it holds when it is left.
// Its main types and their lifecycle: fude_zoom_templates is the caller's struct; fude_zoom_templates_init creates its
// list on the standard heap and fude_zoom_templates_free frees it.
// Threads: called on the main thread. It keeps no global state.
// Files: <parts>/templates.txt beside the parts, plain text with one line for each template:
//   "p <piece id> <canvas>" for a kept piece, "u <part id> <canvas>" for a custom part. A key holds no spaces (a
//   piece's is a number, a part's a slug), so a line splits on them. Written atomically (kfile.h).
// Design: a template canvas is made the first time it is opened (the piece put on it, or the part's inside drawn),
//   and what it holds when it is left is kept as the piece, or made the part again, so every copy of it on every
//   canvas follows. See docs/infinite_canvas_design.md §41.
// ===========================================================================

// What a template belongs to.
typedef enum {
    FUDE_ZOOM_CUSTOM_PIECE = 0,   // a kept piece, keyed by its id (piece.h)
    FUDE_ZOOM_CUSTOM_PART         // a custom part, keyed by its definition's id ("user/…")
} FUDE_ZOOM_CUSTOM_;

#define FUDE_ZOOM_CUSTOM_KEY 64u  // the most bytes of a key, its ending 0 included

// One piece's or part's template.
typedef struct {
    u8  kind;                     // FUDE_ZOOM_CUSTOM_
    c8  key[FUDE_ZOOM_CUSTOM_KEY];
    u32 canvas;                   // its template canvas's note id (drawing/ink/notes.h)
} fude_zoom_template;

// The map from pieces and parts to their template canvases.
typedef struct {
    rde_arr TYPE(fude_zoom_template) list;
} fude_zoom_templates;

// fude_zoom_templates_init sets up an empty map on the standard heap; fude_zoom_templates_free frees it.
void fude_zoom_templates_init(fude_zoom_templates* _t);
void fude_zoom_templates_free(fude_zoom_templates* _t);
// fude_zoom_templates_read replaces the map with what the file at _path says; a missing or unreadable file leaves it
// empty (an unreadable one logged), and lines it cannot read are skipped. fude_zoom_templates_write writes the map to
// _path and returns false when it could not be written.
void fude_zoom_templates_read(fude_zoom_templates* _t, const c8* _path);
b8   fude_zoom_templates_write(const fude_zoom_templates* _t, const c8* _path);
// Returns the template canvas of _kind's _key, or 0 when it has none yet.
u32  fude_zoom_templates_canvas(const fude_zoom_templates* _t, u8 _kind, const c8* _key);
// Returns whose template canvas _canvas is, or NULL when it is nobody's (or _canvas is 0). The pointer is into the map
// and stays valid until the map changes.
const fude_zoom_template* fude_zoom_templates_of(const fude_zoom_templates* _t, u32 _canvas);
// Sets _kind's _key's template canvas to _canvas, replacing any it had. A NULL, empty or too long key, or one with a
// space (which could not be read back), is ignored.
void fude_zoom_templates_set(fude_zoom_templates* _t, u8 _kind, const c8* _key, u32 _canvas);
// Forgets _kind's _key and returns the template canvas it had, or 0 when it had none.
u32  fude_zoom_templates_drop(fude_zoom_templates* _t, u8 _kind, const c8* _key);
// Moves _kind's _key's template to the key _to (a piece's or a part's new id). Returns false, changing nothing, when
// _key has none, when _to could not be a key (as fude_zoom_templates_set refuses it), or when _to has a template of
// its own, which would be left belonging to nobody; true when it moved (or _to is _key).
b8   fude_zoom_templates_rekey(fude_zoom_templates* _t, u8 _kind, const c8* _key, const c8* _to);

#endif
