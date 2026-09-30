#include "userwords.h"
#include "kfile.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See userwords.h. A few hundred words at most: one array, searched in full.
// ===========================================================================

#define KANA_USERWORDS_VERSION 1u
#define KANA_USERWORDS_KIND    KANA_TAG('U', 'W', 'R', 'D')
#define KANA_USERWORDS_CHUNK   KANA_TAG('W', 'R', 'D', 'S')

typedef struct {
    u32 kanji;
    u64 added;
    c8  written[KANA_USERWORD_WRITTEN];
    c8  reading[KANA_USERWORD_READING];
    c8  meaning[KANA_USERWORD_MEANING];
} kana_userword;

static rde_arr TYPE(kana_userword) kana_userwords_all;
static b8                          kana_userwords_ready = false;
static c8                          kana_userwords_path[RDE_MAX_PATH];
static u32                         kana_userwords_changes = 0;

RDE_INTERNAL void kana_userwords_ensure(void) {
    if(!kana_userwords_ready) {
        kana_userwords_all   = rde_arr_new(sizeof(kana_userword), rde_memory_allocator_get_default_std());
        kana_userwords_ready = true;
    }
}

// _s into _out (_size bytes), cut at a whole character.
RDE_INTERNAL void kana_userwords_copy(c8* _out, usize _size, const c8* _s, usize _len) {
    usize _n = _len < _size - 1u ? _len : _size - 1u;
    while(_n > 0 && _n < _len && ((u8)_s[_n] & 0xC0u) == 0x80u) {
        _n--;   // not in the middle of a UTF-8 sequence
    }
    memcpy(_out, _s, _n);
    _out[_n] = 0;
}

RDE_INTERNAL void kana_userwords_put_string(kana_bytes* _b, const c8* _s) {
    const u32 _n = (u32)strlen(_s);
    kana_put_u16(_b, (u16)_n);
    kana_put_data(_b, _s, _n);
}

RDE_INTERNAL b8 kana_userwords_save(void) {
    if(kana_userwords_path[0] == 0) {
        return true;
    }
    const u32  _n = (u32)rde_arr_length(&kana_userwords_all);
    kana_bytes _b = kana_bytes_new(KANA_FILE_HEADER_SIZE + 16u + _n * 128u + 1u);
    kana_put_header(&_b, KANA_USERWORDS_VERSION, KANA_USERWORDS_KIND);
    const u32 _chunk = kana_chunk_begin(&_b, KANA_USERWORDS_CHUNK);
    kana_put_u32(&_b, _n);
    const kana_userword* _w = (const kana_userword*)kana_userwords_all.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        kana_put_u32(&_b, _w[_i].kanji);
        kana_put_u32(&_b, (u32)(_w[_i].added & 0xFFFFFFFFu));
        kana_put_u32(&_b, (u32)(_w[_i].added >> 32));
        kana_userwords_put_string(&_b, _w[_i].written);
        kana_userwords_put_string(&_b, _w[_i].reading);
        kana_userwords_put_string(&_b, _w[_i].meaning);
    }
    kana_chunk_end(&_b, _chunk);
    return kana_bytes_write_and_free(&_b, kana_userwords_path, NULL);
}

// A string as saved, into _out; false past the end.
RDE_INTERNAL b8 kana_userwords_get_string(kana_reader* _r, c8* _out, usize _size) {
    const u32 _n = kana_get_u16(_r);
    if(!_r->ok || _n > _r->size - _r->pos) {
        _r->ok = false;
        return false;
    }
    kana_userwords_copy(_out, _size, (const c8*)&_r->data[_r->pos], _n);
    _r->pos += _n;
    return true;
}

void kana_userwords_open(const c8* _path) {
    kana_userwords_ensure();
    rde_arr_clear(&kana_userwords_all);
    snprintf(kana_userwords_path, sizeof(kana_userwords_path), "%s", _path);
    kana_userwords_changes++;

    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return;   // none added yet
        }
    }
    u32 _size = 0;
    u8* _data = kana_file_read(_from, &_size);
    kana_reader _r = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_USERWORDS_VERSION, KANA_USERWORDS_KIND)) {
        kana_file_free(_data);
        kana_file_set_aside(_from);
        return;
    }
    u32         _tag;
    kana_reader _chunk;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != KANA_USERWORDS_CHUNK) {
            continue;
        }
        const u32 _count = kana_get_u32(&_chunk);
        for(u32 _i = 0; _i < _count && _chunk.ok; _i++) {
            kana_userword _w = { 0 };
            _w.kanji         = kana_get_u32(&_chunk);
            const u32 _lo    = kana_get_u32(&_chunk);
            const u32 _hi    = kana_get_u32(&_chunk);
            _w.added         = (u64)_lo | ((u64)_hi << 32);
            if(kana_userwords_get_string(&_chunk, _w.written, sizeof(_w.written)) &&
               kana_userwords_get_string(&_chunk, _w.reading, sizeof(_w.reading)) &&
               kana_userwords_get_string(&_chunk, _w.meaning, sizeof(_w.meaning)) && _w.written[0] != 0) {
                rde_arr_add(&kana_userwords_all, &_w);
            }
        }
    }
    kana_file_free(_data);
}

