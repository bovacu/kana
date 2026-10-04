// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/export.h"
#include "zoom/shape.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUDE_ZOOM_SVG_EXACT   0.25     // as the renderer: a transform's error under this many points
#define FUDE_ZOOM_SVG_MIN_PX  0.5      // a frame smaller than this on screen is not gone into
#define FUDE_ZOOM_SVG_SPECK   0.35     // a thing smaller than this on screen is left out
#define FUDE_ZOOM_SVG_RUN     0.08     // a stroke's width followed in runs no further apart than this share

typedef struct {
    const fude_zoom_scene* s;
    fude_bytes*            out;
    fude_zoom_v2           half;
    rde_arr TYPE(u32)              found;
    rde_arr TYPE(fude_zoom_qpoint) q;
    rde_arr TYPE(fude_zoom_v2)     pts;
    u32                    masks;      // fills' masks made so far (their ids)
} fude_zoom_svg;

RDE_INTERNAL void fude_zoom_svg_put(fude_zoom_svg* _w, const c8* _fmt, ...) {
    c8      _buf[512];
    va_list _args;
    va_start(_args, _fmt);
    const int _n = vsnprintf(_buf, sizeof _buf, _fmt, _args);
    va_end(_args);
    if(_n <= 0) {
        return;
    }
    if((usize)_n < sizeof _buf) {
        fude_put_data(_w->out, _buf, (u32)_n);
        return;
    }
    c8* _big = (c8*)malloc((usize)_n + 1u);
    if(_big == NULL) {
        return;
    }
    va_start(_args, _fmt);
    vsnprintf(_big, (usize)_n + 1u, _fmt, _args);
    va_end(_args);
    fude_put_data(_w->out, _big, (u32)_n);
    free(_big);
}

// A colour as SVG wants it: "#rrggbb", and how opaque it is.
RDE_INTERNAL void fude_zoom_svg_color(c8* _out, usize _size, rde_color _c) {
    snprintf(_out, _size, "#%02x%02x%02x", _c.r, _c.g, _c.b);
}

// A point of the screen (centre origin, Y up) where the SVG has it (top-left, Y down).
RDE_INTERNAL void fude_zoom_svg_point(fude_zoom_svg* _w, const c8* _cmd, fude_zoom_v2 _p) {
    fude_zoom_svg_put(_w, "%s%.2f %.2f", _cmd, _p.x + _w->half.x, _w->half.y - _p.y);
}

// --- strokes ---------------------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_svg_stroke(fude_zoom_svg* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    rde_arr_clear(&_w->q);
    rde_arr_add_n(&_w->q, _o->count);
    if(_o->count == 0 || !fude_zoom_scene_points(_w->s, _object, (fude_zoom_qpoint*)_w->q.memory)) {
        return;
    }
    const fude_zoom_qpoint* _q     = (const fude_zoom_qpoint*)_w->q.memory;
    const f64               _scale = fude_zoom_sim_scale(_to);
    const rde_color         _c     = fude_theme_resolve(_o->color);
    c8 _col[16];
    fude_zoom_svg_color(_col, sizeof _col, _c);
    c8 _alpha[32] = "";
    if(_c.a < 255u) {
        snprintf(_alpha, sizeof _alpha, " stroke-opacity=\"%.3f\"", (f64)_c.a / 255.0);
    }
    if(_o->count == 1u) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_q[0]));
        const f64          _r = fmax(fude_zoom_scene_radius_at(_o, &_q[0]) * _scale, 0.5);
        fude_zoom_svg_put(_w, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"%s\"%s/>\n", _p.x + _w->half.x, _w->half.y - _p.y, _r, _col,
                          _c.a < 255u ? " fill-opacity=\"0.5\"" : "");
        return;
    }
    // In runs of about one width (one run for an even pen), each its own path:
    // their round ends meet where they join.
    u32 _start = 0;
    while(_start + 1u < _o->count) {
        const f64 _w0 = fmax(fude_zoom_scene_radius_at(_o, &_q[_start]) * _scale, 0.5);
        u32 _end = _start + 1u;
        while(_end + 1u < _o->count && fabs(fmax(fude_zoom_scene_radius_at(_o, &_q[_end]) * _scale, 0.5) - _w0) <= _w0 * FUDE_ZOOM_SVG_RUN) {
            _end++;
        }
        fude_zoom_svg_put(_w, "<path d=\"");
        for(u32 _i = _start; _i <= _end; _i++) {
            fude_zoom_svg_point(_w, _i == _start ? "M" : " L", fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_q[_i])));
        }
        fude_zoom_svg_put(_w, "\" fill=\"none\" stroke=\"%s\"%s stroke-width=\"%.2f\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n", _col, _alpha, _w0 * 2.0);
        _start = _end;
    }
}

