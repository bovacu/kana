// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/mathfont.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See mathfont.h. The OpenType tables as the specification lays them out:
// big-endian, each read checked against the file's end (a damaged font reads
// as zeros, never past its bytes).
// ===========================================================================

RDE_INTERNAL u16 fzm_u16(const fude_zoom_mfont* _f, u32 _at) {
    return _at + 2u <= _f->size ? (u16)((_f->data[_at] << 8) | _f->data[_at + 1u]) : 0u;
}
RDE_INTERNAL i16 fzm_i16(const fude_zoom_mfont* _f, u32 _at) {
    return (i16)fzm_u16(_f, _at);
}
RDE_INTERNAL u32 fzm_u32(const fude_zoom_mfont* _f, u32 _at) {
    return _at + 4u <= _f->size ? ((u32)_f->data[_at] << 24) | ((u32)_f->data[_at + 1u] << 16) | ((u32)_f->data[_at + 2u] << 8) | _f->data[_at + 3u] : 0u;
}

b8 fude_zoom_mfont_load(fude_zoom_mfont* _f, const u8* _data, u32 _size) {
    memset(_f, 0, sizeof(*_f));
    _f->data = _data;
    _f->size = _size;
    const u32 _version = fzm_u32(_f, 0u);
    if(_data == NULL || _size < 12u || (_version != 0x00010000u && _version != 0x74727565u)) {   // (1.0, or 'true')
        _f->size = 0u;
        return false;
    }
    const u16 _tables = fzm_u16(_f, 4u);
    u32 _head = 0, _hhea = 0, _maxp = 0, _cmap = 0;
    for(u32 _i = 0; _i < _tables; _i++) {
        const u32 _rec = 12u + 16u * _i;
        const u32 _tag = fzm_u32(_f, _rec), _off = fzm_u32(_f, _rec + 8u);
        if(_off >= _size) {
            continue;
        }
        switch(_tag) {
        case 0x68656164u: _head = _off; break;       // head
        case 0x68686561u: _hhea = _off; break;       // hhea
        case 0x6D617870u: _maxp = _off; break;       // maxp
        case 0x636D6170u: _cmap = _off; break;       // cmap
        case 0x6C6F6361u: _f->loca = _off; break;    // loca
        case 0x676C7966u: _f->glyf = _off; break;    // glyf
        case 0x686D7478u: _f->hmtx = _off; break;    // hmtx
        case 0x4D415448u: _f->math = _off; break;    // MATH
        default: break;
        }
    }
    if(_head == 0u || _hhea == 0u || _maxp == 0u || _cmap == 0u || _f->loca == 0u || _f->glyf == 0u || _f->hmtx == 0u) {
        _f->size = 0u;
        return false;
    }
    _f->units_per_em = fzm_u16(_f, _head + 18u);
    _f->long_loca    = fzm_i16(_f, _head + 50u) != 0;
    _f->ascender     = fzm_i16(_f, _hhea + 4u);
    _f->descender    = fzm_i16(_f, _hhea + 6u);
    _f->hmetrics     = fzm_u16(_f, _hhea + 34u);
    _f->glyphs       = fzm_u16(_f, _maxp + 4u);
    // (the character maps: a full one, format 12, and the basic plane's, format 4 — Unicode's or Windows')
    const u16 _maps = fzm_u16(_f, _cmap + 2u);
    for(u32 _i = 0; _i < _maps; _i++) {
        const u16 _platform = fzm_u16(_f, _cmap + 4u + 8u * _i), _encoding = fzm_u16(_f, _cmap + 6u + 8u * _i);
        const u32 _sub = _cmap + fzm_u32(_f, _cmap + 8u + 8u * _i);
        const u16 _format = fzm_u16(_f, _sub);
        const b8  _unicode = _platform == 0u || (_platform == 3u && (_encoding == 1u || _encoding == 10u));
        if(_unicode && _format == 12u && _f->cmap12 == 0u) {
            _f->cmap12 = _sub;
        } else if(_unicode && _format == 4u && _f->cmap4 == 0u) {
            _f->cmap4 = _sub;
        }
    }
    if(_f->math != 0u) {
        _f->constants  = _f->math + fzm_u16(_f, _f->math + 4u);
        _f->glyph_info = _f->math + fzm_u16(_f, _f->math + 6u);
        _f->variants   = _f->math + fzm_u16(_f, _f->math + 8u);
    }
    return _f->units_per_em > 0u && _f->glyphs > 0u;
}

