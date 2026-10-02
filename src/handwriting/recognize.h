#ifndef KANA_RECOGNIZE
#define KANA_RECOGNIZE

#include "rde.h"
#include "chars/kanji.h"
#include "chars/catalog.h"
#include "ink/ink.h"
#include "handwriting/match.h"
#include "services/mlkit.h"

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

#define KANA_RECOGNIZE_LINES 10
#define KANA_RECOGNIZE_LINE  256

RDE_STRUCT {
    u32 ticket;                                              // the request in flight (0: none)
    b8  answered;                                            // the lines hold an answer
    u32 line_count;
    c8  lines[KANA_RECOGNIZE_LINES][KANA_RECOGNIZE_LINE];   // ML Kit's readings, best first (UTF-8)
} kana_recognition;

// Can ML Kit read now: on, and its model on the device?
b8   kana_recognize_available(void);

// Starts ML Kit reading the alive strokes of _ink (any units, Y up), and forgets
// the earlier answer. False when it cannot: not available, or another request
// still in flight (then try again on a later frame).
b8   kana_recognize_start(kana_recognition* _r, const kana_ink* _ink);

// True the frame the answer arrives: the lines filled (none when ML Kit could
// read nothing).
b8   kana_recognize_poll(kana_recognition* _r);

// Forgets the request and the answer.
void kana_recognize_forget(kana_recognition* _r);

// The characters of a reading, as records (only those the data has; the rest,
// spaces for one, skipped): how many, at most _max.
u32  kana_recognize_records(const kana_kanji_db* _db, const c8* _line, u32* _out, u32 _max);

// One character's candidates, best first — character _index of a reading
// _length characters long: first what ML Kit's readings of that length have
// there, then _matched (the matcher's own ranking) — each character once, only
// what _catalog lets through with _filter (a NULL _catalog: anything in the
// data). At most _max; how many. ML Kit's cost 0, the matcher's as ranked.
u32  kana_recognize_candidates(const kana_kanji_db* _db, const kana_recognition* _r, u32 _index, u32 _length,
                               const kana_catalog* _catalog, KANA_FILTER_ _filter,
                               const kana_match_result* _matched, u32 _matched_count, kana_match_result* _out, u32 _max);

#endif
