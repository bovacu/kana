// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/symbolart.h"
#include "zoom/symbol.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See symbolart.h.
//
// fude_zoom_symbol_art_load reads parts.txt and each listed SVG into one slot per symbol kind; fzsa_parts is the hook
// symbol.c calls to turn a kind's art into its parts (symbol.h's fude_zoom_symbol_part), flattened afresh at each call
// to as many segments as the symbol is drawn with. Internal names start fzsa_.
// ===========================================================================

// One symbol kind's art: its SVG document (NULL when the kind has none) and its size in the SVG's own units.
typedef struct {
    rde_svg_document* doc;
    f32               w, h;
} fzsa_art;

RDE_INTERNAL rde_arr TYPE(fzsa_art) FZSA = { 0 };   // One slot for each symbol kind, by kind; not inited until loaded.
RDE_INTERNAL u32                  FZSA_N = 0;     // How many slots: the library's kinds when it was loaded.

// Returns whether _c is the grey _v (red, green and blue all _v), whatever its alpha.
RDE_INTERNAL b8 fzsa_is(rde_color _c, u8 _v) {
    return _c.r == _v && _c.g == _v && _c.b == _v;
}

// Adds kind _kind's parts, drawn from its art into a box _hw by _hh each way about its middle, to _points and _parts,
// and returns how many parts _parts then holds; 0 when the kind has no art, so it is drawn with its own lines
// (symbol.h's fude_zoom_symbol_art_fn). Each visible path of each visible shape is one part; a closed path that
// repeats its first point at its end loses the repeat, and an open one never fills.
RDE_INTERNAL u32 fzsa_parts(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    const fzsa_art* _a = _kind < FZSA_N ? &((const fzsa_art*)FZSA.memory)[_kind] : NULL;
    if(_a == NULL || _a->doc == NULL || !(_a->w > 0.0f) || !(_a->h > 0.0f)) {
        return 0;
    }
    const f64 _sx = 2.0 * _hw / (f64)_a->w, _sy = 2.0 * _hh / (f64)_a->h;
    const f32 _tol = rde_math_max_f32(_a->w, _a->h) / (f32)(_segments * 3u);   // (its curves as fine as the symbol is drawn)
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _flat = rde_arr_new(sizeof(rde_vec_2F), _heap);
    const u32 _shapes = rde_svg_document_shape_count(_a->doc);
    for(u32 _s = 0; _s < _shapes; _s++) {
        rde_svg_shape_info _info;
        if(!rde_svg_document_shape(_a->doc, _s, &_info) || !_info.visible) {
            continue;
        }
        // Its colours' roles: black the ink, white the paper; the rest themselves.
        u8 _flags = 0u;
        rde_color _fill = { 0, 0, 0, 0 }, _line = { 0, 0, 0, 0 };
        if(_info.fill_paint != RDE_SVG_PAINT_NONE && _info.fill.a > 0u) {
            if(fzsa_is(_info.fill, 0u) && _info.fill.a == 255u) {
                _flags |= FUDE_ZOOM_SYMBOL_SOLID;
            } else if(fzsa_is(_info.fill, 255u) && _info.fill.a == 255u) {
                _flags |= FUDE_ZOOM_SYMBOL_FILLED;
            } else {
                _flags |= FUDE_ZOOM_SYMBOL_TINTED;
                _fill = _info.fill;
            }
        }
        if(_info.stroke_paint == RDE_SVG_PAINT_NONE || _info.stroke.a == 0u || !(_info.stroke_width > 0.0f)) {
            _flags |= FUDE_ZOOM_SYMBOL_NO_LINE;
        } else if(!fzsa_is(_info.stroke, 0u)) {
            _line = _info.stroke;
        }
        if(_info.dash_count > 0u) {
            _flags |= FUDE_ZOOM_SYMBOL_DASHED;
        }
        const f32 _width = _info.stroke_width * (f32)rde_math_sqrt_f64(rde_math_abs_f64(_sx * _sy));
        u32 _outer = FUDE_ZOOM_NONE;   // (the part its holes are in)
        for(u32 _p = 0; _p < _info.path_count; _p++) {
            b8 _closed = false;
            const f32* _raw = NULL;
            rde_svg_document_path(_a->doc, _s, _p, &_raw, &_closed);
            const u32 _n = rde_svg_document_path_flatten(_a->doc, _s, _p, _tol, NULL, 0u);
            if(_n < 2u) {
                continue;
            }
            rde_arr_clear(&_flat);
            rde_vec_2F* _f = (rde_vec_2F*)rde_arr_add_n(&_flat, _n);
            rde_svg_document_path_flatten(_a->doc, _s, _p, _tol, _f, _n);
            // (into the symbol's box: its middle the box's, Y up)
            u32 _m = _n;
            if(_closed && _m > 2u && rde_math_abs_f32(_f[_m - 1u].x - _f[0].x) + rde_math_abs_f32(_f[_m - 1u].y - _f[0].y) < 1e-4f) {
                _m--;   // (its last point its first again)
            }
            const u32 _first = (u32)rde_arr_length(_points);
            for(u32 _i = 0; _i < _m; _i++) {
                const fude_zoom_v2 _q = { ((f64)_f[_i].x - (f64)_a->w * 0.5) * _sx, ((f64)_a->h * 0.5 - (f64)_f[_i].y) * _sy };
                rde_arr_add(_points, (any)&_q);
            }
            u8 _pf = (u8)(_flags | (_closed ? FUDE_ZOOM_SYMBOL_CLOSED : 0u));
            if(!_closed) {
                _pf = (u8)(_pf & ~(FUDE_ZOOM_SYMBOL_SOLID | FUDE_ZOOM_SYMBOL_FILLED | FUDE_ZOOM_SYMBOL_TINTED));   // (a line fills nothing)
            }
            if(_outer != FUDE_ZOOM_NONE && _closed && (_flags & (FUDE_ZOOM_SYMBOL_SOLID | FUDE_ZOOM_SYMBOL_FILLED | FUDE_ZOOM_SYMBOL_TINTED))) {
                const fude_zoom_symbol_part* _o = &((const fude_zoom_symbol_part*)_parts->memory)[_outer];
                const fude_zoom_v2* _pts = (const fude_zoom_v2*)_points->memory;
                // (inside it by the even-odd rule: rde_math's)
                if(rde_math_point_in_polygon_2d_f64(_pts[_first], &_pts[_o->first], _o->count)) {
                    _pf |= FUDE_ZOOM_SYMBOL_JOIN;   // (a hole in it)
                }
            }
            const fude_zoom_symbol_part _part = { _first, _m, _pf, _fill, _line, _width };
            if(!(_pf & FUDE_ZOOM_SYMBOL_JOIN)) {
                _outer = (u32)rde_arr_length(_parts);
            }
            rde_arr_add(_parts, (any)&_part);
        }
    }
    rde_arr_free(&_flat);
    return (u32)rde_arr_length(_parts);
}

