#include "study/models/marks.h"
#include "drawing/base/kfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See marks.h. The marks are few (hundreds, a few thousand at most): an array
// sorted by code point, searched by bisection. The changes, as many again over
// time, are a list in the order made.
// ===========================================================================

#define FUDE_MARKS_VERSION     1u
#define FUDE_MARKS_KIND        FUDE_TAG('M', 'A', 'R', 'K')
#define FUDE_MARKS_CHUNK       FUDE_TAG('M', 'R', 'K', 'S')
#define FUDE_MARKS_RECORD_SIZE 16u
#define FUDE_MARKS_HISTORY     FUDE_TAG('M', 'H', 'I', 'S')

typedef struct {
    u32 codepoint;
    u8  mark;
    u64 since;
} fude_mark;

static rde_arr TYPE(fude_mark) fude_marks_all;
static rde_arr TYPE(fude_marks_change) fude_marks_log;   // every change, oldest first
static b8                      fude_marks_ready    = false;
static c8                      fude_marks_path[RDE_MAX_PATH];
static u32                     fude_marks_changes  = 0;

RDE_INTERNAL void fude_marks_ensure(void) {
    if(!fude_marks_ready) {
        fude_marks_all   = rde_arr_new(sizeof(fude_mark), rde_memory_allocator_get_default_std());
        fude_marks_log   = rde_arr_new(sizeof(fude_marks_change), rde_memory_allocator_get_default_std());
        fude_marks_ready = true;
    }
}

// Where _codepoint is, or would go.
RDE_INTERNAL u32 fude_marks_find(u32 _codepoint, b8* _found) {
    const fude_mark* _m  = (const fude_mark*)fude_marks_all.memory;
    u32              _lo = 0;
    u32              _hi = (u32)rde_arr_length(&fude_marks_all);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_m[_mid].codepoint < _codepoint) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    *_found = _lo < (u32)rde_arr_length(&fude_marks_all) && _m[_lo].codepoint == _codepoint;
    return _lo;
}

RDE_INTERNAL b8 fude_marks_save(void) {
    if(fude_marks_path[0] == 0) {
        return true;   // in memory only
    }
    const u32  _n = (u32)rde_arr_length(&fude_marks_all);
    const u32  _h = (u32)rde_arr_length(&fude_marks_log);
    fude_bytes _b = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 32u + (_n + _h) * FUDE_MARKS_RECORD_SIZE + 1u);
    fude_put_header(&_b, FUDE_MARKS_VERSION, FUDE_MARKS_KIND);
    const u32 _chunk = fude_chunk_begin(&_b, FUDE_MARKS_CHUNK);
    fude_put_u32(&_b, _n);
    fude_put_u32(&_b, FUDE_MARKS_RECORD_SIZE);
    const fude_mark* _m = (const fude_mark*)fude_marks_all.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        fude_put_u32(&_b, _m[_i].codepoint);
        fude_put_u8(&_b, _m[_i].mark);
        fude_put_u8(&_b, 0u);
        fude_put_u8(&_b, 0u);
        fude_put_u8(&_b, 0u);
        fude_put_u32(&_b, (u32)(_m[_i].since & 0xFFFFFFFFu));
        fude_put_u32(&_b, (u32)(_m[_i].since >> 32));
    }
    fude_chunk_end(&_b, _chunk);

    const u32 _history = fude_chunk_begin(&_b, FUDE_MARKS_HISTORY);
    fude_put_u32(&_b, _h);
    fude_put_u32(&_b, FUDE_MARKS_RECORD_SIZE);
    const fude_marks_change* _c = (const fude_marks_change*)fude_marks_log.memory;
    for(u32 _i = 0; _i < _h; _i++) {
        fude_put_u32(&_b, _c[_i].codepoint);
        fude_put_u8(&_b, _c[_i].mark);
        fude_put_u8(&_b, 0u);
        fude_put_u8(&_b, 0u);
        fude_put_u8(&_b, 0u);
        fude_put_u32(&_b, (u32)(_c[_i].time & 0xFFFFFFFFu));
        fude_put_u32(&_b, (u32)(_c[_i].time >> 32));
    }
    fude_chunk_end(&_b, _history);
    return fude_bytes_write_and_free(&_b, fude_marks_path, NULL);
}

