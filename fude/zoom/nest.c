// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/nest.h"

// ===========================================================================
// See nest.h.
//
// fude_zoom_nest runs fude_zoom_nest_try once for each order (fude_zoom_nest_key) and keeps the best; a try places the
// parts one by one, MaxRects with Best Short Side Fit, through fude_zoom_nest_take. The free rectangles are an
// rde_arr rebuilt on every part placed.
// ===========================================================================

// A rectangle on the board: its lower left corner and its size, in the board's units.
typedef struct {
    f64 x, y, w, h;
} fude_zoom_nest_rect;

// How many orders the parts are tried in (fude_zoom_nest_key's cases).
#define FUDE_ZOOM_NEST_ORDERS 6u
// The slack every comparison of lengths allows, so rounding never makes a part that fits exactly fail to fit.
#define FUDE_ZOOM_NEST_EPS    1e-9

// Returns part _p's key in order _order (0 area, 1 longer side, 2 shorter side, 3 half the perimeter, 4 width,
// 5 height); the parts are tried with the larger keys first.
RDE_INTERNAL f64 fude_zoom_nest_key(const fude_zoom_nest_part* _p, u32 _order) {
    switch(_order) {
        case 0:  return _p->w * _p->h;
        case 1:  return rde_math_max_f64(_p->w, _p->h);
        case 2:  return rde_math_min_f64(_p->w, _p->h);
        case 3:  return _p->w + _p->h;
        case 4:  return _p->w;
        default: return _p->h;
    }
}

// One part's place in a sort: its key, its area to break ties, and its index in the caller's array.
typedef struct {
    f64 key, second;
    u32 index;
} fude_zoom_nest_sort;

// rde_sort's comparison: the larger key first, then the larger area, then the lower index, so every sort comes out the
// same however the sort itself orders equal items.
RDE_INTERNAL int fude_zoom_nest_by_key(const any _a, const any _b) {
    const fude_zoom_nest_sort* _x = (const fude_zoom_nest_sort*)_a;
    const fude_zoom_nest_sort* _y = (const fude_zoom_nest_sort*)_b;
    if(_x->key != _y->key) {
        return _x->key > _y->key ? -1 : 1;
    }
    if(_x->second != _y->second) {
        return _x->second > _y->second ? -1 : 1;
    }
    return _x->index < _y->index ? -1 : (_x->index > _y->index ? 1 : 0);
}

// True when rectangle _a lies inside _b, within FUDE_ZOOM_NEST_EPS.
RDE_INTERNAL b8 fude_zoom_nest_inside(const fude_zoom_nest_rect* _a, const fude_zoom_nest_rect* _b) {
    return _a->x >= _b->x - FUDE_ZOOM_NEST_EPS && _a->y >= _b->y - FUDE_ZOOM_NEST_EPS &&
           _a->x + _a->w <= _b->x + _b->w + FUDE_ZOOM_NEST_EPS && _a->y + _a->h <= _b->y + _b->h + FUDE_ZOOM_NEST_EPS;
}