// --- shapes, fills, pictures -----------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_svg_shape(fude_zoom_svg* _w, u32 _object, fude_zoom_sim _to, f64 _size) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    b8 _closed;
    rde_arr_clear(&_w->pts);
    fude_zoom_scene_shape_outline(_w->s, _object, fude_zoom_shape_segments(_size), &_w->pts, &_closed);
    const u32 _k = (u32)rde_arr_length(&_w->pts);
    if(_k < 2u) {
        return;
    }
    const rde_color _line = fude_theme_resolve(_o->color);
    const b8        _filled = _closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED);
    const rde_color _fill = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_theme_resolve(_o->fill) : _line;
    c8 _lc[16], _fc[16];
    fude_zoom_svg_color(_lc, sizeof _lc, _line);
    fude_zoom_svg_color(_fc, sizeof _fc, _fill);
    fude_zoom_svg_put(_w, "<path d=\"");
    for(u32 _i = 0; _i < _k; _i++) {
        fude_zoom_svg_point(_w, _i == 0 ? "M" : " L", fude_zoom_sim_apply(_to, ((const fude_zoom_v2*)_w->pts.memory)[_i]));
    }
    fude_zoom_svg_put(_w, "%s\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n",
                      _closed ? " Z" : "", _filled ? _fc : "none", _lc, fmax((f64)_o->radius * _o->scale * fude_zoom_sim_scale(_to), 0.5) * 2.0);
}

// A fill: its outline (even-odd), what the eraser cut from it masked out.
RDE_INTERNAL void fude_zoom_svg_fill(fude_zoom_svg* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    rde_arr_clear(&_w->q);
    rde_arr_add_n(&_w->q, _o->count);
    if(_o->count < 3u || !fude_zoom_scene_points(_w->s, _object, (fude_zoom_qpoint*)_w->q.memory)) {
        return;
    }
    const fude_zoom_qpoint* _q = (const fude_zoom_qpoint*)_w->q.memory;
    u32 _outline = 0;
    while(_outline < _o->count && _q[_outline].time == _q[0].time) {
        _outline++;
    }
    c8 _col[16];
    const rde_color _c = fude_theme_resolve(_o->color);
    fude_zoom_svg_color(_col, sizeof _col, _c);
    c8 _mask[48] = "";
    if(_outline < _o->count) {
        const u32 _id = ++_w->masks;
        snprintf(_mask, sizeof _mask, " mask=\"url(#cut%u)\"", _id);
        fude_zoom_svg_put(_w, "<mask id=\"cut%u\" maskUnits=\"userSpaceOnUse\" x=\"0\" y=\"0\" width=\"%.0f\" height=\"%.0f\"><rect width=\"100%%\" height=\"100%%\" fill=\"white\"/><path d=\"",
                          _id, _w->half.x * 2.0, _w->half.y * 2.0);
        for(u32 _i = _outline; _i < _o->count; _i++) {
            const b8 _new = _i == _outline || _q[_i].time != _q[_i - 1u].time;
            fude_zoom_svg_point(_w, _new ? (_i == _outline ? "M" : " Z M") : " L", fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_q[_i])));
        }
        fude_zoom_svg_put(_w, " Z\" fill=\"black\"/></mask>\n");
    }
    fude_zoom_svg_put(_w, "<path d=\"");
    for(u32 _i = 0; _i < _outline; _i++) {
        fude_zoom_svg_point(_w, _i == 0 ? "M" : " L", fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_q[_i])));
    }
    fude_zoom_svg_put(_w, " Z\" fill=\"%s\" fill-rule=\"evenodd\"%s%s/>\n", _col, _mask, _c.a < 255u ? " fill-opacity=\"0.5\"" : "");
}

