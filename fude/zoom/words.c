// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/words.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See words.h.
//
// The file (save, load), then the search: a plain byte-by-byte scan with no index that folds A to Z, and the excerpt
// Find lists. File-local names start with fzwd_ (FZWD_ for its constants; mechrun.c's are fzw_).
// ===========================================================================

// The words file's format version and its kind's tag (words.h has the layout).
#define FZWD_VERSION 1u
#define FZWD_KIND    0x53445257u   // 'WRDS', as FUDE_TAG('W', 'R', 'D', 'S') makes it.

// See the header. The two u64 go in as two u32 halves, low first, as the KANA format keeps them.
b8 fude_zoom_words_save(const c8* _path, u64 _canvas_size, u64 _left, const c8* _words, u32 _len) {
    fude_bytes _b = fude_bytes_new(64u + _len);
    fude_put_header(&_b, FZWD_VERSION, FZWD_KIND);
    fude_put_u32(&_b, (u32)(_canvas_size & 0xFFFFFFFFu));
    fude_put_u32(&_b, (u32)(_canvas_size >> 32));
    fude_put_u32(&_b, (u32)(_left & 0xFFFFFFFFu));
    fude_put_u32(&_b, (u32)(_left >> 32));
    fude_put_u32(&_b, _len);
    fude_put_data(&_b, _words, _len);
    return fude_bytes_write_and_free(&_b, _path, NULL);
}

// See the header. Reads the whole file, checks its header, and copies the words out only when all _len bytes are
// there, so a cut file gives nothing rather than part of the words.
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
    b8 _ok = fude_read_header(&_r, FZWD_VERSION, FZWD_KIND);
    const u32 _lo = _ok ? fude_get_u32(&_r) : 0u, _hi = _ok ? fude_get_u32(&_r) : 0u;
    const u32 _left_lo = _ok ? fude_get_u32(&_r) : 0u, _left_hi = _ok ? fude_get_u32(&_r) : 0u;
    const u32 _len = _ok ? fude_get_u32(&_r) : 0u;
    _ok = _ok && _r.ok && fude_reader_has(&_r, _len);
    if(_ok) {
        c8* _dst = (c8*)rde_arr_add_n(_out, (usize)_len + 1u);
        rde_memcpy(_dst, _r.data + _r.pos, _len);
        _dst[_len] = 0;
        *_canvas_size = (u64)_lo | ((u64)_hi << 32);
        *_left        = (u64)_left_lo | ((u64)_left_hi << 32);
    }
    fude_file_free(_data);
    return _ok;
}

// How many bytes of a words file come before its words: the header, the canvas file's size and when it was left.
#define FZWD_HEAD (FUDE_FILE_HEADER_SIZE + 16u)

// See the header. Only the file's first FZWD_HEAD bytes are read, where they are: not the words after them.
b8 fude_zoom_words_left(const c8* _path, u64* _left) {
    *_left = 0u;
    if(!rde_file_exists(_path)) {
        return false;   // (checked first: RDE reports opening a missing file as an error)
    }
    u8    _head[FZWD_HEAD];
    usize _got = 0u;
    RDE_TRY({
        rde_file* _file = rde_file_open(_path, RDE_FILE_MODE_READ_BYTES);
        if(_file != NULL && !rde_failed()) {
            _got = rde_file_read_at(_file, 0u, _head, sizeof(_head));
            rde_file_close(_file);
        }
    });
    if(_got < sizeof(_head)) {
        return false;
    }
    fude_reader _r = fude_reader_make(_head, (u32)sizeof(_head));
    if(!fude_read_header(&_r, FZWD_VERSION, FZWD_KIND)) {
        return false;
    }
    fude_get_u32(&_r);   // (the canvas file's size, both halves: not wanted here)
    fude_get_u32(&_r);
    const u32 _lo = fude_get_u32(&_r), _hi = fude_get_u32(&_r);
    if(!_r.ok) {
        return false;
    }
    *_left = (u64)_lo | ((u64)_hi << 32);
    return true;
}

// Folds A to Z to lower case and leaves every other byte alone, so UTF-8 is never touched.
RDE_INTERNAL c8 fzwd_fold(c8 _c) {
    return (_c >= 'A' && _c <= 'Z') ? (c8)(_c + 32) : _c;
}