// Takes rectangle _used out of the free rectangles: each free one it overlaps is split into the parts of it left on
// each side (left, right, below, above; they overlap one another, as MaxRects keeps them), and the others are kept
// as they are. Then any free rectangle inside another is dropped, and of two the same, the later one.
RDE_INTERNAL void fude_zoom_nest_take(rde_arr* _free, fude_zoom_nest_rect _used) {
    rde_arr _next = rde_arr_new(sizeof(fude_zoom_nest_rect), rde_memory_allocator_get_default_std());
    const fude_zoom_nest_rect* _f = (const fude_zoom_nest_rect*)_free->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_free); _i++) {
        const fude_zoom_nest_rect _r = _f[_i];
        if(_used.x >= _r.x + _r.w - FUDE_ZOOM_NEST_EPS || _used.x + _used.w <= _r.x + FUDE_ZOOM_NEST_EPS ||
           _used.y >= _r.y + _r.h - FUDE_ZOOM_NEST_EPS || _used.y + _used.h <= _r.y + FUDE_ZOOM_NEST_EPS) {
            rde_arr_add(&_next, (any)&_r);
            continue;
        }
        if(_used.x > _r.x + FUDE_ZOOM_NEST_EPS) {
            const fude_zoom_nest_rect _left = { _r.x, _r.y, _used.x - _r.x, _r.h };
            rde_arr_add(&_next, (any)&_left);
        }
        if(_used.x + _used.w < _r.x + _r.w - FUDE_ZOOM_NEST_EPS) {
            const fude_zoom_nest_rect _right = { _used.x + _used.w, _r.y, _r.x + _r.w - (_used.x + _used.w), _r.h };
            rde_arr_add(&_next, (any)&_right);
        }
        if(_used.y > _r.y + FUDE_ZOOM_NEST_EPS) {
            const fude_zoom_nest_rect _below = { _r.x, _r.y, _r.w, _used.y - _r.y };
            rde_arr_add(&_next, (any)&_below);
        }
        if(_used.y + _used.h < _r.y + _r.h - FUDE_ZOOM_NEST_EPS) {
            const fude_zoom_nest_rect _above = { _r.x, _used.y + _used.h, _r.w, _r.y + _r.h - (_used.y + _used.h) };
            rde_arr_add(&_next, (any)&_above);
        }
    }
    // Those inside another: gone (the same one twice: once).
    fude_zoom_nest_rect* _n = (fude_zoom_nest_rect*)_next.memory;
    u32 _count = (u32)rde_arr_length(&_next);
    rde_arr_clear(_free);
    for(u32 _i = 0; _i < _count; _i++) {
        b8 _inside = false;
        for(u32 _j = 0; _j < _count && !_inside; _j++) {
            if(_i != _j && fude_zoom_nest_inside(&_n[_i], &_n[_j]) && (!fude_zoom_nest_inside(&_n[_j], &_n[_i]) || _j < _i)) {
                _inside = true;
            }
        }
        if(!_inside) {
            rde_arr_add(_free, (any)&_n[_i]);
        }
    }
    rde_arr_free(&_next);
}

// Places the parts in order _order on a board _bw by _bh and writes each part's place into _out (indexed as _parts).
// Returns the area of the parts placed. A part with no area is skipped, unplaced. Each part goes into the free
// rectangle that leaves the least over on its shorter side, then on its longer side, either way round if it can turn.
// One order's go: how much wood it put down, its places into _out.
RDE_INTERNAL f64 fude_zoom_nest_try(f64 _bw, f64 _bh, f64 _kerf, const fude_zoom_nest_part* _parts, const u32* _order, u32 _n, fude_zoom_nest_place* _out) {
    rde_arr _free = rde_arr_new(sizeof(fude_zoom_nest_rect), rde_memory_allocator_get_default_std());
    // The board and each part grown by the kerf: neighbours a kerf apart, the board's edges none.
    const fude_zoom_nest_rect _all = { 0.0, 0.0, _bw + _kerf, _bh + _kerf };
    rde_arr_add(&_free, (any)&_all);
    f64 _area = 0.0;
    for(u32 _k = 0; _k < _n; _k++) {
        const u32 _i = _order[_k];
        const fude_zoom_nest_part* _p = &_parts[_i];
        // (until it is placed: at the board's corner, its size as given, not turned; so no field is left unwritten)
        _out[_i] = (fude_zoom_nest_place){ 0.0, 0.0, _p->w, _p->h, false, false };
        if(!(_p->w > 0.0) || !(_p->h > 0.0)) {
            continue;
        }
        f64 _best_short = 1e300, _best_long = 1e300;
        fude_zoom_nest_rect _best = { 0.0, 0.0, 0.0, 0.0 };
        b8 _turned = false, _found = false;
        const fude_zoom_nest_rect* _f = (const fude_zoom_nest_rect*)_free.memory;
        for(u32 _r = 0; _r < (u32)rde_arr_length(&_free); _r++) {
            for(u32 _turn = 0; _turn < (_p->can_turn ? 2u : 1u); _turn++) {
                const f64 _w = (_turn ? _p->h : _p->w) + _kerf, _h = (_turn ? _p->w : _p->h) + _kerf;
                if(_w > _f[_r].w + FUDE_ZOOM_NEST_EPS || _h > _f[_r].h + FUDE_ZOOM_NEST_EPS) {
                    continue;
                }
                const f64 _lw = _f[_r].w - _w, _lh = _f[_r].h - _h;
                const f64 _short = rde_math_min_f64(_lw, _lh), _long = rde_math_max_f64(_lw, _lh);
                if(_short < _best_short - FUDE_ZOOM_NEST_EPS || (rde_math_abs_f64(_short - _best_short) <= FUDE_ZOOM_NEST_EPS && _long < _best_long)) {
                    _best_short = _short;
                    _best_long  = _long;
                    _best       = (fude_zoom_nest_rect){ _f[_r].x, _f[_r].y, _w, _h };
                    _turned     = _turn != 0u;
                    _found      = true;
                }
            }
        }
        if(!_found) {
            continue;
        }
        fude_zoom_nest_take(&_free, _best);
        _out[_i] = (fude_zoom_nest_place){ _best.x, _best.y, _turned ? _p->h : _p->w, _turned ? _p->w : _p->h, _turned, true };
        _area += _p->w * _p->h;
    }
    rde_arr_free(&_free);
    return _area;
}

