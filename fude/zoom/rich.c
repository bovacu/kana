// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/rich.h"

// ===========================================================================
// See rich.h. A text is read a line at a time: the line's kind from the marks it starts with (a heading, a bullet,
// a number, a task, a table's row, a rule); then its words' styles from the marks in it, as SEGMENTS of the text of
// one style each; then those cut into words (PIECES) and flowed into lines as wide as there is room for, as runs.
//
// Internal names start fzr_ (FZR_). The file goes: a line's words' styles (fzr_inline), a line's kind
// (fzr_read_line), flowing words into lines (fzr_flow), tables, the whole text (fude_zoom_rich_lay_aligned), the
// marks put in by a tap (fude_zoom_rich_format), and the layout made while a text is written, with its caret. Sizes
// below are in letters: multiples of the text's letter size.
// ===========================================================================

#define FZR_HEADINGS  { 1.5f, 1.3f, 1.12f }   // a heading's letter size, of the text's: #, ##, ###
#define FZR_INDENT    1.4f    // a level's indent (letters)
#define FZR_BULLET_AT 0.2f    // a bullet's place in its indent, and its words' (letters)
#define FZR_BULLET_TO 1.1f    // (its words')
#define FZR_BOX       0.72f   // a task's box (letters), its words after it
#define FZR_BOX_TO    1.25f   // (its words')
#define FZR_CELL_X    0.45f   // a table cell's room round its words (letters)
#define FZR_CELL_Y    0.22f   // (above and below)
#define FZR_COLUMNS   16u     // a table's columns, at most

// A line's kind: plain words, a heading, a bullet, a numbered line, a rule, a table's row. A task is a plain line, a
// bullet or a number with a task (fzr_line's task).
typedef enum {
    FZR_PARA = 0, FZR_HEAD, FZR_BULLET, FZR_NUMBER, FZR_RULE, FZR_TABLE,
} FZR_;

// A run of the text in one style (a link's: its address too).
typedef struct {
    u32 from, len;        // its bytes in the text
    u32 addr, addr_len;   // a link's address in the text (0, 0: none)
    u8  style;            // FUDE_ZOOM_RICH_ flags
} fzr_seg;

// A word of one: where a line may break (a space before it), and how wide it is.
typedef struct {
    u32 from, len;        // as fzr_seg's
    u32 addr, addr_len;
    u8  style;
    b8  space;            // a space before it: a line may break there
    f32 w;                // its width, measured
} fzr_piece;

// A line of the text as fzr_read_line reads it.
typedef struct {
    u32 from, to, body;   // the line (its '\n' not in it); where its words start
    u32 num, num_len;     // a numbered line's number ("12.")
    u32 task;             // a task's "[" (FUDE_ZOOM_RICH_NONE: none)
    u8  kind, level;      // FZR_; a heading's 1 to 3, else how far in
    b8  done;             // its task is ticked
} fzr_line;

// A layout's work: the text, the layout filled, the caller's measure, and the arrays a line is worked in.
typedef struct {
    const c8*                 t;
    fude_zoom_rich*           out;
    fude_zoom_rich_measure_fn measure;
    any                       self;
    rde_arr                   segs;     // fzr_seg (a line's, a cell's)
    rde_arr                   pieces;   // fzr_piece
    rde_arr                   addr;     // u32 pairs: each run's link address (with out->runs)
} fzr_ctx;

// Returns whether a byte belongs in a word: a letter, a digit, or any byte of a UTF-8 character past ASCII.
RDE_INTERNAL b8 fzr_word_char(c8 _c) {
    return (_c >= 'a' && _c <= 'z') || (_c >= 'A' && _c <= 'Z') || (_c >= '0' && _c <= '9') || ((u8)_c & 0x80u) != 0u;
}

// Returns whether _t goes on at _i with _s, all of it before _to.
RDE_INTERNAL b8 fzr_starts(const c8* _t, u32 _i, u32 _to, const c8* _s) {
    const u32 _n = (u32)rde_strlen(_s);
    return _i + _n <= _to && rde_memcmp(&_t[_i], _s, _n) == 0;
}

// Returns where the spaces at _i end (_to at most).
RDE_INTERNAL u32 fzr_skip_spaces(const c8* _t, u32 _i, u32 _to) {
    while(_i < _to && _t[_i] == ' ') {
        _i++;
    }
    return _i;
}

// --- a line's words' styles ---------------------------------------------------------------------------

// Adds a segment, unless it is empty.
RDE_INTERNAL void fzr_push(rde_arr* _segs, u32 _from, u32 _len, u8 _style, u32 _addr, u32 _addr_len) {
    if(_len > 0u) {
        const fzr_seg _s = { _from, _len, _addr, _addr_len, _style };
        rde_arr_add(_segs, (any)&_s);
    }
}

// Where the two-byte _mark next closes a span begun before _from, in [_from, _to): not after a space.
RDE_INTERNAL u32 fzr_close_2(const c8* _t, u32 _from, u32 _to, c8 _mark) {
    for(u32 _j = _from; _j + 2u <= _to; _j++) {
        if(_t[_j] == _mark && _t[_j + 1u] == _mark && _t[_j - 1u] != ' ') {
            return _j;
        }
    }
    return FUDE_ZOOM_RICH_NONE;
}

// [_from, _to) of _t as segments of style _style (and its link's address), its marks read: a span's inside read
// again for the marks in it (a bold word in a link), _depth deep at most 8. _keep: its marks kept, as segments of
// their own (FUDE_ZOOM_RICH_MARK: as it is being written).
RDE_INTERNAL void fzr_inline(rde_arr* _segs, const c8* _t, u32 _from, u32 _to, u8 _style, u32 _addr, u32 _addr_len, u32 _depth, b8 _keep) {
    u32 _i = _from, _plain = _from;
    #define FZR_FLUSH() fzr_push(_segs, _plain, _i - _plain, _style, _addr, _addr_len)
    #define FZR_MARK(_at, _n) do { if(_keep) { fzr_push(_segs, (_at), (_n), (u8)(_style | FUDE_ZOOM_RICH_MARK), 0u, 0u); } } while(0)
    while(_i < _to) {
        const c8 _c = _t[_i];
        if(_depth < 8u) {
            if(_c == '`') {
                // `code`: as it is written, its marks not read
                u32 _j = _i + 1u;
                while(_j < _to && _t[_j] != '`') {
                    _j++;
                }
                if(_j < _to && _j > _i + 1u) {
                    FZR_FLUSH();
                    FZR_MARK(_i, 1u);
                    fzr_push(_segs, _i + 1u, _j - _i - 1u, (u8)(_style | FUDE_ZOOM_RICH_CODE), _addr, _addr_len);
                    FZR_MARK(_j, 1u);
                    _i = _plain = _j + 1u;
                    continue;
                }
            } else if((_c == '*' || _c == '~') && _i + 2u < _to && _t[_i + 1u] == _c && _t[_i + 2u] != ' ') {
                // **bold**, ~~struck~~ (at least one character in it)
                const u32 _j = fzr_close_2(_t, _i + 3u, _to, _c);
                if(_j != FUDE_ZOOM_RICH_NONE) {
                    FZR_FLUSH();
                    FZR_MARK(_i, 2u);
                    fzr_inline(_segs, _t, _i + 2u, _j, (u8)(_style | (_c == '*' ? FUDE_ZOOM_RICH_BOLD : FUDE_ZOOM_RICH_STRUCK)), _addr, _addr_len, _depth + 1u, _keep);
                    FZR_MARK(_j, 2u);
                    _i = _plain = _j + 2u;
                    continue;
                }
            } else if((_c == '*' || _c == '_') && _i + 1u < _to && _t[_i + 1u] != ' ' && _t[_i + 1u] != _c &&
                      (_c == '*' || _i == _from || !fzr_word_char(_t[_i - 1u]))) {
                // *italic* or _italic_ (an underscore in a word is the word's: snake_case; 2 * 3 * 4 is maths)
                u32 _j = FUDE_ZOOM_RICH_NONE;
                for(u32 _k = _i + 2u; _k < _to; _k++) {
                    if(_t[_k] != _c || _t[_k - 1u] == ' ') {
                        continue;
                    }
                    const b8 _ends = _c == '*' ? (_k + 1u >= _to || _t[_k + 1u] != '*') && _t[_k - 1u] != '*'
                                               : (_k + 1u >= _to || !fzr_word_char(_t[_k + 1u]));
                    if(_ends) {
                        _j = _k;
                        break;
                    }
                }
                if(_j != FUDE_ZOOM_RICH_NONE) {
                    FZR_FLUSH();
                    FZR_MARK(_i, 1u);
                    fzr_inline(_segs, _t, _i + 1u, _j, (u8)(_style | FUDE_ZOOM_RICH_ITALIC), _addr, _addr_len, _depth + 1u, _keep);
                    FZR_MARK(_j, 1u);
                    _i = _plain = _j + 1u;
                    continue;
                }
            } else if(_c == '[' && !(_style & FUDE_ZOOM_RICH_LINK)) {
                // [words](address)
                u32 _k = _i + 1u;
                while(_k < _to && _t[_k] != ']') {
                    _k++;
                }
                if(_k > _i + 1u && _k + 1u < _to && _t[_k + 1u] == '(') {
                    u32 _m = _k + 2u;
                    while(_m < _to && _t[_m] != ')' && _t[_m] != ' ') {
                        _m++;
                    }
                    if(_m < _to && _t[_m] == ')' && _m > _k + 2u) {
                        FZR_FLUSH();
                        FZR_MARK(_i, 1u);
                        fzr_inline(_segs, _t, _i + 1u, _k, (u8)(_style | FUDE_ZOOM_RICH_LINK), _k + 2u, _m - _k - 2u, _depth + 1u, _keep);
                        FZR_MARK(_k, _m + 1u - _k);   // ("](address)")
                        _i = _plain = _m + 1u;
                        continue;
                    }
                }
            }
        }
        // An address written out, from a word's start (its last full stop or bracket the sentence's).
        if(!(_style & (FUDE_ZOOM_RICH_LINK | FUDE_ZOOM_RICH_CODE)) && (_i == _from || _t[_i - 1u] == ' ' || _t[_i - 1u] == '(') &&
           (fzr_starts(_t, _i, _to, "https://") || fzr_starts(_t, _i, _to, "http://") || fzr_starts(_t, _i, _to, "www."))) {
            u32 _j = _i;
            while(_j < _to && _t[_j] != ' ') {
                _j++;
            }
            while(_j > _i && _t[_j - 1u] != 0 && rde_strchr(".,;:!?)'\"", _t[_j - 1u]) != NULL) {
                _j--;
            }
            if(_j > _i + 4u) {
                FZR_FLUSH();
                fzr_push(_segs, _i, _j - _i, (u8)(_style | FUDE_ZOOM_RICH_LINK), _i, _j - _i);
                _i = _plain = _j;
                continue;
            }
        }
        _i++;
    }
    FZR_FLUSH();
    #undef FZR_FLUSH
    #undef FZR_MARK
}

