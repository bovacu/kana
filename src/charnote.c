#include "charnote.h"
#include "kfile.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See charnote.h. A few hundred notes at most: an array sorted by code point.
// ===========================================================================

#define KANA_CHARNOTES_VERSION 1u
#define KANA_CHARNOTES_KIND    KANA_TAG('C', 'N', 'O', 'T')
#define KANA_CHARNOTES_CHUNK   KANA_TAG('C', 'N', 'T', 'S')

typedef struct {
    u32 codepoint;
    c8  text[KANA_CHARNOTE_TEXT];
} kana_charnote;

static rde_arr TYPE(kana_charnote) kana_charnotes_all;
static b8                          kana_charnotes_ready   = false;
static c8                          kana_charnotes_path[RDE_MAX_PATH];
static u32                         kana_charnotes_changes = 0;

RDE_INTERNAL void kana_charnotes_ensure(void) {
    if(!kana_charnotes_ready) {
        kana_charnotes_all   = rde_arr_new(sizeof(kana_charnote), rde_memory_allocator_get_default_std());
        kana_charnotes_ready = true;
    }
}

// _s (_len bytes) into _out, cut at a whole character.
RDE_INTERNAL void kana_charnotes_copy(c8* _out, usize _size, const c8* _s, usize _len) {
    usize _n = _len < _size - 1u ? _len : _size - 1u;
    while(_n > 0 && _n < _len && ((u8)_s[_n] & 0xC0u) == 0x80u) {
        _n--;
    }
    if(_n > 0) {
        memcpy(_out, _s, _n);
    }
    _out[_n] = 0;
}

RDE_INTERNAL u32 kana_charnotes_find(u32 _codepoint, b8* _found) {
    const kana_charnote* _n  = (const kana_charnote*)kana_charnotes_all.memory;
    u32                  _lo = 0;
    u32                  _hi = (u32)rde_arr_length(&kana_charnotes_all);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_n[_mid].codepoint < _codepoint) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    *_found = _lo < (u32)rde_arr_length(&kana_charnotes_all) && _n[_lo].codepoint == _codepoint;
    return _lo;
}

RDE_INTERNAL void kana_charnotes_save(void) {
    kana_charnotes_changes++;
    if(kana_charnotes_path[0] == 0) {
        return;
    }
    const u32  _count = (u32)rde_arr_length(&kana_charnotes_all);
    kana_bytes _b     = kana_bytes_new(KANA_FILE_HEADER_SIZE + 16u + _count * 64u);
    kana_put_header(&_b, KANA_CHARNOTES_VERSION, KANA_CHARNOTES_KIND);
    const u32 _chunk = kana_chunk_begin(&_b, KANA_CHARNOTES_CHUNK);
    kana_put_u32(&_b, _count);
    const kana_charnote* _n = (const kana_charnote*)kana_charnotes_all.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _len = (u32)strlen(_n[_i].text);
        kana_put_u32(&_b, _n[_i].codepoint);
        kana_put_u16(&_b, (u16)_len);
        kana_put_data(&_b, _n[_i].text, _len);
    }
    kana_chunk_end(&_b, _chunk);
    if(!kana_bytes_write_and_free(&_b, kana_charnotes_path, NULL)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not save the character notes (%s)", kana_charnotes_path);
    }
}

void kana_charnotes_open(const c8* _path) {
    kana_charnotes_ensure();
    rde_arr_clear(&kana_charnotes_all);
    snprintf(kana_charnotes_path, sizeof(kana_charnotes_path), "%s", _path != NULL ? _path : "");
    kana_charnotes_changes++;
    if(_path == NULL) {
        return;
    }
    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return;   // none written yet
        }
    }
    u32         _size = 0;
    u8*         _data = kana_file_read(_from, &_size);
    kana_reader _r    = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_CHARNOTES_VERSION, KANA_CHARNOTES_KIND)) {
        kana_file_free(_data);
        kana_file_set_aside(_from);
        return;
    }
    u32         _tag;
    kana_reader _chunk;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != KANA_CHARNOTES_CHUNK) {
            continue;
        }
        const u32 _count = kana_get_u32(&_chunk);
        for(u32 _i = 0; _i < _count && _chunk.ok; _i++) {
            kana_charnote _n = { 0 };
            _n.codepoint     = kana_get_u32(&_chunk);
            const u32 _len   = kana_get_u16(&_chunk);
            if(!_chunk.ok || _len > _chunk.size - _chunk.pos) {
                break;
            }
            kana_charnotes_copy(_n.text, sizeof(_n.text), (const c8*)&_chunk.data[_chunk.pos], _len);
            _chunk.pos += _len;
            b8        _found;
            const u32 _at = kana_charnotes_find(_n.codepoint, &_found);
            if(!_found && _n.text[0] != 0) {
                rde_arr_insert(&kana_charnotes_all, _at, &_n);
            }
        }
    }
    kana_file_free(_data);
}

void kana_charnotes_close(void) {
    if(kana_charnotes_ready) {
        rde_arr_free(&kana_charnotes_all);
        kana_charnotes_ready = false;
    }
    kana_charnotes_path[0] = 0;
    kana_charnotes_changes++;
}

const c8* kana_charnote_get(u32 _codepoint) {
    if(!kana_charnotes_ready) {
        return "";
    }
    b8        _found;
    const u32 _at = kana_charnotes_find(_codepoint, &_found);
    return _found ? ((const kana_charnote*)kana_charnotes_all.memory)[_at].text : "";
}

void kana_charnote_set(u32 _codepoint, const c8* _text) {
    kana_charnotes_ensure();
    // Without the spaces and line breaks around it.
    const c8* _s = _text != NULL ? _text : "";
    while(*_s == ' ' || *_s == '\n') {
        _s++;
    }
    usize _len = strlen(_s);
    while(_len > 0 && (_s[_len - 1u] == ' ' || _s[_len - 1u] == '\n')) {
        _len--;
    }
    b8        _found;
    const u32 _at = kana_charnotes_find(_codepoint, &_found);
    if(_len == 0) {
        if(_found) {
            rde_arr_remove(&kana_charnotes_all, _at);
            kana_charnotes_save();
        }
        return;
    }
    kana_charnote _n = { .codepoint = _codepoint };
    kana_charnotes_copy(_n.text, sizeof(_n.text), _s, _len);
    if(_found) {
        kana_charnote* _old = &((kana_charnote*)kana_charnotes_all.memory)[_at];
        if(strcmp(_old->text, _n.text) == 0) {
            return;
        }
        *_old = _n;
    } else {
        rde_arr_insert(&kana_charnotes_all, _at, &_n);
    }
    kana_charnotes_save();
}

u32 kana_charnotes_count(void) {
    return kana_charnotes_ready ? (u32)rde_arr_length(&kana_charnotes_all) : 0u;
}

u32 kana_charnotes_revision(void) {
    return kana_charnotes_changes;
}