// True when the _n bytes of _needle match _hay at _i, A to Z in either case; false when they would run past _len.
RDE_INTERNAL b8 fzwd_at(const c8* _hay, u32 _len, u32 _i, const c8* _needle, u32 _n) {
    if(_i + _n > _len) {
        return false;
    }
    for(u32 _k = 0; _k < _n; _k++) {
        if(fzwd_fold(_hay[_i + _k]) != fzwd_fold(_needle[_k])) {
            return false;
        }
    }
    return true;
}

// See the header. A plain scan from the start.
u32 fude_zoom_words_find(const c8* _hay, u32 _len, const c8* _needle) {
    const u32 _n = (u32)rde_strlen(_needle);
    for(u32 _i = 0; _n > 0u && _i + _n <= _len; _i++) {
        if(fzwd_at(_hay, _len, _i, _needle, _n)) {
            return _i;
        }
    }
    return FUDE_ZOOM_WORDS_NONE;
}

// See the header. After a match the scan goes on past it, so matches never overlap.
u32 fude_zoom_words_count(const c8* _hay, u32 _len, const c8* _needle) {
    const u32 _n = (u32)rde_strlen(_needle);
    u32 _count = 0;
    for(u32 _i = 0; _n > 0u && _i + _n <= _len;) {
        if(fzwd_at(_hay, _len, _i, _needle, _n)) {
            _count++;
            _i += _n;
        } else {
            _i++;
        }
    }
    return _count;
}

// Returns the start of the UTF-8 character byte _i is in, stepping back over continuation bytes.
// (the start of the character _i is in: never inside one)
RDE_INTERNAL u32 fzwd_char_start(const c8* _s, u32 _i) {
    while(_i > 0u && ((u8)_s[_i] & 0xC0u) == 0x80u) {
        _i--;
    }
    return _i;
}

// Returns how many UTF-8 characters are in bytes _from to _to (the bytes that are not continuation bytes).
RDE_INTERNAL u32 fzwd_chars(const c8* _s, u32 _from, u32 _to) {
    u32 _n = 0;
    for(u32 _i = _from; _i < _to; _i++) {
        _n += ((u8)_s[_i] & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return _n;
}

// See the header. The match's line is found, cut round the match when it is longer than _most characters (a quarter
// of them before the match, the rest from there, and never short of the match's end), then written out with "…" at
// each end that was cut. _size bounds what is written; when the line does not fit it is cut again at a character's
// start.
b8 fude_zoom_words_excerpt(const c8* _hay, u32 _len, const c8* _needle, u32 _most, c8* _out, u32 _size) {
    if(_size == 0u) {
        return false;
    }
    _out[0] = 0;
    const u32 _at = fude_zoom_words_find(_hay, _len, _needle);
    if(_at == FUDE_ZOOM_WORDS_NONE) {
        return false;
    }
    const u32 _n = (u32)rde_strlen(_needle);
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
    if(_most > 0u && fzwd_chars(_hay, _a, _b) > _most) {
        const u32 _before = _most / 4u;
        _from = _at;
        for(u32 _k = 0; _k < _before && _from > _a; _k++) {
            _from = fzwd_char_start(_hay, _from - 1u);
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
    // Appends _k bytes from _p when they fit with the NUL after them; a piece that does not fit is left out whole.
    #define FZWD_PUT(_p, _k) do { const u32 _m = (_k); if(_w + _m + 1u <= _size) { rde_memcpy(_out + _w, (_p), _m); _w += _m; } } while(0)
    if(_cut_front) {
        FZWD_PUT("\xE2\x80\xA6", 3u);
    }
    // As many whole characters as the room holds. The room keeps space for the NUL and for a closing … whenever one
    // may be needed (the line goes on, or the room cuts it), and is 0, never a wrapped-round u32, when _size is small.
    u32       _end     = _to;
    const u32 _reserve = _w + 1u + 3u;
    const u32 _room    = _size > _reserve ? _size - _reserve : 0u;
    if(_end - _from > _room) {
        _end = fzwd_char_start(_hay, _from + _room);
    }
    FZWD_PUT(_hay + _from, _end - _from);
    if(_cut_back || _end < _to) {
        FZWD_PUT("\xE2\x80\xA6", 3u);
    }
    #undef FZWD_PUT
    _out[_w] = 0;
    return true;
}
