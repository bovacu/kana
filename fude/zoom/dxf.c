// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/dxf.h"
#include "drawing/base/utf8.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUDE_ZOOM_DXF_NAME   32u      // a layer's name, NUL included
#define FUDE_ZOOM_DXF_TEXT   255u     // R12's longest string
#define FUDE_ZOOM_DXF_TURN   2e-6     // degrees: closer angles could come out the same at six decimals
#define FUDE_ZOOM_DXF_ROT    1e-3     // degrees two duplicate texts' angles may differ by
#define FUDE_ZOOM_DXF_ACI    7u       // white on black, black on white: layer "0"'s
#define FUDE_ZOOM_DXF_PI     3.14159265358979323846

enum {
    FUDE_ZOOM_DXF_LINE,
    FUDE_ZOOM_DXF_POLYLINE,
    FUDE_ZOOM_DXF_CIRCLE,
    FUDE_ZOOM_DXF_ARC,
    FUDE_ZOOM_DXF_TEXT_KIND,
};

typedef struct {
    c8 name[FUDE_ZOOM_DXF_NAME];
    u8 aci;
} fude_zoom_dxf_layer_def;

typedef struct {
    u8  kind;
    b8  closed;
    u32 layer;
    u32 first;      // its first point in xy (a centre or insertion point for the others)
    u32 count;      // points
    f64 r;          // circle, arc: radius; text: height
    f64 angle;      // arc: start; text: rotation (degrees, [0, 360))
    f64 sweep;      // arc: degrees counter-clockwise from the start, (0, 360)
    u32 text;       // text: where its escaped string starts in strings
} fude_zoom_dxf_entity;

struct fude_zoom_dxf {
    rde_arr TYPE(fude_zoom_dxf_layer_def) layers;
    rde_arr TYPE(fude_zoom_dxf_entity)    entities;
    rde_arr TYPE(f64)                     xy;
    rde_arr TYPE(c8)                      strings;   // each NUL-terminated
};

typedef struct { f64 x, y; } fude_zoom_dxf_v2;

RDE_INTERNAL f64 fude_zoom_dxf_finite(f64 _v) {
    return isfinite(_v) ? _v : 0.0;
}

// An angle in [0, 360); one that six decimals would round up to 360 is 0.
RDE_INTERNAL f64 fude_zoom_dxf_degrees(f64 _a) {
    f64 _r = fmod(fude_zoom_dxf_finite(_a), 360.0);
    if(_r < 0.0) {
        _r += 360.0;
    }
    return _r >= 360.0 - 5e-7 ? 0.0 : _r;
}

RDE_INTERNAL const f64* fude_zoom_dxf_points(const fude_zoom_dxf* _d, const fude_zoom_dxf_entity* _e) {
    return (const f64*)_d->xy.memory + (usize)_e->first * 2u;
}

RDE_INTERNAL const fude_zoom_dxf_entity* fude_zoom_dxf_entity_at(const fude_zoom_dxf* _d, u32 _i) {
    return (const fude_zoom_dxf_entity*)_d->entities.memory + _i;
}

RDE_INTERNAL fude_zoom_dxf_v2 fude_zoom_dxf_on_arc(const fude_zoom_dxf* _d, const fude_zoom_dxf_entity* _e, f64 _deg) {
    const f64* _c = fude_zoom_dxf_points(_d, _e);
    const f64  _a = _deg * FUDE_ZOOM_DXF_PI / 180.0;
    return (fude_zoom_dxf_v2){ _c[0] + _e->r * cos(_a), _c[1] + _e->r * sin(_a) };
}

// --- building --------------------------------------------------------------------------------------