// --- a line's kind -------------------------------------------------------------------------------------

// Reads the line [_from, _to): its indent (two spaces, or a tab, a level; six at most), then the first of a table's
// row, a rule, a heading (# to ###, a space after), a bullet (- * + or •, a space after) or a number (one to three
// digits, then "." or ")" and a space), then a task's [ ], [x] or [X]. body is where its words start.
RDE_INTERNAL fzr_line fzr_read_line(const c8* _t, u32 _from, u32 _to) {
    fzr_line _l = { _from, _to, _from, 0u, 0u, FUDE_ZOOM_RICH_NONE, FZR_PARA, 0u, false };
    u32 _i = _from, _spaces = 0;
    while(_i < _to && (_t[_i] == ' ' || _t[_i] == '\t')) {
        _spaces += _t[_i] == '\t' ? 2u : 1u;
        _i++;
    }
    _l.level = (u8)(_spaces / 2u > 6u ? 6u : _spaces / 2u);
    _l.body  = _i;
    if(_i < _to && _t[_i] == '|') {
        _l.kind = FZR_TABLE;
        return _l;
    }
    if(_i < _to && (_t[_i] == '-' || _t[_i] == '*' || _t[_i] == '_')) {
        // A rule: three or more of one of them, and nothing else but spaces.
        const c8 _r = _t[_i];
        u32 _n = 0;
        b8  _only = true;
        for(u32 _k = _i; _k < _to && _only; _k++) {
            _n    += _t[_k] == _r ? 1u : 0u;
            _only  = _t[_k] == _r || _t[_k] == ' ';
        }
        if(_only && _n >= 3u) {
            _l.kind = FZR_RULE;
            return _l;
        }
    }
    u32 _h = 0;
    while(_i + _h < _to && _t[_i + _h] == '#' && _h < 4u) {
        _h++;
    }
    if(_h >= 1u && _h <= 3u && _i + _h < _to && _t[_i + _h] == ' ') {
        _l.kind  = FZR_HEAD;
        _l.level = (u8)_h;
        _l.body  = fzr_skip_spaces(_t, _i + _h, _to);
        return _l;
    }
    if(_i + 1u < _to && (_t[_i] == '-' || _t[_i] == '*' || _t[_i] == '+') && _t[_i + 1u] == ' ') {
        _l.kind = FZR_BULLET;
        _i      = fzr_skip_spaces(_t, _i + 1u, _to);
    } else if(_i + 3u < _to && rde_memcmp(&_t[_i], FUDE_ZOOM_RICH_BULLET, 3u) == 0 && _t[_i + 3u] == ' ') {
        _l.kind = FZR_BULLET;
        _i      = fzr_skip_spaces(_t, _i + 3u, _to);
    } else {
        u32 _d = 0;
        while(_i + _d < _to && _t[_i + _d] >= '0' && _t[_i + _d] <= '9' && _d < 4u) {
            _d++;
        }
        if(_d >= 1u && _d <= 3u && _i + _d + 1u < _to && (_t[_i + _d] == '.' || _t[_i + _d] == ')') && _t[_i + _d + 1u] == ' ') {
            _l.kind    = FZR_NUMBER;
            _l.num     = _i;
            _l.num_len = _d + 1u;
            _i         = fzr_skip_spaces(_t, _i + _d + 1u, _to);
        }
    }
    if(_i + 3u <= _to && _t[_i] == '[' && (_t[_i + 1u] == ' ' || _t[_i + 1u] == 'x' || _t[_i + 1u] == 'X') && _t[_i + 2u] == ']' &&
       (_i + 3u == _to || _t[_i + 3u] == ' ')) {
        _l.task = _i;
        _l.done = _t[_i + 1u] != ' ';
        _i      = fzr_skip_spaces(_t, _i + 3u, _to);
    }
    _l.body = _i;
    return _l;
}

// --- flowing words into lines -----------------------------------------------------------------------------

// The segments cut into words, each measured at _px.
RDE_INTERNAL u32 fzr_pieces(fzr_ctx* _c, f32 _px) {
    rde_arr_clear(&_c->pieces);
    const fzr_seg* _s = (const fzr_seg*)_c->segs.memory;
    b8 _space = false;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_c->segs); _k++) {
        u32 _i = _s[_k].from;
        const u32 _end = _s[_k].from + _s[_k].len;
        while(_i < _end) {
            if(_c->t[_i] == ' ') {
                _space = true;
                _i++;
                continue;
            }
            u32 _j = _i;
            while(_j < _end && _c->t[_j] != ' ') {
                _j++;
            }
            const fzr_piece _p = { _i, _j - _i, _s[_k].addr, _s[_k].addr_len, _s[_k].style, _space,
                                   _c->measure(_c->self, &_c->t[_i], _j - _i, _px, _s[_k].style) };
            rde_arr_add(&_c->pieces, (any)&_p);
            _space = false;
            _i     = _j;
        }
    }
    return (u32)rde_arr_length(&_c->pieces);
}

