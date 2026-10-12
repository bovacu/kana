// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_WORDS
#define FUDE_ZOOM_WORDS

#include "rde.h"

// ===========================================================================
// A CANVAS'S WORDS (0.1.92), kept beside it (<id>.words) so Find can look through
// every canvas without opening them: its texts, sticky notes, diagram labels,
// boards' names and the handwriting read on it so far, a line each — written when
// the canvas is left, with how big its file was then (a file grown since: its
// words read again from it).
//
//   header  "KANA" u32 version u32 kind 'WRDS'
//   u64     the canvas file's size when they were gathered
//   u64     when the canvas was left then (Unix seconds: the home screen's newest first)
//   u32     their bytes, then the bytes (UTF-8, '\n' between)
//
// Letters are matched as Find matches them: A to Z in either case alike, the rest
// as written.
// ===========================================================================

// Kept (false: not written); read back into _out (c8, NUL after them; false: none, or not one — *_left 0).
b8  fude_zoom_words_save(const c8* _path, u64 _canvas_size, u64 _left, const c8* _words, u32 _len);
b8  fude_zoom_words_load(const c8* _path, u64* _canvas_size, u64* _left, rde_arr* _out);

// How many times _needle is in _hay (_len bytes), not overlapping (0: an empty needle).
u32 fude_zoom_words_count(const c8* _hay, u32 _len, const c8* _needle);
// Where it first is (FUDE_ZOOM_WORDS_NONE: nowhere).
#define FUDE_ZOOM_WORDS_NONE 0xFFFFFFFFu
u32 fude_zoom_words_find(const c8* _hay, u32 _len, const c8* _needle);

// The line its first match is on, into _out (_size bytes, NUL-ended): as much round the match as fits _most
// characters, "…" where it was cut (a whole character never split), its marks (# - * ` [ ]) as written.
// False: no match.
b8  fude_zoom_words_excerpt(const c8* _hay, u32 _len, const c8* _needle, u32 _most, c8* _out, u32 _size);

#endif