fude_zoom_dxf* fude_zoom_dxf_new(void) {
    fude_zoom_dxf* _d = (fude_zoom_dxf*)calloc(1, sizeof(fude_zoom_dxf));
    if(_d == NULL) {
        return NULL;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _d->layers   = rde_arr_new(sizeof(fude_zoom_dxf_layer_def), _heap);
    _d->entities = rde_arr_new(sizeof(fude_zoom_dxf_entity), _heap);
    _d->xy       = rde_arr_new(sizeof(f64), _heap);
    _d->strings  = rde_arr_new(sizeof(c8), _heap);
    fude_zoom_dxf_layer_def _zero = { "0", FUDE_ZOOM_DXF_ACI };
    rde_arr_add(&_d->layers, (any)&_zero);
    return _d;
}

void fude_zoom_dxf_free(fude_zoom_dxf* _d) {
    if(_d == NULL) {
        return;
    }
    rde_arr_free(&_d->layers);
    rde_arr_free(&_d->entities);
    rde_arr_free(&_d->xy);
    rde_arr_free(&_d->strings);
    free(_d);
}

RDE_INTERNAL b8 fude_zoom_dxf_same_name(const c8* _a, const c8* _b) {
    for(;; _a++, _b++) {
        c8 _x = *_a, _y = *_b;
        if(_x >= 'a' && _x <= 'z') {
            _x = (c8)(_x - 'a' + 'A');
        }
        if(_y >= 'a' && _y <= 'z') {
            _y = (c8)(_y - 'a' + 'A');
        }
        if(_x != _y) {
            return false;
        }
        if(_x == 0) {
            return true;
        }
    }
}

u32 fude_zoom_dxf_layer(fude_zoom_dxf* _d, const c8* _name, u8 _aci) {
    if(_d == NULL) {
        return 0;
    }
    // A character at a time, so that one accented letter is one '_', not two.
    fude_zoom_dxf_layer_def _l = { "", _aci == 0 ? (u8)FUDE_ZOOM_DXF_ACI : _aci };
    u32       _n = 0;
    const c8* _s = _name != NULL ? _name : "";
    while(_n + 1u < FUDE_ZOOM_DXF_NAME) {
        const u32 _cp = fude_utf8_next(&_s);
        if(_cp == 0) {
            break;
        }
        const b8 _keep = (_cp >= 'a' && _cp <= 'z') || (_cp >= 'A' && _cp <= 'Z') || (_cp >= '0' && _cp <= '9') || _cp == '_' || _cp == '-';
        _l.name[_n++] = _keep ? (c8)_cp : '_';
    }
    _l.name[_n] = 0;
    if(_n == 0) {
        return 0;
    }
    const u32 _count = (u32)rde_arr_length(&_d->layers);
    for(u32 _i = 0; _i < _count; _i++) {
        if(fude_zoom_dxf_same_name(((const fude_zoom_dxf_layer_def*)_d->layers.memory)[_i].name, _l.name)) {
            return _i;
        }
    }
    rde_arr_add(&_d->layers, (any)&_l);
    return _count;
}

// A new entity, its _count points (if any) taken from _xy, made finite.
RDE_INTERNAL fude_zoom_dxf_entity* fude_zoom_dxf_add(fude_zoom_dxf* _d, u8 _kind, u32 _layer, const f64* _xy, u32 _count) {
    fude_zoom_dxf_entity _e;
    memset(&_e, 0, sizeof _e);
    _e.kind  = _kind;
    _e.layer = _layer < (u32)rde_arr_length(&_d->layers) ? _layer : 0u;
    _e.first = (u32)(rde_arr_length(&_d->xy) / 2u);
    _e.count = _count;
    if(_count > 0) {
        f64* _to = (f64*)rde_arr_add_n(&_d->xy, (usize)_count * 2u);
        for(u32 _i = 0; _i < _count * 2u; _i++) {
            _to[_i] = fude_zoom_dxf_finite(_xy[_i]);
        }
    }
    return (fude_zoom_dxf_entity*)rde_arr_add(&_d->entities, (any)&_e);
}

void fude_zoom_dxf_line(fude_zoom_dxf* _d, u32 _layer, f64 _x1, f64 _y1, f64 _x2, f64 _y2) {
    if(_d == NULL) {
        return;
    }
    const f64 _xy[4] = { _x1, _y1, _x2, _y2 };
    fude_zoom_dxf_add(_d, FUDE_ZOOM_DXF_LINE, _layer, _xy, 2u);
}

void fude_zoom_dxf_polyline(fude_zoom_dxf* _d, u32 _layer, const f64* _xy, u32 _count, b8 _closed) {
    if(_d == NULL || _xy == NULL) {
        return;
    }
    if(_closed && _count >= 3u && _xy[0] == _xy[(_count - 1u) * 2u] && _xy[1] == _xy[(_count - 1u) * 2u + 1u]) {
        _count--;
    }
    if(_count < 2u) {
        return;
    }
    fude_zoom_dxf_add(_d, FUDE_ZOOM_DXF_POLYLINE, _layer, _xy, _count)->closed = _closed ? true : false;
}

void fude_zoom_dxf_circle(fude_zoom_dxf* _d, u32 _layer, f64 _cx, f64 _cy, f64 _r) {
    if(_d == NULL) {
        return;
    }
    const f64 _c[2] = { _cx, _cy };
    fude_zoom_dxf_add(_d, FUDE_ZOOM_DXF_CIRCLE, _layer, _c, 1u)->r = fabs(fude_zoom_dxf_finite(_r));
}

void fude_zoom_dxf_arc(fude_zoom_dxf* _d, u32 _layer, f64 _cx, f64 _cy, f64 _r, f64 _start_deg, f64 _end_deg) {
    if(_d == NULL) {
        return;
    }
    const f64 _turned = fude_zoom_dxf_finite(_end_deg) - fude_zoom_dxf_finite(_start_deg);
    f64       _sweep  = fmod(_turned, 360.0);
    if(_sweep < 0.0) {
        _sweep += 360.0;
    }
    // A whole turn, or near enough that both angles would be written the same: a circle.
    if(fabs(_turned) >= 360.0 - FUDE_ZOOM_DXF_TURN || _sweep > 360.0 - FUDE_ZOOM_DXF_TURN) {
        fude_zoom_dxf_circle(_d, _layer, _cx, _cy, _r);
        return;
    }
    if(_sweep < FUDE_ZOOM_DXF_TURN) {
        return;
    }
    const f64             _c[2] = { _cx, _cy };
    fude_zoom_dxf_entity* _e    = fude_zoom_dxf_add(_d, FUDE_ZOOM_DXF_ARC, _layer, _c, 1u);
    _e->r     = fabs(fude_zoom_dxf_finite(_r));
    _e->angle = fude_zoom_dxf_degrees(_start_deg);
    _e->sweep = _sweep;
}

// One character's DXF spelling into _out (room for 16). _next: what follows it.
RDE_INTERNAL u32 fude_zoom_dxf_escape(u32 _cp, const c8* _next, c8* _out) {
    if(_cp < 0x20u || _cp == 0x7Fu) {
        _out[0] = ' ';
        return 1u;
    }
    if(_cp == '%' && _next[0] == '%') {
        memcpy(_out, "%%%", 3u);
        return 3u;
    }
    if(_cp == '\\' && (_next[0] == 'U' || _next[0] == 'u' || _next[0] == 'M' || _next[0] == 'm') && _next[1] == '+') {
        memcpy(_out, "\\U+005C", 7u);
        return 7u;
    }
    if(_cp < 0x80u) {
        _out[0] = (c8)_cp;
        return 1u;
    }
    if(_cp <= 0xFFFFu) {
        return (u32)snprintf(_out, 16u, "\\U+%04X", (unsigned)_cp);
    }
    const u32 _v = _cp - 0x10000u;
    return (u32)snprintf(_out, 16u, "\\U+%04X\\U+%04X", (unsigned)(0xD800u + (_v >> 10)), (unsigned)(0xDC00u + (_v & 0x3FFu)));
}

void fude_zoom_dxf_text(fude_zoom_dxf* _d, u32 _layer, f64 _x, f64 _y, f64 _height, f64 _rotation_deg, const c8* _utf8) {
    if(_d == NULL || _utf8 == NULL) {
        return;
    }
    c8        _text[FUDE_ZOOM_DXF_TEXT + 1u];
    u32       _n = 0;
    const c8* _s = _utf8;
    for(;;) {
        const u32 _cp = fude_utf8_next(&_s);
        if(_cp == 0) {
            break;
        }
        c8        _one[16];
        const u32 _k = fude_zoom_dxf_escape(_cp, _s, _one);
        if(_n + _k > FUDE_ZOOM_DXF_TEXT) {
            break;
        }
        memcpy(_text + _n, _one, _k);
        _n += _k;
    }
    if(_n == 0) {
        return;
    }
    _text[_n] = 0;
    const u32 _at = (u32)rde_arr_length(&_d->strings);
    memcpy(rde_arr_add_n(&_d->strings, _n + 1u), _text, _n + 1u);
    const f64             _p[2] = { _x, _y };
    fude_zoom_dxf_entity* _e    = fude_zoom_dxf_add(_d, FUDE_ZOOM_DXF_TEXT_KIND, _layer, _p, 1u);
    _e->r     = fabs(fude_zoom_dxf_finite(_height));
    _e->r     = _e->r > 0.0 ? _e->r : 1.0;
    _e->angle = fude_zoom_dxf_degrees(_rotation_deg);
    _e->text  = _at;
}

// --- writing ---------------------------------------------------------------------------------------

// Up to six decimals, no trailing zeros, no exponent; by hand, as printf's
// decimal point follows the locale.
RDE_INTERNAL void fude_zoom_dxf_number(c8* _buf, usize _size, f64 _v) {
    _v = fude_zoom_dxf_finite(_v);
    if(fabs(_v) >= 1e9) {
        // A thousand kilometres and more: no decimals (so no decimal point to
        // print), and a millionth would be noise anyway.
        snprintf(_buf, _size, "%.0f", _v);
        return;
    }
    const long long          _q     = llround(_v * 1e6);   // under 1e15: exact in a double and a long long
    const unsigned long long _m     = (unsigned long long)(_q < 0 ? -_q : _q);
    const unsigned long long _whole = _m / 1000000ull;
    unsigned long long       _frac  = _m % 1000000ull;
    if(_frac == 0) {
        snprintf(_buf, _size, "%s%llu", _q < 0 ? "-" : "", _whole);   // never "-0": a zero _q has no sign
        return;
    }
    int _digits = 6;
    while(_frac % 10ull == 0) {
        _frac /= 10ull;
        _digits--;
    }
    snprintf(_buf, _size, "%s%llu.%0*llu", _q < 0 ? "-" : "", _whole, _digits, _frac);
}

RDE_INTERNAL void fude_zoom_dxf_put(fude_bytes* _out, i32 _code, const c8* _value) {
    c8        _head[16];
    const int _n = snprintf(_head, sizeof _head, "%3d\r\n", (int)_code);
    fude_put_data(_out, _head, (u32)_n);
    fude_put_data(_out, _value, (u32)strlen(_value));
    fude_put_data(_out, "\r\n", 2u);
}

RDE_INTERNAL void fude_zoom_dxf_put_int(fude_bytes* _out, i32 _code, i32 _v) {
    c8 _buf[16];
    snprintf(_buf, sizeof _buf, "%d", (int)_v);
    fude_zoom_dxf_put(_out, _code, _buf);
}

RDE_INTERNAL void fude_zoom_dxf_put_num(fude_bytes* _out, i32 _code, f64 _v) {
    c8 _buf[400];   // room for the largest double's whole digits
    fude_zoom_dxf_number(_buf, sizeof _buf, _v);
    fude_zoom_dxf_put(_out, _code, _buf);
}

// A point at _code (10, 11...), its z 0: R12 readers expect all three.
RDE_INTERNAL void fude_zoom_dxf_put_point(fude_bytes* _out, i32 _code, f64 _x, f64 _y) {
    fude_zoom_dxf_put_num(_out, _code, _x);
    fude_zoom_dxf_put_num(_out, _code + 10, _y);
    fude_zoom_dxf_put_num(_out, _code + 20, 0.0);
}

RDE_INTERNAL void fude_zoom_dxf_extend(f64* _box, f64 _x, f64 _y) {
    _box[0] = fmin(_box[0], _x);
    _box[1] = fmin(_box[1], _y);
    _box[2] = fmax(_box[2], _x);
    _box[3] = fmax(_box[3], _y);
}

// min x, min y, max x, max y of everything; all 0 when there is nothing.
RDE_INTERNAL void fude_zoom_dxf_extents(const fude_zoom_dxf* _d, f64* _box) {
    const u32 _n = (u32)rde_arr_length(&_d->entities);
    _box[0] = _box[1] = INFINITY;
    _box[2] = _box[3] = -INFINITY;
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_dxf_entity* _e = fude_zoom_dxf_entity_at(_d, _i);
        const f64*                  _p = fude_zoom_dxf_points(_d, _e);
        switch(_e->kind) {
            case FUDE_ZOOM_DXF_CIRCLE:
                fude_zoom_dxf_extend(_box, _p[0] - _e->r, _p[1] - _e->r);
                fude_zoom_dxf_extend(_box, _p[0] + _e->r, _p[1] + _e->r);
                break;
            case FUDE_ZOOM_DXF_ARC: {
                const fude_zoom_dxf_v2 _a = fude_zoom_dxf_on_arc(_d, _e, _e->angle);
                const fude_zoom_dxf_v2 _b = fude_zoom_dxf_on_arc(_d, _e, _e->angle + _e->sweep);
                fude_zoom_dxf_extend(_box, _a.x, _a.y);
                fude_zoom_dxf_extend(_box, _b.x, _b.y);
                // The quarters of the circle the arc passes through.
                for(u32 _q = 0; _q < 4u; _q++) {
                    if(fude_zoom_dxf_degrees(90.0 * _q - _e->angle) <= _e->sweep) {
                        const f64 _dx[4] = { 1.0, 0.0, -1.0, 0.0 }, _dy[4] = { 0.0, 1.0, 0.0, -1.0 };
                        fude_zoom_dxf_extend(_box, _p[0] + _dx[_q] * _e->r, _p[1] + _dy[_q] * _e->r);
                    }
                }
                break;
            }
            default:
                for(u32 _k = 0; _k < _e->count; _k++) {
                    fude_zoom_dxf_extend(_box, _p[_k * 2u], _p[_k * 2u + 1u]);
                }
                break;
        }
    }
    if(_n == 0) {
        _box[0] = _box[1] = _box[2] = _box[3] = 0.0;
    }
}

