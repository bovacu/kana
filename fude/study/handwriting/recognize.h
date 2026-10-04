#ifndef FUDE_RECOGNIZE
#define FUDE_RECOGNIZE

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "drawing/ink/ink.h"
#include "study/handwriting/match.h"
#include "study/services/mlkit.h"

// ===========================================================================
// Recognition — what is written — in one place, for every screen that asks:
// Check (a selection on a canvas, any number of characters) and Browse's
// draw-to-search (one character on its pad). The same drawing reads the same
// in both.
//
// Google ML Kit (mlkit.h) reads when it is on and has its model. Its answer is
// text: a best reading and its alternatives. Kana's matcher (match.h) is what
// is left without it, and what fills in the candidates ML Kit does not name.
//
// ML Kit answers later: start, then poll each frame. It reads one request at a
// time for the whole app, so a request is a ticket, and an answer for a ticket
// nobody holds any more is dropped.
// ===========================================================================

#define FUDE_RECOGNIZE_LINES 10
#define FUDE_RECOGNIZE_LINE  256

RDE_STRUCT {
    u32 ticket;                                              // the request in flight (0: none)
    b8  answered;                                            // the lines hold an answer
    u32 line_count;
    c8  lines[FUDE_RECOGNIZE_LINES][FUDE_RECOGNIZE_LINE];   // ML Kit's readings, best first (UTF-8)
} fude_recognition;

// Can ML Kit read now: on, and its model on the device?
b8   fude_recognize_available(void);

// Starts ML Kit reading the alive strokes of _ink (any units, Y up), and forgets
// the earlier answer. False when it cannot: not available, or another request
// still in flight (then try again on a later frame).
b8   fude_recognize_start(fude_recognition* _r, const fude_ink* _ink);

// True the frame the answer arrives: the lines filled (none when ML Kit could
// read nothing).
b8   fude_recognize_poll(fude_recognition* _r);

// Forgets the request and the answer.
void fude_recognize_forget(fude_recognition* _r);

// The characters of a reading, as records (only those the data has; the rest,
// spaces for one, skipped): how many, at most _max.
u32  fude_recognize_records(const fude_kanji_db* _db, const c8* _line, u32* _out, u32 _max);

// Are records _a and _b the same letter, a joined form as its letter (lang.h:
// fude_lang_letter; Arabic's ﺑ is ب)? Recognition reads letters, not forms.
b8   fude_recognize_same(const fude_kanji_db* _db, u32 _a, u32 _b);

// One character's candidates, best first — character _index of a reading
// _length characters long: first what ML Kit's readings of that length have
// there, then _matched (the matcher's own ranking) — each character once, only
// what _catalog lets through with _filter (a NULL _catalog: anything in the
// data). At most _max; how many. ML Kit's cost 0, the matcher's as ranked.
u32  fude_recognize_candidates(const fude_kanji_db* _db, const fude_recognition* _r, u32 _index, u32 _length,
                               const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                               const fude_match_result* _matched, u32 _matched_count, fude_match_result* _out, u32 _max);

#endif