// See the header. A negative kerf is taken as none. Every order is tried; an order replaces the best so far only when
// it puts down more area, so the first of equal orders wins.
u32 fude_zoom_nest(f64 _board_w, f64 _board_h, f64 _kerf, const fude_zoom_nest_part* _parts, u32 _n, fude_zoom_nest_place* _out) {
    if(_n == 0) {
        return 0;
    }
    _kerf = _kerf > 0.0 ? _kerf : 0.0;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _keys_arr = rde_arr_new(sizeof(fude_zoom_nest_sort), _heap), _order_arr = rde_arr_new(sizeof(u32), _heap), _try_arr = rde_arr_new(sizeof(fude_zoom_nest_place), _heap);
    rde_arr_resize(&_keys_arr, _n);
    rde_arr_resize(&_order_arr, _n);
    rde_arr_resize(&_try_arr, _n);
    fude_zoom_nest_sort*  _keys  = (fude_zoom_nest_sort*)_keys_arr.memory;   // (sized once: they stay put)
    u32*                  _order = (u32*)_order_arr.memory;
    fude_zoom_nest_place* _try   = (fude_zoom_nest_place*)_try_arr.memory;
    f64 _best = -1.0;
    for(u32 _o = 0; _o < FUDE_ZOOM_NEST_ORDERS; _o++) {
        for(u32 _i = 0; _i < _n; _i++) {
            _keys[_i] = (fude_zoom_nest_sort){ fude_zoom_nest_key(&_parts[_i], _o), _parts[_i].w * _parts[_i].h, _i };
        }
        rde_sort(_keys, _n, sizeof(fude_zoom_nest_sort), fude_zoom_nest_by_key);
        for(u32 _i = 0; _i < _n; _i++) {
            _order[_i] = _keys[_i].index;
        }
        const f64 _area = fude_zoom_nest_try(_board_w, _board_h, _kerf, _parts, _order, _n, _try);
        if(_area > _best + FUDE_ZOOM_NEST_EPS) {
            _best = _area;
            rde_memcpy(_out, _try, (usize)_n * sizeof(fude_zoom_nest_place));
        }
    }
    u32 _placed = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _placed += _out[_i].placed ? 1u : 0u;
    }
    rde_arr_free(&_keys_arr);
    rde_arr_free(&_order_arr);
    rde_arr_free(&_try_arr);
    return _placed;
}

// --- a typed list of parts, a line at a time ------------------------------------------------------------------------

// Returns whether a word is a whole number, digits only (a count: no units, no point).
RDE_INTERNAL b8 fude_zoom_nest_whole(const c8* _w) {
    if(_w[0] == 0) {
        return false;
    }
    for(const c8* _c = _w; *_c != 0; _c++) {
        if(*_c < '0' || *_c > '9') {
            return false;
        }
    }
    return true;
}

