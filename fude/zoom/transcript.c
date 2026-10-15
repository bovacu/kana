// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/transcript.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See transcript.h.
//
// A transcript is two arrays on the std allocator: the phrases' words back to back in one text, and the phrases, each
// pointing into it. Whatever joins them (join, lines, tail) puts the spaces between phrases back, and join and lines
// a paragraph after a pause. The file's own names start with FZTR_ and fztr_.
// ===========================================================================

// The .text file's format version and kind (the layout is in transcript.h).
#define FZTR_VERSION 1u
#define FZTR_KIND    0x534E5254u   // 'TRNS'
// The most phrases a file may claim: a larger count is taken as damage.
#define FZTR_MOST    1000000u

// See the header.
void fude_zoom_transcript_init(fude_zoom_transcript* _t) {
    rde_memset(_t, 0, sizeof(*_t));
    _t->segments = rde_arr_new(sizeof(fude_zoom_transcript_segment), rde_memory_allocator_get_default_std());
    _t->text     = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
}

// See the header.
void fude_zoom_transcript_free(fude_zoom_transcript* _t) {
    rde_arr_free(&_t->segments);
    rde_arr_free(&_t->text);
}

// See the header.
void fude_zoom_transcript_clear(fude_zoom_transcript* _t) {
    rde_arr_clear(&_t->segments);
    rde_arr_clear(&_t->text);
    _t->language[0] = 0;
    _t->done        = 0u;
    _t->complete    = false;
}

// Returns whether _c is a space, a tab or a line break.
RDE_INTERNAL b8 fztr_space(c8 _c) {
    return _c == ' ' || _c == '\t' || _c == '\n' || _c == '\r';
}

// Returns whether the _n bytes at _w are only a sound's tag, whisper's mark for what is not speech: one tag in
// brackets, parentheses or stars ([BLANK_AUDIO], (music), *laughs*), or only music notes (♪ ♫) and spaces.
RDE_INTERNAL b8 fztr_tag_only(const c8* _w, u32 _n) {
    if(_n >= 2u && ((_w[0] == '[' && _w[_n - 1u] == ']') || (_w[0] == '(' && _w[_n - 1u] == ')') || (_w[0] == '*' && _w[_n - 1u] == '*'))) {
        for(u32 _i = 1; _i + 1u < _n; _i++) {
            if(_w[_i] == '[' || _w[_i] == ']' || _w[_i] == '(' || _w[_i] == ')') {
                return false;   // (two tags, or words between them)
            }
        }
        return true;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        const u8 _c = (u8)_w[_i];
        if(!(_c == 0xE2u || _c == 0x99u || _c == 0xAAu || _c == 0xABu || fztr_space((c8)_c))) {
            return false;   // (♪ ♫ and spaces only: music)
        }
    }
    return true;
}

// See the header. A phrase ending before it began is taken to end when it began.
b8 fude_zoom_transcript_add(fude_zoom_transcript* _t, u32 _t0_ms, u32 _t1_ms, const c8* _words) {
    if(_words == NULL) {
        return false;
    }
    u32 _a = 0, _b = (u32)rde_strlen(_words);
    while(_a < _b && fztr_space(_words[_a])) {
        _a++;
    }
    while(_b > _a && fztr_space(_words[_b - 1u])) {
        _b--;
    }
    if(_b == _a || fztr_tag_only(_words + _a, _b - _a)) {
        return false;
    }
    const fude_zoom_transcript_segment _s = { _t0_ms, _t1_ms >= _t0_ms ? _t1_ms : _t0_ms, (u32)rde_arr_length(&_t->text), _b - _a };
    rde_memcpy(rde_arr_add_n(&_t->text, _b - _a), _words + _a, _b - _a);
    rde_arr_add(&_t->segments, (any)&_s);
    return true;
}