// A run of _len bytes from _from at (x, y): joined to the line's last if it goes on from it (the same style and
// link, one space between), else a run of its own.
RDE_INTERNAL void fzr_place(fzr_ctx* _c, const fzr_piece* _p, u32 _from, u32 _len, f32 _w, f32 _x, f32 _y, f32 _px, u32 _line_first) {
    const u32 _n = (u32)rde_arr_length(&_c->out->runs);
    if(_n > _line_first) {
        fude_zoom_rich_run* _last = &((fude_zoom_rich_run*)_c->out->runs.memory)[_n - 1u];
        const u32*          _la   = &((const u32*)_c->addr.memory)[2u * (_n - 1u)];
        if(_last->style == _p->style && _la[0] == _p->addr && _la[1] == _p->addr_len && _last->from + _last->len + 1u == _from && _c->t[_from - 1u] == ' ') {
            _last->len = _from + _len - _last->from;
            _last->w   = _x + _w - _last->x;
            return;
        }
    }
    const fude_zoom_rich_run _r = { _x, _y, _w, _px, _from, _len, _p->style };
    rde_arr_add(&_c->out->runs, (any)&_r);
    const u32 _a[2] = { _p->addr, _p->addr_len };
    rde_arr_add(&_c->addr, (any)&_a[0]);
    rde_arr_add(&_c->addr, (any)&_a[1]);
}

// A line done: its runs from _first moved to its side (_align: 0 left, 1 middle, 2 right), by the room between where
// it ends (_x_end) and _x1. Nothing moves with no room to share out.
RDE_INTERNAL void fzr_align(fzr_ctx* _c, u32 _first, f32 _x_end, f32 _x1, u8 _align) {
    if(_align == 0u || !(_x1 > 0.0f) || _x_end >= _x1) {
        return;
    }
    const f32 _shift = (_x1 - _x_end) * (_align == 1u ? 0.5f : 1.0f);
    fude_zoom_rich_run* _r = (fude_zoom_rich_run*)_c->out->runs.memory;
    for(u32 _i = _first; _i < (u32)rde_arr_length(&_c->out->runs); _i++) {
        _r[_i].x += _shift;
    }
}

// The pieces flowed from _x0 to _x1 (0: as far as they go), the first baseline at _y, lines _adv apart: placed as
// runs when _emit (else only measured). A word longer than a whole line is cut between its characters (never left
// out). How many lines; *_right how far right they went.
RDE_INTERNAL u32 fzr_flow(fzr_ctx* _c, f32 _x0, f32 _x1, f32 _y, f32 _px, f32 _adv, u8 _align, b8 _emit, f32* _right) {
    const fzr_piece* _p = (const fzr_piece*)_c->pieces.memory;
    const u32 _n = (u32)rde_arr_length(&_c->pieces);
    const b8  _wraps = _x1 > _x0;
    const f32 _fit = _x1 + (_wraps ? _px * 0.002f : 0.0f);   // (a word exactly as wide as the room fits it, whatever the sums' last bits say)
    f32 _x = _x0, _most = _x0;
    u32 _lines = 1, _first_run = (u32)rde_arr_length(&_c->out->runs);
    b8  _fresh = true;
    #define FZR_NEW_LINE() do { if(_emit) { fzr_align(_c, _first_run, _x, _x1, _align); } _most = rde_math_max_f32(_most, _x); _y += _adv; _x = _x0; _lines++; \
                                _fresh = true; _first_run = (u32)rde_arr_length(&_c->out->runs); } while(0)
    for(u32 _i = 0; _i < _n;) {
        // The word: pieces with no space between them (bold in the middle of a word) go together.
        u32 _g = _i + 1u;
        f32 _gw = _p[_i].w;
        while(_g < _n && !_p[_g].space) {
            _gw += _p[_g].w;
            _g++;
        }
        // (the space the words before it's: "Some **bold**"'s is plain)
        f32 _sp = !_fresh && _p[_i].space ? _c->measure(_c->self, " ", 1u, _px, _p[_i > 0u ? _i - 1u : _i].style) : 0.0f;
        if(!_fresh && _wraps && _x + _sp + _gw > _fit) {
            FZR_NEW_LINE();
            _sp = 0.0f;
        }
        if(_fresh && _wraps && _gw > _fit - _x0) {
            // Longer than a line: as much of it as fits on each.
            for(u32 _k = _i; _k < _g; _k++) {
                u32 _at = _p[_k].from;
                const u32 _end = _p[_k].from + _p[_k].len;
                while(_at < _end) {
                    {
                        // (not even its next character fits after what is on the line: on the next)
                        u32 _q = _at + 1u;
                        while(_q < _end && ((u8)_c->t[_q] & 0xC0u) == 0x80u) {
                            _q++;
                        }
                        if(!_fresh && _x + _c->measure(_c->self, &_c->t[_at], _q - _at, _px, _p[_k].style) > _fit) {
                            FZR_NEW_LINE();
                        }
                    }
                    u32 _cut = _at, _next = _at;
                    f32 _w = 0.0f;
                    while(_next < _end) {
                        u32 _q = _next + 1u;
                        while(_q < _end && ((u8)_c->t[_q] & 0xC0u) == 0x80u) {
                            _q++;
                        }
                        const f32 _qw = _c->measure(_c->self, &_c->t[_at], _q - _at, _px, _p[_k].style);
                        if(_x + _qw > _fit && _cut > _at) {
                            break;
                        }
                        _cut  = _q;
                        _next = _q;
                        _w    = _qw;
                        if(_x + _qw > _fit) {
                            break;   // (one character wider than the line: on it alone)
                        }
                    }
                    if(_cut == _at) {
                        break;
                    }
                    if(_emit) {
                        fzr_place(_c, &_p[_k], _at, _cut - _at, _w, _x, _y, _px, _first_run);
                    }
                    _x     += _w;
                    _fresh  = false;
                    _at     = _cut;
                    if(_at < _end) {
                        FZR_NEW_LINE();
                    }
                }
            }
            _i = _g;
            continue;
        }
        _x += _sp;
        for(u32 _k = _i; _k < _g; _k++) {
            if(_emit) {
                fzr_place(_c, &_p[_k], _p[_k].from, _p[_k].len, _p[_k].w, _x, _y, _px, _first_run);
            }
            _x += _p[_k].w;
        }
        _fresh = false;
        _i = _g;
    }
    if(_emit) {
        fzr_align(_c, _first_run, _x, _x1, _align);
    }
    #undef FZR_NEW_LINE
    *_right = rde_math_max_f32(_most, _x);
    return _lines;
}

// Adds a mark to the layout.
RDE_INTERNAL void fzr_mark(fude_zoom_rich* _r, f32 _x0, f32 _y0, f32 _x1, f32 _y1, u8 _kind, u8 _soft) {
    const fude_zoom_rich_mark _m = { _x0, _y0, _x1, _y1, _kind, _soft };
    rde_arr_add(&_r->marks, (any)&_m);
}

// Adds a hit to the layout.
RDE_INTERNAL void fzr_hit(fude_zoom_rich* _r, f32 _x0, f32 _y0, f32 _x1, f32 _y1, u32 _at, u32 _len, u8 _kind) {
    const fude_zoom_rich_hit _h = { _x0, _y0, _x1, _y1, _at, _len, _kind };
    rde_arr_add(&_r->hits, (any)&_h);
}

// --- tables ---------------------------------------------------------------------------------------------

// A table row's cells: each one's start and end (its spaces round it left out). How many.
RDE_INTERNAL u32 fzr_cells(const c8* _t, const fzr_line* _l, u32* _from, u32* _to, u32 _max) {
    u32 _end = _l->to;
    while(_end > _l->body && (_t[_end - 1u] == ' ' || _t[_end - 1u] == '\r')) {
        _end--;
    }
    if(_end > _l->body + 1u && _t[_end - 1u] == '|') {
        _end--;   // (its closing bar)
    }
    u32 _n = 0, _i = _l->body + 1u;
    while(_i <= _end && _n < _max) {
        u32 _j = _i;
        while(_j < _end && _t[_j] != '|') {
            _j++;
        }
        u32 _a = _i, _b = _j;
        while(_a < _b && _t[_a] == ' ') {
            _a++;
        }
        while(_b > _a && _t[_b - 1u] == ' ') {
            _b--;
        }
        _from[_n] = _a;
        _to[_n]   = _b;
        _n++;
        _i = _j + 1u;
    }
    return _n;
}