RDE_INTERNAL void fude_zoom_dxf_tables(const fude_zoom_dxf* _d, fude_bytes* _out) {
    fude_zoom_dxf_put(_out, 0, "SECTION");
    fude_zoom_dxf_put(_out, 2, "TABLES");

    fude_zoom_dxf_put(_out, 0, "TABLE");
    fude_zoom_dxf_put(_out, 2, "LTYPE");
    fude_zoom_dxf_put_int(_out, 70, 1);
    fude_zoom_dxf_put(_out, 0, "LTYPE");
    fude_zoom_dxf_put(_out, 2, "CONTINUOUS");
    fude_zoom_dxf_put_int(_out, 70, 0);
    fude_zoom_dxf_put(_out, 3, "Solid line");
    fude_zoom_dxf_put_int(_out, 72, 65);
    fude_zoom_dxf_put_int(_out, 73, 0);
    fude_zoom_dxf_put_num(_out, 40, 0.0);
    fude_zoom_dxf_put(_out, 0, "ENDTAB");

    const u32 _layers = (u32)rde_arr_length(&_d->layers);
    fude_zoom_dxf_put(_out, 0, "TABLE");
    fude_zoom_dxf_put(_out, 2, "LAYER");
    fude_zoom_dxf_put_int(_out, 70, (i32)_layers);
    for(u32 _i = 0; _i < _layers; _i++) {
        const fude_zoom_dxf_layer_def* _l = (const fude_zoom_dxf_layer_def*)_d->layers.memory + _i;
        fude_zoom_dxf_put(_out, 0, "LAYER");
        fude_zoom_dxf_put(_out, 2, _l->name);
        fude_zoom_dxf_put_int(_out, 70, 0);
        fude_zoom_dxf_put_int(_out, 62, _l->aci);
        fude_zoom_dxf_put(_out, 6, "CONTINUOUS");
    }
    fude_zoom_dxf_put(_out, 0, "ENDTAB");

    fude_zoom_dxf_put(_out, 0, "TABLE");
    fude_zoom_dxf_put(_out, 2, "STYLE");
    fude_zoom_dxf_put_int(_out, 70, 1);
    fude_zoom_dxf_put(_out, 0, "STYLE");
    fude_zoom_dxf_put(_out, 2, "STANDARD");
    fude_zoom_dxf_put_int(_out, 70, 0);
    fude_zoom_dxf_put_num(_out, 40, 0.0);    // no fixed height: each text has its own
    fude_zoom_dxf_put_num(_out, 41, 1.0);
    fude_zoom_dxf_put_num(_out, 50, 0.0);
    fude_zoom_dxf_put_int(_out, 71, 0);
    fude_zoom_dxf_put_num(_out, 42, 2.5);
    fude_zoom_dxf_put(_out, 3, "txt");
    fude_zoom_dxf_put(_out, 4, "");
    fude_zoom_dxf_put(_out, 0, "ENDTAB");

    fude_zoom_dxf_put(_out, 0, "ENDSEC");
}