u32 fude_zoom_mfont_glyph(const fude_zoom_mfont* _f, u32 _c) {
    if(_f->size == 0u) {
        return 0u;
    }
    if(_f->cmap12 != 0u) {
        u32 _lo = 0, _hi = fzm_u32(_f, _f->cmap12 + 12u);
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u, _g = _f->cmap12 + 16u + 12u * _mid;
            const u32 _start = fzm_u32(_f, _g), _end = fzm_u32(_f, _g + 4u);
            if(_c < _start) {
                _hi = _mid;
            } else if(_c > _end) {
                _lo = _mid + 1u;
            } else {
                return fzm_u32(_f, _g + 8u) + (_c - _start);
            }
        }
        return 0u;
    }
    if(_f->cmap4 == 0u || _c > 0xFFFFu) {
        return 0u;
    }
    const u32 _t = _f->cmap4;
    const u16 _segs2 = fzm_u16(_f, _t + 6u);
    const u32 _ends = _t + 14u, _starts = _ends + _segs2 + 2u, _deltas = _starts + _segs2, _ranges = _deltas + _segs2;
    for(u32 _s = 0; _s < _segs2; _s += 2u) {
        if(fzm_u16(_f, _ends + _s) < _c) {
            continue;
        }
        const u16 _start = fzm_u16(_f, _starts + _s);
        if(_c < _start) {
            return 0u;
        }
        const u16 _delta = fzm_u16(_f, _deltas + _s), _range = fzm_u16(_f, _ranges + _s);
        if(_range == 0u) {
            return (_c + _delta) & 0xFFFFu;
        }
        const u16 _g = fzm_u16(_f, _ranges + _s + _range + 2u * (_c - _start));
        return _g != 0u ? (u32)((_g + _delta) & 0xFFFFu) : 0u;
    }
    return 0u;
}

i32 fude_zoom_mfont_advance(const fude_zoom_mfont* _f, u32 _g) {
    if(_f->size == 0u || _f->hmetrics == 0u) {
        return 0;
    }
    const u32 _k = _g < _f->hmetrics ? _g : _f->hmetrics - 1u;
    return fzm_u16(_f, _f->hmtx + 4u * _k);
}

// Glyph _g's bytes in glyf (false: none — an empty glyph).
RDE_INTERNAL b8 fzm_glyph_at(const fude_zoom_mfont* _f, u32 _g, u32* _at, u32* _len) {
    if(_f->size == 0u || _g >= _f->glyphs) {
        return false;
    }
    u32 _a, _b;
    if(_f->long_loca) {
        _a = fzm_u32(_f, _f->loca + 4u * _g);
        _b = fzm_u32(_f, _f->loca + 4u * (_g + 1u));
    } else {
        _a = 2u * fzm_u16(_f, _f->loca + 2u * _g);
        _b = 2u * fzm_u16(_f, _f->loca + 2u * (_g + 1u));
    }
    if(_b <= _a || _f->glyf + _b > _f->size) {
        return false;
    }
    *_at  = _f->glyf + _a;
    *_len = _b - _a;
    return true;
}

b8 fude_zoom_mfont_box(const fude_zoom_mfont* _f, u32 _g, i32 _box[4]) {
    u32 _at, _len;
    if(!fzm_glyph_at(_f, _g, &_at, &_len) || _len < 10u) {
        _box[0] = _box[1] = _box[2] = _box[3] = 0;
        return false;
    }
    for(u32 _k = 0; _k < 4u; _k++) {
        _box[_k] = fzm_i16(_f, _at + 2u + 2u * _k);
    }
    return true;
}

// --- outlines --------------------------------------------------------------------------------------

typedef struct {
    f64 x, y;
    b8  on;
} fzm_point;

