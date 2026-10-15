// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/marks.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"


// ===========================================================================
// See marks.h.
//
// The marks are few (hundreds, a few thousand at most), so they are an array sorted by code point and searched by
// bisection. The changes, which grow to as many again over time, are a second array kept in the order they were made.
// Both are written whole on every change.
// ===========================================================================

// The file's format version, the tags of its kind and of its marks chunk, the size in bytes of a mark or change record
// as written (a reader accepts longer ones), and the tag of the changes chunk.
#define FUDE_MARKS_VERSION     1u
#define FUDE_MARKS_KIND        FUDE_TAG('M', 'A', 'R', 'K')
#define FUDE_MARKS_CHUNK       FUDE_TAG('M', 'R', 'K', 'S')
#define FUDE_MARKS_RECORD_SIZE 16u
#define FUDE_MARKS_HISTORY     FUDE_TAG('M', 'H', 'I', 'S')

// One marked character: its mark (a FUDE_MARK_, never NONE) and when it was set, in Unix seconds.
typedef struct {
    u32 codepoint;
    u8  mark;
    u64 since;
} fude_mark;

// The marked characters, sorted by code point. Both arrays are created on first use (fude_marks_ensure), so the marks
// work in memory without a file, as the tests use them.
static rde_arr TYPE(fude_mark) fude_marks_all;
static rde_arr TYPE(fude_marks_change) fude_marks_log;   // Every change, oldest first.
static b8                      fude_marks_ready    = false;
// The file the marks are saved to, or "" to keep them in memory only.
static c8                      fude_marks_path[RDE_MAX_PATH];
// Goes up on every change; fude_marks_revision returns it.
static u32                     fude_marks_changes  = 0;

// Creates the two arrays the first time they are needed.
RDE_INTERNAL void fude_marks_ensure(void) {
    if(!fude_marks_ready) {
        fude_marks_all   = rde_arr_new(sizeof(fude_mark), rde_memory_allocator_get_default_std());
        fude_marks_log   = rde_arr_new(sizeof(fude_marks_change), rde_memory_allocator_get_default_std());
        fude_marks_ready = true;
    }
}

// Finds _codepoint's mark by bisection. When it has one, sets *_found to true and returns its index; otherwise sets
// *_found to false and returns the index where it would be inserted to keep the array sorted.
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

// Writes the whole file again: the marks chunk, then the changes chunk. Returns false when the file could not be
// written, and true when it was or when the marks are kept in memory only.
RDE_INTERNAL b8 fude_marks_save(void) {
    if(fude_marks_path[0] == 0) {
        return true;   // The marks are kept in memory only.
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
        fude_put_u64(&_b, _m[_i].since);
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
        fude_put_u64(&_b, _c[_i].time);
    }
    fude_chunk_end(&_b, _history);
    return fude_bytes_write_and_free(&_b, fude_marks_path, NULL);
}

// The order for rde_sort of the changes: oldest first and, within the same second, by code point.
RDE_INTERNAL int fude_marks_by_time(const any _a, const any _b) {
    const fude_marks_change* _x = (const fude_marks_change*)_a;
    const fude_marks_change* _y = (const fude_marks_change*)_b;
    if(_x->time != _y->time) {
        return _x->time < _y->time ? -1 : 1;
    }
    return _x->codepoint < _y->codepoint ? -1 : _x->codepoint > _y->codepoint ? 1 : 0;
}

