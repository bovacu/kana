#include "marks.h"
#include "kfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See marks.h. The marks are few (hundreds, a few thousand at most): an array
// sorted by code point, searched by bisection.
// ===========================================================================

#define KANA_MARKS_VERSION     1u
#define KANA_MARKS_KIND        KANA_TAG('M', 'A', 'R', 'K')
#define KANA_MARKS_CHUNK       KANA_TAG('M', 'R', 'K', 'S')
#define KANA_MARKS_RECORD_SIZE 16u

typedef struct {
    u32 codepoint;
    u8  mark;
    u64 since;
} kana_mark;

static rde_arr TYPE(kana_mark) kana_marks_all;
static b8                      kana_marks_ready    = false;
static c8                      kana_marks_path[RDE_MAX_PATH];
static u32                     kana_marks_changes  = 0;

RDE_INTERNAL void kana_marks_ensure(void) {
    if(!kana_marks_ready) {
        kana_marks_all   = rde_arr_new(sizeof(kana_mark), rde_memory_allocator_get_default_std());
        kana_marks_ready = true;
    }
}

// Where _codepoint is, or would go.
RDE_INTERNAL u32 kana_marks_find(u32 _codepoint, b8* _found) {
    const kana_mark* _m  = (const kana_mark*)kana_marks_all.memory;
    u32              _lo = 0;
    u32              _hi = (u32)rde_arr_length(&kana_marks_all);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_m[_mid].codepoint < _codepoint) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    *_found = _lo < (u32)rde_arr_length(&kana_marks_all) && _m[_lo].codepoint == _codepoint;
    return _lo;
}

RDE_INTERNAL b8 kana_marks_save(void) {
    if(kana_marks_path[0] == 0) {
        return true;   // in memory only
    }
    const u32  _n = (u32)rde_arr_length(&kana_marks_all);
    kana_bytes _b = kana_bytes_new(KANA_FILE_HEADER_SIZE + 16u + _n * KANA_MARKS_RECORD_SIZE + 1u);
    kana_put_header(&_b, KANA_MARKS_VERSION, KANA_MARKS_KIND);
    const u32 _chunk = kana_chunk_begin(&_b, KANA_MARKS_CHUNK);
    kana_put_u32(&_b, _n);
    kana_put_u32(&_b, KANA_MARKS_RECORD_SIZE);
    const kana_mark* _m = (const kana_mark*)kana_marks_all.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        kana_put_u32(&_b, _m[_i].codepoint);
        kana_put_u8(&_b, _m[_i].mark);
        kana_put_u8(&_b, 0u);
        kana_put_u8(&_b, 0u);
        kana_put_u8(&_b, 0u);
        kana_put_u32(&_b, (u32)(_m[_i].since & 0xFFFFFFFFu));
        kana_put_u32(&_b, (u32)(_m[_i].since >> 32));
    }
    kana_chunk_end(&_b, _chunk);
    return kana_bytes_write_and_free(&_b, kana_marks_path, NULL);
}

void kana_marks_open(const c8* _path) {
    kana_marks_ensure();
    rde_arr_clear(&kana_marks_all);
    snprintf(kana_marks_path, sizeof(kana_marks_path), "%s", _path);
    kana_marks_changes++;

    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return;   // nothing marked yet
        }
    }

    u32 _size = 0;
    u8* _data = kana_file_read(_from, &_size);
    kana_reader _r = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_MARKS_VERSION, KANA_MARKS_KIND)) {
        kana_file_free(_data);
        kana_file_set_aside(_from);
        return;
    }

    u32         _tag;
    kana_reader _chunk;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != KANA_MARKS_CHUNK) {
            continue;
        }
        const u32 _count  = kana_get_u32(&_chunk);
        const u32 _record = kana_get_u32(&_chunk);
        if(!_chunk.ok || _record < 16u || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
            break;
        }
        for(u32 _i = 0; _i < _count; _i++) {
            kana_reader _e = kana_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
            kana_mark   _m = { 0 };
            _m.codepoint   = kana_get_u32(&_e);
            _m.mark        = kana_get_u8(&_e);
            kana_get_u8(&_e); kana_get_u8(&_e); kana_get_u8(&_e);
            const u32 _lo  = kana_get_u32(&_e);
            const u32 _hi  = kana_get_u32(&_e);
            _m.since       = (u64)_lo | ((u64)_hi << 32);
            b8 _found = false;
            const u32 _at = kana_marks_find(_m.codepoint, &_found);
            if(_e.ok && !_found && _m.mark > KANA_MARK_NONE && _m.mark < KANA_MARK_COUNT) {
                rde_arr_insert(&kana_marks_all, _at, &_m);
            }
        }
    }
    kana_file_free(_data);
}

void kana_marks_close(void) {
    if(kana_marks_ready) {
        rde_arr_free(&kana_marks_all);
        kana_marks_ready = false;
    }
    kana_marks_path[0] = 0;
    kana_marks_changes++;
}

KANA_MARK_ kana_marks_get(u32 _codepoint) {
    if(!kana_marks_ready) {
        return KANA_MARK_NONE;
    }
    b8        _found = false;
    const u32 _at    = kana_marks_find(_codepoint, &_found);
    return _found ? (KANA_MARK_)((const kana_mark*)kana_marks_all.memory)[_at].mark : KANA_MARK_NONE;
}

// One mark changed in memory; true when anything changed.
RDE_INTERNAL b8 kana_marks_put(u32 _codepoint, KANA_MARK_ _mark, u64 _now) {
    kana_marks_ensure();
    b8        _found = false;
    const u32 _at    = kana_marks_find(_codepoint, &_found);
    if(_found) {
        kana_mark* _m = &((kana_mark*)kana_marks_all.memory)[_at];
        if(_mark == KANA_MARK_NONE) {
            rde_arr_remove(&kana_marks_all, _at);
            return true;
        }
        if(_m->mark == (u8)_mark) {
            return false;
        }
        _m->mark  = (u8)_mark;
        _m->since = _now;
        return true;
    }
    if(_mark == KANA_MARK_NONE) {
        return false;
    }
    const kana_mark _m = { .codepoint = _codepoint, .mark = (u8)_mark, .since = _now };
    rde_arr_insert(&kana_marks_all, _at, &_m);
    return true;
}

b8 kana_marks_set_many(const u32* _codepoints, u32 _count, KANA_MARK_ _mark) {
    const u64 _now     = (u64)time(NULL);
    b8        _changed = false;
    for(u32 _i = 0; _i < _count; _i++) {
        _changed = kana_marks_put(_codepoints[_i], _mark, _now) || _changed;
    }
    if(!_changed) {
        return true;
    }
    kana_marks_changes++;
    return kana_marks_save();
}

b8 kana_marks_set(u32 _codepoint, KANA_MARK_ _mark) {
    return kana_marks_set_many(&_codepoint, 1u, _mark);
}

u32 kana_marks_count(KANA_MARK_ _mark) {
    return kana_marks_list(_mark, NULL, UINT32_MAX);
}

u32 kana_marks_list(KANA_MARK_ _mark, u32* _out, u32 _max) {
    if(!kana_marks_ready) {
        return 0;
    }
    const kana_mark* _m = (const kana_mark*)kana_marks_all.memory;
    u32              _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&kana_marks_all) && _n < _max; _i++) {
        if(_m[_i].mark == (u8)_mark) {
            if(_out != NULL) {
                _out[_n] = _m[_i].codepoint;
            }
            _n++;
        }
    }
    return _n;
}

u32 kana_marks_revision(void) {
    return kana_marks_changes;
}
