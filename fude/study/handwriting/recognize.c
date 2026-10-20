// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/handwriting/recognize.h"
#include "drawing/base/redraw.h"
#include "lang/lang.h"

// ===========================================================================
// See recognize.h.
//
// ML Kit reads one request at a time for the whole app, so each request gets a ticket. The answer is kept here, with
// the ticket it is for, until the fude_recognition that holds that ticket polls for it; an answer whose ticket
// nobody holds any more just waits until it is replaced. While ML Kit has not answered, no other request can start,
// but never for long: a reading with no answer after 10 seconds is taken as an empty answer (mlkit.h's
// fude_mlkit_poll; FUDE_MLKIT_GIVE_UP, GIVE_UP_NS), which frees the ticket here like any other answer.
// ===========================================================================

static u32 fude_recognize_next = 1;       // the next ticket to hand out
static u32 fude_recognize_inflight;       // the ticket ML Kit is reading for (0: none)
static u32 fude_recognize_done;           // the ticket of the answer waiting (0: none)
// The answer waiting: ML Kit's readings, one a line, best first.
static c8  fude_recognize_answer[4096];

// Takes ML Kit's answer, when it has come, and keeps it for its ticket.
static void fude_recognize_pump(void) {
    if(fude_recognize_inflight != 0 && fude_mlkit_poll(fude_recognize_answer, sizeof(fude_recognize_answer), NULL)) {
        fude_recognize_done     = fude_recognize_inflight;
        fude_recognize_inflight = 0;
        fude_redraw_changed();   // (render on demand: the screen waiting on it shows it)
    } else if(fude_recognize_inflight != 0) {
        fude_redraw_after(FUDE_REDRAW_POLL);   // (render on demand: ML Kit's answer is polled, with no event)
    }
}

// True when ML Kit is switched on and its model is ready.
b8 fude_recognize_available(void) {
    return fude_mlkit_enabled() && fude_mlkit_state() == FUDE_MLKIT_READY;
}

// Forgets the request and the answer (see the header).
void fude_recognize_forget(fude_recognition* _r) {
    _r->ticket     = 0;
    _r->answered   = false;
    _r->line_count = 0;
}

// Starts ML Kit reading _ink (see the header), with a new ticket. Ticket 0 is skipped when the count wraps, because
// it means none.
b8 fude_recognize_start(fude_recognition* _r, const fude_ink* _ink) {
    fude_recognize_forget(_r);
    fude_recognize_pump();
    if(!fude_recognize_available() || fude_recognize_inflight != 0 || !fude_mlkit_recognize(_ink, NULL)) {
        return false;
    }
    _r->ticket = fude_recognize_inflight = fude_recognize_next++;
    if(fude_recognize_next == 0) {
        fude_recognize_next = 1;
    }
    return true;
}

// Takes the answer for _r's ticket, when it has come, into its lines (see the header).
b8 fude_recognize_poll(fude_recognition* _r) {
    fude_recognize_pump();
    if(_r->ticket == 0 || fude_recognize_done != _r->ticket) {
        return false;
    }
    // One reading a line. Empty lines, and those too long for FUDE_RECOGNIZE_LINE, are left out.
    _r->line_count = 0;
    const c8* _p = fude_recognize_answer;
    while(*_p != 0 && _r->line_count < FUDE_RECOGNIZE_LINES) {
        const c8*   _end = rde_strchr(_p, '\n');
        const usize _n   = _end != NULL ? (usize)(_end - _p) : rde_strlen(_p);
        if(_n > 0 && _n < FUDE_RECOGNIZE_LINE) {
            rde_memcpy(_r->lines[_r->line_count], _p, _n);
            _r->lines[_r->line_count][_n] = 0;
            _r->line_count++;
        }
        _p += _n + (_end != NULL ? 1u : 0u);
    }
    _r->answered        = true;
    _r->ticket          = 0;
    fude_recognize_done = 0;
    return true;
}

// Turns a reading's characters into records (see the header). Spaces, both ASCII and the ideographic one (U+3000),
// are skipped.
u32 fude_recognize_records(const fude_kanji_db* _db, const c8* _line, u32* _out, u32 _max) {
    u32       _n = 0;
    const c8* _p = _line;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0 && _n < _max; _cp = fude_utf8_next(&_p)) {
        u32 _record;
        if(_cp != ' ' && _cp != 0x3000 && fude_kanji_find_index(_db, _cp, &_record)) {
            _out[_n++] = _record;
        }
    }
    return _n;
}

// True when two records are the same letter (see the header).
b8 fude_recognize_same(const fude_kanji_db* _db, u32 _a, u32 _b) {
    if(_a == _b) {
        return true;
    }
    fude_kanji_info _ia, _ib;
    return _db != NULL && fude_kanji_at(_db, _a, &_ia) && fude_kanji_at(_db, _b, &_ib) &&
           fude_lang_letter(_ia.codepoint) == fude_lang_letter(_ib.codepoint);
}

// Adds _record to _out with _cost, unless it is there already, does not pass the filter, or _out is full.
static void fude_recognize_add(const fude_catalog* _catalog, FUDE_FILTER_ _filter, u32 _record, f32 _cost,
                               fude_match_result* _out, u32* _n, u32 _max) {
    if(*_n >= _max || (_catalog != NULL && !fude_catalog_passes(_catalog, _record, _filter))) {
        return;
    }
    for(u32 _i = 0; _i < *_n; _i++) {
        if(_out[_i].record == _record) {
            return;
        }
    }
    _out[(*_n)++] = (fude_match_result){ .record = _record, .cost = _cost };
}

// Gathers one character's candidates (see the header): ML Kit's first, at cost 0, then the matcher's.
u32 fude_recognize_candidates(const fude_kanji_db* _db, const fude_recognition* _r, u32 _index, u32 _length,
                              const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                              const fude_match_result* _matched, u32 _matched_count, fude_match_result* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _l = 0; _r != NULL && _r->answered && _l < _r->line_count; _l++) {
        u32       _records[FUDE_RECOGNIZE_LINE];
        const u32 _count = fude_recognize_records(_db, _r->lines[_l], _records, FUDE_RECOGNIZE_LINE);
        if(_count == _length && _index < _count) {
            fude_recognize_add(_catalog, _filter, _records[_index], 0.0f, _out, &_n, _max);
        }
    }
    for(u32 _m = 0; _m < _matched_count; _m++) {
        fude_recognize_add(_catalog, _filter, _matched[_m].record, _matched[_m].cost, _out, &_n, _max);
    }
    return _n;
}