// A picture: itself, inside, placed by its transform (the SVG's Y runs down).
RDE_INTERNAL void fude_zoom_svg_image(fude_zoom_svg* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    f64       _hw, _hh;
    const u8* _bytes = NULL;
    u32       _size  = 0;
    if(!fude_zoom_scene_image(_w->s, _object, &_hw, &_hh, &_bytes, &_size) || _size < 4u) {
        return;
    }
    const b8  _png  = _bytes[0] == 0x89 && _bytes[1] == 'P';
    const usize _len = rde_base64_encoded_size(_size);
    c8* _b64 = (c8*)malloc(_len + 1u);
    if(_b64 == NULL) {
        return;
    }
    const usize _n = rde_base64_encode(_bytes, _size, _b64, _len + 1u);
    const fude_zoom_sim _m = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
    fude_zoom_svg_put(_w, "<image x=\"%.4f\" y=\"%.4f\" width=\"%.4f\" height=\"%.4f\" preserveAspectRatio=\"none\" transform=\"matrix(%.6f %.6f %.6f %.6f %.2f %.2f)\" href=\"data:image/%s;base64,",
                      -_hw, -_hh, _hw * 2.0, _hh * 2.0, _m.a, -_m.b, _m.b, _m.a, _m.tx + _w->half.x, _w->half.y - _m.ty, _png ? "png" : "jpeg");
    fude_put_data(_w->out, _b64, (u32)_n);
    fude_zoom_svg_put(_w, "\"/>\n");
    free(_b64);
}

// --- frames ----------------------------------------------------------------------------------------

typedef struct {
    u64 z;
    u32 object;
} fude_zoom_svg_item;

