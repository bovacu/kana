#ifndef KANA_MARKS
#define KANA_MARKS

#include "rde.h"

// ===========================================================================
// Study marks: which characters are being STUDIED, and which are KNOWN — by
// code point, so they survive a re-bake of the character data. Set by hand (the
// viewer's Study, Select mode's Study) and by exams (exam.h): three exams passed
// in a row make a character Known, and failing a Known one sends it back to
// Studying. Browse filters by them; exams can take them as their source.
//
// One small file (<save dir>/marks.kana), loaded by kana_marks_open and written
// whole on every change (atomically, see kfile.h). Without kana_marks_open the
// marks live in memory only (the tests).
//
// Every change is kept too, oldest first — when each character became Studying,
// Known, or neither — for the statistics to show the marks over time. A file
// from before the changes were kept starts its history with the marks it has,
// each at the time it was set.
//
// FILE ('KANA' header, kind 'MARK'):
//   'MRKS'  u32 count, u32 record size (16), then per marked character, sorted
//           by code point: u32 code point, u8 mark (KANA_MARK_), 3 reserved,
//           u64 when it was set (Unix seconds)
//   'MHIS'  u32 count, u32 record size (16), then per change, oldest first:
//           u32 code point, u8 its new mark (KANA_MARK_NONE: unmarked),
//           3 reserved, u64 when (Unix seconds)
// ===========================================================================

typedef enum {
    KANA_MARK_NONE = 0,
    KANA_MARK_STUDYING,
    KANA_MARK_KNOWN,
    KANA_MARK_COUNT
} KANA_MARK_;

// Loads the marks from _path (missing: none yet), and saves there from now on.
void       kana_marks_open(const c8* _path);
// Forgets them (and the path).
void       kana_marks_close(void);

KANA_MARK_ kana_marks_get(u32 _codepoint);
// Sets one (NONE removes it) and saves. False when the file could not be written
// (the mark is set all the same).
b8         kana_marks_set(u32 _codepoint, KANA_MARK_ _mark);
// Several at once, one save.
b8         kana_marks_set_many(const u32* _codepoints, u32 _count, KANA_MARK_ _mark);
// The same, as if at _time (Unix seconds) — for the tests' planted histories.
b8         kana_marks_set_many_at(const u32* _codepoints, u32 _count, KANA_MARK_ _mark, u64 _time);

// How many carry _mark; their code points, in code point order (at most _max).
u32        kana_marks_count(KANA_MARK_ _mark);
u32        kana_marks_list(KANA_MARK_ _mark, u32* _out, u32 _max);

// Goes up on every change, for screens that show marks to notice.
u32        kana_marks_revision(void);

// One change of mark.
RDE_STRUCT {
    u32 codepoint;
    u8  mark;        // KANA_MARK_: the new one (NONE: unmarked)
    u64 time;        // Unix seconds
} kana_marks_change;

// Every change, oldest first.
u32                      kana_marks_history_count(void);
const kana_marks_change* kana_marks_history(void);

#endif