void kana_userwords_close(void) {
    if(kana_userwords_ready) {
        rde_arr_free(&kana_userwords_all);
        kana_userwords_ready = false;
    }
    kana_userwords_path[0] = 0;
    kana_userwords_changes++;
}

// The i-th word of _kanji, as an index into all of them (-1: none).
RDE_INTERNAL i32 kana_userwords_find(u32 _kanji, u32 _nth) {
    if(!kana_userwords_ready) {
        return -1;
    }
    const kana_userword* _w = (const kana_userword*)kana_userwords_all.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&kana_userwords_all); _i++) {
        if(_w[_i].kanji == _kanji && _nth-- == 0u) {
            return (i32)_i;
        }
    }
    return -1;
}

u32 kana_userwords_count(u32 _kanji) {
    u32 _n = 0;
    while(kana_userwords_find(_kanji, _n) >= 0) {
        _n++;
    }
    return _n;
}

b8 kana_userwords_at(u32 _kanji, u32 _index, kana_kanji_word* _out) {
    const i32 _at = kana_userwords_find(_kanji, _index);
    if(_at < 0) {
        return false;
    }
    const kana_userword* _w = &((const kana_userword*)kana_userwords_all.memory)[_at];
    _out->written = _w->written;
    _out->reading = _w->reading;
    _out->meaning = _w->meaning;
    return true;
}

// Where _written / _reading is among _kanji's (-1: not there).
RDE_INTERNAL i32 kana_userwords_where(u32 _kanji, const c8* _written, const c8* _reading) {
    if(!kana_userwords_ready) {
        return -1;
    }
    const kana_userword* _w = (const kana_userword*)kana_userwords_all.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&kana_userwords_all); _i++) {
        if(_w[_i].kanji == _kanji && strcmp(_w[_i].written, _written) == 0 && strcmp(_w[_i].reading, _reading) == 0) {
            return (i32)_i;
        }
    }
    return -1;
}

b8 kana_userwords_has(u32 _kanji, const c8* _written, const c8* _reading) {
    return kana_userwords_where(_kanji, _written, _reading) >= 0;
}

b8 kana_userwords_add(u32 _kanji, const c8* _written, const c8* _reading, const c8* _meaning) {
    kana_userwords_ensure();
    if(_written == NULL || _written[0] == 0) {
        return false;
    }
    kana_userword _w = { .kanji = _kanji, .added = (u64)time(NULL) };
    kana_userwords_copy(_w.written, sizeof(_w.written), _written, strlen(_written));
    kana_userwords_copy(_w.reading, sizeof(_w.reading), _reading != NULL ? _reading : "", _reading != NULL ? strlen(_reading) : 0u);
    kana_userwords_copy(_w.meaning, sizeof(_w.meaning), _meaning != NULL ? _meaning : "", _meaning != NULL ? strlen(_meaning) : 0u);
    if(kana_userwords_where(_kanji, _w.written, _w.reading) >= 0) {
        return true;   // there already
    }
    rde_arr_add(&kana_userwords_all, &_w);
    kana_userwords_changes++;
    return kana_userwords_save();
}

b8 kana_userwords_remove(u32 _kanji, const c8* _written, const c8* _reading) {
    const i32 _at = kana_userwords_where(_kanji, _written, _reading);
    if(_at < 0) {
        return true;
    }
    rde_arr_remove(&kana_userwords_all, (usize)_at);
    kana_userwords_changes++;
    return kana_userwords_save();
}

u32 kana_userwords_total(void) {
    return kana_userwords_ready ? (u32)rde_arr_length(&kana_userwords_all) : 0u;
}

u32 kana_userwords_revision(void) {
    return kana_userwords_changes;
}