RDE_INTERNAL int fude_zoom_svg_by_z(const void* _a, const void* _b) {
    const u64 _x = ((const fude_zoom_svg_item*)_a)->z, _y = ((const fude_zoom_svg_item*)_b)->z;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// A frame's things on screen, in order (markers first, as the renderer draws
// them), the frames in it in their places.
RDE_INTERNAL void fude_zoom_svg_frame(fude_zoom_svg* _w, u32 _frame, fude_zoom_sim _to, u32 _depth) {
    if(_depth > 4096u) {
        return;
    }
    const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(_to), (fude_zoom_box){ -_w->half.x, -_w->half.y, _w->half.x, _w->half.y });
    rde_arr_clear(&_w->found);
    fude_zoom_scene_query(_w->s, _frame, _view, &_w->found);
    // The frames in it are not in its index: its list of them (as the renderer).
    const fude_zoom_frame* _f = fude_zoom_scene_frame(_w->s, _frame);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->kids); _i++) {
        const u32 _k = ((const u32*)_f->kids.memory)[_i];
        if(fude_zoom_box_overlaps(fude_zoom_scene_object(_w->s, _k)->box, _view)) {
            rde_arr_add(&_w->found, (any)&_k);
        }
    }
    const u32 _n = (u32)rde_arr_length(&_w->found);
    if(_n == 0) {
        return;
    }
    fude_zoom_svg_item* _items = (fude_zoom_svg_item*)malloc((usize)_n * sizeof(fude_zoom_svg_item));
    if(_items == NULL) {
        return;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = ((const u32*)_w->found.memory)[_i];
        _items[_i] = (fude_zoom_svg_item){ fude_zoom_scene_object(_w->s, _o)->z, _o };
    }
    qsort(_items, _n, sizeof(_items[0]), fude_zoom_svg_by_z);
    const f64 _scale = fude_zoom_sim_scale(_to);
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        for(u32 _i = 0; _i < _n; _i++) {
            const u32               _object = _items[_i].object;
            const fude_zoom_object* _o      = fude_zoom_scene_object(_w->s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                continue;
            }
            const f64 _size = fmax(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * _scale;
            if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
                if(((_o->flags & FUDE_ZOOM_FLAG_MARKER) != 0) == (_pass == 0u) && _size >= FUDE_ZOOM_SVG_SPECK) {
                    fude_zoom_svg_stroke(_w, _object, _to);
                }
            } else if(_pass == 1u && _size >= FUDE_ZOOM_SVG_SPECK) {
                if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                    fude_zoom_svg_shape(_w, _object, _to, _size);
                } else if(_o->kind == FUDE_ZOOM_KIND_FILL) {
                    fude_zoom_svg_fill(_w, _object, _to);
                } else if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
                    fude_zoom_svg_image(_w, _object, _to);
                } else if(_o->kind == FUDE_ZOOM_KIND_FRAME && _size >= FUDE_ZOOM_SVG_MIN_PX) {
                    const fude_zoom_frame* _c = fude_zoom_scene_frame(_w->s, _o->child);
                    if(!_c->removed) {
                        // Its own query list: the parent's is still being walked.
                        rde_arr _kept = _w->found;
                        _w->found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
                        fude_zoom_svg_frame(_w, _o->child, fude_zoom_sim_compose(_to, fude_zoom_sim_from_xform(_c->xf)), _depth + 1u);
                        rde_arr_free(&_w->found);
                        _w->found = _kept;
                    }
                }
            }
        }
    }
    free(_items);
}

b8 fude_zoom_export_svg(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_svg _w = {
        .s = _s, .out = _out, .half = _half,
        .found = rde_arr_new(sizeof(u32), _heap), .q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap), .pts = rde_arr_new(sizeof(fude_zoom_v2), _heap),
    };
    c8 _bg[16];
    fude_zoom_svg_color(_bg, sizeof _bg, _paper);
    fude_zoom_svg_put(&_w, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fude_zoom_svg_put(&_w, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.0f\" height=\"%.0f\" viewBox=\"0 0 %.0f %.0f\">\n",
                      _half.x * 2.0, _half.y * 2.0, _half.x * 2.0, _half.y * 2.0);
    fude_zoom_svg_put(&_w, "<rect width=\"100%%\" height=\"100%%\" fill=\"%s\"/>\n", _bg);
    // From the highest frame drawn exactly (three up at most), as the renderer.
    const fude_zoom_camera* _cam     = &_s->camera;
    const fude_zoom_sim     _cam_sim = fude_zoom_camera_sim(_cam);
    u32           _top     = _cam->frame;
    fude_zoom_sim _top_sim = _cam_sim;
    for(u32 _l = 0; _l < 3u; _l++) {
        const u32 _up = fude_zoom_scene_frame(_s, _top)->parent;
        if(_up == FUDE_ZOOM_NONE) {
            break;
        }
        const fude_zoom_sim _up_sim = fude_zoom_sim_compose(_cam_sim, fude_zoom_scene_sim(_s, _up, _cam->frame));
        if((fabs(_up_sim.tx) + fabs(_up_sim.ty)) * 2.220446049250313e-16 >= FUDE_ZOOM_SVG_EXACT) {
            break;
        }
        _top     = _up;
        _top_sim = _up_sim;
    }
    fude_zoom_svg_frame(&_w, _top, _top_sim, 0u);
    fude_zoom_svg_put(&_w, "</svg>\n");
    rde_arr_free(&_w.found);
    rde_arr_free(&_w.q);
    rde_arr_free(&_w.pts);
    return fude_bytes_size(_out) > 0;
}