// See the header. Each phrase goes through fude_zoom_transcript_add, through a NUL-ended copy of at most 4095 bytes.
void fude_zoom_transcript_append(fude_zoom_transcript* _t, const fude_zoom_transcript* _more) {
    const fude_zoom_transcript_segment* _s = (const fude_zoom_transcript_segment*)_more->segments.memory;
    const c8* _w = (const c8*)_more->text.memory;
    c8 _one[4096];
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_more->segments); _i++) {
        const u32 _n = _s[_i].len < sizeof(_one) - 1u ? _s[_i].len : (u32)sizeof(_one) - 1u;
        rde_memcpy(_one, _w + _s[_i].at, _n);
        _one[_n] = 0;
        fude_zoom_transcript_add(_t, _s[_i].t0_ms, _s[_i].t1_ms, _one);
    }
}

// See the header.
void fude_zoom_transcript_join(const fude_zoom_transcript* _t, u32 _pause_ms, rde_arr* _out) {
    rde_arr_clear(_out);
    const fude_zoom_transcript_segment* _s = (const fude_zoom_transcript_segment*)_t->segments.memory;
    const c8* _w = (const c8*)_t->text.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_t->segments); _i++) {
        if(_i > 0u) {
            const b8 _pause = _s[_i].t0_ms >= _s[_i - 1u].t1_ms + _pause_ms;
            rde_memcpy(rde_arr_add_n(_out, _pause ? 2u : 1u), _pause ? "\n\n" : " ", _pause ? 2u : 1u);
        }
        rde_memcpy(rde_arr_add_n(_out, _s[_i].len), _w + _s[_i].at, _s[_i].len);
    }
    *(c8*)rde_arr_add_n(_out, 1u) = 0;
}

// Returns how many UTF-8 characters the _n bytes at _s hold (every byte but a continuation byte): a line's width is
// counted in characters, as it is seen, not in bytes.
RDE_INTERNAL u32 fztr_chars(const c8* _s, u32 _n) {
    u32 _c = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _c += ((u8)_s[_i] & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return _c;
}

// See the header. Each phrase is broken into words at spaces; each word goes on the line being written, after a space,
// while it fits in _width characters, else it begins a new line, which is said from its phrase's start. A width of 0
// is taken as 1.
u32 fude_zoom_transcript_lines(const fude_zoom_transcript* _t, u32 _width, u32 _pause_ms, rde_arr* _out, rde_arr* _lines) {
    rde_arr_clear(_out);
    rde_arr_clear(_lines);
    const fude_zoom_transcript_segment* _s = (const fude_zoom_transcript_segment*)_t->segments.memory;
    const c8* _w = (const c8*)_t->text.memory;
    _width = _width > 0u ? _width : 1u;
    fude_zoom_transcript_line _line = { 0u, 0u, 0u };
    b8  _open = false;   // (a line begun)
    u32 _chars = 0;
    // FZTR_END_LINE closes the line being written, if one is, and adds it to _lines. FZTR_NEW_LINE begins a line said
    // from _ms, after a '\n' unless it is the text's first.
    #define FZTR_END_LINE() do { if(_open) { _line.len = (u32)rde_arr_length(_out) - _line.at; rde_arr_add(_lines, (any)&_line); _open = false; } } while(0)
    #define FZTR_NEW_LINE(_ms) do { if(rde_arr_length(_out) > 0u) { *(c8*)rde_arr_add_n(_out, 1u) = '\n'; } _line.at = (u32)rde_arr_length(_out); _line.t0_ms = (_ms); _open = true; _chars = 0u; } while(0)
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_t->segments); _i++) {
        if(_i > 0u && _s[_i].t0_ms >= _s[_i - 1u].t1_ms + _pause_ms && _open) {
            FZTR_END_LINE();
            *(c8*)rde_arr_add_n(_out, 1u) = '\n';   // (a pause: an empty line between, a paragraph)
            _line.at = (u32)rde_arr_length(_out);
            rde_arr_add(_lines, &(fude_zoom_transcript_line){ _line.at, 0u, _s[_i].t0_ms });
        }
        // Its words one by one, onto the line while they fit.
        const c8* _p = _w + _s[_i].at;
        u32 _k = 0;
        while(_k < _s[_i].len) {
            while(_k < _s[_i].len && fztr_space(_p[_k])) {
                _k++;
            }
            u32 _e = _k;
            while(_e < _s[_i].len && !fztr_space(_p[_e])) {
                _e++;
            }
            if(_e == _k) {
                break;
            }
            const u32 _wc = fztr_chars(_p + _k, _e - _k);
            if(!_open) {
                FZTR_NEW_LINE(_s[_i].t0_ms);
            } else if(_chars + 1u + _wc > _width) {
                FZTR_END_LINE();
                FZTR_NEW_LINE(_s[_i].t0_ms);
            } else {
                *(c8*)rde_arr_add_n(_out, 1u) = ' ';
                _chars++;
            }
            rde_memcpy(rde_arr_add_n(_out, _e - _k), _p + _k, _e - _k);
            _chars += _wc;
            _k = _e;
        }
    }
    FZTR_END_LINE();
    #undef FZTR_END_LINE
    #undef FZTR_NEW_LINE
    *(c8*)rde_arr_add_n(_out, 1u) = 0;
    return (u32)rde_arr_length(_lines);
}