// Glyph _g's points (through the transform _m: x' = a x + c y + e, y' = b x + d y + f), each contour's end — a
// composite's parts' in turn.
RDE_INTERNAL void fzm_points(const fude_zoom_mfont* _f, u32 _g, const f64 _m[6], u32 _depth, rde_arr* _pts, rde_arr* _ends) {
    u32 _at, _len;
    if(_depth > 8u || !fzm_glyph_at(_f, _g, &_at, &_len) || _len < 10u) {
        return;
    }
    const i16 _contours = fzm_i16(_f, _at);
    const u32 _end = _at + _len;
    if(_contours >= 0) {
        const u32 _first_end = (u32)rde_arr_length(_pts);
        u32 _p = _at + 10u;
        u32 _count = 0;
        for(i32 _c = 0; _c < _contours; _c++) {
            _count = (u32)fzm_u16(_f, _p + 2u * (u32)_c) + 1u;
        }
        _p += 2u * (u32)_contours;
        _p += 2u + fzm_u16(_f, _p);   // (its instructions: not run)
        if(_count == 0u || _count > 65535u) {
            return;
        }
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        rde_arr _flags_arr = rde_arr_new(sizeof(u8), _heap);
        rde_arr_resize(&_flags_arr, _count);
        u8* _flags = (u8*)_flags_arr.memory;
        for(u32 _i = 0; _i < _count && _p < _end;) {
            const u8 _fl = _f->data[_p++];
            u32 _repeat = 1u;
            if((_fl & 0x08u) && _p < _end) {
                _repeat += _f->data[_p++];
            }
            for(u32 _r = 0; _r < _repeat && _i < _count; _r++) {
                _flags[_i++] = _fl;
            }
        }
        rde_arr _xy_arr = rde_arr_new(sizeof(i32), _heap);
        rde_arr_resize(&_xy_arr, 2u * _count);
        i32* _xy = (i32*)_xy_arr.memory;
        for(u32 _axis = 0; _axis < 2u; _axis++) {
            const u8 _short = _axis == 0u ? 0x02u : 0x04u, _same = _axis == 0u ? 0x10u : 0x20u;
            i32 _v = 0;
            for(u32 _i = 0; _i < _count; _i++) {
                if(_flags[_i] & _short) {
                    const i32 _d = _p < _end ? _f->data[_p] : 0;
                    _p++;
                    _v += (_flags[_i] & _same) ? _d : -_d;
                } else if(!(_flags[_i] & _same)) {
                    _v += fzm_i16(_f, _p);
                    _p += 2u;
                }
                _xy[2u * _i + _axis] = _v;
            }
        }
        for(u32 _i = 0; _i < _count; _i++) {
            const f64 _x = (f64)_xy[2u * _i], _y = (f64)_xy[2u * _i + 1u];
            const fzm_point _q = { _m[0] * _x + _m[2] * _y + _m[4], _m[1] * _x + _m[3] * _y + _m[5], (_flags[_i] & 0x01u) != 0u };
            rde_arr_add(_pts, (any)&_q);
        }
        for(i32 _c = 0; _c < _contours; _c++) {
            const u32 _e = _first_end + (u32)fzm_u16(_f, _at + 10u + 2u * (u32)_c) + 1u;
            rde_arr_add(_ends, (any)&_e);
        }
        rde_arr_free(&_flags_arr);
        rde_arr_free(&_xy_arr);
        return;
    }
    // A composite: its parts, each moved (and scaled) as it says.
    u32 _p = _at + 10u;
    for(u32 _guard = 0; _guard < 64u && _p + 4u <= _end; _guard++) {
        const u16 _fl = fzm_u16(_f, _p), _part = fzm_u16(_f, _p + 2u);
        _p += 4u;
        f64 _dx, _dy;
        if(_fl & 0x0001u) {
            _dx = fzm_i16(_f, _p);
            _dy = fzm_i16(_f, _p + 2u);
            _p += 4u;
        } else {
            _dx = (i8)_f->data[_p];
            _dy = (i8)_f->data[_p + 1u];
            _p += 2u;
        }
        if(!(_fl & 0x0002u)) {
            _dx = _dy = 0.0;   // (matched points, not offsets: not done — placed where they are)
        }
        f64 _a = 1.0, _b = 0.0, _c = 0.0, _d = 1.0;
        if(_fl & 0x0008u) {
            _a = _d = fzm_i16(_f, _p) / 16384.0;
            _p += 2u;
        } else if(_fl & 0x0040u) {
            _a = fzm_i16(_f, _p) / 16384.0;
            _d = fzm_i16(_f, _p + 2u) / 16384.0;
            _p += 4u;
        } else if(_fl & 0x0080u) {
            _a = fzm_i16(_f, _p) / 16384.0;
            _b = fzm_i16(_f, _p + 2u) / 16384.0;
            _c = fzm_i16(_f, _p + 4u) / 16384.0;
            _d = fzm_i16(_f, _p + 6u) / 16384.0;
            _p += 8u;
        }
        // (the part's transform, then the composite's)
        const f64 _n[6] = { _m[0] * _a + _m[2] * _b, _m[1] * _a + _m[3] * _b, _m[0] * _c + _m[2] * _d, _m[1] * _c + _m[3] * _d,
                            _m[0] * _dx + _m[2] * _dy + _m[4], _m[1] * _dx + _m[3] * _dy + _m[5] };
        fzm_points(_f, _part, _n, _depth + 1u, _pts, _ends);
        if(!(_fl & 0x0020u)) {
            break;
        }
    }
}

