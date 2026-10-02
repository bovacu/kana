#include "study/models/charnote.h"
#include "drawing/base/kfile.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See charnote.h. A few hundred notes at most: an array sorted by code point.
// ===========================================================================

#define FUDE_CHARNOTES_VERSION 1u
#define FUDE_CHARNOTES_KIND    FUDE_TAG('C', 'N', 'O', 'T')
#define FUDE_CHARNOTES_CHUNK   FUDE_TAG('C', 'N', 'T', 'S')

typedef struct {
    u32 codepoint;
    c8  text[FUDE_CHARNOTE_TEXT];
} fude_charnote;

static rde_arr TYPE(fude_charnote) fude_charnotes_all;
static b8                          fude_charnotes_ready   = false;
static c8                          fude_charnotes_path[RDE_MAX_PATH];
static u32                         fude_charnotes_changes = 0;

RDE_INTERNAL void fude_charnotes_ensure(void) {
    if(!fude_charnotes_ready) {
        fude_charnotes_all   = rde_arr_new(sizeof(fude_charnote), rde_memory_allocator_get_default_std());
        fude_charnotes_ready = true;
    }
}

// _s (_len bytes) into _out, cut at a whole character.
RDE_INTERNAL void fude_charnotes_copy(c8* _out, usize _size, const c8* _s, usize _len) {
    usize _n = _len < _size - 1u ? _len : _size - 1u;
    while(_n > 0 && _n < _len && ((u8)_s[_n] & 0xC0u) == 0x80u) {
        _n--;
    }
    if(_n > 0) {
        memcpy(_out, _s, _n);
    }
    _out[_n] = 0;
}

RDE_INTERNAL u32 fude_charnotes_find(u32 _codepoint, b8* _found) {
    const fude_charnote* _n  = (const fude_charnote*)fude_charnotes_all.memory;
    u32                  _lo = 0;
    u32                  _hi = (u32)rde_arr_length(&fude_charnotes_all);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_n[_mid].codepoint < _codepoint) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    *_found = _lo < (u32)rde_arr_length(&fude_charnotes_all) && _n[_lo].codepoint == _codepoint;
    return _lo;
}

RDE_INTERNAL void fude_charnotes_save(void) {
    fude_charnotes_changes++;
    if(fude_charnotes_path[0] == 0) {
        return;
    }
    const u32  _count = (u32)rde_arr_length(&fude_charnotes_all);
    fude_bytes _b     = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 16u + _count * 64u);
    fude_put_header(&_b, FUDE_CHARNOTES_VERSION, FUDE_CHARNOTES_KIND);
    const u32 _chunk = fude_chunk_begin(&_b, FUDE_CHARNOTES_CHUNK);
    fude_put_u32(&_b, _count);
    const fude_charnote* _n = (const fude_charnote*)fude_charnotes_all.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _len = (u32)strlen(_n[_i].text);
        fude_put_u32(&_b, _n[_i].codepoint);
        fude_put_u16(&_b, (u16)_len);
        fude_put_data(&_b, _n[_i].text, _len);
    }
    fude_chunk_end(&_b, _chunk);
    if(!fude_bytes_write_and_free(&_b, fude_charnotes_path, NULL)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not save the character notes (%s)", fude_charnotes_path);
    }
}

void fude_charnotes_open(const c8* _path) {
    fude_charnotes_ensure();
    rde_arr_clear(&fude_charnotes_all);
    snprintf(fude_charnotes_path, sizeof(fude_charnotes_path), "%s", _path != NULL ? _path : "");
    fude_charnotes_changes++;
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
    u8*         _data = fude_file_read(_from, &_size);
    fude_reader _r    = fude_reader_make(_data, _size);
    if(_data == NULL || !fude_read_header(&_r, FUDE_CHARNOTES_VERSION, FUDE_CHARNOTES_KIND)) {
        fude_file_free(_data);
        fude_file_set_aside(_from);
        return;
    }
    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_CHARNOTES_CHUNK) {
            continue;
        }
        const u32 _count = fude_get_u32(&_chunk);
        for(u32 _i = 0; _i < _count && _chunk.ok; _i++) {
            fude_charnote _n = { 0 };
            _n.codepoint     = fude_get_u32(&_chunk);
            const u32 _len   = fude_get_u16(&_chunk);
            if(!_chunk.ok || _len > _chunk.size - _chunk.pos) {
                break;
            }
            fude_charnotes_copy(_n.text, sizeof(_n.text), (const c8*)&_chunk.data[_chunk.pos], _len);
            _chunk.pos += _len;
            b8        _found;
            const u32 _at = fude_charnotes_find(_n.codepoint, &_found);
            if(!_found && _n.text[0] != 0) {
                rde_arr_insert(&fude_charnotes_all, _at, &_n);
            }
        }
    }
    fude_file_free(_data);
}

void fude_charnotes_close(void) {
    if(fude_charnotes_ready) {
        rde_arr_free(&fude_charnotes_all);
        fude_charnotes_ready = false;
    }
    fude_charnotes_path[0] = 0;
    fude_charnotes_changes++;
}

const c8* fude_charnote_get(u32 _codepoint) {
    if(!fude_charnotes_ready) {
        return "";
    }
    b8        _found;
    const u32 _at = fude_charnotes_find(_codepoint, &_found);
    return _found ? ((const fude_charnote*)fude_charnotes_all.memory)[_at].text : "";
}

void fude_charnote_set(u32 _codepoint, const c8* _text) {
    fude_charnotes_ensure();
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
    const u32 _at = fude_charnotes_find(_codepoint, &_found);
    if(_len == 0) {
        if(_found) {
            rde_arr_remove(&fude_charnotes_all, _at);
            fude_charnotes_save();
        }
        return;
    }
    fude_charnote _n = { .codepoint = _codepoint };
    fude_charnotes_copy(_n.text, sizeof(_n.text), _s, _len);
    if(_found) {
        fude_charnote* _old = &((fude_charnote*)fude_charnotes_all.memory)[_at];
        if(strcmp(_old->text, _n.text) == 0) {
            return;
        }
        *_old = _n;
    } else {
        rde_arr_insert(&fude_charnotes_all, _at, &_n);
    }
    fude_charnotes_save();
}

u32 fude_charnotes_count(void) {
    return fude_charnotes_ready ? (u32)rde_arr_length(&fude_charnotes_all) : 0u;
}

u32 fude_charnotes_revision(void) {
    return fude_charnotes_changes;
}