// Oldest first; at the same second, by code point.
RDE_INTERNAL int fude_marks_by_time(const void* _a, const void* _b) {
    const fude_marks_change* _x = (const fude_marks_change*)_a;
    const fude_marks_change* _y = (const fude_marks_change*)_b;
    if(_x->time != _y->time) {
        return _x->time < _y->time ? -1 : 1;
    }
    return _x->codepoint < _y->codepoint ? -1 : _x->codepoint > _y->codepoint ? 1 : 0;
}

void fude_marks_open(const c8* _path) {
    fude_marks_ensure();
    rde_arr_clear(&fude_marks_all);
    rde_arr_clear(&fude_marks_log);
    snprintf(fude_marks_path, sizeof(fude_marks_path), "%s", _path);
    fude_marks_changes++;

    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return;   // nothing marked yet
        }
    }

    u32 _size = 0;
    u8* _data = fude_file_read(_from, &_size);
    fude_reader _r = fude_reader_make(_data, _size);
    if(_data == NULL || !fude_read_header(&_r, FUDE_MARKS_VERSION, FUDE_MARKS_KIND)) {
        fude_file_free(_data);
        fude_file_set_aside(_from);
        return;
    }

    u32         _tag;
    fude_reader _chunk;
    b8          _has_history = false;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag == FUDE_MARKS_HISTORY) {
            const u32 _count  = fude_get_u32(&_chunk);
            const u32 _record = fude_get_u32(&_chunk);
            if(!_chunk.ok || _record < 16u || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
                continue;   // damaged: seeded from the marks below, as an old file's
            }
            for(u32 _i = 0; _i < _count; _i++) {
                fude_reader       _e = fude_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
                fude_marks_change _c = { 0 };
                _c.codepoint         = fude_get_u32(&_e);
                _c.mark              = fude_get_u8(&_e);
                fude_get_u8(&_e); fude_get_u8(&_e); fude_get_u8(&_e);
                const u32 _lo        = fude_get_u32(&_e);
                const u32 _hi        = fude_get_u32(&_e);
                _c.time              = (u64)_lo | ((u64)_hi << 32);
                if(_e.ok && _c.mark < FUDE_MARK_COUNT) {
                    rde_arr_add(&fude_marks_log, &_c);
                }
            }
            _has_history = true;
            continue;
        }
        if(_tag != FUDE_MARKS_CHUNK) {
            continue;
        }
        const u32 _count  = fude_get_u32(&_chunk);
        const u32 _record = fude_get_u32(&_chunk);
        if(!_chunk.ok || _record < 16u || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
            break;
        }
        for(u32 _i = 0; _i < _count; _i++) {
            fude_reader _e = fude_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
            fude_mark   _m = { 0 };
            _m.codepoint   = fude_get_u32(&_e);
            _m.mark        = fude_get_u8(&_e);
            fude_get_u8(&_e); fude_get_u8(&_e); fude_get_u8(&_e);
            const u32 _lo  = fude_get_u32(&_e);
            const u32 _hi  = fude_get_u32(&_e);
            _m.since       = (u64)_lo | ((u64)_hi << 32);
            b8 _found = false;
            const u32 _at = fude_marks_find(_m.codepoint, &_found);
            if(_e.ok && !_found && _m.mark > FUDE_MARK_NONE && _m.mark < FUDE_MARK_COUNT) {
                rde_arr_insert(&fude_marks_all, _at, &_m);
            }
        }
    }
    fude_file_free(_data);

    // From before changes were kept: the history starts with the marks there are.
    if(!_has_history) {
        const fude_mark* _m = (const fude_mark*)fude_marks_all.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&fude_marks_all); _i++) {
            const fude_marks_change _c = { .codepoint = _m[_i].codepoint, .mark = _m[_i].mark, .time = _m[_i].since };
            rde_arr_add(&fude_marks_log, &_c);
        }
        if(rde_arr_length(&fude_marks_log) > 1) {
            qsort(fude_marks_log.memory, rde_arr_length(&fude_marks_log), sizeof(fude_marks_change), fude_marks_by_time);
        }
    }
}

