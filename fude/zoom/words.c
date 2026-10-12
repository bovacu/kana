// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/words.h"
#include "drawing/base/kfile.h"

#include <string.h>

#define FZW_VERSION 1u
#define FZW_KIND    0x53445257u   // 'WRDS'

b8 fude_zoom_words_save(const c8* _path, u64 _canvas_size, u64 _left, const c8* _words, u32 _len) {
    fude_bytes _b = fude_bytes_new(64u + _len);
    fude_put_header(&_b, FZW_VERSION, FZW_KIND);
    fude_put_u32(&_b, (u32)(_canvas_size & 0xFFFFFFFFu));
    fude_put_u32(&_b, (u32)(_canvas_size >> 32));
    fude_put_u32(&_b, (u32)(_left & 0xFFFFFFFFu));
    fude_put_u32(&_b, (u32)(_left >> 32));
    fude_put_u32(&_b, _len);
    fude_put_data(&_b, _words, _len);
    return fude_bytes_write_and_free(&_b, _path, NULL);
}

b8 fude_zoom_words_load(const c8* _path, u64* _canvas_size, u64* _left, rde_arr* _out) {
    rde_arr_clear(_out);
    *_canvas_size = 0u;
    *_left        = 0u;
    if(!rde_file_exists(_path)) {
        return false;
    }
    u32 _bytes = 0;
    u8* _data = fude_file_read(_path, &_bytes);
    if(_data == NULL) {
        return false;
    }
    fude_reader _r = fude_reader_make(_data, _bytes);
    b8 _ok = fude_read_header(&_r, FZW_VERSION, FZW_KIND);
    const u32 _lo = _ok ? fude_get_u32(&_r) : 0u, _hi = _ok ? fude_get_u32(&_r) : 0u;
    const u32 _left_lo = _ok ? fude_get_u32(&_r) : 0u, _left_hi = _ok ? fude_get_u32(&_r) : 0u;
    const u32 _len = _ok ? fude_get_u32(&_r) : 0u;
    _ok = _ok && _r.ok && fude_reader_has(&_r, _len);
    if(_ok) {
        c8* _dst = (c8*)rde_arr_add_n(_out, (usize)_len + 1u);
        memcpy(_dst, _r.data + _r.pos, _len);
        _dst[_len] = 0;
        *_canvas_size = (u64)_lo | ((u64)_hi << 32);
        *_left        = (u64)_left_lo | ((u64)_left_hi << 32);
    }
    fude_file_free(_data);
    return _ok;
}

RDE_INTERNAL c8 fzw_fold(c8 _c) {
    return (_c >= 'A' && _c <= 'Z') ? (c8)(_c + 32) : _c;
}

RDE_INTERNAL b8 fzw_at(const c8* _hay, u32 _len, u32 _i, const c8* _needle, u32 _n) {
    if(_i + _n > _len) {
        return false;
    }
    for(u32 _k = 0; _k < _n; _k++) {
        if(fzw_fold(_hay[_i + _k]) != fzw_fold(_needle[_k])) {
            return false;
        }
    }
    return true;
}

u32 fude_zoom_words_find(const c8* _hay, u32 _len, const c8* _needle) {
    const u32 _n = (u32)strlen(_needle);
    for(u32 _i = 0; _n > 0u && _i + _n <= _len; _i++) {
        if(fzw_at(_hay, _len, _i, _needle, _n)) {
            return _i;
        }
    }
    return FUDE_ZOOM_WORDS_NONE;
}

u32 fude_zoom_words_count(const c8* _hay, u32 _len, const c8* _needle) {
    const u32 _n = (u32)strlen(_needle);
    u32 _count = 0;
    for(u32 _i = 0; _n > 0u && _i + _n <= _len;) {
        if(fzw_at(_hay, _len, _i, _needle, _n)) {
            _count++;
            _i += _n;
        } else {
            _i++;
        }
    }
    return _count;
}

// (the start of the character _i is in: never inside one)
RDE_INTERNAL u32 fzw_char_start(const c8* _s, u32 _i) {
    while(_i > 0u && ((u8)_s[_i] & 0xC0u) == 0x80u) {
        _i--;
    }
    return _i;
}

RDE_INTERNAL u32 fzw_chars(const c8* _s, u32 _from, u32 _to) {
    u32 _n = 0;
    for(u32 _i = _from; _i < _to; _i++) {
        _n += ((u8)_s[_i] & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return _n;
}

b8 fude_zoom_words_excerpt(const c8* _hay, u32 _len, const c8* _needle, u32 _most, c8* _out, u32 _size) {
    if(_size == 0u) {
        return false;
    }
    _out[0] = 0;
    const u32 _at = fude_zoom_words_find(_hay, _len, _needle);
    if(_at == FUDE_ZOOM_WORDS_NONE) {
        return false;
    }
    const u32 _n = (u32)strlen(_needle);
    // Its line.
    u32 _a = _at, _b = _at + _n;
    while(_a > 0u && _hay[_a - 1u] != '\n') {
        _a--;
    }
    while(_b < _len && _hay[_b] != '\n') {
        _b++;
    }
    // As much round it as fits: a little before it, the rest after.
    u32 _from = _a, _to = _b;
    if(_most > 0u && fzw_chars(_hay, _a, _b) > _most) {
        const u32 _before = _most / 4u;
        _from = _at;
        for(u32 _k = 0; _k < _before && _from > _a; _k++) {
            _from = fzw_char_start(_hay, _from - 1u);
        }
        _to = _from;
        for(u32 _k = 0; _k < _most && _to < _b; _k++) {
            _to++;
            while(_to < _b && ((u8)_hay[_to] & 0xC0u) == 0x80u) {
                _to++;
            }
        }
        _to = _to < _at + _n ? _at + _n : _to;
    }
    const b8 _cut_front = _from > _a, _cut_back = _to < _b;
    u32 _w = 0;
    #define FZW_PUT(_p, _k) do { const u32 _m = (_k); if(_w + _m + 1u <= _size) { memcpy(_out + _w, (_p), _m); _w += _m; } } while(0)
    if(_cut_front) {
        FZW_PUT("\xE2\x80\xA6", 3u);
    }
    // (as many whole characters as the room holds)
    u32 _end = _to;
    const u32 _room = _size - 1u - _w - (_cut_back ? 3u : 0u);
    if(_end - _from > _room) {
        _end = fzw_char_start(_hay, _from + _room);
    }
    FZW_PUT(_hay + _from, _end - _from);
    if(_cut_back || _end < _to) {
        FZW_PUT("\xE2\x80\xA6", 3u);
    }
    #undef FZW_PUT
    _out[_w] = 0;
    return true;
}