RDE_INTERNAL void fzm_emit(rde_arr* _out, f64 _x, f64 _y) {
    rde_arr_add(_out, (any)&_x);
    rde_arr_add(_out, (any)&_y);
}

// A quadratic curve from _a through _c to _b, in pieces no further than _tol from it (its start left out: there already).
RDE_INTERNAL void fzm_curve(rde_arr* _out, fzm_point _a, fzm_point _c, fzm_point _b, f64 _tol) {
    const f64 _ddx = _a.x - 2.0 * _c.x + _b.x, _ddy = _a.y - 2.0 * _c.y + _b.y;
    const f64 _bend = sqrt(_ddx * _ddx + _ddy * _ddy);
    u32 _n = (u32)ceil(sqrt(_bend / (4.0 * fmax(_tol, 1e-6))));
    _n = _n < 1u ? 1u : (_n > 64u ? 64u : _n);
    for(u32 _i = 1; _i <= _n; _i++) {
        const f64 _t = (f64)_i / (f64)_n, _u = 1.0 - _t;
        fzm_emit(_out, _u * _u * _a.x + 2.0 * _u * _t * _c.x + _t * _t * _b.x, _u * _u * _a.y + 2.0 * _u * _t * _c.y + _t * _t * _b.y);
    }
}

u32 fude_zoom_mfont_outline(const fude_zoom_mfont* _f, u32 _g, f64 _tol, rde_arr* _points, rde_arr* _ends) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fzm_point), _heap), _cends = rde_arr_new(sizeof(u32), _heap);
    const f64 _identity[6] = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
    fzm_points(_f, _g, _identity, 0u, &_pts, &_cends);
    const fzm_point* _p = (const fzm_point*)_pts.memory;
    const u32 _base = (u32)(rde_arr_length(_points) / 2u);
    u32 _contours = 0, _from = 0;
    for(u32 _c = 0; _c < (u32)rde_arr_length(&_cends); _c++) {
        const u32 _to = ((const u32*)_cends.memory)[_c];
        const u32 _n = _to > _from && _to <= (u32)rde_arr_length(&_pts) ? _to - _from : 0u;
        if(_n < 2u) {
            _from = _to;
            continue;
        }
        const fzm_point* _q = &_p[_from];
        // (its start: its first point on the curve, the rest after it in turn; all off the curve, the middle between its
        // last and its first — every one of them then after it)
        u32 _s = 0;
        while(_s < _n && !_q[_s].on) {
            _s++;
        }
        const b8 _any_on = _s < _n;
        const fzm_point _start = _any_on ? _q[_s] : (fzm_point){ 0.5 * (_q[_n - 1u].x + _q[0].x), 0.5 * (_q[_n - 1u].y + _q[0].y), true };
        const u32 _steps = _any_on ? _n - 1u : _n;
        const u32 _before = (u32)rde_arr_length(_points);
        fzm_emit(_points, _start.x, _start.y);
        fzm_point _pen = _start, _ctrl = { 0.0, 0.0, false };
        b8 _have_ctrl = false;
        for(u32 _k = 0; _k <= _steps; _k++) {
            const fzm_point _cur = _k == _steps ? _start : _q[_any_on ? (_s + 1u + _k) % _n : _k];
            if(_cur.on) {
                if(_have_ctrl) {
                    fzm_curve(_points, _pen, _ctrl, _cur, _tol);
                    _have_ctrl = false;
                } else {
                    fzm_emit(_points, _cur.x, _cur.y);
                }
                _pen = _cur;
            } else if(_have_ctrl) {
                const fzm_point _mid = { 0.5 * (_ctrl.x + _cur.x), 0.5 * (_ctrl.y + _cur.y), true };
                fzm_curve(_points, _pen, _ctrl, _mid, _tol);
                _pen  = _mid;
                _ctrl = _cur;
            } else {
                _ctrl = _cur;
                _have_ctrl = true;
            }
        }
        // (the closing point the same as the start: left out)
        if(rde_arr_length(_points) >= _before + 4u) {
            f64* _all = (f64*)_points->memory;
            const usize _last = rde_arr_length(_points) - 2u;
            if(fabs(_all[_last] - _start.x) < 1e-9 && fabs(_all[_last + 1u] - _start.y) < 1e-9) {
                rde_arr_resize(_points, _last);
            }
        }
        const u32 _end = (u32)(rde_arr_length(_points) / 2u) - _base;
        rde_arr_add(_ends, (any)&_end);
        _contours++;
        _from = _to;
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_cends);
    return _contours;
}