void fude_marks_close(void) {
    if(fude_marks_ready) {
        rde_arr_free(&fude_marks_all);
        rde_arr_free(&fude_marks_log);
        fude_marks_ready = false;
    }
    fude_marks_path[0] = 0;
    fude_marks_changes++;
}

FUDE_MARK_ fude_marks_get(u32 _codepoint) {
    if(!fude_marks_ready) {
        return FUDE_MARK_NONE;
    }
    b8        _found = false;
    const u32 _at    = fude_marks_find(_codepoint, &_found);
    return _found ? (FUDE_MARK_)((const fude_mark*)fude_marks_all.memory)[_at].mark : FUDE_MARK_NONE;
}

// One mark changed in memory, and the change kept; true when anything changed.
RDE_INTERNAL b8 fude_marks_put(u32 _codepoint, FUDE_MARK_ _mark, u64 _now) {
    fude_marks_ensure();
    b8        _found = false;
    const u32 _at    = fude_marks_find(_codepoint, &_found);
    if(_found) {
        fude_mark* _m = &((fude_mark*)fude_marks_all.memory)[_at];
        if(_m->mark == (u8)_mark) {
            return false;
        }
        if(_mark == FUDE_MARK_NONE) {
            rde_arr_remove(&fude_marks_all, _at);
        } else {
            _m->mark  = (u8)_mark;
            _m->since = _now;
        }
    } else {
        if(_mark == FUDE_MARK_NONE) {
            return false;
        }
        const fude_mark _m = { .codepoint = _codepoint, .mark = (u8)_mark, .since = _now };
        rde_arr_insert(&fude_marks_all, _at, &_m);
    }
    const fude_marks_change _c = { .codepoint = _codepoint, .mark = (u8)_mark, .time = _now };
    rde_arr_add(&fude_marks_log, &_c);
    return true;
}

b8 fude_marks_set_many(const u32* _codepoints, u32 _count, FUDE_MARK_ _mark) {
    return fude_marks_set_many_at(_codepoints, _count, _mark, (u64)time(NULL));
}

b8 fude_marks_set_many_at(const u32* _codepoints, u32 _count, FUDE_MARK_ _mark, u64 _now) {
    b8 _changed = false;
    for(u32 _i = 0; _i < _count; _i++) {
        _changed = fude_marks_put(_codepoints[_i], _mark, _now) || _changed;
    }
    if(!_changed) {
        return true;
    }
    fude_marks_changes++;
    return fude_marks_save();
}

b8 fude_marks_set(u32 _codepoint, FUDE_MARK_ _mark) {
    return fude_marks_set_many(&_codepoint, 1u, _mark);
}

u32 fude_marks_count(FUDE_MARK_ _mark) {
    return fude_marks_list(_mark, NULL, UINT32_MAX);
}

u32 fude_marks_list(FUDE_MARK_ _mark, u32* _out, u32 _max) {
    if(!fude_marks_ready) {
        return 0;
    }
    const fude_mark* _m = (const fude_mark*)fude_marks_all.memory;
    u32              _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fude_marks_all) && _n < _max; _i++) {
        if(_m[_i].mark == (u8)_mark) {
            if(_out != NULL) {
                _out[_n] = _m[_i].codepoint;
            }
            _n++;
        }
    }
    return _n;
}

u32 fude_marks_revision(void) {
    return fude_marks_changes;
}

u32 fude_marks_history_count(void) {
    return fude_marks_ready ? (u32)rde_arr_length(&fude_marks_log) : 0u;
}

const fude_marks_change* fude_marks_history(void) {
    return fude_marks_ready ? (const fude_marks_change*)fude_marks_log.memory : NULL;
}