RDE_INTERNAL void fude_zoom_dxf_entity_put(const fude_zoom_dxf* _d, const fude_zoom_dxf_entity* _e, fude_bytes* _out) {
    const c8*  _layer = ((const fude_zoom_dxf_layer_def*)_d->layers.memory)[_e->layer].name;
    const f64* _p     = fude_zoom_dxf_points(_d, _e);
    switch(_e->kind) {
        case FUDE_ZOOM_DXF_LINE:
            fude_zoom_dxf_put(_out, 0, "LINE");
            fude_zoom_dxf_put(_out, 8, _layer);
            fude_zoom_dxf_put_point(_out, 10, _p[0], _p[1]);
            fude_zoom_dxf_put_point(_out, 11, _p[2], _p[3]);
            break;
        case FUDE_ZOOM_DXF_POLYLINE:
            fude_zoom_dxf_put(_out, 0, "POLYLINE");
            fude_zoom_dxf_put(_out, 8, _layer);
            fude_zoom_dxf_put_int(_out, 66, 1);           // vertices follow
            fude_zoom_dxf_put_point(_out, 10, 0.0, 0.0);  // the elevation, R12's dummy point
            fude_zoom_dxf_put_int(_out, 70, _e->closed ? 1 : 0);
            for(u32 _k = 0; _k < _e->count; _k++) {
                fude_zoom_dxf_put(_out, 0, "VERTEX");
                fude_zoom_dxf_put(_out, 8, _layer);
                fude_zoom_dxf_put_point(_out, 10, _p[_k * 2u], _p[_k * 2u + 1u]);
            }
            fude_zoom_dxf_put(_out, 0, "SEQEND");
            fude_zoom_dxf_put(_out, 8, _layer);
            break;
        case FUDE_ZOOM_DXF_CIRCLE:
            fude_zoom_dxf_put(_out, 0, "CIRCLE");
            fude_zoom_dxf_put(_out, 8, _layer);
            fude_zoom_dxf_put_point(_out, 10, _p[0], _p[1]);
            fude_zoom_dxf_put_num(_out, 40, _e->r);
            break;
        case FUDE_ZOOM_DXF_ARC:
            fude_zoom_dxf_put(_out, 0, "ARC");
            fude_zoom_dxf_put(_out, 8, _layer);
            fude_zoom_dxf_put_point(_out, 10, _p[0], _p[1]);
            fude_zoom_dxf_put_num(_out, 40, _e->r);
            fude_zoom_dxf_put_num(_out, 50, _e->angle);
            fude_zoom_dxf_put_num(_out, 51, fude_zoom_dxf_degrees(_e->angle + _e->sweep));
            break;
        default:
            fude_zoom_dxf_put(_out, 0, "TEXT");
            fude_zoom_dxf_put(_out, 8, _layer);
            fude_zoom_dxf_put_point(_out, 10, _p[0], _p[1]);
            fude_zoom_dxf_put_num(_out, 40, _e->r);
            fude_zoom_dxf_put(_out, 1, (const c8*)_d->strings.memory + _e->text);
            fude_zoom_dxf_put_num(_out, 50, _e->angle);
            break;
    }
}

