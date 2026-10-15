// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/mathfont.h"

// ===========================================================================
// See mathfont.h.
//
// The OpenType tables are read as the specification lays them out, big-endian, through fzmf_u8, fzmf_u16, fzmf_i16 and
// fzmf_u32, which return 0 for anything past the file's end. Its helpers are fzmf_. The file goes: loading (the table
// directory, then head, hhea, maxp, cmap and MATH's three subtables), the character maps, metrics, glyph outlines
// (fzmf_points decodes a glyph's points, composites included; fude_zoom_mfont_outline turns them into polygons), the
// MATH table's constants, glyph info and variants, and last what embedding a font needs (pdf.c): any table, a glyph's
// bytes and parts, and a CFF font's CIDs.
// ===========================================================================

// Returns the font's byte at offset _at, or 0 past the file's end.
RDE_INTERNAL u8 fzmf_u8(const fude_zoom_mfont* _f, u32 _at) {
    return _at < _f->size ? _f->data[_at] : 0u;
}
// Returns whether _n bytes from offset _at are all inside the file. It is worked out without adding to _at, since an
// offset read from a damaged file may be near 2^32, where _at + _n would wrap round to a small number.
RDE_INTERNAL b8 fzmf_inside(const fude_zoom_mfont* _f, u32 _at, u32 _n) {
    return _at < _f->size && _f->size - _at >= _n;
}
// Returns the big-endian u16 at offset _at, or 0 when it runs past the file's end.
RDE_INTERNAL u16 fzmf_u16(const fude_zoom_mfont* _f, u32 _at) {
    return fzmf_inside(_f, _at, 2u) ? (u16)((_f->data[_at] << 8) | _f->data[_at + 1u]) : 0u;
}
// Returns the big-endian i16 at offset _at, or 0 when it runs past the file's end.
RDE_INTERNAL i16 fzmf_i16(const fude_zoom_mfont* _f, u32 _at) {
    return (i16)fzmf_u16(_f, _at);
}
// Returns the big-endian u32 at offset _at, or 0 when it runs past the file's end.
RDE_INTERNAL u32 fzmf_u32(const fude_zoom_mfont* _f, u32 _at) {
    return fzmf_inside(_f, _at, 4u) ? ((u32)_f->data[_at] << 24) | ((u32)_f->data[_at + 1u] << 16) | ((u32)_f->data[_at + 2u] << 8) | _f->data[_at + 3u] : 0u;
}

// See mathfont.h. Only the tables it needs are found in the table directory; a table whose offset is past the file's
// end counts as missing. The character maps taken are Unicode's: platform 0, or Windows' (3) in its BMP (1) or full
// (10) encoding, the first of format 12 and the first of format 4. Every refusal leaves size 0. No bytes are read
// before _data is known to be there and long enough for the table directory's head.
b8 fude_zoom_mfont_load(fude_zoom_mfont* _f, const u8* _data, u32 _size) {
    rde_memset(_f, 0, sizeof(*_f));
    if(_data == NULL || _size < 12u) {
        return false;   // (size left 0)
    }
    _f->data = _data;
    _f->size = _size;
    const u32 _version = fzmf_u32(_f, 0u);
    if(_version != 0x00010000u && _version != 0x74727565u && _version != 0x4F54544Fu) {   // (1.0, 'true', or 'OTTO': CFF)
        _f->size = 0u;
        return false;
    }
    const u16 _tables = fzmf_u16(_f, 4u);
    u32 _head = 0, _hhea = 0, _maxp = 0, _cmap = 0;
    for(u32 _i = 0; _i < _tables; _i++) {
        const u32 _rec = 12u + 16u * _i;
        const u32 _tag = fzmf_u32(_f, _rec), _off = fzmf_u32(_f, _rec + 8u);
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
        case 0x43464620u: _f->cff = _off; break;     // CFF
        default: break;
        }
    }
    const b8 _outlines = (_f->loca != 0u && _f->glyf != 0u) || _f->cff != 0u;
    if(_head == 0u || _hhea == 0u || _maxp == 0u || _cmap == 0u || _f->hmtx == 0u || !_outlines) {
        _f->size = 0u;
        return false;
    }
    if(_f->loca == 0u || _f->glyf == 0u) {
        _f->loca = _f->glyf = 0u;   // (CFF outlines: no glyph is looked for in glyf)
    }
    _f->units_per_em = fzmf_u16(_f, _head + 18u);
    _f->long_loca    = fzmf_i16(_f, _head + 50u) != 0;
    _f->ascender     = fzmf_i16(_f, _hhea + 4u);
    _f->descender    = fzmf_i16(_f, _hhea + 6u);
    _f->hmetrics     = fzmf_u16(_f, _hhea + 34u);
    _f->glyphs       = fzmf_u16(_f, _maxp + 4u);
    // (the character maps: a full one, format 12, and the basic plane's, format 4 — Unicode's or Windows')
    const u16 _maps = fzmf_u16(_f, _cmap + 2u);
    for(u32 _i = 0; _i < _maps; _i++) {
        const u16 _platform = fzmf_u16(_f, _cmap + 4u + 8u * _i), _encoding = fzmf_u16(_f, _cmap + 6u + 8u * _i);
        const u32 _sub = _cmap + fzmf_u32(_f, _cmap + 8u + 8u * _i);
        const u16 _format = fzmf_u16(_f, _sub);
        const b8  _unicode = _platform == 0u || (_platform == 3u && (_encoding == 1u || _encoding == 10u));
        if(_unicode && _format == 12u && _f->cmap12 == 0u) {
            _f->cmap12 = _sub;
        } else if(_unicode && _format == 4u && _f->cmap4 == 0u) {
            _f->cmap4 = _sub;
        }
    }
    if(_f->math != 0u) {
        _f->constants  = _f->math + fzmf_u16(_f, _f->math + 4u);
        _f->glyph_info = _f->math + fzmf_u16(_f, _f->math + 6u);
        _f->variants   = _f->math + fzmf_u16(_f, _f->math + 8u);
    }
    if(_f->units_per_em == 0u || _f->glyphs == 0u) {
        _f->size = 0u;   // as every refusal above: every call on it then answers nothing
        return false;
    }
    return true;
}