// Is this row only a header's line (- : and spaces)? Its columns' sides into _align (0 left, 1 middle, 2 right).
RDE_INTERNAL b8 fzr_header_line(const c8* _t, const u32* _from, const u32* _to, u32 _n, u8* _align) {
    if(_n == 0u) {
        return false;
    }
    for(u32 _c = 0; _c < _n; _c++) {
        b8 _dash = false;
        for(u32 _i = _from[_c]; _i < _to[_c]; _i++) {
            if(_t[_i] != '-' && _t[_i] != ':' && _t[_i] != ' ') {
                return false;
            }
            _dash = _dash || _t[_i] == '-';
        }
        if(!_dash) {
            return false;
        }
        if(_align != NULL && _c < FZR_COLUMNS) {
            const b8 _l = _t[_from[_c]] == ':', _r = _to[_c] > _from[_c] && _t[_to[_c] - 1u] == ':';
            _align[_c] = _l && _r ? 1u : _r ? 2u : 0u;
        }
    }
    return true;
}

// The table of rows _rows (_n of them) from _y down: its cells' words, its lines, its header's ground. How tall; its
// right edge in *_right. Each column is as wide as its widest cell, all shrunk alike when they do not fit _width; a
// row of only - : | is the header's line, read for the columns' sides and not drawn.
RDE_INTERNAL f32 fzr_table(fzr_ctx* _c, const fzr_line* _rows, u32 _n, f32 _y, f32 _width, f32 _px, f32 _line, f32* _right) {
    const c8* _t = _c->t;
    u8  _align[FZR_COLUMNS];
    rde_memset(_align, 0, sizeof(_align));
    u32 _from[FZR_COLUMNS], _to[FZR_COLUMNS];
    const b8 _header = _n >= 2u && fzr_header_line(_t, _from, _to, fzr_cells(_t, &_rows[1], _from, _to, FZR_COLUMNS), _align);
    const f32 _pad_x = FZR_CELL_X * _px, _pad_y = FZR_CELL_Y * _px, _adv = _px * _line;
    const f32 _x0 = (f32)_rows[0].level * FZR_INDENT * _px;
    // Each column as wide as its widest cell, its room round it.
    f32 _nat[FZR_COLUMNS];
    u32 _cols = 0;
    for(u32 _k = 0; _k < FZR_COLUMNS; _k++) {
        _nat[_k] = 2.0f * _pad_x + _px;
    }
    for(u32 _r = 0; _r < _n; _r++) {
        const u32 _nc = fzr_cells(_t, &_rows[_r], _from, _to, FZR_COLUMNS);
        if(fzr_header_line(_t, _from, _to, _nc, NULL)) {
            continue;
        }
        _cols = _nc > _cols ? _nc : _cols;
        for(u32 _k = 0; _k < _nc; _k++) {
            rde_arr_clear(&_c->segs);
            fzr_inline(&_c->segs, _t, _from[_k], _to[_k], _header && _r == 0u ? FUDE_ZOOM_RICH_BOLD : 0u, 0u, 0u, 0u, false);
            fzr_pieces(_c, _px);
            f32 _w = 0.0f;
            fzr_flow(_c, 0.0f, 0.0f, 0.0f, _px, _adv, 0u, false, &_w);
            _nat[_k] = rde_math_max_f32(_nat[_k], _w + 2.0f * _pad_x);
        }
    }
    if(_cols == 0u) {
        *_right = _x0;
        return 0.0f;
    }
    // Narrower where there is not room for it: its columns shrunk alike, their words wrapped.
    f32 _cw[FZR_COLUMNS], _total = 0.0f;
    for(u32 _k = 0; _k < _cols; _k++) {
        _cw[_k] = _nat[_k];
        _total += _nat[_k];
    }
    if(_width > 0.0f && _x0 + _total > _width) {
        const f32 _room = rde_math_max_f32(_width - _x0, (f32)_cols * (2.0f * _pad_x + _px));
        f32 _sum = 0.0f;
        for(u32 _k = 0; _k < _cols; _k++) {
            _cw[_k] = rde_math_max_f32(_nat[_k] * _room / _total, 2.0f * _pad_x + _px);
            _sum   += _cw[_k];
        }
        _total = _sum;
    }
    // Its rows, a line over each (the header's under it as strong as its sides, the rest fainter).
    f32 _top = _y;
    b8  _first = true;
    for(u32 _r = 0; _r < _n; _r++) {
        const u32 _nc = fzr_cells(_t, &_rows[_r], _from, _to, FZR_COLUMNS);
        if(fzr_header_line(_t, _from, _to, _nc, NULL)) {
            continue;
        }
        const b8 _head = _header && _r == 0u;
        fzr_mark(_c->out, _x0, _top, _x0 + _total, _top, FUDE_ZOOM_RICH_MARK_LINE, _first || (_header && _r == 2u) ? 0u : 1u);
        _first = false;
        const u32 _ground = (u32)rde_arr_length(&_c->out->marks);
        if(_head) {
            fzr_mark(_c->out, _x0, _top, _x0 + _total, _top, FUDE_ZOOM_RICH_MARK_GROUND, 1u);   // (its height once known)
        }
        u32 _most = 1;
        f32 _cx = _x0;
        for(u32 _k = 0; _k < _cols; _k++) {
            if(_k < _nc) {
                rde_arr_clear(&_c->segs);
                fzr_inline(&_c->segs, _t, _from[_k], _to[_k], _head ? FUDE_ZOOM_RICH_BOLD : 0u, 0u, 0u, 0u, false);
                fzr_pieces(_c, _px);
                f32 _w = 0.0f;
                const u32 _l = fzr_flow(_c, _cx + _pad_x, _cx + _cw[_k] - _pad_x, _top + _pad_y + _px * 0.95f, _px, _adv, _align[_k], true, &_w);
                _most = _l > _most ? _l : _most;
            }
            _cx += _cw[_k];
        }
        const f32 _h = (f32)_most * _adv + 2.0f * _pad_y;
        if(_head) {
            ((fude_zoom_rich_mark*)_c->out->marks.memory)[_ground].y1 = _top + _h;
        }
        _top += _h;
    }
    fzr_mark(_c->out, _x0, _top, _x0 + _total, _top, FUDE_ZOOM_RICH_MARK_LINE, 0u);
    f32 _cx = _x0;
    for(u32 _k = 0; _k <= _cols; _k++) {
        fzr_mark(_c->out, _cx, _y, _cx, _top, FUDE_ZOOM_RICH_MARK_LINE, _k == 0u || _k == _cols ? 0u : 1u);
        _cx += _k < _cols ? _cw[_k] : 0.0f;
    }
    *_right = _x0 + _total;
    return _top - _y;
}

// --- the whole text ---------------------------------------------------------------------------------------

// See the header.
void fude_zoom_rich_init(fude_zoom_rich* _r) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _r->runs  = rde_arr_new(sizeof(fude_zoom_rich_run), _heap);
    _r->marks = rde_arr_new(sizeof(fude_zoom_rich_mark), _heap);
    _r->hits  = rde_arr_new(sizeof(fude_zoom_rich_hit), _heap);
    _r->lines = rde_arr_new(sizeof(fude_zoom_rich_line), _heap);
    _r->w = _r->h = 0.0f;
    _r->line = 1.3f;
}

// See the header.
void fude_zoom_rich_free(fude_zoom_rich* _r) {
    rde_arr_free(&_r->runs);
    rde_arr_free(&_r->marks);
    rde_arr_free(&_r->hits);
    rde_arr_free(&_r->lines);
}

// See the header: left-aligned.
void fude_zoom_rich_lay(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, fude_zoom_rich_measure_fn _measure, any _self) {
    fude_zoom_rich_lay_aligned(_r, _text, _len, _width, _px, _line, 0u, _measure, _self);
}