b8 fude_zoom_dxf_finish(const fude_zoom_dxf* _d, fude_bytes* _out) {
    if(_d == NULL || _out == NULL || !rde_arr_is_inited(_out)) {
        return false;
    }
    f64 _box[4];
    fude_zoom_dxf_extents(_d, _box);

    fude_zoom_dxf_put(_out, 0, "SECTION");
    fude_zoom_dxf_put(_out, 2, "HEADER");
    fude_zoom_dxf_put(_out, 9, "$ACADVER");
    fude_zoom_dxf_put(_out, 1, "AC1009");
    fude_zoom_dxf_put(_out, 9, "$DWGCODEPAGE");     // the file is ASCII; this only stops readers asking
    fude_zoom_dxf_put(_out, 3, "ANSI_1252");
    fude_zoom_dxf_put(_out, 9, "$INSBASE");
    fude_zoom_dxf_put_point(_out, 10, 0.0, 0.0);
    fude_zoom_dxf_put(_out, 9, "$EXTMIN");
    fude_zoom_dxf_put_point(_out, 10, _box[0], _box[1]);
    fude_zoom_dxf_put(_out, 9, "$EXTMAX");
    fude_zoom_dxf_put_point(_out, 10, _box[2], _box[3]);
    fude_zoom_dxf_put(_out, 9, "$INSUNITS");
    fude_zoom_dxf_put_int(_out, 70, 4);
    fude_zoom_dxf_put(_out, 9, "$MEASUREMENT");
    fude_zoom_dxf_put_int(_out, 70, 1);
    fude_zoom_dxf_put(_out, 0, "ENDSEC");

    fude_zoom_dxf_tables(_d, _out);

    fude_zoom_dxf_put(_out, 0, "SECTION");
    fude_zoom_dxf_put(_out, 2, "BLOCKS");
    fude_zoom_dxf_put(_out, 0, "ENDSEC");

    fude_zoom_dxf_put(_out, 0, "SECTION");
    fude_zoom_dxf_put(_out, 2, "ENTITIES");
    const u32 _n = (u32)rde_arr_length(&_d->entities);
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_dxf_entity_put(_d, fude_zoom_dxf_entity_at(_d, _i), _out);
    }
    fude_zoom_dxf_put(_out, 0, "ENDSEC");
    fude_zoom_dxf_put(_out, 0, "EOF");
    return true;
}

