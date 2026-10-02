#include "study/handwriting/recognize.h"

#include <string.h>

// ===========================================================================
// See recognize.h.
// ===========================================================================

static u32 fude_recognize_next = 1;       // the next ticket
static u32 fude_recognize_inflight;       // the ticket ML Kit is reading for (0: none)
static u32 fude_recognize_done;           // the ticket of the answer waiting (0: none)
static c8  fude_recognize_answer[4096];

// ML Kit's answer, when it has come, kept for its ticket.
static void fude_recognize_pump(void) {
    if(fude_recognize_inflight != 0 && fude_mlkit_poll(fude_recognize_answer, sizeof(fude_recognize_answer), NULL)) {
        fude_recognize_done     = fude_recognize_inflight;
        fude_recognize_inflight = 0;
    }
}

b8 fude_recognize_available(void) {
    return fude_mlkit_enabled() && fude_mlkit_state() == FUDE_MLKIT_READY;
}

void fude_recognize_forget(fude_recognition* _r) {
    _r->ticket     = 0;
    _r->answered   = false;
    _r->line_count = 0;
}

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

b8 fude_recognize_poll(fude_recognition* _r) {
    fude_recognize_pump();
    if(_r->ticket == 0 || fude_recognize_done != _r->ticket) {
        return false;
    }
    // One reading a line.
    _r->line_count = 0;
    const c8* _p = fude_recognize_answer;
    while(*_p != 0 && _r->line_count < FUDE_RECOGNIZE_LINES) {
        const c8*   _end = strchr(_p, '\n');
        const usize _n   = _end != NULL ? (usize)(_end - _p) : strlen(_p);
        if(_n > 0 && _n < FUDE_RECOGNIZE_LINE) {
            memcpy(_r->lines[_r->line_count], _p, _n);
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

// Adds _record to _out unless it is there already, or does not pass.
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