// See the header. The joined text is a scratch array on the std allocator, freed before returning.
void fude_zoom_transcript_tail(const fude_zoom_transcript* _t, u32 _most, c8* _out, usize _size) {
    if(_size == 0u) {
        return;
    }
    _out[0] = 0;
    // The text keeps the phrases back to back (each found by its segment), so the last ones are joined again here
    // with a space between them: read straight off the text, one phrase's last word would run into the next's first.
    const fude_zoom_transcript_segment* _s = (const fude_zoom_transcript_segment*)_t->segments.memory;
    const c8*                           _w = (const c8*)_t->text.memory;
    const u32                           _n = (u32)rde_arr_length(&_t->segments);
    u32 _first = _n, _gathered = 0;
    while(_first > 0u && _gathered < _most) {
        _first--;
        _gathered += _s[_first].len + 1u;
    }
    rde_arr TYPE(c8) _joined = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    for(u32 _i = _first; _i < _n; _i++) {
        if(_i > _first) {
            *(c8*)rde_arr_add_n(&_joined, 1u) = ' ';
        }
        rde_memcpy(rde_arr_add_n(&_joined, _s[_i].len), _w + _s[_i].at, _s[_i].len);
    }
    // Its last _most bytes (and no more than _out holds), from a word's start: never half a word, nor, since a word
    // starts after an ASCII space, half a character.
    const u32  _total = (u32)rde_arr_length(&_joined);
    const c8*  _j     = (const c8*)_joined.memory;
    const u32  _room  = (u32)(_size - 1u < (usize)_most ? _size - 1u : (usize)_most);
    u32        _from  = _total > _room ? _total - _room : 0u;
    while(_from > 0u && _from < _total && !fztr_space(_j[_from - 1u])) {
        _from++;
    }
    rde_memcpy(_out, _j + _from, _total - _from);
    _out[_total - _from] = 0;
    rde_arr_free(&_joined);
}

// See the header. The language is written up to 7 bytes, so it is read back NUL-ended into its 8.
b8 fude_zoom_transcript_save(const fude_zoom_transcript* _t, const c8* _path) {
    const u32 _n = (u32)rde_arr_length(&_t->segments);
    fude_bytes _b = fude_bytes_new(64u + _n * 16u + (u32)rde_arr_length(&_t->text));
    fude_put_header(&_b, FZTR_VERSION, FZTR_KIND);
    u8 _ll = 0;
    while(_ll + 1u < sizeof(_t->language) && _t->language[_ll] != 0) {
        _ll++;
    }
    fude_put_u8(&_b, _ll);
    fude_put_data(&_b, _t->language, _ll);
    fude_put_u32(&_b, _t->done);
    fude_put_u8(&_b, _t->complete ? 1u : 0u);
    fude_put_u32(&_b, _n);
    const fude_zoom_transcript_segment* _s = (const fude_zoom_transcript_segment*)_t->segments.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        fude_put_u32(&_b, _s[_i].t0_ms);
        fude_put_u32(&_b, _s[_i].t1_ms);
        fude_put_u32(&_b, _s[_i].len);
        fude_put_data(&_b, (const c8*)_t->text.memory + _s[_i].at, _s[_i].len);
    }
    return fude_bytes_write_and_free(&_b, _path, NULL);
}