// See the header. Each line is read and laid out under the one before: a table takes its rows (64 at most; more
// start a table of their own), a rule's mark is drawn across the whole width once it is known, and every run's own
// marks and a link's hit are added at the end. c.addr keeps two u32s for each run: its link's address.
void fude_zoom_rich_lay_aligned(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, u8 _align, fude_zoom_rich_measure_fn _measure, any _self) {
    rde_arr_clear(&_r->runs);
    rde_arr_clear(&_r->marks);
    rde_arr_clear(&_r->hits);
    rde_arr_clear(&_r->lines);
    _r->w = _r->h = 0.0f;
    _r->line = _line;
    if(_text == NULL || _len == 0u || _measure == NULL || !(_px > 0.0f)) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fzr_ctx _c = { _text, _r, _measure, _self, rde_arr_new(sizeof(fzr_seg), _heap), rde_arr_new(sizeof(fzr_piece), _heap), rde_arr_new(sizeof(u32), _heap) };
    const f32 _heads[3] = FZR_HEADINGS;
    rde_arr _rules = rde_arr_new(sizeof(u32), _heap);   // (the rules' marks: across the whole once it is known)
    f32 _y = 0.0f, _most = 0.0f;
    for(u32 _i = 0; _i <= _len;) {
        u32 _e = _i;
        while(_e < _len && _text[_e] != '\n') {
            _e++;
        }
        fzr_line _l = fzr_read_line(_text, _i, _e > _i && _text[_e - 1u] == '\r' ? _e - 1u : _e);
        if(_l.kind == FZR_TABLE) {
            // The table's rows: this line and those after it.
            fzr_line _rows[64];
            u32 _n = 0, _next = _i;
            while(_next <= _len && _n < 64u) {
                u32 _f = _next;
                while(_f < _len && _text[_f] != '\n') {
                    _f++;
                }
                const fzr_line _row = fzr_read_line(_text, _next, _f > _next && _text[_f - 1u] == '\r' ? _f - 1u : _f);
                if(_row.kind != FZR_TABLE) {
                    break;
                }
                _rows[_n++] = _row;
                _next = _f + 1u;
            }
            f32 _right = 0.0f;
            _y   += fzr_table(&_c, _rows, _n, _y, _width, _px, _line, &_right);
            _most = rde_math_max_f32(_most, _right);
            _i    = _next;
            continue;
        }
        const f32 _lpx = _l.kind == FZR_HEAD ? _px * _heads[_l.level - 1u] : _px;
        const f32 _adv = _lpx * _line;
        if(_l.kind == FZR_RULE) {
            const u32 _m = (u32)rde_arr_length(&_r->marks);
            rde_arr_add(&_rules, (any)&_m);
            fzr_mark(_r, 0.0f, _y + _adv * 0.4f, 0.0f, _y + _adv * 0.4f, FUDE_ZOOM_RICH_MARK_LINE, 0u);
            _y += _adv * 0.8f;
            _i  = _e + 1u;
            continue;
        }
        const f32 _indent = _l.kind == FZR_HEAD ? 0.0f : (f32)_l.level * FZR_INDENT * _px;
        const f32 _base   = _y + _lpx * 0.95f;
        f32 _x = _indent;
        if(_l.task != FUDE_ZOOM_RICH_NONE) {
            const f32 _b = FZR_BOX * _lpx;
            fzr_mark(_r, _indent + 0.05f * _lpx, _base - _b, _indent + 0.05f * _lpx + _b, _base, _l.done ? FUDE_ZOOM_RICH_MARK_TICKED : FUDE_ZOOM_RICH_MARK_BOX, 0u);
            fzr_hit(_r, _indent - 0.3f * _lpx, _y, _indent + FZR_BOX_TO * _lpx, _y + _adv, _l.task, 3u, _l.done ? FUDE_ZOOM_RICH_HIT_DONE : FUDE_ZOOM_RICH_HIT_TASK);
            _x = _indent + FZR_BOX_TO * _lpx;
        } else if(_l.kind == FZR_BULLET) {
            const f32 _bw = _measure(_self, FUDE_ZOOM_RICH_BULLET, 3u, _lpx, 0u);
            const fude_zoom_rich_run _b = { _indent + FZR_BULLET_AT * _lpx, _base, _bw, _lpx, 0u, 3u, FUDE_ZOOM_RICH_OWN };
            rde_arr_add(&_r->runs, (any)&_b);
            const u32 _none[2] = { 0u, 0u };
            rde_arr_add(&_c.addr, (any)&_none[0]);
            rde_arr_add(&_c.addr, (any)&_none[1]);
            _x = _indent + FZR_BULLET_TO * _lpx;
        } else if(_l.kind == FZR_NUMBER) {
            const f32 _nw = _measure(_self, &_text[_l.num], _l.num_len, _lpx, 0u);
            const fude_zoom_rich_run _b = { _indent, _base, _nw, _lpx, _l.num, _l.num_len, 0u };
            rde_arr_add(&_r->runs, (any)&_b);
            const u32 _none[2] = { 0u, 0u };
            rde_arr_add(&_c.addr, (any)&_none[0]);
            rde_arr_add(&_c.addr, (any)&_none[1]);
            _x = _indent + rde_math_max_f32(_nw + 0.4f * _lpx, FZR_BULLET_TO * _lpx);
        }
        _most = rde_math_max_f32(_most, _x > _indent ? _x : 0.0f);
        {
            const fude_zoom_rich_line _rl = { _l.from, _l.to, _x, _base, _lpx };
            rde_arr_add(&_r->lines, (any)&_rl);
        }
        rde_arr_clear(&_c.segs);
        const u8 _style = (u8)((_l.kind == FZR_HEAD ? FUDE_ZOOM_RICH_BOLD : 0u) | (_l.done ? FUDE_ZOOM_RICH_SOFT | FUDE_ZOOM_RICH_STRUCK : 0u));
        fzr_inline(&_c.segs, _text, _l.body, _l.to, _style, 0u, 0u, 0u, false);
        fzr_pieces(&_c, _lpx);
        f32 _right = 0.0f;
        const b8  _side  = _width > 0.0f && (_l.kind == FZR_PARA || _l.kind == FZR_HEAD) && _l.task == FUDE_ZOOM_RICH_NONE;   // (a list from the left)
        const u32 _lines = fzr_flow(&_c, _x, _width > 0.0f ? rde_math_max_f32(_width, _x + _lpx) : 0.0f, _base, _lpx, _adv, _side ? _align : 0u, true, &_right);
        _most = rde_math_max_f32(_most, _right);
        _y   += (f32)_lines * _adv;
        _i    = _e + 1u;
    }
    // Each run's marks (a code span's ground, a strike, a link's line) and a link's hit.
    const fude_zoom_rich_run* _run = (const fude_zoom_rich_run*)_r->runs.memory;
    const u32* _addr = (const u32*)_c.addr.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_r->runs); _k++) {
        const fude_zoom_rich_run _q = _run[_k];
        if(_q.style & FUDE_ZOOM_RICH_CODE) {
            fzr_mark(_r, _q.x - 0.12f * _q.px, _q.y - 0.86f * _q.px, _q.x + _q.w + 0.12f * _q.px, _q.y + 0.26f * _q.px, FUDE_ZOOM_RICH_MARK_GROUND, 1u);
        }
        if(_q.style & FUDE_ZOOM_RICH_STRUCK) {
            fzr_mark(_r, _q.x, _q.y - 0.3f * _q.px, _q.x + _q.w, _q.y - 0.3f * _q.px, FUDE_ZOOM_RICH_MARK_LINE, 0u);
        }
        if(_q.style & FUDE_ZOOM_RICH_LINK) {
            fzr_mark(_r, _q.x, _q.y + 0.12f * _q.px, _q.x + _q.w, _q.y + 0.12f * _q.px, FUDE_ZOOM_RICH_MARK_LINE, 0u);
            fzr_hit(_r, _q.x - 0.1f * _q.px, _q.y - 0.95f * _q.px, _q.x + _q.w + 0.1f * _q.px, _q.y + 0.35f * _q.px, _addr[2u * _k], _addr[2u * _k + 1u], FUDE_ZOOM_RICH_HIT_LINK);
        }
    }
    // The rules across all of it.
    const f32 _across = _width > 0.0f ? _width : _most;
    fude_zoom_rich_mark* _m = (fude_zoom_rich_mark*)_r->marks.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_rules); _k++) {
        _m[((const u32*)_rules.memory)[_k]].x1 = _across;
    }
    _r->w = _most;
    _r->h = _y;
    rde_arr_free(&_rules);
    rde_arr_free(&_c.segs);
    rde_arr_free(&_c.pieces);
    rde_arr_free(&_c.addr);
}

