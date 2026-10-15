// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_PAGEFILE
#define FUDE_PAGEFILE

#include "rde.h"
#include "drawing/base/save.h"
#include "drawing/ink/ink.h"
#include "drawing/ink/canvas.h"

// ===========================================================================
// Page file: a page's file, holding its ink, its view and its paper, in the KANA format (kfile.h).
//
// Where it sits: drawing/ink, over ink.h and canvas.h. The session (app/session.c) saves and loads the open canvas's
// page with it, and app/documents.c writes a new document canvas's empty page.
// Threads: main thread only.
// Files: the page file, of kind DOCUMENT ('DOC '):
//   'VIEW'  f32 offset.x, offset.y, zoom
//   'PAGE'  u8 paper (FUDE_PAPER_: 0 dots, 1 squares, 2 lines, 3 none, then an app's own papers). Optional: a file
//           from before it has none, which means dots.
//   'TURN'  a document's pages turned (canvas.h): u32 count, then per page u16 page, u8 quarter turns clockwise.
//           Optional: without it, no page is turned.
//   'STRK'  u32 count, u32 record size, then per stroke: u32 point_count, u8 r g b a, u8 flags (bit 0: from_pen;
//           bit 1: the marker's, see-through under the other ink), 3 reserved bytes. A colour of r g b a all 0 means
//           the theme's ink; 30 30 36 255, the default ink from before themes, is read as that too.
//   'PNTS'  u32 count, u32 record size, then every point of those strokes in order: f32 x, y, pressure, radius,
//           time (canvas units, seconds).
// Design: only ALIVE strokes are written, and not the undo history: a loaded page is where the history starts, as in
// most drawing apps. A reader parses the whole file before it touches the ink, so a damaged file changes nothing.
// Records carry their size, so a newer build's longer ones are read and their extra fields skipped.
// ===========================================================================

#define FUDE_PAGEFILE_VERSION   1u
#define FUDE_SAVE_DOCUMENT_FILE "page.kana"   // the one page's file, in the save folder, from before there were notes

// Saves the ink's alive strokes, the view and the page's paper and turns to _path, atomically (kfile.h). Returns
// false when the file could not be written. _out_bytes (may be NULL) gets the file's size.
b8         fude_save_document(const c8* _path, const fude_ink* _ink, fude_view _view, fude_page _page, u32* _out_bytes);
// Loads a page file, or its backup (save.h), into an EMPTY ink (fresh from fude_ink_init), the view and the page. The
// page (zeroed, or one already in use) gets the file's list of turns in place of its own, which is freed
// (canvas.h: fude_page_reset). Returns how the load went; on MISSING or CORRUPT, _ink, _view and _page are untouched.
FUDE_LOAD_ fude_load_document(const c8* _path, fude_ink* _ink, fude_view* _view, fude_page* _page);

#endif