// See the header. Anything wrong in the file (a short read, a language too long, a count past FZTR_MOST) drops it
// all, so a damaged file never gives half a transcript. A phrase of no bytes, which fude_zoom_transcript_add never
// makes, is passed over: it holds nothing to lose, and every reader of the phrases (join, lines, tail) adds each one's
// bytes with rde_arr_add_n, which takes no count of 0.
b8 fude_zoom_transcript_load(fude_zoom_transcript* _t, const c8* _path) {
    fude_zoom_transcript_clear(_t);
    if(!rde_file_exists(_path)) {
        return false;
    }
    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        return false;
    }
    fude_reader _r = fude_reader_make(_data, _size);
    b8 _ok = fude_read_header(&_r, FZTR_VERSION, FZTR_KIND);
    const u8 _ll = _ok ? fude_get_u8(&_r) : 0u;
    _ok = _ok && _r.ok && _ll < sizeof(_t->language) && fude_reader_has(&_r, _ll);
    if(_ok) {
        rde_memcpy(_t->language, _r.data + _r.pos, _ll);
        _t->language[_ll] = 0;
        _r.pos += _ll;
    }
    _t->done     = _ok ? fude_get_u32(&_r) : 0u;
    _t->complete = _ok ? fude_get_u8(&_r) != 0u : false;
    const u32 _n = _ok ? fude_get_u32(&_r) : 0u;
    _ok = _ok && _r.ok && _n <= FZTR_MOST;
    for(u32 _i = 0; _ok && _i < _n; _i++) {
        fude_zoom_transcript_segment _s;
        _s.t0_ms = fude_get_u32(&_r);
        _s.t1_ms = fude_get_u32(&_r);
        _s.len   = fude_get_u32(&_r);
        _ok = _r.ok && fude_reader_has(&_r, _s.len);
        if(_ok && _s.len > 0u) {
            _s.at = (u32)rde_arr_length(&_t->text);
            rde_memcpy(rde_arr_add_n(&_t->text, _s.len), _r.data + _r.pos, _s.len);
            _r.pos += _s.len;
            rde_arr_add(&_t->segments, (any)&_s);
        }
    }
    fude_file_free(_data);
    if(!_ok) {
        fude_zoom_transcript_clear(_t);
    }
    return _ok;
}

// See the header. The tenths of a second are measured by their energy (the sum of the squared samples), stepping
// half a tenth at a time, and at least a sample: at a rate under 20 a tenth is a single sample, whose half is 0.
u32 fude_zoom_transcript_cut(const f32* _pcm, u32 _n, u32 _rate, u32 _window_ms) {
    const u32 _window = (u32)((u64)_rate * _window_ms / 1000u), _step = _rate / 10u;
    if(_n <= _window || _step == 0u) {
        return _n;
    }
    const u32 _advance = _step / 2u > 0u ? _step / 2u : 1u;
    // (the quietest tenth of a second in the last window: its middle)
    u32 _best = _n;
    f64 _least = 1e300;
    for(u32 _a = _n - _window; _a + _step <= _n; _a += _advance) {
        f64 _e = 0.0;
        for(u32 _i = _a; _i < _a + _step; _i++) {
            _e += (f64)_pcm[_i] * (f64)_pcm[_i];
        }
        if(_e < _least) {
            _least = _e;
            _best  = _a + _step / 2u;
        }
    }
    return _best;
}
