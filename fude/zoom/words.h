// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_WORDS
#define FUDE_ZOOM_WORDS

#include "rde.h"

// ===========================================================================
// Words: a canvas's words (0.1.92), kept in a file beside it (<id>.words) so Find can look through every canvas
// without opening them, and the plain search Find uses on them.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_find.c) gathers and saves a canvas's words when the
// canvas is left, and searches them; the home screens (home.c, homelist.c) read them back.
// Its main types and their lifecycle: none of its own; the caller owns the arrays and buffers.
// Threads: called on the main thread. It keeps no state between calls.
// Files: <id>.words beside the canvas, in the KANA format (drawing/base/kfile.h: little-endian, 64-bit numbers as two
//   32-bit halves, low first), written atomically:
//     header  "KANA" u32 version (1) u32 kind 'WRDS'
//     u64     the canvas file's size when the words were gathered
//     u64     when the canvas was left (Unix seconds; the home screen lists the newest first)
//     u32     how many bytes of words follow, then the bytes (UTF-8, '\n' between lines)
// Design:
//   The words are the canvas's texts, sticky notes, diagram labels, boards' names and the handwriting read on it so
//   far, one to a line. They are written when the canvas is left, with how big its file was then. A canvas file
//   whose size has changed since has its words read again from it, so the kept words never go stale.
//   Letters are matched as Find matches them: A to Z alike in either case, every other byte exactly as written.
// ===========================================================================

// fude_zoom_words_save writes the words file at _path: the canvas file's size _canvas_size, the time it was left
// _left (Unix seconds), and _len bytes of _words. Returns false when it could not be written.
// fude_zoom_words_load reads one back: the words into _out (an rde_arr of c8, cleared first, with a 0 after them),
// and the size and time into *_canvas_size and *_left. Returns false, with *_canvas_size and *_left 0 and _out empty,
// when there is no such file or it is not a words file.
b8  fude_zoom_words_save(const c8* _path, u64 _canvas_size, u64 _left, const c8* _words, u32 _len);
b8  fude_zoom_words_load(const c8* _path, u64* _canvas_size, u64* _left, rde_arr* _out);
// Reads only when the canvas was left (Unix seconds) from the words file at _path into *_left: the start of the file,
// not its words, so a list of every canvas (the home screen's) costs a few bytes each however much each holds. Returns
// false, with *_left 0, when there is no such file or it does not start as a words file. The words after it are not
// looked at: a file whose words were cut short still gives its time.
b8  fude_zoom_words_left(const c8* _path, u64* _left);

// Returns how many times _needle (ended by a 0) is in _hay (_len bytes), not overlapping; 0 for an empty needle.
u32 fude_zoom_words_count(const c8* _hay, u32 _len, const c8* _needle);
// What fude_zoom_words_find returns when there is no match.
#define FUDE_ZOOM_WORDS_NONE 0xFFFFFFFFu
// Returns the byte where _needle is first in _hay, or FUDE_ZOOM_WORDS_NONE when it is nowhere (or empty).
u32 fude_zoom_words_find(const c8* _hay, u32 _len, const c8* _needle);

// Writes the line _needle is first found on into _out (_size bytes, ended by a 0), for Find's list: as much of the
// line around the match as fits in _most characters (0: the whole line), with "…" where it was cut. A character is
// never split, and the line's marks (# - * ` [ ]) are left as written. Returns false when there is no match.
b8  fude_zoom_words_excerpt(const c8* _hay, u32 _len, const c8* _needle, u32 _most, c8* _out, u32 _size);

#endif