// See mathfont.h. Format 12 is searched by halves over its groups; format 4's segments are walked in order to the first
// that ends at or after the code point.
u32 fude_zoom_mfont_glyph(const fude_zoom_mfont* _f, u32 _c) {
    if(_f->size == 0u) {
        return 0u;
    }
    if(_f->cmap12 != 0u) {
        u32 _lo = 0, _hi = fzmf_u32(_f, _f->cmap12 + 12u);
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u, _g = _f->cmap12 + 16u + 12u * _mid;
            const u32 _start = fzmf_u32(_f, _g), _end = fzmf_u32(_f, _g + 4u);
            if(_c < _start) {
                _hi = _mid;
            } else if(_c > _end) {
                _lo = _mid + 1u;
            } else {
                return fzmf_u32(_f, _g + 8u) + (_c - _start);
            }
        }
        return 0u;
    }
    if(_f->cmap4 == 0u || _c > 0xFFFFu) {
        return 0u;
    }
    const u32 _t = _f->cmap4;
    const u16 _segs2 = fzmf_u16(_f, _t + 6u);
    const u32 _ends = _t + 14u, _starts = _ends + _segs2 + 2u, _deltas = _starts + _segs2, _ranges = _deltas + _segs2;
    for(u32 _s = 0; _s < _segs2; _s += 2u) {
        if(fzmf_u16(_f, _ends + _s) < _c) {
            continue;
        }
        const u16 _start = fzmf_u16(_f, _starts + _s);
        if(_c < _start) {
            return 0u;
        }
        const u16 _delta = fzmf_u16(_f, _deltas + _s), _range = fzmf_u16(_f, _ranges + _s);
        if(_range == 0u) {
            return (_c + _delta) & 0xFFFFu;
        }
        const u16 _g = fzmf_u16(_f, _ranges + _s + _range + 2u * (_c - _start));
        return _g != 0u ? (u32)((_g + _delta) & 0xFFFFu) : 0u;
    }
    return 0u;
}

// See mathfont.h. Glyphs past the last of hmtx's long metrics take its advance, as the specification says.
i32 fude_zoom_mfont_advance(const fude_zoom_mfont* _f, u32 _g) {
    if(_f->size == 0u || _f->hmetrics == 0u) {
        return 0;
    }
    const u32 _k = _g < _f->hmetrics ? _g : _f->hmetrics - 1u;
    return fzmf_u16(_f, _f->hmtx + 4u * _k);
}

// Finds glyph _g's bytes in glyf through loca: *_at is where they start (a byte offset in the file) and *_len how many
// there are. Returns false for a glyph with none, such as a space, or one out of range or past the file's end.
RDE_INTERNAL b8 fzmf_glyph_at(const fude_zoom_mfont* _f, u32 _g, u32* _at, u32* _len) {
    if(_f->size == 0u || _f->glyf == 0u || _g >= _f->glyphs) {
        return false;
    }
    u32 _a, _b;
    if(_f->long_loca) {
        _a = fzmf_u32(_f, _f->loca + 4u * _g);
        _b = fzmf_u32(_f, _f->loca + 4u * (_g + 1u));
    } else {
        _a = 2u * fzmf_u16(_f, _f->loca + 2u * _g);
        _b = 2u * fzmf_u16(_f, _f->loca + 2u * (_g + 1u));
    }
    // (glyf + _b compared without the sum, which an offset near 2^32 would wrap; load keeps glyf inside the file)
    if(_b <= _a || _f->glyf >= _f->size || _b > _f->size - _f->glyf) {
        return false;
    }
    *_at  = _f->glyf + _a;
    *_len = _b - _a;
    return true;
}