// --- the MATH table --------------------------------------------------------------------------------

// Glyph _g's index in the coverage table at _at (-1: not in it).
RDE_INTERNAL i32 fzm_coverage(const fude_zoom_mfont* _f, u32 _at, u32 _g) {
    const u16 _format = fzm_u16(_f, _at);
    if(_format == 1u) {
        u32 _lo = 0, _hi = fzm_u16(_f, _at + 2u);
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u;
            const u16 _v = fzm_u16(_f, _at + 4u + 2u * _mid);
            if(_v == _g) {
                return (i32)_mid;
            }
            if(_v < _g) {
                _lo = _mid + 1u;
            } else {
                _hi = _mid;
            }
        }
    } else if(_format == 2u) {
        u32 _lo = 0, _hi = fzm_u16(_f, _at + 2u);
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u, _r = _at + 4u + 6u * _mid;
            const u16 _start = fzm_u16(_f, _r), _end = fzm_u16(_f, _r + 2u);
            if(_g < _start) {
                _hi = _mid;
            } else if(_g > _end) {
                _lo = _mid + 1u;
            } else {
                return (i32)(fzm_u16(_f, _r + 4u) + (_g - _start));
            }
        }
    }
    return -1;
}

i32 fude_zoom_mfont_constant(const fude_zoom_mfont* _f, u32 _which) {
    if(_f->math == 0u || _which >= FUDE_ZOOM_MATH_CONSTANTS) {
        return 0;
    }
    const u32 _at = _which < 4u ? _f->constants + 2u * _which :
                    _which < FUDE_ZOOM_MATH_RADICAL_DEGREE_BOTTOM_RAISE_PERCENT ? _f->constants + 8u + 4u * (_which - 4u) :
                                                                                 _f->constants + 8u + 4u * 51u;
    return _which == FUDE_ZOOM_MATH_DELIMITED_SUB_FORMULA_MIN_HEIGHT || _which == FUDE_ZOOM_MATH_DISPLAY_OPERATOR_MIN_HEIGHT ? (i32)fzm_u16(_f, _at)
                                                                                                                             : (i32)fzm_i16(_f, _at);
}

