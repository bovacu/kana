// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_MARKS
#define FUDE_MARKS

#include "rde.h"

// ===========================================================================
// Study marks: which characters are being STUDIED and which are KNOWN. They are kept by code point, so they survive a
// re-bake of the character data. The learner sets them by hand (the viewer's Study button, Select mode's Study), and
// exams set them too (exam.h): three exams passed in a row make a character Known, and getting a Known one wrong
// sends it back to Studying. Browse filters by them, exams can take them as what to ask, and reviews schedule the
// marked characters (review.h).
//
// Where it sits: a study model (study/models). Its users are the viewer, Select mode, the exams, Browse and the chart
// (which draw each cell's mark with select.h), the statistics, and the study's verbs (study/app/verbs.c); the study's
// session opens the file (study/app/files.c).
// Its main types and their lifecycle: there is no object; the marks are one set held by this module. fude_marks_open
// loads them when the session opens and fude_marks_close frees them when the app ends. Without fude_marks_open the
// marks live in memory only, which is what the tests use.
// Threads: main thread only.
// Files: one small file, <save dir>/marks.kana, in the KANA format (kfile.h), written whole and atomically on every
// change and read back with fude_save_load (save.h: the main file, else its .bak). Its layout:
//   header kind 'MARK', then:
//   'MRKS'  u32 count, u32 record size (16), then per marked character, sorted by code point: u32 code point,
//           u8 mark (FUDE_MARK_), 3 reserved bytes, u64 when it was set (Unix seconds).
//   'MHIS'  u32 count, u32 record size (16), then per change, oldest first: u32 code point, u8 its new mark
//           (FUDE_MARK_NONE when it was unmarked), 3 reserved bytes, u64 when (Unix seconds).
// Design: every change is kept as well, oldest first (when each character became Studying, Known or neither), so the
// statistics can show the marks over time. A file from before the changes were kept has no 'MHIS' chunk; its history
// then starts with the marks it has, each at the time it was set. The marks are few (hundreds, a few thousand at
// most), so they are an array sorted by code point, searched by bisection.
// ===========================================================================

typedef enum {
    FUDE_MARK_NONE = 0,
    FUDE_MARK_STUDYING,
    FUDE_MARK_KNOWN,
    FUDE_MARK_COUNT
} FUDE_MARK_;

// Loads the marks from _path, and saves there from now on. When nothing has been saved yet there are no marks.
// A NULL _path keeps the marks in memory only (the tests), as not opening them at all does.
void       fude_marks_open(const c8* _path);
// Forgets the marks, their history and the path, and frees their memory. Nothing is saved.
void       fude_marks_close(void);

// The mark on _codepoint (FUDE_MARK_NONE when it has none, or the marks are not open).
FUDE_MARK_ fude_marks_get(u32 _codepoint);
// Sets one character's mark (FUDE_MARK_NONE removes it), keeps the change, and saves. Returns false when the file
// could not be written; the mark is set in memory all the same. Setting the mark it already has changes nothing.
b8         fude_marks_set(u32 _codepoint, FUDE_MARK_ _mark);
// The same for several characters at once, with one save.
b8         fude_marks_set_many(const u32* _codepoints, u32 _count, FUDE_MARK_ _mark);
// The same, as if done at _time (Unix seconds): for the tests' and the demo's planted histories.
b8         fude_marks_set_many_at(const u32* _codepoints, u32 _count, FUDE_MARK_ _mark, u64 _time);

// How many characters carry _mark; and their code points, in code point order, at most _max of them into _out.
// fude_marks_list returns how many it wrote (_out may be NULL to only count). Neither counts FUDE_MARK_NONE.
u32        fude_marks_count(FUDE_MARK_ _mark);
u32        fude_marks_list(FUDE_MARK_ _mark, u32* _out, u32 _max);

// A number that goes up on every change, so the screens that show marks notice.
u32        fude_marks_revision(void);

// One change of mark.
RDE_STRUCT {
    u32 codepoint;
    u8  mark;        // The new FUDE_MARK_ (FUDE_MARK_NONE: it was unmarked).
    u64 time;        // When, in Unix seconds.
} fude_marks_change;

// Every change of mark, oldest first, and how many there are. The pointer is the module's: NULL before the marks are
// opened, and valid only until the next change, open or close.
u32                      fude_marks_history_count(void);
const fude_marks_change* fude_marks_history(void);

#endif