// See mathfont.h. The box is the glyph header's xMin, yMin, xMax, yMax.
b8 fude_zoom_mfont_box(const fude_zoom_mfont* _f, u32 _g, i32 _box[4]) {
    u32 _at, _len;
    if(!fzmf_glyph_at(_f, _g, &_at, &_len) || _len < 10u) {
        _box[0] = _box[1] = _box[2] = _box[3] = 0;
        return false;
    }
    for(u32 _k = 0; _k < 4u; _k++) {
        _box[_k] = fzmf_i16(_f, _at + 2u + 2u * _k);
    }
    return true;
}

// --- outlines --------------------------------------------------------------------------------------

// A glyph's point as glyf gives it, transformed: where it is, and whether it is on the curve (else a quadratic's
// control point).
typedef struct {
    f64 x, y;
    b8  on;
} fzmf_point;

// What one glyph's outline may cost at most (fzmf_points): a glyph read 1, and a point 1. A real glyph costs hundreds,
// an accented letter of a few parts a little more; past a million is a damaged font, and the rest of it is left out.
#define FZMF_OUTLINE_MOST (1u << 20)

// One part of a composite glyph, as its record in glyf gives it.
typedef struct {
    u16 flags;        // the record's flags; MORE_COMPONENTS (0x0020) says another record follows it
    u32 glyph;        // the glyph it draws
    f64 dx, dy;       // where it is moved to (0 when it is placed by matching points, which is not done)
    f64 a, b, c, d;   // its scale and turn: x' = a x + c y, y' = b x + d y
} fzmf_component;

// Reads the composite glyph record at _p into _out and returns where the record after it starts. Its offset is read
// from words or bytes and its transform from one scale, two, or a 2 by 2 matrix (F2Dot14 numbers), as its flags say.
RDE_INTERNAL u32 fzmf_component_at(const fude_zoom_mfont* _f, u32 _p, fzmf_component* _out) {
    const u16 _fl = fzmf_u16(_f, _p);
    _out->flags = _fl;
    _out->glyph = fzmf_u16(_f, _p + 2u);
    _p += 4u;
    if(_fl & 0x0001u) {
        _out->dx = fzmf_i16(_f, _p);
        _out->dy = fzmf_i16(_f, _p + 2u);
        _p += 4u;
    } else {
        _out->dx = (i8)fzmf_u8(_f, _p);
        _out->dy = (i8)fzmf_u8(_f, _p + 1u);
        _p += 2u;
    }
    if(!(_fl & 0x0002u)) {
        _out->dx = _out->dy = 0.0;   // (matched points, not offsets: not done — placed where they are)
    }
    _out->a = 1.0;
    _out->b = 0.0;
    _out->c = 0.0;
    _out->d = 1.0;
    if(_fl & 0x0008u) {
        _out->a = _out->d = fzmf_i16(_f, _p) / 16384.0;
        _p += 2u;
    } else if(_fl & 0x0040u) {
        _out->a = fzmf_i16(_f, _p) / 16384.0;
        _out->d = fzmf_i16(_f, _p + 2u) / 16384.0;
        _p += 4u;
    } else if(_fl & 0x0080u) {
        _out->a = fzmf_i16(_f, _p) / 16384.0;
        _out->b = fzmf_i16(_f, _p + 2u) / 16384.0;
        _out->c = fzmf_i16(_f, _p + 4u) / 16384.0;
        _out->d = fzmf_i16(_f, _p + 6u) / 16384.0;
        _p += 8u;
    }
    return _p;
}