// A value from a table of MathValueRecords whose coverage is at its start (_table + its own offset) — _table's
// offsets: coverage at +0, count at +2, records from +4.
RDE_INTERNAL b8 fzm_glyph_value(const fude_zoom_mfont* _f, u32 _table, u32 _g, i32* _out) {
    const i32 _k = fzm_coverage(_f, _table + fzm_u16(_f, _table), _g);
    if(_k < 0 || (u32)_k >= fzm_u16(_f, _table + 2u)) {
        return false;
    }
    *_out = fzm_i16(_f, _table + 4u + 4u * (u32)_k);
    return true;
}

i32 fude_zoom_mfont_italic(const fude_zoom_mfont* _f, u32 _g) {
    i32 _v = 0;
    if(_f->math != 0u) {
        fzm_glyph_value(_f, _f->glyph_info + fzm_u16(_f, _f->glyph_info), _g, &_v);
    }
    return _v;
}

i32 fude_zoom_mfont_accent_at(const fude_zoom_mfont* _f, u32 _g) {
    i32 _v = 0;
    if(_f->math != 0u && fzm_u16(_f, _f->glyph_info + 2u) != 0u && fzm_glyph_value(_f, _f->glyph_info + fzm_u16(_f, _f->glyph_info + 2u), _g, &_v)) {
        return _v;
    }
    i32 _box[4];
    return fude_zoom_mfont_box(_f, _g, _box) ? (_box[0] + _box[2]) / 2 : fude_zoom_mfont_advance(_f, _g) / 2;
}

// Glyph _g's construction table up or across (0: none).
RDE_INTERNAL u32 fzm_construction(const fude_zoom_mfont* _f, u32 _g, b8 _vertical) {
    if(_f->math == 0u) {
        return 0u;
    }
    const u32 _v = _f->variants;
    const u16 _vert_count = fzm_u16(_f, _v + 6u), _horiz_count = fzm_u16(_f, _v + 8u);
    const u16 _cov = fzm_u16(_f, _v + (_vertical ? 2u : 4u));
    if(_cov == 0u) {
        return 0u;
    }
    const i32 _k = fzm_coverage(_f, _v + _cov, _g);
    if(_k < 0 || (u32)_k >= (_vertical ? _vert_count : _horiz_count)) {
        return 0u;
    }
    const u32 _slot = _v + 10u + 2u * ((_vertical ? 0u : _vert_count) + (u32)_k);
    const u16 _off = fzm_u16(_f, _slot);
    return _off != 0u ? _v + _off : 0u;
}

u32 fude_zoom_mfont_variants(const fude_zoom_mfont* _f, u32 _g, b8 _vertical, u32* _glyphs, i32* _sizes, u32 _most) {
    const u32 _c = fzm_construction(_f, _g, _vertical);
    if(_c == 0u) {
        return 0u;
    }
    const u16 _count = fzm_u16(_f, _c + 2u);
    u32 _n = 0;
    for(u32 _i = 0; _i < _count && _n < _most; _i++) {
        _glyphs[_n] = fzm_u16(_f, _c + 4u + 4u * _i);
        _sizes[_n]  = fzm_u16(_f, _c + 6u + 4u * _i);
        _n++;
    }
    return _n;
}

u32 fude_zoom_mfont_assembly(const fude_zoom_mfont* _f, u32 _g, b8 _vertical, fude_zoom_mfont_part* _parts, u32 _most) {
    const u32 _c = fzm_construction(_f, _g, _vertical);
    if(_c == 0u || fzm_u16(_f, _c) == 0u) {
        return 0u;
    }
    const u32 _a = _c + fzm_u16(_f, _c);
    const u16 _count = fzm_u16(_f, _a + 4u);
    u32 _n = 0;
    for(u32 _i = 0; _i < _count && _n < _most; _i++) {
        const u32 _p = _a + 6u + 10u * _i;
        _parts[_n] = (fude_zoom_mfont_part){ fzm_u16(_f, _p), fzm_u16(_f, _p + 2u), fzm_u16(_f, _p + 4u), fzm_u16(_f, _p + 6u), (fzm_u16(_f, _p + 8u) & 1u) != 0u };
        _n++;
    }
    return _n;
}

i32 fude_zoom_mfont_min_overlap(const fude_zoom_mfont* _f) {
    return _f->math != 0u ? fzm_u16(_f, _f->variants) : 0;
}