// See the header. Kinds are counted from the library first, so every kind has a slot and a kind's art is at its index.
// Each line of parts.txt is trimmed of a trailing '\r' and spaces and cut at 95 bytes; an id the library does not know
// is skipped. The hook is set even when no SVG loaded.
u32 fude_zoom_symbol_art_load(const c8* _folder) {
    if(rde_arr_is_inited(&FZSA)) {
        return 0;
    }
    c8 _path[512];
    rde_snprintf(_path, sizeof(_path), "%s/parts.txt", _folder);
    u32 _size = 0;
    u8* _list = fude_file_read(_path, &_size);
    if(_list == NULL) {
        return 0;
    }
    // (as many as there are symbols: a kind's art at its kind)
    u32 _kinds = 0;
    while(fude_zoom_symbol_info_of(_kinds) != NULL) {
        _kinds++;
    }
    FZSA   = rde_arr_new(sizeof(fzsa_art), rde_memory_allocator_get_default_std());
    rde_arr_resize(&FZSA, _kinds);
    FZSA_N = _kinds;
    fzsa_art* _art = (fzsa_art*)FZSA.memory;   // (sized once: it stays put)
    u32 _loaded = 0;
    for(u32 _at = 0; _at < _size;) {
        u32 _end = _at;
        while(_end < _size && _list[_end] != '\n') {
            _end++;
        }
        c8 _id[96];
        u32 _len = _end - _at;
        while(_len > 0u && (_list[_at + _len - 1u] == '\r' || _list[_at + _len - 1u] == ' ')) {
            _len--;
        }
        _len = _len < sizeof(_id) - 1u ? _len : (u32)sizeof(_id) - 1u;
        rde_memcpy(_id, &_list[_at], _len);
        _id[_len] = 0;
        _at = _end + 1u;
        const u32 _kind = _id[0] != 0 && _id[0] != '#' ? fude_zoom_symbol_find(_id) : FUDE_ZOOM_NONE;
        if(_kind == FUDE_ZOOM_NONE || _kind >= _kinds) {
            continue;
        }
        c8 _file[256];
        for(u32 _i = 0; _i <= _len; _i++) {
            _file[_i] = _id[_i] == ' ' ? '_' : _id[_i];
        }
        rde_snprintf(_path, sizeof(_path), "%s/%s.svg", _folder, _file);
        u32 _bytes = 0;
        u8* _svg = fude_file_read(_path, &_bytes);
        if(_svg == NULL) {
            rde_log_color(RDE_LOG_COLOR_YELLOW, "symbols: no art %s", _path);
            continue;
        }
        rde_svg_document* _doc = rde_svg_document_load_from_memory((const c8*)_svg, _bytes, NULL);
        fude_file_free(_svg);
        if(_doc == NULL) {
            rde_log_color(RDE_LOG_COLOR_YELLOW, "symbols: art %s could not be read", _path);
            continue;
        }
        _art[_kind].doc = _doc;
        rde_svg_document_get_size(_doc, &_art[_kind].w, &_art[_kind].h);
        _loaded++;
    }
    fude_file_free(_list);
    fude_zoom_symbol_set_art(fzsa_parts);
    return _loaded;
}

// See the header. Kinds without art have a NULL document, which rde_svg_document_destroy ignores.
void fude_zoom_symbol_art_free(void) {
    for(u32 _i = 0; _i < FZSA_N; _i++) {
        rde_svg_document_destroy(((fzsa_art*)FZSA.memory)[_i].doc);
    }
    if(rde_arr_is_inited(&FZSA)) {
        rde_arr_free(&FZSA);
    }
    FZSA_N = 0;
    fude_zoom_symbol_set_art(NULL);
}