// --- pre-flight ------------------------------------------------------------------------------------

typedef struct { f64 key; u32 index; } fude_zoom_dxf_sorted;
typedef struct { f64 x, y; u32 piece; b8 away; } fude_zoom_dxf_end;   // away: the piece goes further than _tiny from its start

RDE_INTERNAL int fude_zoom_dxf_by_key(const void* _a, const void* _b) {
    const fude_zoom_dxf_sorted* _x = (const fude_zoom_dxf_sorted*)_a;
    const fude_zoom_dxf_sorted* _y = (const fude_zoom_dxf_sorted*)_b;
    if(_x->key != _y->key) {
        return _x->key < _y->key ? -1 : 1;
    }
    return _x->index < _y->index ? -1 : (_x->index > _y->index ? 1 : 0);
}

RDE_INTERNAL int fude_zoom_dxf_by_x(const void* _a, const void* _b) {
    const f64 _x = ((const fude_zoom_dxf_end*)_a)->x, _y = ((const fude_zoom_dxf_end*)_b)->x;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL b8 fude_zoom_dxf_near(f64 _ax, f64 _ay, f64 _bx, f64 _by, f64 _tiny) {
    const f64 _dx = _ax - _bx, _dy = _ay - _by;
    return _dx * _dx + _dy * _dy <= _tiny * _tiny;
}

RDE_INTERNAL b8 fude_zoom_dxf_near_v(fude_zoom_dxf_v2 _a, fude_zoom_dxf_v2 _b, f64 _tiny) {
    return fude_zoom_dxf_near(_a.x, _a.y, _b.x, _b.y, _tiny);
}

RDE_INTERNAL b8 fude_zoom_dxf_is_path(const fude_zoom_dxf_entity* _e) {
    return _e->kind == FUDE_ZOOM_DXF_LINE || _e->kind == FUDE_ZOOM_DXF_POLYLINE;
}

// B's points against A's, B's _i-th at A's (_shift + _step * _i) mod _n.
RDE_INTERNAL b8 fude_zoom_dxf_same_run(const f64* _a, const f64* _b, u32 _n, u32 _shift, i32 _step, f64 _tiny) {
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _j = _step > 0 ? (_shift + _i) % _n : (_shift + _n - _i) % _n;
        if(!fude_zoom_dxf_near(_a[_j * 2u], _a[_j * 2u + 1u], _b[_i * 2u], _b[_i * 2u + 1u], _tiny)) {
            return false;
        }
    }
    return true;
}

RDE_INTERNAL b8 fude_zoom_dxf_same(const fude_zoom_dxf* _d, const fude_zoom_dxf_entity* _a, const fude_zoom_dxf_entity* _b, f64 _tiny) {
    if(_a->layer != _b->layer) {
        return false;
    }
    const f64* _p = fude_zoom_dxf_points(_d, _a);
    const f64* _q = fude_zoom_dxf_points(_d, _b);
    if(fude_zoom_dxf_is_path(_a) && fude_zoom_dxf_is_path(_b)) {
        const u32 _n = _a->count;
        if(_n != _b->count || _a->closed != _b->closed) {
            return false;
        }
        if(!_a->closed) {
            return fude_zoom_dxf_same_run(_p, _q, _n, 0u, 1, _tiny) || fude_zoom_dxf_same_run(_p, _q, _n, _n - 1u, -1, _tiny);
        }
        for(u32 _k = 0; _k < _n; _k++) {
            if(fude_zoom_dxf_same_run(_p, _q, _n, _k, 1, _tiny) || fude_zoom_dxf_same_run(_p, _q, _n, _k, -1, _tiny)) {
                return true;
            }
        }
        return false;
    }
    if(_a->kind != _b->kind || !fude_zoom_dxf_near(_p[0], _p[1], _q[0], _q[1], _tiny) || fabs(_a->r - _b->r) > _tiny) {
        return false;
    }
    switch(_a->kind) {
        case FUDE_ZOOM_DXF_CIRCLE:
            return true;
        case FUDE_ZOOM_DXF_ARC:
            return fude_zoom_dxf_near_v(fude_zoom_dxf_on_arc(_d, _a, _a->angle), fude_zoom_dxf_on_arc(_d, _b, _b->angle), _tiny) &&
                   fude_zoom_dxf_near_v(fude_zoom_dxf_on_arc(_d, _a, _a->angle + _a->sweep), fude_zoom_dxf_on_arc(_d, _b, _b->angle + _b->sweep), _tiny) &&
                   fude_zoom_dxf_near_v(fude_zoom_dxf_on_arc(_d, _a, _a->angle + _a->sweep * 0.5), fude_zoom_dxf_on_arc(_d, _b, _b->angle + _b->sweep * 0.5), _tiny);
        default: {
            const f64 _turn = fude_zoom_dxf_degrees(_a->angle - _b->angle);
            return fmin(_turn, 360.0 - _turn) <= FUDE_ZOOM_DXF_ROT &&
                   strcmp((const c8*)_d->strings.memory + _a->text, (const c8*)_d->strings.memory + _b->text) == 0;
        }
    }
}