// See the header: a line of any kind but plain, a task, or any style or link inside a plain line.
b8 fude_zoom_rich_marked(const c8* _text, u32 _len) {
    if(_text == NULL || _len == 0u) {
        return false;
    }
    rde_arr _segs = rde_arr_new(sizeof(fzr_seg), rde_memory_allocator_get_default_std());
    b8 _marked = false;
    for(u32 _i = 0; _i <= _len && !_marked;) {
        u32 _e = _i;
        while(_e < _len && _text[_e] != '\n') {
            _e++;
        }
        const fzr_line _l = fzr_read_line(_text, _i, _e);
        if(_l.kind != FZR_PARA || _l.task != FUDE_ZOOM_RICH_NONE) {
            _marked = true;
        } else {
            rde_arr_clear(&_segs);
            fzr_inline(&_segs, _text, _l.body, _l.to, 0u, 0u, 0u, 0u, false);
            const u32 _n = (u32)rde_arr_length(&_segs);
            _marked = _n > 1u || (_n == 1u && ((const fzr_seg*)_segs.memory)[0].style != 0u);
        }
        _i = _e + 1u;
    }
    rde_arr_free(&_segs);
    return _marked;
}

// See the header.
u32 fude_zoom_rich_hit_at(const fude_zoom_rich* _r, f32 _x, f32 _y, f32 _reach) {
    const fude_zoom_rich_hit* _h = (const fude_zoom_rich_hit*)_r->hits.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->hits); _i++) {
        if(_x >= _h[_i].x0 - _reach && _x <= _h[_i].x1 + _reach && _y >= _h[_i].y0 - _reach && _y <= _h[_i].y1 + _reach) {
            return _i;
        }
    }
    return FUDE_ZOOM_RICH_NONE;
}

// See the header.
b8 fude_zoom_rich_tick(c8* _text, u32 _len, u32 _at) {
    if(_text == NULL || _at + 2u >= _len || _text[_at] != '[' || _text[_at + 2u] != ']') {
        return false;
    }
    const c8 _c = _text[_at + 1u];
    if(_c != ' ' && _c != 'x' && _c != 'X') {
        return false;
    }
    _text[_at + 1u] = _c == ' ' ? 'x' : ' ';
    return true;
}

// See the header. A scheme is up to 11 letters, digits, + - or . before a colon.
void fude_zoom_rich_address(const c8* _text, u32 _at, u32 _len, c8* _out, usize _size) {
    if(_size == 0u) {
        return;
    }
    _out[0] = 0;
    if(_text == NULL || _len == 0u) {
        return;
    }
    // (a way named: https:, mailto:, tel:… — a scheme's letters, then a colon)
    b8 _way = false;
    for(u32 _i = 0; _i < _len && _i < 12u; _i++) {
        const c8 _c = _text[_at + _i];
        if(_c == ':') {
            _way = _i > 0u;
            break;
        }
        if(!((_c >= 'a' && _c <= 'z') || (_c >= 'A' && _c <= 'Z') || (_c >= '0' && _c <= '9') || _c == '+' || _c == '-' || _c == '.')) {
            break;
        }
    }
    rde_snprintf(_out, _size, "%s%.*s", _way ? "" : "https://", (int)_len, &_text[_at]);
}

// --- marks put in by a tap -------------------------------------------------------------------------------

// Where fude_zoom_rich_format writes: the buffer, its size, how much is written, and whether something did not fit.
typedef struct {
    c8* out;
    u32 size, at;
    b8  full;
} fzr_put;

// Appends _n bytes, keeping room for the ending 0; once they do not fit, full is set and nothing more is written.
RDE_INTERNAL void fzr_out(fzr_put* _p, const c8* _s, u32 _n) {
    if(_p->at + _n + 1u > _p->size) {
        _p->full = true;
        return;
    }
    rde_memcpy(&_p->out[_p->at], _s, _n);
    _p->at += _n;
}

// A line's own marks: where they start (after its indent) and end (its words' start), and what they are.
typedef struct {
    u32 indent, words;   // its indent's end (the marks' start); its words' start
    u8  kind;            // FZR_ (FZR_PARA: none)
    b8  task;
} fzr_marks;

// Returns the line [_from, _to)'s own marks. A table's row and a rule count as having none, so a button's marks go in
// front of them.
RDE_INTERNAL fzr_marks fzr_marks_of(const c8* _t, u32 _from, u32 _to) {
    const fzr_line _l = fzr_read_line(_t, _from, _to);
    u32 _indent = _from;
    while(_indent < _to && (_t[_indent] == ' ' || _t[_indent] == '\t')) {
        _indent++;
    }
    const fzr_marks _m = { _indent, _l.kind == FZR_TABLE || _l.kind == FZR_RULE ? _indent : _l.body, _l.kind == FZR_TABLE || _l.kind == FZR_RULE ? (u8)FZR_PARA : _l.kind,
                           _l.task != FUDE_ZOOM_RICH_NONE };
    return _m;
}

// The line buttons' work for fude_zoom_rich_format (_do one of them): each line of _text (_len bytes) that the
// selection [_from, _to) touches written out again into _put, its indent kept and its old marks replaced by _do's (or
// taken off when every one of them has them already); the selection's ends (*_sel_a, *_sel_b) moved to follow its
// words. It takes the writer and the ends under the names the function had, and hands them back.
RDE_INTERNAL void fzr_format_lines(fzr_put* _put, const c8* _text, u32 _len, u32 _from, u32 _to, u8 _do, u32* _sel_a, u32* _sel_b) {
    fzr_put _p = *_put;
    u32 _a = *_sel_a, _b = *_sel_b;
    // The lines it touches (a selection ending at a line's start: not that one).
    u32 _first = _from;
    while(_first > 0u && _text[_first - 1u] != '\n') {
        _first--;
    }
    const u32 _end = _to > _from && _text[_to - 1u] == '\n' ? _to - 1u : _to;
    // All of them as it would make them already? Then plain again.
    b8 _all = true;
    for(u32 _i = _first; _i <= _end && _i <= _len;) {
        u32 _e = _i;
        while(_e < _len && _text[_e] != '\n') {
            _e++;
        }
        const fzr_marks _m = fzr_marks_of(_text, _i, _e);
        _all = _all && (_do == FUDE_ZOOM_RICH_DO_BULLETS ? _m.kind == FZR_BULLET && !_m.task :
                        _do == FUDE_ZOOM_RICH_DO_NUMBERS ? _m.kind == FZR_NUMBER :
                        _do == FUDE_ZOOM_RICH_DO_TASKS   ? _m.task : true);
        _i = _e + 1u;
    }
    fzr_out(&_p, _text, _first);
    u32 _number = 1;
    for(u32 _i = _first; _i <= _end && _i <= _len;) {
        u32 _e = _i;
        while(_e < _len && _text[_e] != '\n') {
            _e++;
        }
        const fzr_marks _m = fzr_marks_of(_text, _i, _e);
        c8 _mark[16] = { 0 };
        if(_do == FUDE_ZOOM_RICH_DO_HEADING) {
            const u32 _next = _m.kind == FZR_HEAD ? (u32)fzr_read_line(_text, _i, _e).level + 1u : 1u;   // (### then plain)
            if(_next <= 3u) {
                rde_snprintf(_mark, sizeof(_mark), "%.*s ", (int)_next, "###");
            }
        } else if(!_all) {
            if(_do == FUDE_ZOOM_RICH_DO_BULLETS) {
                rde_snprintf(_mark, sizeof(_mark), "- ");
            } else if(_do == FUDE_ZOOM_RICH_DO_NUMBERS) {
                rde_snprintf(_mark, sizeof(_mark), "%u. ", _number++);
            } else {
                rde_snprintf(_mark, sizeof(_mark), "- [%c] ", _m.task && _text[fzr_read_line(_text, _i, _e).task + 1u] != ' ' ? 'x' : ' ');
            }
        }
        fzr_out(&_p, &_text[_i], _m.indent - _i);   // (its indent kept)
        fzr_out(&_p, _mark, (u32)rde_strlen(_mark));
        const u32 _words = _p.at;
        fzr_out(&_p, &_text[_m.words], _e - _m.words);
        // (the selection's ends on this line: by its words, or at their start if they were in its marks)
        if(_from >= _i && _from <= _e) {
            _a = _from >= _m.words ? _words + (_from - _m.words) : _words;
        }
        if(_to >= _i && _to <= _e) {
            _b = _to >= _m.words ? _words + (_to - _m.words) : _words;
        }
        if(_e < _len) {
            fzr_out(&_p, "\n", 1u);
        }
        _i = _e + 1u;
        if(_e >= _len) {
            break;
        }
    }
    // The rest after them (a selection to the next line's start: to it still).
    if(_to > _end) {
        _b = _p.at;
    }
    u32 _rest = _end;
    while(_rest < _len && _text[_rest] != '\n') {
        _rest++;
    }
    if(_rest < _len) {
        fzr_out(&_p, &_text[_rest + 1u], _len - _rest - 1u);
    }
    if(_b < _a) {
        _b = _a;
    }
    *_put = _p;
    *_sel_a = _a;
    *_sel_b = _b;
}