// Reads the marks file's bytes into the marks and the changes, starting from none; fude_save_load (save.h) calls it, a
// second time for the .bak when the main file fails. A record with an unknown mark, and a second mark on the same
// character, are dropped. When the file has no usable changes chunk (one from before the changes were kept, or a
// damaged one), the changes start with the marks there are, each at the time it was set. Returns false when the
// header is not that of a marks file of this version.
RDE_INTERNAL b8 fude_marks_read(const u8* _data, u32 _size, any _user) {
    RDE_UNUSED(_user);
    rde_arr_clear(&fude_marks_all);
    rde_arr_clear(&fude_marks_log);
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_MARKS_VERSION, FUDE_MARKS_KIND)) {
        return false;
    }
    u32         _tag;
    fude_reader _chunk;
    b8          _has_history = false;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag == FUDE_MARKS_HISTORY) {
            const u32 _count  = fude_get_u32(&_chunk);
            const u32 _record = fude_get_u32(&_chunk);
            if(!_chunk.ok || _record < 16u || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
                continue;   // The changes are damaged; they are rebuilt from the marks below, as for an old file.
            }
            for(u32 _i = 0; _i < _count; _i++) {
                fude_reader       _e = fude_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
                fude_marks_change _c = { 0 };
                _c.codepoint         = fude_get_u32(&_e);
                _c.mark              = fude_get_u8(&_e);
                fude_get_u8(&_e); fude_get_u8(&_e); fude_get_u8(&_e);
                _c.time              = fude_get_u64(&_e);
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
            // The marks themselves are damaged: the file is, so it is set aside and its backup read (save.h), rather
            // than read on without its marks (and the history after them rebuilt from none).
            return false;
        }
        for(u32 _i = 0; _i < _count; _i++) {
            fude_reader _e = fude_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
            fude_mark   _m = { 0 };
            _m.codepoint   = fude_get_u32(&_e);
            _m.mark        = fude_get_u8(&_e);
            fude_get_u8(&_e); fude_get_u8(&_e); fude_get_u8(&_e);
            _m.since       = fude_get_u64(&_e);
            b8 _found = false;
            const u32 _at = fude_marks_find(_m.codepoint, &_found);
            if(_e.ok && !_found && _m.mark > FUDE_MARK_NONE && _m.mark < FUDE_MARK_COUNT) {
                rde_arr_insert(&fude_marks_all, _at, &_m);
            }
        }
    }

    // A file from before the changes were kept: the changes start with the marks it has, oldest first.
    if(!_has_history) {
        const fude_mark* _m = (const fude_mark*)fude_marks_all.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&fude_marks_all); _i++) {
            const fude_marks_change _c = { .codepoint = _m[_i].codepoint, .mark = _m[_i].mark, .time = _m[_i].since };
            rde_arr_add(&fude_marks_log, &_c);
        }
        if(rde_arr_length(&fude_marks_log) > 1) {
            rde_sort(fude_marks_log.memory, rde_arr_length(&fude_marks_log), sizeof(fude_marks_change), fude_marks_by_time);
        }
    }
    return true;
}

// Replaces the marks and changes with those saved at _path (none for NULL). See the header.
void fude_marks_open(const c8* _path) {
    fude_marks_ensure();
    rde_arr_clear(&fude_marks_all);
    rde_arr_clear(&fude_marks_log);
    rde_snprintf(fude_marks_path, sizeof(fude_marks_path), "%s", _path != NULL ? _path : "");
    fude_marks_changes++;
    if(_path == NULL) {
        return;   // Keep the marks in memory only, as the other models allow.
    }
    fude_save_load(_path, fude_marks_read, NULL);   // When nothing has been saved yet, there are no marks.
}

// Frees the marks and changes and forgets the path, without saving.
void fude_marks_close(void) {
    if(fude_marks_ready) {
        rde_arr_free(&fude_marks_all);
        rde_arr_free(&fude_marks_log);
        fude_marks_ready = false;
    }
    fude_marks_path[0] = 0;
    fude_marks_changes++;
}

// Returns _codepoint's mark, or FUDE_MARK_NONE when it has none or the marks were never used.
FUDE_MARK_ fude_marks_get(u32 _codepoint) {
    if(!fude_marks_ready) {
        return FUDE_MARK_NONE;
    }
    b8        _found = false;
    const u32 _at    = fude_marks_find(_codepoint, &_found);
    return _found ? (FUDE_MARK_)((const fude_mark*)fude_marks_all.memory)[_at].mark : FUDE_MARK_NONE;
}

// Changes one character's mark in memory, as of _now (Unix seconds), and adds the change to the list, without saving.
// FUDE_MARK_NONE removes the mark. Returns true when anything changed, false when it already had that mark.
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

// Sets several characters' mark as of now. See the header.
b8 fude_marks_set_many(const u32* _codepoints, u32 _count, FUDE_MARK_ _mark) {
    return fude_marks_set_many_at(_codepoints, _count, _mark, (u64)rde_date_now_unix_seconds());
}

// Sets several characters' mark as of _now, and saves once when anything changed. Returns false only when the file
// could not be written.
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

// Sets one character's mark as of now. See the header.
b8 fude_marks_set(u32 _codepoint, FUDE_MARK_ _mark) {
    return fude_marks_set_many(&_codepoint, 1u, _mark);
}

// Returns how many characters carry _mark.
u32 fude_marks_count(FUDE_MARK_ _mark) {
    return fude_marks_list(_mark, NULL, UINT32_MAX);
}

// Writes into _out (when not NULL) the code points that carry _mark, in code point order, at most _max of them, and
// returns how many there were.
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

// Returns the change counter, which goes up on every change so the screens that show marks notice.
u32 fude_marks_revision(void) {
    return fude_marks_changes;
}

// Returns how many changes of mark are kept.
u32 fude_marks_history_count(void) {
    return fude_marks_ready ? (u32)rde_arr_length(&fude_marks_log) : 0u;
}

// Returns the changes of mark, oldest first, or NULL before the marks are used. See the header for how long it stays
// valid.
const fude_marks_change* fude_marks_history(void) {
    return fude_marks_ready ? (const fude_marks_change*)fude_marks_log.memory : NULL;
}