// The smallest x of the points that must match for two entities to be the
// same: two the same have keys within _tiny of each other.
RDE_INTERNAL f64 fude_zoom_dxf_key(const fude_zoom_dxf* _d, const fude_zoom_dxf_entity* _e) {
    const f64* _p   = fude_zoom_dxf_points(_d, _e);
    f64        _min = _p[0];
    for(u32 _k = 1; _k < _e->count; _k++) {
        _min = fmin(_min, _p[_k * 2u]);
    }
    return _min;
}

// Marks _dup[i] for every entity the same as an earlier one. How many.
RDE_INTERNAL u32 fude_zoom_dxf_duplicates(const fude_zoom_dxf* _d, f64 _tiny, b8* _dup) {
    const u32             _n    = (u32)rde_arr_length(&_d->entities);
    fude_zoom_dxf_sorted* _sort = (fude_zoom_dxf_sorted*)malloc((usize)_n * sizeof(fude_zoom_dxf_sorted));
    if(_sort == NULL) {
        return 0;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _sort[_i].key   = fude_zoom_dxf_key(_d, fude_zoom_dxf_entity_at(_d, _i));
        _sort[_i].index = _i;
    }
    qsort(_sort, _n, sizeof(fude_zoom_dxf_sorted), fude_zoom_dxf_by_key);
    u32 _count = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        for(u32 _j = _i + 1u; _j < _n && _sort[_j].key - _sort[_i].key <= _tiny; _j++) {
            const u32 _a = _sort[_i].index, _b = _sort[_j].index;
            const u32 _later = _a > _b ? _a : _b;
            if(!_dup[_later] && fude_zoom_dxf_same(_d, fude_zoom_dxf_entity_at(_d, _a), fude_zoom_dxf_entity_at(_d, _b), _tiny)) {
                _dup[_later] = true;
                _count++;
            }
        }
    }
    free(_sort);
    return _count;
}

RDE_INTERNAL u32 fude_zoom_dxf_root(u32* _parent, u32 _i) {
    while(_parent[_i] != _i) {
        _parent[_i] = _parent[_parent[_i]];
        _i = _parent[_i];
    }
    return _i;
}