// See the header. Bold and italic wrap the selection (or take off the marks round it); a link puts the selection in
// square brackets with "(https://" and ")" after it, the caret where the address goes on (with no selection,
// "https://" alone); a table goes on lines of its own; the line buttons rewrite each line the selection touches, its
// indent kept and its old marks replaced (fzr_format_lines). The selection is moved to follow the words.
b8 fude_zoom_rich_format(const c8* _text, u32 _len, u32 _from, u32 _to, u8 _do, c8* _out, u32 _size, u32* _sel_from, u32* _sel_to) {
    if(_out == NULL || _size == 0u) {
        return false;
    }
    _text = _text != NULL ? _text : "";
    _from = _from > _len ? _len : _from;
    _to   = _to > _len ? _len : _to;
    if(_from > _to) {
        const u32 _x = _from;
        _from = _to;
        _to   = _x;
    }
    fzr_put _p = { _out, _size, 0u, false };
    u32 _a = _from, _b = _to;
    if(_do == FUDE_ZOOM_RICH_DO_BOLD || _do == FUDE_ZOOM_RICH_DO_ITALIC) {
        const c8* _mark = _do == FUDE_ZOOM_RICH_DO_BOLD ? "**" : "*";
        const u32 _m = (u32)rde_strlen(_mark);
        if(_from >= _m && _to + _m <= _len && rde_memcmp(&_text[_from - _m], _mark, _m) == 0 && rde_memcmp(&_text[_to], _mark, _m) == 0 &&
           (_do == FUDE_ZOOM_RICH_DO_BOLD || ((_from < _m + 1u || _text[_from - _m - 1u] != '*') && (_to + _m >= _len || _text[_to + _m] != '*')))) {
            // Already so: its marks out.
            fzr_out(&_p, _text, _from - _m);
            fzr_out(&_p, &_text[_from], _to - _from);
            fzr_out(&_p, &_text[_to + _m], _len - _to - _m);
            _a = _from - _m;
            _b = _to - _m;
        } else {
            fzr_out(&_p, _text, _from);
            fzr_out(&_p, _mark, _m);
            fzr_out(&_p, &_text[_from], _to - _from);
            fzr_out(&_p, _mark, _m);
            fzr_out(&_p, &_text[_to], _len - _to);
            _a = _from + _m;
            _b = _to + _m;
        }
    } else if(_do == FUDE_ZOOM_RICH_DO_LINK) {
        fzr_out(&_p, _text, _from);
        if(_to > _from) {
            fzr_out(&_p, "[", 1u);
            fzr_out(&_p, &_text[_from], _to - _from);
            fzr_out(&_p, "](https://", 10u);
            _a = _b = _p.at;
            fzr_out(&_p, ")", 1u);
        } else {
            fzr_out(&_p, "https://", 8u);
            _a = _b = _p.at;
        }
        fzr_out(&_p, &_text[_to], _len - _to);
    } else if(_do == FUDE_ZOOM_RICH_DO_TABLE) {
        // On lines of its own, after the line it is on (on it, if it is empty).
        u32 _eol = _to, _bol = _to;
        while(_eol < _len && _text[_eol] != '\n') {
            _eol++;
        }
        while(_bol > 0u && _text[_bol - 1u] != '\n') {
            _bol--;
        }
        fzr_out(&_p, _text, _eol);
        if(_eol > _bol) {
            fzr_out(&_p, "\n", 1u);
        }
        _a = _b = _p.at + 2u;
        static const c8 _table[] = "|  |  |\n|---|---|\n|  |  |";
        fzr_out(&_p, _table, (u32)sizeof(_table) - 1u);
        fzr_out(&_p, &_text[_eol], _len - _eol);
    } else if(_do < FUDE_ZOOM_RICH_DOS) {
        fzr_format_lines(&_p, _text, _len, _from, _to, _do, &_a, &_b);
    } else {
        fzr_out(&_p, _text, _len);
    }
    _out[_p.full ? 0u : _p.at] = 0;
    if(_sel_from != NULL) {
        *_sel_from = _a;
    }
    if(_sel_to != NULL) {
        *_sel_to = _b;
    }
    return !_p.full;
}

// --- as it is being written --------------------------------------------------------------------------

// See the header. Each line is one flow: its indent measured as its spaces, its own marks a faint segment, then its
// words with their span marks kept (fzr_inline with _keep).
void fude_zoom_rich_lay_raw(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, fude_zoom_rich_measure_fn _measure, any _self) {
    rde_arr_clear(&_r->runs);
    rde_arr_clear(&_r->marks);
    rde_arr_clear(&_r->hits);
    rde_arr_clear(&_r->lines);
    _r->w = _r->h = 0.0f;
    _r->line = _line;
    if(_measure == NULL || !(_px > 0.0f)) {
        return;
    }
    _text = _text != NULL ? _text : "";
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fzr_ctx _c = { _text, _r, _measure, _self, rde_arr_new(sizeof(fzr_seg), _heap), rde_arr_new(sizeof(fzr_piece), _heap), rde_arr_new(sizeof(u32), _heap) };
    const f32 _heads[3] = FZR_HEADINGS;
    f32 _y = 0.0f, _most = 0.0f;
    for(u32 _i = 0; _i <= _len;) {
        u32 _e = _i;
        while(_e < _len && _text[_e] != '\n') {
            _e++;
        }
        const fzr_line _l = fzr_read_line(_text, _i, _e);
        const b8  _head = _l.kind == FZR_HEAD;
        const f32 _lpx  = _head ? _px * _heads[_l.level - 1u] : _px;
        const f32 _adv  = _lpx * _line;
        const f32 _base = _y + _lpx * 0.95f;
        // Its indent kept (as wide as its spaces), its own marks faint, then its words as they will look.
        u32 _a = _i;
        while(_a < _e && (_text[_a] == ' ' || _text[_a] == '\t')) {
            _a++;
        }
        const f32 _x0 = _a > _i ? _measure(_self, &_text[_i], _a - _i, _lpx, 0u) : 0.0f;
        const b8  _plain_line = _l.kind == FZR_TABLE || _l.kind == FZR_RULE;
        const u32 _body = _plain_line ? _a : _l.body;
        rde_arr_clear(&_c.segs);
        if(_body > _a) {
            fzr_push(&_c.segs, _a, _body - _a, FUDE_ZOOM_RICH_MARK, 0u, 0u);
        }
        const u8 _style = (u8)((_head ? FUDE_ZOOM_RICH_BOLD : 0u) | (_l.done ? FUDE_ZOOM_RICH_SOFT | FUDE_ZOOM_RICH_STRUCK : 0u));
        fzr_inline(&_c.segs, _text, _body, _e, _style, 0u, 0u, 0u, true);
        fzr_pieces(&_c, _lpx);
        {
            const fude_zoom_rich_line _rl = { _i, _e, _x0, _base, _lpx };
            rde_arr_add(&_r->lines, (any)&_rl);
        }
        f32 _right = 0.0f;
        const u32 _lines = fzr_flow(&_c, _x0, _width > 0.0f ? rde_math_max_f32(_width, _x0 + _lpx) : 0.0f, _base, _lpx, _adv, 0u, true, &_right);
        _most = rde_math_max_f32(_most, _right);
        _y   += (f32)_lines * _adv;
        _i    = _e + 1u;
    }
    _r->w = _most;
    _r->h = _y;
    rde_arr_free(&_c.segs);
    rde_arr_free(&_c.pieces);
    rde_arr_free(&_c.addr);
}

