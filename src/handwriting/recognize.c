#include "handwriting/recognize.h"

#include <string.h>

// ===========================================================================
// See recognize.h.
// ===========================================================================

static u32 kana_recognize_next = 1;       // the next ticket
static u32 kana_recognize_inflight;       // the ticket ML Kit is reading for (0: none)
static u32 kana_recognize_done;           // the ticket of the answer waiting (0: none)
static c8  kana_recognize_answer[4096];

// ML Kit's answer, when it has come, kept for its ticket.
static void kana_recognize_pump(void) {
    if(kana_recognize_inflight != 0 && kana_mlkit_poll(kana_recognize_answer, sizeof(kana_recognize_answer), NULL)) {
        kana_recognize_done     = kana_recognize_inflight;
        kana_recognize_inflight = 0;
    }
}

b8 kana_recognize_available(void) {
    return kana_mlkit_enabled() && kana_mlkit_state() == KANA_MLKIT_READY;
}

void kana_recognize_forget(kana_recognition* _r) {
    _r->ticket     = 0;
    _r->answered   = false;
    _r->line_count = 0;
}

b8 kana_recognize_start(kana_recognition* _r, const kana_ink* _ink) {
    kana_recognize_forget(_r);
    kana_recognize_pump();
    if(!kana_recognize_available() || kana_recognize_inflight != 0 || !kana_mlkit_recognize(_ink, NULL)) {
        return false;
    }
    _r->ticket = kana_recognize_inflight = kana_recognize_next++;
    if(kana_recognize_next == 0) {
        kana_recognize_next = 1;
    }
    return true;
}

b8 kana_recognize_poll(kana_recognition* _r) {
    kana_recognize_pump();
    if(_r->ticket == 0 || kana_recognize_done != _r->ticket) {
        return false;
    }
    // One reading a line.
    _r->line_count = 0;
    const c8* _p = kana_recognize_answer;
    while(*_p != 0 && _r->line_count < KANA_RECOGNIZE_LINES) {
        const c8*   _end = strchr(_p, '\n');
        const usize _n   = _end != NULL ? (usize)(_end - _p) : strlen(_p);
        if(_n > 0 && _n < KANA_RECOGNIZE_LINE) {
            memcpy(_r->lines[_r->line_count], _p, _n);
            _r->lines[_r->line_count][_n] = 0;
            _r->line_count++;
        }
        _p += _n + (_end != NULL ? 1u : 0u);
    }
    _r->answered        = true;
    _r->ticket          = 0;
    kana_recognize_done = 0;
    return true;
}

u32 kana_recognize_records(const kana_kanji_db* _db, const c8* _line, u32* _out, u32 _max) {
    u32       _n = 0;
    const c8* _p = _line;
    for(u32 _cp = kana_kanji_utf8_next(&_p); _cp != 0 && _n < _max; _cp = kana_kanji_utf8_next(&_p)) {
        u32 _record;
        if(_cp != ' ' && _cp != 0x3000 && kana_kanji_find_index(_db, _cp, &_record)) {
            _out[_n++] = _record;
        }
    }
    return _n;
}

// Adds _record to _out unless it is there already, or does not pass.
static void kana_recognize_add(const kana_catalog* _catalog, KANA_FILTER_ _filter, u32 _record, f32 _cost,
                               kana_match_result* _out, u32* _n, u32 _max) {
    if(*_n >= _max || (_catalog != NULL && !kana_catalog_passes(_catalog, _record, _filter))) {
        return;
    }
    for(u32 _i = 0; _i < *_n; _i++) {
        if(_out[_i].record == _record) {
            return;
        }
    }
    _out[(*_n)++] = (kana_match_result){ .record = _record, .cost = _cost };
}

u32 kana_recognize_candidates(const kana_kanji_db* _db, const kana_recognition* _r, u32 _index, u32 _length,
                              const kana_catalog* _catalog, KANA_FILTER_ _filter,
                              const kana_match_result* _matched, u32 _matched_count, kana_match_result* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _l = 0; _r != NULL && _r->answered && _l < _r->line_count; _l++) {
        u32       _records[KANA_RECOGNIZE_LINE];
        const u32 _count = kana_recognize_records(_db, _r->lines[_l], _records, KANA_RECOGNIZE_LINE);
        if(_count == _length && _index < _count) {
            kana_recognize_add(_catalog, _filter, _records[_index], 0.0f, _out, &_n, _max);
        }
    }
    for(u32 _m = 0; _m < _matched_count; _m++) {
        kana_recognize_add(_catalog, _filter, _matched[_m].record, _matched[_m].cost, _out, &_n, _max);
    }
    return _n;
}