// See the header. The line is copied with its "x"s, "×"s and "*"s spaced out as words of their own (an "x" only next to
// a number, a space, an inch mark or millimetres' "m", so the x of a name is left in it), then read a word at a time:
// a number (with its units) is a size while there are fewer than two, and after them and an "x", how many; an "x" is
// only that; any other word is part of the name. A whole number then an "x" at the very start, followed by a word that
// is not a number, were how many all along ("2x Side"): when that word comes, the number is taken back from the sizes.
b8 fude_zoom_nest_read_line(const c8* _text, usize _len, FUDE_ZOOM_UNIT_ _unit, u32 _most, fude_zoom_nest_line* _out) {
    rde_memset(_out, 0, sizeof(*_out));
    _out->count = 1u;
    c8 _line[FUDE_ZOOM_NEST_LINE];
    usize _k = 0;
    for(usize _i = 0; _i < _len && _k + 4u < sizeof(_line); _i++) {
        const u8 _c = (u8)_text[_i];
        if(_c == 0xC3u && _i + 1u < _len && (u8)_text[_i + 1u] == 0x97u) {   // ×
            _line[_k++] = ' '; _line[_k++] = 'x'; _line[_k++] = ' ';
            _i++;
        } else if(_c == '*' || ((_c == 'x' || _c == 'X') && (_i == 0 || (_text[_i - 1u] >= '0' && _text[_i - 1u] <= '9') || _text[_i - 1u] == ' ' || _text[_i - 1u] == '"' || _text[_i - 1u] == 'm') &&
                                 (_i + 1u >= _len || (_text[_i + 1u] >= '0' && _text[_i + 1u] <= '9') || _text[_i + 1u] == ' ' || _text[_i + 1u] == '.'))) {
            _line[_k++] = ' '; _line[_k++] = 'x'; _line[_k++] = ' ';
        } else {
            _line[_k++] = (c8)_c;
        }
    }
    _line[_k] = 0;
    f64 _num[2] = { 0.0, 0.0 };
    u32 _nums = 0, _word = 0;   // the sizes read so far; which word this is (an "x" counted too)
    b8  _after_x = false;       // the word before was an "x"
    b8  _lead = false;          // the first word was a whole number (how many, if an "x" and a name follow it)
    i64 _lead_n = 0;
    c8* _rest = NULL;   // (where rde_strtok goes on from)
    for(c8* _w = rde_strtok(_line, " \t\r", &_rest); _w != NULL; _w = rde_strtok(NULL, " \t\r", &_rest), _word++) {
        if(rde_strcmp(_w, "x") == 0) {
            _after_x = true;
            continue;
        }
        const b8 _number = (_w[0] >= '0' && _w[0] <= '9') || _w[0] == '.';
        if(_word == 0u && fude_zoom_nest_whole(_w)) {
            _lead   = true;
            _lead_n = (i64)rde_strtol(_w, NULL, 10);
        }
        if(_number && _nums >= 2u && _after_x) {
            _out->count = (u32)rde_math_max_f64(1.0, rde_math_min_f64((f64)(i32)rde_strtol(_w, NULL, 10), (f64)_most));   // "x 2" after the sizes
            _after_x = false;
            continue;
        }
        fude_zoom_units_error _e;
        f64 _mm = 0.0;
        if(_number && _nums < 2u && fude_zoom_units_length(_w, _unit, NULL, &_mm, &_e) && _mm > 0.0) {
            _num[_nums++] = _mm;
            _after_x = false;
            continue;
        }
        if(_word == 2u && _lead && _after_x && _nums == 1u) {
            // "2x Side": the first number was how many, not a size.
            _out->count = (u32)rde_math_max_f64(1.0, rde_math_min_f64((f64)_lead_n, (f64)_most));
            _nums = 0u;
        }
        const usize _have = rde_strlen(_out->name);
        rde_snprintf(_out->name + _have, sizeof(_out->name) - _have, "%s%s", _have > 0 ? " " : "", _w);
        _after_x = false;
    }
    if(_nums != 2u) {
        rde_memset(_out, 0, sizeof(*_out));
        return false;
    }
    _out->length = rde_math_max_f64(_num[0], _num[1]);
    _out->width  = rde_math_min_f64(_num[0], _num[1]);
    return true;
}