// The line byte _at is in (its end: that line's).
RDE_INTERNAL u32 fzr_line_of(const fude_zoom_rich* _r, u32 _at) {
    const fude_zoom_rich_line* _l = (const fude_zoom_rich_line*)_r->lines.memory;
    const u32 _n = (u32)rde_arr_length(&_r->lines);
    for(u32 _i = 0; _i < _n; _i++) {
        if(_at <= _l[_i].to) {
            return _i;
        }
    }
    return _n > 0u ? _n - 1u : FUDE_ZOOM_RICH_NONE;
}

// See the header. In a line's indent, as far in as the spaces before it; else by the last run of its line that starts
// at or before it (past that run's end, a space's width for each byte after it).
void fude_zoom_rich_caret(const fude_zoom_rich* _r, const c8* _text, u32 _len, u32 _at, fude_zoom_rich_measure_fn _measure, any _self,
                          f32* _x, f32* _y, f32* _px) {
    *_x = 0.0f;
    *_y = 0.0f;
    *_px = 0.0f;
    _at = _at > _len ? _len : _at;
    const u32 _li = fzr_line_of(_r, _at);
    if(_li == FUDE_ZOOM_RICH_NONE) {
        return;
    }
    const fude_zoom_rich_line _l = ((const fude_zoom_rich_line*)_r->lines.memory)[_li];
    *_x = _l.x;
    *_y = _l.y;
    *_px = _l.px;
    // (in its indent: as far in as the spaces before it)
    u32 _a = _l.from;
    while(_a < _l.to && (_text[_a] == ' ' || _text[_a] == '\t')) {
        _a++;
    }
    if(_at <= _a) {
        *_x = _at > _l.from ? _measure(_self, &_text[_l.from], _at - _l.from, _l.px, 0u) : 0.0f;
        return;
    }
    // The last run of its line starting at or before it: in it, or just past it (a space, a line's end).
    const fude_zoom_rich_run* _q = (const fude_zoom_rich_run*)_r->runs.memory;
    const u32 _n = (u32)rde_arr_length(&_r->runs);
    u32 _best = FUDE_ZOOM_RICH_NONE;
    for(u32 _k = 0; _k < _n; _k++) {
        if(_q[_k].from >= _l.from && _q[_k].from <= _l.to && _q[_k].from <= _at && !(_q[_k].style & FUDE_ZOOM_RICH_OWN)) {
            _best = _k;
        }
    }
    if(_best == FUDE_ZOOM_RICH_NONE) {
        return;
    }
    const fude_zoom_rich_run _b = _q[_best];
    *_y  = _b.y;
    *_px = _b.px;
    if(_at <= _b.from + _b.len) {
        *_x = _b.x + _measure(_self, &_text[_b.from], _at - _b.from, _b.px, _b.style);
        return;
    }
    // (after it: its spaces)
    *_x = _b.x + _b.w + _measure(_self, " ", 1u, _b.px, (u8)(_b.style & (FUDE_ZOOM_RICH_BOLD | FUDE_ZOOM_RICH_ITALIC))) * (f32)(_at - _b.from - _b.len);
}

// See the header. The row is the nearest of the runs' and the empty lines' bands; then the place on it.
u32 fude_zoom_rich_byte_at(const fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _x, f32 _y, fude_zoom_rich_measure_fn _measure, any _self) {
    const fude_zoom_rich_run*  _q = (const fude_zoom_rich_run*)_r->runs.memory;
    const fude_zoom_rich_line* _l = (const fude_zoom_rich_line*)_r->lines.memory;
    const u32 _nq = (u32)rde_arr_length(&_r->runs), _nl = (u32)rde_arr_length(&_r->lines);
    if(_nl == 0u) {
        return 0u;
    }
    // The row it is on: each run's baseline's band, and each line with no words's — the nearest.
    f32 _row = _l[0].y, _gap = 1e30f;
    #define FZR_ROW(_base, _p) do { const f32 _top = (_base) - (_p) * 0.95f, _bot = _top + (_p) * _r->line; \
                                    const f32 _d = _y < _top ? _top - _y : _y > _bot ? _y - _bot : 0.0f; \
                                    if(_d < _gap) { _gap = _d; _row = (_base); } } while(0)
    for(u32 _k = 0; _k < _nq; _k++) {
        FZR_ROW(_q[_k].y, _q[_k].px);
    }
    for(u32 _i = 0; _i < _nl; _i++) {
        FZR_ROW(_l[_i].y, _l[_i].px);
    }
    #undef FZR_ROW
    // Its runs, left to right.
    u32 _first = FUDE_ZOOM_RICH_NONE, _last = FUDE_ZOOM_RICH_NONE;
    for(u32 _k = 0; _k < _nq; _k++) {
        if(rde_math_abs_f32(_q[_k].y - _row) < 0.01f && !(_q[_k].style & FUDE_ZOOM_RICH_OWN)) {
            _first = _first == FUDE_ZOOM_RICH_NONE ? _k : _first;
            _last  = _k;
        }
    }
    if(_first == FUDE_ZOOM_RICH_NONE) {
        // (a line with no words: its start — or as far into its spaces as it is)
        for(u32 _i = 0; _i < _nl; _i++) {
            if(rde_math_abs_f32(_l[_i].y - _row) < 0.01f) {
                u32 _at = _l[_i].from;
                while(_at < _l[_i].to && _measure(_self, &_text[_l[_i].from], _at + 1u - _l[_i].from, _l[_i].px, 0u) <= _x) {
                    _at++;
                }
                return _at;
            }
        }
        return _len;
    }
    if(_x <= _q[_first].x) {
        // (before its first word: its start, its spaces in front of it too if it is a line's first row)
        const u32 _li = fzr_line_of(_r, _q[_first].from);
        if(_li != FUDE_ZOOM_RICH_NONE && rde_math_abs_f32(_l[_li].y - _row) < 0.01f) {
            u32 _at = _l[_li].from;
            while(_at < _q[_first].from && _measure(_self, &_text[_l[_li].from], _at + 1u - _l[_li].from, _l[_li].px, 0u) <= _x) {
                _at++;
            }
            return _at;
        }
        return _q[_first].from;
    }
    for(u32 _k = _first; _k <= _last; _k++) {
        const fude_zoom_rich_run _b = _q[_k];
        if(rde_math_abs_f32(_b.y - _row) >= 0.01f || (_b.style & FUDE_ZOOM_RICH_OWN)) {
            continue;
        }
        if(_x <= _b.x + _b.w) {
            // In it: the nearest place between its characters.
            u32 _best = _b.from;
            f32 _bd = rde_math_abs_f32(_x - _b.x);
            for(u32 _i = _b.from + 1u; _i <= _b.from + _b.len; _i++) {
                if(_i < _b.from + _b.len && ((u8)_text[_i] & 0xC0u) == 0x80u) {
                    continue;
                }
                const f32 _cx = _b.x + _measure(_self, &_text[_b.from], _i - _b.from, _b.px, _b.style);
                if(rde_math_abs_f32(_x - _cx) < _bd) {
                    _bd   = rde_math_abs_f32(_x - _cx);
                    _best = _i;
                }
            }
            return _best;
        }
        // Between it and the next on the row: whichever is nearer.
        u32 _next = FUDE_ZOOM_RICH_NONE;
        for(u32 _m = _k + 1u; _m <= _last; _m++) {
            if(rde_math_abs_f32(_q[_m].y - _row) < 0.01f && !(_q[_m].style & FUDE_ZOOM_RICH_OWN)) {
                _next = _m;
                break;
            }
        }
        if(_next != FUDE_ZOOM_RICH_NONE && _x < _q[_next].x) {
            return _x - (_b.x + _b.w) < _q[_next].x - _x ? _b.from + _b.len : _q[_next].from;
        }
    }
    // Past its last word: the row's end (the line's end, if this is its last row).
    const fude_zoom_rich_run _b = _q[_last];
    const u32 _li = fzr_line_of(_r, _b.from);
    b8 _more = false;   // (its line goes on, on a row under this one)
    for(u32 _k = _last + 1u; _li != FUDE_ZOOM_RICH_NONE && _k < _nq; _k++) {
        _more = _more || (_q[_k].from >= _l[_li].from && _q[_k].from <= _l[_li].to && !(_q[_k].style & FUDE_ZOOM_RICH_OWN));
    }
    if(_li != FUDE_ZOOM_RICH_NONE && !_more) {
        return _l[_li].to;
    }
    return _b.from + _b.len;
}