// Appends glyph _g's points to _pts (fzmf_point), through the transform _m (x' = a x + c y + e, y' = b x + d y + f,
// with _m = { a, b, c, d, e, f }), and each contour's end to _ends: the index one past its last point in _pts. A
// composite's parts are appended in turn, each through its own transform and then the composite's. Composites nested
// deeper than 8, or with more than 64 parts, are cut short there; a part placed by matching points, rather than by an
// offset, is put at no offset. The glyph's instructions are skipped, not run. *_left is what the whole outline may
// still cost (FZMF_OUTLINE_MOST to start): each glyph read takes 1, each of a composite's parts 1 and each of a simple
// glyph's points 1, and nothing more is read once it is spent. A damaged font's composite that names itself 64 times
// would otherwise be read 64^8 times over (its depth alone stops it), and one naming large glyphs would fill memory.
RDE_INTERNAL void fzmf_points(const fude_zoom_mfont* _f, u32 _g, const f64 _m[6], u32 _depth, u32* _left, rde_arr* _pts, rde_arr* _ends) {
    u32 _at, _len;
    if(_depth > 8u || *_left == 0u || !fzmf_glyph_at(_f, _g, &_at, &_len) || _len < 10u) {
        return;
    }
    (*_left)--;
    const i16 _contours = fzmf_i16(_f, _at);
    const u32 _end = _at + _len;
    if(_contours >= 0) {
        const u32 _first_end = (u32)rde_arr_length(_pts);
        u32 _p = _at + 10u;
        u32 _count = 0;
        for(i32 _c = 0; _c < _contours; _c++) {
            _count = (u32)fzmf_u16(_f, _p + 2u * (u32)_c) + 1u;
        }
        _p += 2u * (u32)_contours;
        _p += 2u + fzmf_u16(_f, _p);   // (its instructions: not run)
        if(_count == 0u || _count > 65535u || _count > *_left) {
            return;
        }
        *_left -= _count;
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        rde_arr _flags_arr = rde_arr_new(sizeof(u8), _heap);
        rde_arr_resize(&_flags_arr, _count);
        u8* _flags = (u8*)_flags_arr.memory;
        for(u32 _i = 0; _i < _count && _p < _end;) {
            const u8 _fl = fzmf_u8(_f, _p++);
            u32 _repeat = 1u;
            if((_fl & 0x08u) && _p < _end) {
                _repeat += fzmf_u8(_f, _p++);
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
                    const i32 _d = _p < _end ? fzmf_u8(_f, _p) : 0;
                    _p++;
                    _v += (_flags[_i] & _same) ? _d : -_d;
                } else if(!(_flags[_i] & _same)) {
                    _v += fzmf_i16(_f, _p);
                    _p += 2u;
                }
                _xy[2u * _i + _axis] = _v;
            }
        }
        for(u32 _i = 0; _i < _count; _i++) {
            const f64 _x = (f64)_xy[2u * _i], _y = (f64)_xy[2u * _i + 1u];
            const fzmf_point _q = { _m[0] * _x + _m[2] * _y + _m[4], _m[1] * _x + _m[3] * _y + _m[5], (_flags[_i] & 0x01u) != 0u };
            rde_arr_add(_pts, (any)&_q);
        }
        for(i32 _c = 0; _c < _contours; _c++) {
            const u32 _e = _first_end + (u32)fzmf_u16(_f, _at + 10u + 2u * (u32)_c) + 1u;
            rde_arr_add(_ends, (any)&_e);
        }
        rde_arr_free(&_flags_arr);
        rde_arr_free(&_xy_arr);
        return;
    }
    // A composite: its parts, each moved (and scaled) as it says, each costing 1 (*_left: so even parts too deep to be
    // read are counted).
    u32 _p = _at + 10u;
    for(u32 _guard = 0; _guard < 64u && _p + 4u <= _end && *_left > 0u; _guard++) {
        (*_left)--;
        fzmf_component _k;
        _p = fzmf_component_at(_f, _p, &_k);
        // (the part's transform, then the composite's)
        const f64 _n[6] = { _m[0] * _k.a + _m[2] * _k.b, _m[1] * _k.a + _m[3] * _k.b, _m[0] * _k.c + _m[2] * _k.d, _m[1] * _k.c + _m[3] * _k.d,
                            _m[0] * _k.dx + _m[2] * _k.dy + _m[4], _m[1] * _k.dx + _m[3] * _k.dy + _m[5] };
        fzmf_points(_f, _k.glyph, _n, _depth + 1u, _left, _pts, _ends);
        if(!(_k.flags & 0x0020u)) {
            break;
        }
    }
}

// Appends one point to _out as an x, y pair of f64s.
RDE_INTERNAL void fzmf_emit(rde_arr* _out, f64 _x, f64 _y) {
    rde_arr_add(_out, (any)&_x);
    rde_arr_add(_out, (any)&_y);
}

// Appends a quadratic curve from _a through control point _c to _b, cut into straight pieces no further than _tol from
// it: the number of pieces is worked out from how much it bends, from 1 to 64. Its start is left out, since it is
// there already.
RDE_INTERNAL void fzmf_curve(rde_arr* _out, fzmf_point _a, fzmf_point _c, fzmf_point _b, f64 _tol) {
    const f64 _ddx = _a.x - 2.0 * _c.x + _b.x, _ddy = _a.y - 2.0 * _c.y + _b.y;
    const f64 _bend = rde_math_sqrt_f64(_ddx * _ddx + _ddy * _ddy);
    u32 _n = (u32)rde_math_ceil_f64(rde_math_sqrt_f64(_bend / (4.0 * rde_math_max_f64(_tol, 1e-6))));
    _n = _n < 1u ? 1u : (_n > 64u ? 64u : _n);
    for(u32 _i = 1; _i <= _n; _i++) {
        const f64 _t = (f64)_i / (f64)_n, _u = 1.0 - _t;
        fzmf_emit(_out, _u * _u * _a.x + 2.0 * _u * _t * _c.x + _t * _t * _b.x, _u * _u * _a.y + 2.0 * _u * _t * _c.y + _t * _t * _b.y);
    }
}