// Open contours among the pieces that are not duplicates (see the header),
// with room for every entity as a piece: _piece and _parent _n each, _loose _n,
// _met and _ends twice _n.
RDE_INTERNAL u32 fude_zoom_dxf_open_in(const fude_zoom_dxf* _d, f64 _tiny, const b8* _dup, u32* _piece, u32* _parent, b8* _loose, b8* _met,
                                       fude_zoom_dxf_end* _ends) {
    const u32 _n      = (u32)rde_arr_length(&_d->entities);
    u32       _pieces = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_dxf_entity* _e = fude_zoom_dxf_entity_at(_d, _i);
        fude_zoom_dxf_v2            _a, _b;
        b8                          _away = false;
        if(_dup[_i] || _e->closed) {
            continue;
        }
        if(fude_zoom_dxf_is_path(_e)) {
            const f64* _p = fude_zoom_dxf_points(_d, _e);
            _a = (fude_zoom_dxf_v2){ _p[0], _p[1] };
            _b = (fude_zoom_dxf_v2){ _p[(_e->count - 1u) * 2u], _p[(_e->count - 1u) * 2u + 1u] };
            for(u32 _k = 1; _k + 1u < _e->count && !_away; _k++) {
                _away = !fude_zoom_dxf_near(_p[0], _p[1], _p[_k * 2u], _p[_k * 2u + 1u], _tiny);
            }
        } else if(_e->kind == FUDE_ZOOM_DXF_ARC) {
            _a    = fude_zoom_dxf_on_arc(_d, _e, _e->angle);
            _b    = fude_zoom_dxf_on_arc(_d, _e, _e->angle + _e->sweep);
            _away = !fude_zoom_dxf_near_v(_a, fude_zoom_dxf_on_arc(_d, _e, _e->angle + _e->sweep * 0.5), _tiny);
        } else {
            continue;
        }
        _piece[_pieces]          = _i;
        _parent[_pieces]         = _pieces;
        _ends[_pieces * 2u]      = (fude_zoom_dxf_end){ _a.x, _a.y, _pieces, _away };
        _ends[_pieces * 2u + 1u] = (fude_zoom_dxf_end){ _b.x, _b.y, _pieces, _away };
        _pieces++;
    }
    if(_pieces == 0) {
        return 0;
    }
    const u32 _count = _pieces * 2u;
    qsort(_ends, _count, sizeof(fude_zoom_dxf_end), fude_zoom_dxf_by_x);
    for(u32 _i = 0; _i < _count; _i++) {
        for(u32 _j = _i + 1u; _j < _count && _ends[_j].x - _ends[_i].x <= _tiny; _j++) {
            const u32                   _pa = _ends[_i].piece, _pb = _ends[_j].piece;
            const fude_zoom_dxf_entity* _ea = fude_zoom_dxf_entity_at(_d, _piece[_pa]);
            // Contours are each layer's own (a score line does not close a cut).
            // A piece's own two ends close it only once it has gone away from
            // them: a speck, or a line, does not close on itself.
            if(_ea->layer != fude_zoom_dxf_entity_at(_d, _piece[_pb])->layer || (_pa == _pb && !_ends[_i].away)) {
                continue;
            }
            if(fude_zoom_dxf_near(_ends[_i].x, _ends[_i].y, _ends[_j].x, _ends[_j].y, _tiny)) {
                _met[_i] = _met[_j] = true;
                _parent[fude_zoom_dxf_root(_parent, _pa)] = fude_zoom_dxf_root(_parent, _pb);
            }
        }
    }
    for(u32 _i = 0; _i < _count; _i++) {
        if(!_met[_i]) {
            _loose[fude_zoom_dxf_root(_parent, _ends[_i].piece)] = true;
        }
    }
    u32 _open = 0;
    for(u32 _i = 0; _i < _pieces; _i++) {
        if(fude_zoom_dxf_root(_parent, _i) == _i && _loose[_i]) {
            _open++;
        }
    }
    return _open;
}

RDE_INTERNAL u32 fude_zoom_dxf_open(const fude_zoom_dxf* _d, f64 _tiny, const b8* _dup) {
    const usize        _n      = rde_arr_length(&_d->entities);
    u32*               _piece  = (u32*)malloc(_n * sizeof(u32));
    u32*               _parent = (u32*)malloc(_n * sizeof(u32));
    b8*                _loose  = (b8*)calloc(_n, sizeof(b8));
    b8*                _met    = (b8*)calloc(_n * 2u, sizeof(b8));
    fude_zoom_dxf_end* _ends   = (fude_zoom_dxf_end*)malloc(_n * 2u * sizeof(fude_zoom_dxf_end));
    u32                _open   = 0;
    if(_piece != NULL && _parent != NULL && _loose != NULL && _met != NULL && _ends != NULL) {
        _open = fude_zoom_dxf_open_in(_d, _tiny, _dup, _piece, _parent, _loose, _met, _ends);
    }
    free(_piece);
    free(_parent);
    free(_loose);
    free(_met);
    free(_ends);
    return _open;
}

fude_zoom_dxf_check fude_zoom_dxf_preflight(const fude_zoom_dxf* _d, f64 _tiny_mm) {
    fude_zoom_dxf_check _c;
    memset(&_c, 0, sizeof _c);
    if(_d == NULL) {
        return _c;
    }
    const f64 _tiny = _tiny_mm >= 0.0 ? _tiny_mm : 0.0;   // NaN too
    const u32 _n    = (u32)rde_arr_length(&_d->entities);
    _c.entities = _n;
    if(_n == 0) {
        return _c;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_dxf_entity* _e = fude_zoom_dxf_entity_at(_d, _i);
        const f64*                  _p = fude_zoom_dxf_points(_d, _e);
        switch(_e->kind) {
            case FUDE_ZOOM_DXF_LINE:
            case FUDE_ZOOM_DXF_POLYLINE: {
                const u32 _sides = _e->closed ? _e->count : _e->count - 1u;
                for(u32 _k = 0; _k < _sides; _k++) {
                    const u32 _m = (_k + 1u) % _e->count;
                    if(hypot(_p[_m * 2u] - _p[_k * 2u], _p[_m * 2u + 1u] - _p[_k * 2u + 1u]) < _tiny) {
                        _c.tiny_segments++;
                    }
                }
                break;
            }
            case FUDE_ZOOM_DXF_CIRCLE:
                _c.tiny_segments += 2.0 * FUDE_ZOOM_DXF_PI * _e->r < _tiny ? 1u : 0u;
                break;
            case FUDE_ZOOM_DXF_ARC:
                _c.tiny_segments += _e->r * _e->sweep * FUDE_ZOOM_DXF_PI / 180.0 < _tiny ? 1u : 0u;
                break;
            default:
                break;
        }
    }
    b8* _dup = (b8*)calloc((usize)_n, sizeof(b8));
    if(_dup == NULL) {
        return _c;
    }
    _c.duplicates = fude_zoom_dxf_duplicates(_d, _tiny, _dup);
    _c.open_paths = fude_zoom_dxf_open(_d, _tiny, _dup);
    free(_dup);
    return _c;
}
