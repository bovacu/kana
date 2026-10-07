// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/symbolart.h"
#include "zoom/symbol.h"
#include "zoom/zoom.h"
#include "zoom/scene.h"
#include "drawing/base/kfile.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    rde_svg_document* doc;
    f32               w, h;
} fza_art;

RDE_INTERNAL rde_arr TYPE(fza_art) FZA = { 0 };   // by symbol kind
RDE_INTERNAL u32                  FZA_N = 0;

// Is _p inside the polygon _q (_n points; even-odd)?
RDE_INTERNAL b8 fza_inside(const fude_zoom_v2* _q, u32 _n, fude_zoom_v2 _p) {
    b8 _in = false;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        if((_q[_i].y > _p.y) != (_q[_j].y > _p.y) && _p.x < (_q[_j].x - _q[_i].x) * (_p.y - _q[_i].y) / (_q[_j].y - _q[_i].y) + _q[_i].x) {
            _in = !_in;
        }
    }
    return _in;
}

RDE_INTERNAL b8 fza_is(rde_color _c, u8 _v) {
    return _c.r == _v && _c.g == _v && _c.b == _v;
}

// Kind _kind's parts from its art (symbol.h's fude_zoom_symbol_art_fn).
RDE_INTERNAL u32 fza_parts(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    const fza_art* _a = _kind < FZA_N ? &((const fza_art*)FZA.memory)[_kind] : NULL;
    if(_a == NULL || _a->doc == NULL || !(_a->w > 0.0f) || !(_a->h > 0.0f)) {
        return 0;
    }
    const f64 _sx = 2.0 * _hw / (f64)_a->w, _sy = 2.0 * _hh / (f64)_a->h;
    const f32 _tol = fmaxf(_a->w, _a->h) / (f32)(_segments * 3u);   // (its curves as fine as the symbol is drawn)
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
            if(fza_is(_info.fill, 0u) && _info.fill.a == 255u) {
                _flags |= FUDE_ZOOM_SYMBOL_SOLID;
            } else if(fza_is(_info.fill, 255u) && _info.fill.a == 255u) {
                _flags |= FUDE_ZOOM_SYMBOL_FILLED;
            } else {
                _flags |= FUDE_ZOOM_SYMBOL_TINTED;
                _fill = _info.fill;
            }
        }
        if(_info.stroke_paint == RDE_SVG_PAINT_NONE || _info.stroke.a == 0u || !(_info.stroke_width > 0.0f)) {
            _flags |= FUDE_ZOOM_SYMBOL_NO_LINE;
        } else if(!fza_is(_info.stroke, 0u)) {
            _line = _info.stroke;
        }
        if(_info.dash_count > 0u) {
            _flags |= FUDE_ZOOM_SYMBOL_DASHED;
        }
        const f32 _width = _info.stroke_width * (f32)sqrt(fabs(_sx * _sy));
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
            if(_closed && _m > 2u && fabsf(_f[_m - 1u].x - _f[0].x) + fabsf(_f[_m - 1u].y - _f[0].y) < 1e-4f) {
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
                if(fza_inside(&_pts[_o->first], _o->count, _pts[_first])) {
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

u32 fude_zoom_symbol_art_load(const c8* _folder) {
    if(rde_arr_is_inited(&FZA)) {
        return 0;
    }
    c8 _path[512];
    snprintf(_path, sizeof(_path), "%s/parts.txt", _folder);
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
    FZA   = rde_arr_new(sizeof(fza_art), rde_memory_allocator_get_default_std());
    rde_arr_resize(&FZA, _kinds);
    FZA_N = _kinds;
    fza_art* _art = (fza_art*)FZA.memory;   // (sized once: it stays put)
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
        memcpy(_id, &_list[_at], _len);
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
        snprintf(_path, sizeof(_path), "%s/%s.svg", _folder, _file);
        u32 _bytes = 0;
        u8* _svg = fude_file_read(_path, &_bytes);
        if(_svg == NULL) {
            rde_log_color(RDE_LOG_COLOR_YELLOW, "fude: no art %s", _path);
            continue;
        }
        rde_svg_document* _doc = rde_svg_document_load_from_memory((const c8*)_svg, _bytes, NULL);
        fude_file_free(_svg);
        if(_doc == NULL) {
            rde_log_color(RDE_LOG_COLOR_YELLOW, "fude: art %s could not be read", _path);
            continue;
        }
        _art[_kind].doc = _doc;
        rde_svg_document_get_size(_doc, &_art[_kind].w, &_art[_kind].h);
        _loaded++;
    }
    fude_file_free(_list);
    fude_zoom_symbol_set_art(fza_parts);
    return _loaded;
}

void fude_zoom_symbol_art_free(void) {
    for(u32 _i = 0; _i < FZA_N; _i++) {
        rde_svg_document_destroy(((fza_art*)FZA.memory)[_i].doc);
    }
    if(rde_arr_is_inited(&FZA)) {
        rde_arr_free(&FZA);
    }
    FZA_N = 0;
    fude_zoom_symbol_set_art(NULL);
}