// See mathfont.h. Each contour starts at its first point on the curve, or, when all of them are off it, at the middle
// between its last and its first. Two control points in a row have an on-curve point between them, halfway, as
// TrueType implies. Contours of fewer than 2 points are skipped.
u32 fude_zoom_mfont_outline(const fude_zoom_mfont* _f, u32 _g, f64 _tol, rde_arr* _points, rde_arr* _ends) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fzmf_point), _heap), _cends = rde_arr_new(sizeof(u32), _heap);
    const f64 _identity[6] = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
    u32 _left = FZMF_OUTLINE_MOST;
    fzmf_points(_f, _g, _identity, 0u, &_left, &_pts, &_cends);
    const fzmf_point* _p = (const fzmf_point*)_pts.memory;
    const u32 _base = (u32)(rde_arr_length(_points) / 2u);
    u32 _contours = 0, _from = 0;
    for(u32 _c = 0; _c < (u32)rde_arr_length(&_cends); _c++) {
        const u32 _to = ((const u32*)_cends.memory)[_c];
        const u32 _n = _to > _from && _to <= (u32)rde_arr_length(&_pts) ? _to - _from : 0u;
        if(_n < 2u) {
            _from = _to;
            continue;
        }
        const fzmf_point* _q = &_p[_from];
        // (its start: its first point on the curve, the rest after it in turn; all off the curve, the middle between
        // its last and its first — every one of them then after it)
        u32 _s = 0;
        while(_s < _n && !_q[_s].on) {
            _s++;
        }
        const b8 _any_on = _s < _n;
        const fzmf_point _start = _any_on ? _q[_s] : (fzmf_point){ 0.5 * (_q[_n - 1u].x + _q[0].x), 0.5 * (_q[_n - 1u].y + _q[0].y), true };
        const u32 _steps = _any_on ? _n - 1u : _n;
        const u32 _before = (u32)rde_arr_length(_points);
        fzmf_emit(_points, _start.x, _start.y);
        fzmf_point _pen = _start, _ctrl = { 0.0, 0.0, false };
        b8 _have_ctrl = false;
        for(u32 _k = 0; _k <= _steps; _k++) {
            const fzmf_point _cur = _k == _steps ? _start : _q[_any_on ? (_s + 1u + _k) % _n : _k];
            if(_cur.on) {
                if(_have_ctrl) {
                    fzmf_curve(_points, _pen, _ctrl, _cur, _tol);
                    _have_ctrl = false;
                } else {
                    fzmf_emit(_points, _cur.x, _cur.y);
                }
                _pen = _cur;
            } else if(_have_ctrl) {
                const fzmf_point _mid = { 0.5 * (_ctrl.x + _cur.x), 0.5 * (_ctrl.y + _cur.y), true };
                fzmf_curve(_points, _pen, _ctrl, _mid, _tol);
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
            if(rde_math_abs_f64(_all[_last] - _start.x) < 1e-9 && rde_math_abs_f64(_all[_last + 1u] - _start.y) < 1e-9) {
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

// Returns glyph _g's index in the coverage table at _at (format 1, a sorted list, or format 2, sorted ranges), or -1
// when it is not in it or the format is neither.
RDE_INTERNAL i32 fzmf_coverage(const fude_zoom_mfont* _f, u32 _at, u32 _g) {
    const u16 _format = fzmf_u16(_f, _at);
    if(_format == 1u) {
        u32 _lo = 0, _hi = fzmf_u16(_f, _at + 2u);
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u;
            const u16 _v = fzmf_u16(_f, _at + 4u + 2u * _mid);
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
        u32 _lo = 0, _hi = fzmf_u16(_f, _at + 2u);
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u, _r = _at + 4u + 6u * _mid;
            const u16 _start = fzmf_u16(_f, _r), _end = fzmf_u16(_f, _r + 2u);
            if(_g < _start) {
                _hi = _mid;
            } else if(_g > _end) {
                _lo = _mid + 1u;
            } else {
                return (i32)(fzmf_u16(_f, _r + 4u) + (_g - _start));
            }
        }
    }
    return -1;
}

// See mathfont.h. The table holds the first four as plain 16-bit values, the next 51 as MathValueRecords of 4 bytes
// (the value, then a device table's offset, not read), and the last as a plain value again. The two minimum heights
// are unsigned; the rest are signed.
i32 fude_zoom_mfont_constant(const fude_zoom_mfont* _f, u32 _which) {
    if(_f->math == 0u || _which >= FUDE_ZOOM_MATH_CONSTANTS) {
        return 0;
    }
    const u32 _at = _which < 4u ? _f->constants + 2u * _which :
                    _which < FUDE_ZOOM_MATH_RADICAL_DEGREE_BOTTOM_RAISE_PERCENT ? _f->constants + 8u + 4u * (_which - 4u) :
                                                                                 _f->constants + 8u + 4u * 51u;
    return _which == FUDE_ZOOM_MATH_DELIMITED_SUB_FORMULA_MIN_HEIGHT || _which == FUDE_ZOOM_MATH_DISPLAY_OPERATOR_MIN_HEIGHT ? (i32)fzmf_u16(_f, _at)
                                                                                                                             : (i32)fzmf_i16(_f, _at);
}

// Reads glyph _g's value from a table of MathValueRecords laid out as MathItalicsCorrectionInfo and
// MathTopAccentAttachment are: its coverage's offset at +0, its count at +2, its records from +4. Returns false when
// the glyph is not in it.
RDE_INTERNAL b8 fzmf_glyph_value(const fude_zoom_mfont* _f, u32 _table, u32 _g, i32* _out) {
    const i32 _k = fzmf_coverage(_f, _table + fzmf_u16(_f, _table), _g);
    if(_k < 0 || (u32)_k >= fzmf_u16(_f, _table + 2u)) {
        return false;
    }
    *_out = fzmf_i16(_f, _table + 4u + 4u * (u32)_k);
    return true;
}

// See mathfont.h. From MathGlyphInfo's italics correction table.
i32 fude_zoom_mfont_italic(const fude_zoom_mfont* _f, u32 _g) {
    i32 _v = 0;
    if(_f->math != 0u) {
        fzmf_glyph_value(_f, _f->glyph_info + fzmf_u16(_f, _f->glyph_info), _g, &_v);
    }
    return _v;
}

// See mathfont.h. From MathGlyphInfo's top accent attachment table, when the font has one.
i32 fude_zoom_mfont_accent_at(const fude_zoom_mfont* _f, u32 _g) {
    i32 _v = 0;
    if(_f->math != 0u && fzmf_u16(_f, _f->glyph_info + 2u) != 0u && fzmf_glyph_value(_f, _f->glyph_info + fzmf_u16(_f, _f->glyph_info + 2u), _g, &_v)) {
        return _v;
    }
    i32 _box[4];
    return fude_zoom_mfont_box(_f, _g, _box) ? (_box[0] + _box[2]) / 2 : fude_zoom_mfont_advance(_f, _g) / 2;
}

// Returns where glyph _g's MathGlyphConstruction is for growing up (_vertical) or across, found through MathVariants'
// coverage for that direction; 0 when it has none.
RDE_INTERNAL u32 fzmf_construction(const fude_zoom_mfont* _f, u32 _g, b8 _vertical) {
    if(_f->math == 0u) {
        return 0u;
    }
    const u32 _v = _f->variants;
    const u16 _vert_count = fzmf_u16(_f, _v + 6u), _horiz_count = fzmf_u16(_f, _v + 8u);
    const u16 _cov = fzmf_u16(_f, _v + (_vertical ? 2u : 4u));
    if(_cov == 0u) {
        return 0u;
    }
    const i32 _k = fzmf_coverage(_f, _v + _cov, _g);
    if(_k < 0 || (u32)_k >= (_vertical ? _vert_count : _horiz_count)) {
        return 0u;
    }
    const u32 _slot = _v + 10u + 2u * ((_vertical ? 0u : _vert_count) + (u32)_k);
    const u16 _off = fzmf_u16(_f, _slot);
    return _off != 0u ? _v + _off : 0u;
}

// See mathfont.h. The forms are read in the construction's order.
u32 fude_zoom_mfont_variants(const fude_zoom_mfont* _f, u32 _g, b8 _vertical, u32* _glyphs, i32* _sizes, u32 _most) {
    const u32 _c = fzmf_construction(_f, _g, _vertical);
    if(_c == 0u) {
        return 0u;
    }
    const u16 _count = fzmf_u16(_f, _c + 2u);
    u32 _n = 0;
    for(u32 _i = 0; _i < _count && _n < _most; _i++) {
        _glyphs[_n] = fzmf_u16(_f, _c + 4u + 4u * _i);
        _sizes[_n]  = fzmf_u16(_f, _c + 6u + 4u * _i);
        _n++;
    }
    return _n;
}

// See mathfont.h. The GlyphAssembly's parts in its order; bit 1 of a part's flags marks an extender.
u32 fude_zoom_mfont_assembly(const fude_zoom_mfont* _f, u32 _g, b8 _vertical, fude_zoom_mfont_part* _parts, u32 _most) {
    const u32 _c = fzmf_construction(_f, _g, _vertical);
    if(_c == 0u || fzmf_u16(_f, _c) == 0u) {
        return 0u;
    }
    const u32 _a = _c + fzmf_u16(_f, _c);
    const u16 _count = fzmf_u16(_f, _a + 4u);
    u32 _n = 0;
    for(u32 _i = 0; _i < _count && _n < _most; _i++) {
        const u32 _p = _a + 6u + 10u * _i;
        _parts[_n] = (fude_zoom_mfont_part){ fzmf_u16(_f, _p), fzmf_u16(_f, _p + 2u), fzmf_u16(_f, _p + 4u), fzmf_u16(_f, _p + 6u), (fzmf_u16(_f, _p + 8u) & 1u) != 0u };
        _n++;
    }
    return _n;
}

// See mathfont.h: MathVariants' minConnectorOverlap.
i32 fude_zoom_mfont_min_overlap(const fude_zoom_mfont* _f) {
    return _f->math != 0u ? fzmf_u16(_f, _f->variants) : 0;
}

// --- embedding: any table, a glyph's bytes and parts, a CFF font's CIDs ------------------------------------------

// See mathfont.h.
u32 fude_zoom_mfont_u16(const fude_zoom_mfont* _f, u32 _at) {
    return fzmf_u16(_f, _at);
}

// See mathfont.h.
u32 fude_zoom_mfont_u32(const fude_zoom_mfont* _f, u32 _at) {
    return fzmf_u32(_f, _at);
}

// See mathfont.h. The table directory is walked in its order; the first record with the tag is the table.
u32 fude_zoom_mfont_table(const fude_zoom_mfont* _f, u32 _tag, u32* _size) {
    if(_f->size == 0u) {
        return 0u;
    }
    const u16 _tables = fzmf_u16(_f, 4u);
    for(u32 _i = 0; _i < _tables; _i++) {
        const u32 _rec = 12u + 16u * _i;
        if(fzmf_u32(_f, _rec) != _tag) {
            continue;
        }
        const u32 _off = fzmf_u32(_f, _rec + 8u), _len = fzmf_u32(_f, _rec + 12u);
        if(_off == 0u || !fzmf_inside(_f, _off, _len)) {
            return 0u;
        }
        if(_size != NULL) {
            *_size = _len;
        }
        return _off;
    }
    return 0u;
}

// See mathfont.h.
b8 fude_zoom_mfont_glyph_data(const fude_zoom_mfont* _f, u32 _g, u32* _at, u32* _len) {
    return fzmf_glyph_at(_f, _g, _at, _len);
}

// See mathfont.h. The records are read as fzmf_points reads them, with the same limit of 64.
u32 fude_zoom_mfont_parts(const fude_zoom_mfont* _f, u32 _g, u32* _glyphs, u32 _most) {
    u32 _at, _len;
    if(!fzmf_glyph_at(_f, _g, &_at, &_len) || _len < 10u || fzmf_i16(_f, _at) >= 0) {
        return 0u;   // (no glyph, or a simple one: its contour count is not negative)
    }
    const u32 _end = _at + _len;
    u32 _p = _at + 10u, _n = 0;
    for(u32 _guard = 0; _guard < 64u && _p + 4u <= _end; _guard++) {
        fzmf_component _k;
        _p = fzmf_component_at(_f, _p, &_k);
        if(_n < _most) {
            _glyphs[_n++] = _k.glyph;
        }
        if(!(_k.flags & 0x0020u)) {
            break;
        }
    }
    return _n;
}

// Returns item _i's offset in a CFF INDEX whose offsets start at _offsets, each _size bytes (1 to 4). CFF counts the
// offsets from 1, from the byte before the INDEX's data.
RDE_INTERNAL u32 fzmf_cff_offset(const fude_zoom_mfont* _f, u32 _offsets, u32 _size, u32 _i) {
    u32 _v = 0;
    for(u32 _k = 0; _k < _size; _k++) {
        _v = (_v << 8) | fzmf_u8(_f, _offsets + _i * _size + _k);
    }
    return _v;
}

// Reads the CFF INDEX at _at: its item count into *_count, and where its first item starts and ends into *_from and
// *_to. Returns where the INDEX ends, or 0 when its offsets' size is not 1 to 4 (a damaged CFF). An empty INDEX is its
// count alone.
RDE_INTERNAL u32 fzmf_cff_index(const fude_zoom_mfont* _f, u32 _at, u32* _count, u32* _from, u32* _to) {
    *_count = fzmf_u16(_f, _at);
    *_from = *_to = 0u;
    if(*_count == 0u) {
        return _at + 2u;
    }
    const u32 _size = fzmf_u8(_f, _at + 2u), _offsets = _at + 3u;
    if(_size < 1u || _size > 4u) {
        return 0u;
    }
    const u32 _base = _offsets + (*_count + 1u) * _size - 1u;   // (the byte before the data: offsets count from 1)
    *_from = _base + fzmf_cff_offset(_f, _offsets, _size, 0u);
    *_to   = _base + fzmf_cff_offset(_f, _offsets, _size, 1u);
    return _base + fzmf_cff_offset(_f, _offsets, _size, *_count);
}

// Reads a CFF Top DICT (bytes _from to _to): whether it has ROS (operator 12 30, a CID-keyed font's) and the charset's
// offset (operator 15's operand; 0 when there is none). Operands come before their operator; a real (30) is skipped
// nibble by nibble to the one that ends it, its value not needed here.
RDE_INTERNAL b8 fzmf_cff_top(const fude_zoom_mfont* _f, u32 _from, u32 _to, u32* _charset) {
    b8  _ros = false, _have = false;
    i32 _last = 0;   // the last operand read
    *_charset = 0u;
    for(u32 _p = _from; _p < _to;) {
        const u8 _b = fzmf_u8(_f, _p);
        if(_b <= 21u) {
            u32 _op = _b;
            _p++;
            if(_b == 12u) {
                _op = 0x0C00u | fzmf_u8(_f, _p);
                _p++;
            }
            _ros = _ros || _op == 0x0C1Eu;
            if(_op == 15u && _have && _last > 0) {
                *_charset = (u32)_last;
            }
            _have = false;
            continue;
        }
        if(_b == 28u) {
            _last = fzmf_i16(_f, _p + 1u);
            _p += 3u;
        } else if(_b == 29u) {
            _last = (i32)fzmf_u32(_f, _p + 1u);
            _p += 5u;
        } else if(_b == 30u) {
            _p++;
            while(_p < _to) {
                const u8 _x = fzmf_u8(_f, _p++);
                if((_x & 0x0Fu) == 0x0Fu || (_x >> 4) == 0x0Fu) {
                    break;
                }
            }
            _last = 0;
        } else if(_b >= 32u && _b <= 246u) {
            _last = (i32)_b - 139;
            _p++;
        } else if(_b >= 247u && _b <= 250u) {
            _last = ((i32)_b - 247) * 256 + (i32)fzmf_u8(_f, _p + 1u) + 108;
            _p += 2u;
        } else if(_b >= 251u && _b <= 254u) {
            _last = -((i32)_b - 251) * 256 - (i32)fzmf_u8(_f, _p + 1u) - 108;
            _p += 2u;
        } else {
            _p++;   // (a reserved byte: skipped)
        }
        _have = true;
    }
    return _ros;
}

// See mathfont.h. The CFF's header gives the Name INDEX's place; the Top DICT INDEX follows it, and its first DICT is
// the font's. The charset (format 0: a CID for each glyph after glyph 0; 1 and 2: ranges of consecutive CIDs, their
// lengths in a byte or in two) is read for every glyph the font has; the offsets 0 to 2, CFF's predefined charsets,
// are for fonts that are not CID-keyed.
b8 fude_zoom_mfont_cids(const fude_zoom_mfont* _f, rde_arr* _cids) {
    if(_f->size == 0u || _f->cff == 0u) {
        return false;
    }
    const u32 _c = _f->cff;
    u32 _count, _from, _to, _charset;
    const u32 _top = fzmf_cff_index(_f, _c + fzmf_u8(_f, _c + 2u), &_count, &_from, &_to);   // (the Name INDEX, skipped)
    if(_top == 0u || fzmf_cff_index(_f, _top, &_count, &_from, &_to) == 0u || _count == 0u) {
        return false;
    }
    _to = _to < _f->size ? _to : _f->size;   // (a damaged offset far past the file: the DICT read to the file's end, no further)
    if(_to <= _from) {
        return false;
    }
    if(!fzmf_cff_top(_f, _from, _to, &_charset) || _charset <= 2u) {
        return false;
    }
    u32 _at = _c + _charset;
    const u8 _format = fzmf_u8(_f, _at++);
    if(_format > 2u) {
        return false;
    }
    rde_arr_resize(_cids, 0u);
    rde_arr_resize(_cids, _f->glyphs);   // (every CID 0 to start with)
    u16* _cid = (u16*)_cids->memory;
    if(_format == 0u) {
        for(u32 _g = 1u; _g < _f->glyphs; _g++, _at += 2u) {
            _cid[_g] = fzmf_u16(_f, _at);
        }
    } else {
        // (each range names one glyph at least, so the walk ends within the glyph count)
        for(u32 _g = 1u; _g < _f->glyphs && fzmf_inside(_f, _at, _format == 1u ? 3u : 4u);) {
            const u32 _first = fzmf_u16(_f, _at), _left = _format == 1u ? fzmf_u8(_f, _at + 2u) : fzmf_u16(_f, _at + 2u);
            _at += _format == 1u ? 3u : 4u;
            for(u32 _k = 0; _k <= _left && _g < _f->glyphs; _k++) {
                _cid[_g++] = (u16)(_first + _k);
            }
        }
    }
    return true;
}
