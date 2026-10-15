// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/export.h"
#include "zoom/symbol.h"
#include "zoom/plot.h"
#include "zoom/shape.h"
#include "zoom/sheet.h"
#include "zoom/fill.h"
#include "zoom/dxf.h"
#include "zoom/pdf.h"
#include "zoom/formula.h"
#include "zoom/paper.h"
#include "drawing/base/theme.h"

// ===========================================================================
// See export.h. The writers, each a sink of the walk (export.c): SVG (fude_zoom_svg_*), PDF (fude_zoom_pdfw_*,
// on pdf.h: a canvas, its views, a notebook's pages), DXF (fude_zoom_dxfw_*, on dxf.h); then the true-size tiles
// (fude_zoom_tile_*), the printer's check (fude_zoom_check_*) and the cut list (fude_zoom_cutlist_*, fude_zoom_csv_*),
// which reads the scene itself. (Each its own names: cut.h's fude_zoom_cut_ and circuit.h's fude_zoom_part are other
// modules'.)
//
// It starts with what the writers share: a stroke's width followed in runs (fude_zoom_export_run_end, with
// FUDE_ZOOM_SVG_RUN), then each writer in turn. It shares nothing private with export.c: it walks through export.h's
// fude_zoom_export_walk. Main thread only.
// ===========================================================================

#define FUDE_ZOOM_SVG_RUN     0.08     // a stroke's width followed in runs no further apart than this share

// A stroke's width followed in runs: where each run ends (the next starts
// there), from _start. One run for an even pen.
RDE_INTERNAL u32 fude_zoom_export_run_end(const f64* _r, u32 _n, u32 _start) {
    const f64 _w0 = rde_math_max_f64(_r[_start], 0.5);
    u32 _end = _start + 1u;
    while(_end + 1u < _n && rde_math_abs_f64(rde_math_max_f64(_r[_end], 0.5) - _w0) <= _w0 * FUDE_ZOOM_SVG_RUN) {
        _end++;
    }
    return _end;
}

// --- SVG ---------------------------------------------------------------------------------------------

// The SVG writer's state: the bytes, the screen's half size (the SVG's origin is its top left), and how many fills'
// masks have been made (their ids).
typedef struct {
    fude_bytes*  out;
    fude_zoom_v2 half;
    u32          masks;      // fills' masks made so far (their ids)
} fude_zoom_svg;

// Appends printf-style text to the SVG: through a 512-byte buffer, or a scratch array on the std allocator when it is
// longer.
RDE_INTERNAL void fude_zoom_svg_put(fude_zoom_svg* _w, const c8* _fmt, ...) {
    c8      _buf[512];
    va_list _args;
    va_start(_args, _fmt);
    const int _n = rde_vsprintf(_buf, sizeof _buf, _fmt, _args);
    va_end(_args);
    if(_n <= 0) {
        return;
    }
    if((usize)_n < sizeof _buf) {
        fude_put_data(_w->out, _buf, (u32)_n);
        return;
    }
    rde_arr _big = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_big, (usize)_n + 1u);
    va_start(_args, _fmt);
    rde_vsprintf((c8*)_big.memory, (usize)_n + 1u, _fmt, _args);
    va_end(_args);
    fude_put_data(_w->out, _big.memory, (u32)_n);
    rde_arr_free(&_big);
}

// A colour as SVG wants it: "#rrggbb".
RDE_INTERNAL void fude_zoom_svg_color(c8* _out, usize _size, rde_color _c) {
    rde_snprintf(_out, _size, "#%02x%02x%02x", _c.r, _c.g, _c.b);
}

// A point of the screen (centre origin, Y up) where the SVG has it (top-left, Y down).
RDE_INTERNAL void fude_zoom_svg_point(fude_zoom_svg* _w, const c8* _cmd, fude_zoom_v2 _p) {
    fude_zoom_svg_put(_w, "%s%.2f %.2f", _cmd, _p.x + _w->half.x, _w->half.y - _p.y);
}

// A paint's opacity, written after it: attribute _attr ("stroke-opacity", "fill-opacity") from _c's alpha into _out,
// with a space before it; "" when it is opaque (SVG's default).
RDE_INTERNAL void fude_zoom_svg_opacity(c8* _out, usize _size, const c8* _attr, rde_color _c) {
    _out[0] = 0;
    if(_c.a < 255u) {
        rde_snprintf(_out, _size, " %s=\"%.3f\"", _attr, (f64)_c.a / 255.0);
    }
}

// A stroke: each run its own path, their round ends meeting where they join; a dot, a circle filled in its colour.
// Each as see-through as its colour's alpha says; nothing at all when that is 0.
RDE_INTERNAL void fude_zoom_svg_stroke(any _self, const fude_zoom_v2* _p, const f64* _r, u32 _n, rde_color _c) {
    fude_zoom_svg* _w = (fude_zoom_svg*)_self;
    if(_c.a == 0u) {
        return;
    }
    c8 _col[16];
    fude_zoom_svg_color(_col, sizeof _col, _c);
    c8 _alpha[32];
    if(_n == 1u) {
        fude_zoom_svg_opacity(_alpha, sizeof _alpha, "fill-opacity", _c);
        fude_zoom_svg_put(_w, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"%s\"%s/>\n", _p[0].x + _w->half.x, _w->half.y - _p[0].y, rde_math_max_f64(_r[0], 0.5), _col,
                          _alpha);
        return;
    }
    fude_zoom_svg_opacity(_alpha, sizeof _alpha, "stroke-opacity", _c);
    for(u32 _start = 0; _start + 1u < _n;) {
        const u32 _end = fude_zoom_export_run_end(_r, _n, _start);
        fude_zoom_svg_put(_w, "<path d=\"");
        for(u32 _i = _start; _i <= _end; _i++) {
            fude_zoom_svg_point(_w, _i == _start ? "M" : " L", _p[_i]);
        }
        fude_zoom_svg_put(_w, "\" fill=\"none\" stroke=\"%s\"%s stroke-width=\"%.2f\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n",
                          _col, _alpha, rde_math_max_f64(_r[_start], 0.5) * 2.0);
        _start = _end;
    }
}

// A shape: one path, its line at least 1 point wide, each paint as see-through as its colour's alpha says: filled when
// the fill's alpha is above 0, lined when the line's is (a symbol's part may have no line).
RDE_INTERNAL void fude_zoom_svg_shape(any _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill) {
    fude_zoom_svg* _w = (fude_zoom_svg*)_self;
    c8 _lc[16], _fc[16], _la[32], _fa[32];
    fude_zoom_svg_color(_lc, sizeof _lc, _line);
    fude_zoom_svg_color(_fc, sizeof _fc, _fill);
    fude_zoom_svg_opacity(_la, sizeof _la, "stroke-opacity", _line);
    fude_zoom_svg_opacity(_fa, sizeof _fa, "fill-opacity", _fill);
    fude_zoom_svg_put(_w, "<path d=\"");
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_svg_point(_w, _i == 0 ? "M" : " L", _p[_i]);
    }
    fude_zoom_svg_put(_w, "%s\" fill=\"%s\"%s", _closed ? " Z" : "", _fill.a > 0u ? _fc : "none", _fill.a > 0u ? _fa : "");
    if(_line.a > 0u) {
        fude_zoom_svg_put(_w, " stroke=\"%s\"%s stroke-width=\"%.2f\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n", _lc, _la,
                          rde_math_max_f64(_radius, 0.5) * 2.0);
    } else {
        fude_zoom_svg_put(_w, " stroke=\"none\"/>\n");
    }
}

// A fill: its outline (even-odd), what the eraser cut from it masked out, as see-through as its colour's alpha says.
RDE_INTERNAL void fude_zoom_svg_fill(any _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _c) {
    fude_zoom_svg* _w = (fude_zoom_svg*)_self;
    u32 _outline = 0;
    while(_outline < _n && _rings[_outline] == _rings[0]) {
        _outline++;
    }
    c8 _col[16];
    fude_zoom_svg_color(_col, sizeof _col, _c);
    c8 _mask[48] = "";
    if(_outline < _n) {
        const u32 _id = ++_w->masks;
        rde_snprintf(_mask, sizeof _mask, " mask=\"url(#cut%u)\"", _id);
        fude_zoom_svg_put(_w, "<mask id=\"cut%u\" maskUnits=\"userSpaceOnUse\" x=\"0\" y=\"0\" width=\"%.0f\" height=\"%.0f\"><rect width=\"100%%\" height=\"100%%\" fill=\"white\"/><path d=\"",
                          _id, _w->half.x * 2.0, _w->half.y * 2.0);
        for(u32 _i = _outline; _i < _n; _i++) {
            const b8 _new = _i == _outline || _rings[_i] != _rings[_i - 1u];
            fude_zoom_svg_point(_w, _new ? (_i == _outline ? "M" : " Z M") : " L", _p[_i]);
        }
        fude_zoom_svg_put(_w, " Z\" fill=\"black\"/></mask>\n");
    }
    fude_zoom_svg_put(_w, "<path d=\"");
    for(u32 _i = 0; _i < _outline; _i++) {
        fude_zoom_svg_point(_w, _i == 0 ? "M" : " L", _p[_i]);
    }
    c8 _opacity[32];
    fude_zoom_svg_opacity(_opacity, sizeof _opacity, "fill-opacity", _c);
    fude_zoom_svg_put(_w, " Z\" fill=\"%s\" fill-rule=\"evenodd\"%s%s/>\n", _col, _mask, _opacity);
}

// A picture: itself, inside, placed by its transform (the SVG's Y runs down).
RDE_INTERNAL void fude_zoom_svg_image(any _self, const u8* _bytes, u32 _size, f64 _hw, f64 _hh, fude_zoom_sim _m) {
    fude_zoom_svg* _w = (fude_zoom_svg*)_self;
    const b8    _png = _bytes[0] == 0x89 && _bytes[1] == 'P';
    const usize _len = rde_base64_encoded_size(_size);
    rde_arr _b64_arr = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_b64_arr, _len + 1u);
    c8* _b64 = (c8*)_b64_arr.memory;
    const usize _n = rde_base64_encode(_bytes, _size, _b64, _len + 1u);
    fude_zoom_svg_put(_w, "<image x=\"%.4f\" y=\"%.4f\" width=\"%.4f\" height=\"%.4f\" preserveAspectRatio=\"none\" transform=\"matrix(%.6f %.6f %.6f %.6f %.2f %.2f)\" href=\"data:image/%s;base64,",
                      -_hw, -_hh, _hw * 2.0, _hh * 2.0, _m.a, -_m.b, _m.b, _m.a, _m.tx + _w->half.x, _w->half.y - _m.ty, _png ? "png" : "jpeg");
    fude_put_data(_w->out, _b64, (u32)_n);
    fude_zoom_svg_put(_w, "\"/>\n");
    rde_arr_free(&_b64_arr);
}

// A text: a note's paper, then each line, its characters escaped for XML; the paper and the ink each as see-through as
// its colour's alpha says (fude_zoom_svg_opacity).
RDE_INTERNAL void fude_zoom_svg_text(any _self, fude_zoom_v2 _tl, f64 _px, f64 _w, f64 _h, u8 _style, rde_color _ink, rde_color _paper, const c8* const* _lines, u32 _count) {
    fude_zoom_svg* _sw = (fude_zoom_svg*)_self;
    c8 _col[16], _alpha[32];
    const b8  _centred = (_style & FUDE_ZOOM_EXPORT_TEXT_CENTRED) != 0u, _right = (_style & FUDE_ZOOM_EXPORT_TEXT_RIGHT) != 0u;
    const b8  _bold = (_style & FUDE_ZOOM_EXPORT_TEXT_BOLD) != 0u, _italic = (_style & FUDE_ZOOM_EXPORT_TEXT_ITALIC) != 0u;
    _style = (u8)(_style & ~FUDE_ZOOM_EXPORT_TEXT_FLAGS);
    const f64 _pad = _style == FUDE_ZOOM_TEXT_STICKY ? 0.6 * _px : 0.0;
    if(_style == FUDE_ZOOM_TEXT_STICKY) {
        fude_zoom_svg_color(_col, sizeof _col, _paper);
        fude_zoom_svg_opacity(_alpha, sizeof _alpha, "fill-opacity", _paper);
        fude_zoom_svg_put(_sw, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"%s\"%s/>\n", _tl.x + _sw->half.x, _sw->half.y - _tl.y, _w, _h, _col, _alpha);
    }
    fude_zoom_svg_color(_col, sizeof _col, _ink);
    fude_zoom_svg_opacity(_alpha, sizeof _alpha, "fill-opacity", _ink);
    for(u32 _i = 0; _i < _count; _i++) {
        fude_zoom_svg_put(_sw, "<text x=\"%.2f\" y=\"%.2f\"%s%s%s font-family=\"sans-serif\" font-size=\"%.2f\" fill=\"%s\"%s>",
                          (_centred ? _tl.x + _w * 0.5 : _right ? _tl.x + _w : _tl.x + _pad) + _sw->half.x, _sw->half.y - _tl.y + _pad + _px * 0.95 + (f64)_i * _px * 1.3,
                          _centred ? " text-anchor=\"middle\"" : _right ? " text-anchor=\"end\"" : "", _bold ? " font-weight=\"bold\"" : "",
                          _italic ? " font-style=\"italic\"" : "", _px, _col, _alpha);
        for(const c8* _c = _lines[_i]; *_c != 0; _c++) {
            if(*_c == '<')      { fude_zoom_svg_put(_sw, "&lt;"); }
            else if(*_c == '>') { fude_zoom_svg_put(_sw, "&gt;"); }
            else if(*_c == '&') { fude_zoom_svg_put(_sw, "&amp;"); }
            else                { fude_put_data(_sw->out, _c, 1u); }
        }
        fude_zoom_svg_put(_sw, "</text>\n");
    }
}

// See the header. The SVG is the screen's size, a point a pixel. It is only text appended to _out in memory, which
// cannot fail part way (RDE's arrays stop the app rather than run out of memory), so it is always whole: what can fail
// is writing it to a file, which is the caller's to do and to report (page_export.c says when it was not written).
b8 fude_zoom_export_svg(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out) {
    fude_zoom_svg _w = { .out = _out, .half = _half };
    c8 _bg[16];
    fude_zoom_svg_color(_bg, sizeof _bg, _paper);
    fude_zoom_svg_put(&_w, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fude_zoom_svg_put(&_w, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.0f\" height=\"%.0f\" viewBox=\"0 0 %.0f %.0f\">\n",
                      _half.x * 2.0, _half.y * 2.0, _half.x * 2.0, _half.y * 2.0);
    fude_zoom_svg_put(&_w, "<rect width=\"100%%\" height=\"100%%\" fill=\"%s\"/>\n", _bg);
    const fude_zoom_export_sink _sink = { .self = &_w, .stroke = fude_zoom_svg_stroke, .shape = fude_zoom_svg_shape, .fill = fude_zoom_svg_fill,
                                          .image = fude_zoom_svg_image, .text = fude_zoom_svg_text };
    fude_zoom_export_walk(_s, _half, &_sink);
    fude_zoom_svg_put(&_w, "</svg>\n");
    return fude_bytes_size(_out) > 0;
}

// --- PDF ---------------------------------------------------------------------------------------------

// The PDF writer's state: the PDF, the screen's half size (the page's middle), scratch arrays for a path's numbers and
// a fill's triangles, and the pictures already put in, so a picture drawn again (on another page, in another tile, a
// copy of it) is put in once and drawn from there.
typedef struct {
    fude_zoom_pdf* pdf;
    fude_zoom_v2   half;       // the page's middle (the screen's centre)
    rde_arr TYPE(f64)          xy;
    rde_arr TYPE(fude_zoom_v2) tris;
    // Each picture put in, by its bytes' address (the scene's: copies share them, and they do not move while the PDF
    // is written), to its image in the PDF. Every one is kept, for the writer's life: a canvas's
    // pictures are user data.
    rde_hash_map TYPE(u64, u32) images;
} fude_zoom_pdfw;

// Makes a PDF writer's state for a page whose middle is _half: a new PDF (NULL in pdf when none could be made), empty
// scratch arrays and no pictures. fude_zoom_pdfw_free lets all of it go.
RDE_INTERNAL fude_zoom_pdfw fude_zoom_pdfw_make(fude_zoom_v2 _half) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    return (fude_zoom_pdfw){
        .pdf = fude_zoom_pdf_new(), .half = _half, .xy = rde_arr_new(sizeof(f64), _heap), .tris = rde_arr_new(sizeof(fude_zoom_v2), _heap),
        .images = rde_hash_map_new(sizeof(u64), sizeof(u32), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, _heap),
    };
}

// Frees what fude_zoom_pdfw_make made, the PDF too (once it is finished, or when it was never begun).
RDE_INTERNAL void fude_zoom_pdfw_free(fude_zoom_pdfw* _w) {
    if(_w->pdf != NULL) {
        fude_zoom_pdf_free(_w->pdf);
    }
    rde_arr_free(&_w->xy);
    rde_arr_free(&_w->tris);
    rde_hash_map_free(&_w->images);
}

// Points of the screen as the page's (its origin bottom left, Y up as the screen's).
RDE_INTERNAL void fude_zoom_pdfw_path(fude_zoom_pdfw* _w, const fude_zoom_v2* _p, u32 _n, b8 _closed) {
    rde_arr_clear(&_w->xy);
    f64* _xy = rde_arr_add_n(&_w->xy, _n * 2u);
    for(u32 _i = 0; _i < _n; _i++) {
        _xy[_i * 2u]      = _p[_i].x + _w->half.x;
        _xy[_i * 2u + 1u] = _p[_i].y + _w->half.y;
    }
    fude_zoom_pdf_polyline(_w->pdf, _xy, _n, _closed);
}

// A dot: a circle of four curves.
RDE_INTERNAL void fude_zoom_pdfw_dot(fude_zoom_pdfw* _w, fude_zoom_v2 _c, f64 _r) {
    const f64 _k = 0.5522847498 * _r, _x = _c.x + _w->half.x, _y = _c.y + _w->half.y;
    fude_zoom_pdf_move(_w->pdf, _x + _r, _y);
    fude_zoom_pdf_curve(_w->pdf, _x + _r, _y + _k, _x + _k, _y + _r, _x, _y + _r);
    fude_zoom_pdf_curve(_w->pdf, _x - _k, _y + _r, _x - _r, _y + _k, _x - _r, _y);
    fude_zoom_pdf_curve(_w->pdf, _x - _r, _y - _k, _x - _k, _y - _r, _x, _y - _r);
    fude_zoom_pdf_curve(_w->pdf, _x + _k, _y - _r, _x + _r, _y - _k, _x + _r, _y);
    fude_zoom_pdf_close(_w->pdf);
    fude_zoom_pdf_fill(_w->pdf, false);
}

// A stroke: a dot for one point, else each run of even width its own path with round ends and joins.
RDE_INTERNAL void fude_zoom_pdfw_stroke(any _self, const fude_zoom_v2* _p, const f64* _r, u32 _n, rde_color _c) {
    fude_zoom_pdfw* _w = (fude_zoom_pdfw*)_self;
    if(_n == 1u) {
        fude_zoom_pdf_fill_color(_w->pdf, _c);
        fude_zoom_pdfw_dot(_w, _p[0], rde_math_max_f64(_r[0], 0.5));
        return;
    }
    fude_zoom_pdf_stroke_color(_w->pdf, _c);
    fude_zoom_pdf_line_style(_w->pdf, 1u, 1u);
    for(u32 _start = 0; _start + 1u < _n;) {
        const u32 _end = fude_zoom_export_run_end(_r, _n, _start);
        fude_zoom_pdf_line_width(_w->pdf, rde_math_max_f64(_r[_start], 0.5) * 2.0);
        fude_zoom_pdfw_path(_w, &_p[_start], _end - _start + 1u, false);
        fude_zoom_pdf_stroke(_w->pdf);
        _start = _end;
    }
}

// A shape: its path stroked at least 1 point wide, and filled too when the fill's alpha is above 0.
RDE_INTERNAL void fude_zoom_pdfw_shape(any _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill) {
    fude_zoom_pdfw* _w = (fude_zoom_pdfw*)_self;
    fude_zoom_pdf_stroke_color(_w->pdf, _line);
    fude_zoom_pdf_line_style(_w->pdf, 1u, 1u);
    fude_zoom_pdf_line_width(_w->pdf, rde_math_max_f64(_radius, 0.5) * 2.0);
    fude_zoom_pdfw_path(_w, _p, _n, _closed);
    if(_fill.a > 0) {
        fude_zoom_pdf_fill_color(_w->pdf, _fill);
        fude_zoom_pdf_fill_stroke(_w->pdf, false);
    } else {
        fude_zoom_pdf_stroke(_w->pdf);
    }
}

// A fill: its outline (even-odd); with the eraser's cuts, its triangles (as
// the renderer has them: the cuts taken out however they overlap), filled
// together in one go so no seams show between them.
RDE_INTERNAL void fude_zoom_pdfw_fill(any _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _c) {
    fude_zoom_pdfw* _w = (fude_zoom_pdfw*)_self;
    fude_zoom_pdf_fill_color(_w->pdf, _c);
    u32 _outline = 0;
    while(_outline < _n && _rings[_outline] == _rings[0]) {
        _outline++;
    }
    if(_outline == _n) {
        fude_zoom_pdfw_path(_w, _p, _n, true);
        fude_zoom_pdf_fill(_w->pdf, true);
        return;
    }
    rde_arr_clear(&_w->tris);
    const u32 _k = fude_zoom_fill_triangulate_rings(_p, _rings, _n, &_w->tris);
    const fude_zoom_v2* _t = (const fude_zoom_v2*)_w->tris.memory;
    for(u32 _i = 0; _i < _k; _i++) {
        fude_zoom_pdf_move(_w->pdf, _t[_i * 3u].x + _w->half.x, _t[_i * 3u].y + _w->half.y);
        fude_zoom_pdf_line(_w->pdf, _t[_i * 3u + 1u].x + _w->half.x, _t[_i * 3u + 1u].y + _w->half.y);
        fude_zoom_pdf_line(_w->pdf, _t[_i * 3u + 2u].x + _w->half.x, _t[_i * 3u + 2u].y + _w->half.y);
        fude_zoom_pdf_close(_w->pdf);
    }
    if(_k > 0) {
        fude_zoom_pdf_fill(_w->pdf, false);
    }
}

// A picture: a JPEG as it is; a PNG read into pixels (its transparency kept), put in the first time its bytes are drawn
// and drawn from there after. Drawn opaque (PDF paints pictures through the fill's alpha). (The walk hands over only
// pictures of 4 bytes or more, export.h, so its first two can be looked at.)
RDE_INTERNAL void fude_zoom_pdfw_image(any _self, const u8* _bytes, u32 _size, f64 _hw, f64 _hh, fude_zoom_sim _m) {
    fude_zoom_pdfw* _w     = (fude_zoom_pdfw*)_self;
    const u64       _key   = (u64)(uptr)_bytes;
    const u32*      _known = (const u32*)rde_hash_map_find(&_w->images, (const any)&_key);
    u32             _id    = _known != NULL ? *_known : 0u;
    if(_id == 0) {
        if(_bytes[0] == 0xFF && _bytes[1] == 0xD8) {
            _id = fude_zoom_pdf_jpeg(_w->pdf, _bytes, _size);
        } else {
            u32 _pw = 0, _ph = 0;
            rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
            u8* _px = rde_image_decode(_bytes, _size, &_pw, &_ph, _heap);
            if(_px != NULL) {
                _id = fude_zoom_pdf_rgba(_w->pdf, _px, _pw, _ph);
                _heap->free(_heap->allocator, _px);
            }
        }
        if(_id == 0) {
            return;
        }
        rde_hash_map_add(&_w->images, (const any)&_key, (const any)&_id);
    }
    // The unit square (its top row at the top) onto the picture's place: its
    // corner at (-hw, -hh) of its own units, 2hw across, 2hh up.
    const f64 _a = _m.a * 2.0 * _hw, _b = _m.b * 2.0 * _hw;
    const f64 _c = -_m.b * 2.0 * _hh, _d = _m.a * 2.0 * _hh;
    const f64 _e = _m.tx - _m.a * _hw + _m.b * _hh + _w->half.x;
    const f64 _f = _m.ty - _m.b * _hw - _m.a * _hh + _w->half.y;
    fude_zoom_pdf_fill_color(_w->pdf, (rde_color){ 0, 0, 0, 255 });
    fude_zoom_pdf_image(_w->pdf, _id, _a, _b, _c, _d, _e, _f);
}

// A text: a note's paper, then each line in its face of the PDF's fonts (pdf.h: the app's, as the screen draws it, when
// the page set them), centred or set right by that face's widths.
RDE_INTERNAL void fude_zoom_pdfw_text(any _self, fude_zoom_v2 _tl, f64 _px, f64 _w, f64 _h, u8 _style, rde_color _ink, rde_color _paper, const c8* const* _lines, u32 _count) {
    fude_zoom_pdfw* _pw = (fude_zoom_pdfw*)_self;
    const b8  _centred = (_style & FUDE_ZOOM_EXPORT_TEXT_CENTRED) != 0u, _right = (_style & FUDE_ZOOM_EXPORT_TEXT_RIGHT) != 0u;
    const u8  _face = (u8)(((_style & FUDE_ZOOM_EXPORT_TEXT_BOLD) ? 1u : 0u) + ((_style & FUDE_ZOOM_EXPORT_TEXT_ITALIC) ? 2u : 0u));
    _style = (u8)(_style & ~FUDE_ZOOM_EXPORT_TEXT_FLAGS);
    const f64 _x = _tl.x + _pw->half.x, _y = _tl.y + _pw->half.y;
    const f64 _pad = _style == FUDE_ZOOM_TEXT_STICKY ? 0.6 * _px : 0.0;
    if(_style == FUDE_ZOOM_TEXT_STICKY) {
        fude_zoom_pdf_fill_color(_pw->pdf, _paper);
        fude_zoom_pdf_rect(_pw->pdf, _x, _y - _h, _w, _h);
        fude_zoom_pdf_fill(_pw->pdf, false);
    }
    for(u32 _i = 0; _i < _count; _i++) {
        const f64 _tw = _centred || _right ? fude_zoom_pdf_text_width_face(_px, _lines[_i], _face) : 0.0;
        const f64 _lx = _centred ? _x + (_w - _tw) * 0.5 : _right ? _x + _w - _tw : _x + _pad;
        fude_zoom_pdf_text_face(_pw->pdf, _lx, _y - _pad - _px * 0.95 - (f64)_i * _px * 1.3, _px, _ink, _lines[_i], _face);
    }
}

// See the header.
b8 fude_zoom_export_pdf(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out) {
    fude_zoom_pdfw _w = fude_zoom_pdfw_make(_half);
    if(_w.pdf == NULL) {
        fude_zoom_pdfw_free(&_w);
        return false;
    }
    // A page the size of the screen, a point a point; the paper behind.
    fude_zoom_pdf_page_begin(_w.pdf, _half.x * 2.0, _half.y * 2.0);
    fude_zoom_pdf_fill_color(_w.pdf, _paper);
    fude_zoom_pdf_rect(_w.pdf, 0.0, 0.0, _half.x * 2.0, _half.y * 2.0);
    fude_zoom_pdf_fill(_w.pdf, false);
    const fude_zoom_export_sink _sink = { .self = &_w, .stroke = fude_zoom_pdfw_stroke, .shape = fude_zoom_pdfw_shape, .fill = fude_zoom_pdfw_fill,
                                          .image = fude_zoom_pdfw_image, .text = fude_zoom_pdfw_text };
    fude_zoom_export_walk(_s, _half, &_sink);
    fude_zoom_pdf_page_end(_w.pdf);
    const b8 _ok = fude_zoom_pdf_finish(_w.pdf, _out);
    fude_zoom_pdfw_free(&_w);
    return _ok;
}

// See the header.
b8 fude_zoom_export_pdf_views(fude_zoom_scene* _s, rde_color _paper, const fude_zoom_camera* _views, const fude_zoom_v2* _halves, u32 _n, fude_bytes* _out) {
    if(_n == 0u) {
        return false;
    }
    fude_zoom_pdfw _w = fude_zoom_pdfw_make(_halves[0]);
    if(_w.pdf == NULL) {
        fude_zoom_pdfw_free(&_w);
        return false;
    }
    const fude_zoom_camera _was = _s->camera;
    const fude_zoom_export_sink _sink = { .self = &_w, .stroke = fude_zoom_pdfw_stroke, .shape = fude_zoom_pdfw_shape, .fill = fude_zoom_pdfw_fill,
                                          .image = fude_zoom_pdfw_image, .text = fude_zoom_pdfw_text };
    for(u32 _i = 0; _i < _n; _i++) {
        _w.half    = _halves[_i];
        _s->camera = _views[_i];
        fude_zoom_pdf_page_begin(_w.pdf, _w.half.x * 2.0, _w.half.y * 2.0);
        fude_zoom_pdf_fill_color(_w.pdf, _paper);
        fude_zoom_pdf_rect(_w.pdf, 0.0, 0.0, _w.half.x * 2.0, _w.half.y * 2.0);
        fude_zoom_pdf_fill(_w.pdf, false);
        fude_zoom_export_walk(_s, _w.half, &_sink);
        fude_zoom_pdf_page_end(_w.pdf);
    }
    _s->camera = _was;
    const b8 _ok = fude_zoom_pdf_finish(_w.pdf, _out);
    fude_zoom_pdfw_free(&_w);
    return _ok;
}

// A page's paper's lines in a printed PDF: their widths in points (a faint one's, a heavy one's), and a dot's radius.
#define FUDE_ZOOM_PDFW_PAPER_LINE  0.45   // points
#define FUDE_ZOOM_PDFW_PAPER_FAINT 0.3
#define FUDE_ZOOM_PDFW_PAPER_HEAVY 0.8
#define FUDE_ZOOM_PDFW_PAPER_DOT   0.6    // a dot's radius

// A page's paper (paper.h's lines, in the camera's frame's units) under its ink: _theme's ink laid solid on its page,
// as strongly as each says (the margin's line a faint red); hairlines, its own lines a little heavier, dots round.
RDE_INTERNAL void fude_zoom_pdfw_paper(fude_zoom_pdfw* _w, const fude_zoom_camera* _view, const rde_arr* _lines, const fude_theme* _theme) {
    const fude_zoom_paper_line* _l = (const fude_zoom_paper_line*)_lines->memory;
    const u32 _n = (u32)rde_arr_length(_lines);
    const rde_color _paper = _theme->page;
    fude_zoom_pdf_line_style(_w->pdf, 1u, 1u);
    for(u32 _i = 0; _i < _n; _i++) {
        const rde_color _ink = _l[_i].kind == FUDE_ZOOM_PAPER_MARGIN ? (rde_color){ 214, 96, 96, 110 } : _theme->page_dots;
        const f32 _k = (f32)_ink.a / 255.0f * _l[_i].strength;
        const rde_color _c = { (u8)((f32)_paper.r + ((f32)_ink.r - (f32)_paper.r) * _k), (u8)((f32)_paper.g + ((f32)_ink.g - (f32)_paper.g) * _k),
                               (u8)((f32)_paper.b + ((f32)_ink.b - (f32)_paper.b) * _k), 255 };
        const fude_zoom_v2 _p[2] = { { (_l[_i].a.x - _view->at.x) * _view->z, (_l[_i].a.y - _view->at.y) * _view->z },
                                     { (_l[_i].b.x - _view->at.x) * _view->z, (_l[_i].b.y - _view->at.y) * _view->z } };
        if(!(_k > 0.0f)) {
            continue;
        }
        if(_l[_i].kind == FUDE_ZOOM_PAPER_DOT) {
            fude_zoom_pdf_fill_color(_w->pdf, _c);
            fude_zoom_pdfw_dot(_w, _p[0], FUDE_ZOOM_PDFW_PAPER_DOT);
            continue;
        }
        fude_zoom_pdf_stroke_color(_w->pdf, _c);
        fude_zoom_pdf_line_width(_w->pdf, _l[_i].heavy ? FUDE_ZOOM_PDFW_PAPER_HEAVY : _l[_i].strength < 0.5f ? FUDE_ZOOM_PDFW_PAPER_FAINT : FUDE_ZOOM_PDFW_PAPER_LINE);
        fude_zoom_pdfw_path(_w, _p, 2u, false);
        fude_zoom_pdf_stroke(_w->pdf);
    }
}

// See the header. Each page is drawn in the paper theme, its paper and ink scaled round its middle to undo the
// printer's error.
b8 fude_zoom_export_pdf_pages(fude_zoom_scene* _s, const fude_zoom_camera* _views, const fude_zoom_v2* _halves, u32 _n, fude_zoom_v2 _printer,
                              const rde_arr* _papers, fude_bytes* _out) {
    if(_n == 0u) {
        return false;
    }
    fude_zoom_pdfw _w = fude_zoom_pdfw_make(_halves[0]);
    if(_w.pdf == NULL) {
        fude_zoom_pdfw_free(&_w);
        return false;
    }
    const fude_theme* _paper_theme = fude_theme_get(FUDE_THEME_PAPER);
    const fude_zoom_camera _was = _s->camera;
    const fude_zoom_export_sink _sink = { .self = &_w, .stroke = fude_zoom_pdfw_stroke, .shape = fude_zoom_pdfw_shape, .fill = fude_zoom_pdfw_fill,
                                          .image = fude_zoom_pdfw_image, .text = fude_zoom_pdfw_text };
    // The printer's own error undone (as the tiles' are): everything drawn smaller (or larger) by what
    // it prints too large.
    const f64 _fx = _printer.x > 0.5 && _printer.x < 2.0 ? 1.0 / _printer.x : 1.0;
    const f64 _fy = _printer.y > 0.5 && _printer.y < 2.0 ? 1.0 / _printer.y : 1.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _w.half    = _halves[_i];
        _s->camera = _views[_i];
        const f64 _pw = _w.half.x * 2.0, _ph = _w.half.y * 2.0;
        fude_zoom_pdf_page_begin(_w.pdf, _pw, _ph);
        fude_zoom_pdf_fill_color(_w.pdf, _paper_theme->page);
        fude_zoom_pdf_rect(_w.pdf, 0.0, 0.0, _pw, _ph);
        fude_zoom_pdf_fill(_w.pdf, false);
        fude_zoom_pdf_save(_w.pdf);
        if(_fx != 1.0 || _fy != 1.0) {
            fude_zoom_pdf_transform(_w.pdf, _fx, 0.0, 0.0, _fy, _pw * 0.5 * (1.0 - _fx), _ph * 0.5 * (1.0 - _fy));
        }
        if(_papers != NULL) {
            fude_zoom_pdfw_paper(&_w, &_views[_i], &_papers[_i], _paper_theme);
        }
        fude_zoom_export_walk_as(_s, _w.half, &_sink, _paper_theme);
        fude_zoom_pdf_restore(_w.pdf);
        fude_zoom_pdf_page_end(_w.pdf);
    }
    _s->camera = _was;
    const b8 _ok = fude_zoom_pdf_finish(_w.pdf, _out);
    fude_zoom_pdfw_free(&_w);
    return _ok;
}

// --- DXF -------------------------------------------------------------------------------------------

// The DXF writer's state: the DXF, millimetres a screen point, a scratch array for a path's numbers, and a layer for
// each colour met (32 remembered; past that, a colour's layer is asked of dxf.h each time).
typedef struct {
    fude_zoom_dxf* dxf;
    f64            mm;          // millimetres a screen point
    rde_arr TYPE(f64) xy;
    struct { rde_color c; u32 layer; } layers[32];
    u32            layer_count;
} fude_zoom_dxfw;

// AutoCAD's colour nearest a colour: red, yellow, green, cyan, blue, magenta,
// black (7, shown white on a dark screen), the greys, orange.
RDE_INTERNAL u8 fude_zoom_dxfw_aci(rde_color _c) {
    static const struct { u8 aci, r, g, b; } _known[] = {
        { 1, 255, 0, 0 }, { 2, 255, 255, 0 }, { 3, 0, 255, 0 }, { 4, 0, 255, 255 }, { 5, 0, 0, 255 }, { 6, 255, 0, 255 },
        { 7, 0, 0, 0 }, { 8, 128, 128, 128 }, { 9, 192, 192, 192 }, { 30, 255, 127, 0 },
    };
    u32 _best = 0, _d = 0xFFFFFFFFu;
    for(u32 _i = 0; _i < sizeof(_known) / sizeof(_known[0]); _i++) {
        const i32 _dr = (i32)_c.r - _known[_i].r, _dg = (i32)_c.g - _known[_i].g, _db = (i32)_c.b - _known[_i].b;
        const u32 _e = (u32)(_dr * _dr + _dg * _dg + _db * _db);
        if(_e < _d) { _d = _e; _best = _i; }
    }
    return _known[_best].aci;
}

// A layer for each colour drawn with ("INK_1E1E1E"): what a CAM program sorts into operations.
RDE_INTERNAL u32 fude_zoom_dxfw_layer(fude_zoom_dxfw* _w, rde_color _c) {
    for(u32 _i = 0; _i < _w->layer_count; _i++) {
        if(_w->layers[_i].c.r == _c.r && _w->layers[_i].c.g == _c.g && _w->layers[_i].c.b == _c.b) {
            return _w->layers[_i].layer;
        }
    }
    c8 _name[24];
    rde_snprintf(_name, sizeof _name, "INK_%02X%02X%02X", _c.r, _c.g, _c.b);
    const u32 _layer = fude_zoom_dxf_layer(_w->dxf, _name, fude_zoom_dxfw_aci(_c));
    if(_w->layer_count < 32u) {
        _w->layers[_w->layer_count].c     = _c;
        _w->layers[_w->layer_count].layer = _layer;
        _w->layer_count++;
    }
    return _layer;
}

// A path of screen points on _layer, in millimetres.
RDE_INTERNAL void fude_zoom_dxfw_path(fude_zoom_dxfw* _w, u32 _layer, const fude_zoom_v2* _p, u32 _n, b8 _closed) {
    rde_arr_clear(&_w->xy);
    f64* _xy = rde_arr_add_n(&_w->xy, _n * 2u);
    for(u32 _i = 0; _i < _n; _i++) {
        _xy[_i * 2u]      = _p[_i].x * _w->mm;
        _xy[_i * 2u + 1u] = _p[_i].y * _w->mm;
    }
    fude_zoom_dxf_polyline(_w->dxf, _layer, _xy, _n, _closed);
}

// A stroke: its centreline (where a blade or a beam goes).
RDE_INTERNAL void fude_zoom_dxfw_stroke(any _self, const fude_zoom_v2* _p, const f64* _r, u32 _n, rde_color _c) {
    RDE_UNUSED(_r);
    fude_zoom_dxfw* _w = (fude_zoom_dxfw*)_self;
    if(_n >= 2u) {
        fude_zoom_dxfw_path(_w, fude_zoom_dxfw_layer(_w, _c), _p, _n, false);
    }
}

// A shape: its outline, on its line's colour's layer; its width and fill are nothing to a blade.
RDE_INTERNAL void fude_zoom_dxfw_shape(any _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill) {
    RDE_UNUSED(_radius);
    RDE_UNUSED(_fill);
    fude_zoom_dxfw* _w = (fude_zoom_dxfw*)_self;
    fude_zoom_dxfw_path(_w, fude_zoom_dxfw_layer(_w, _line), _p, _n, _closed);
}

// A fill: its outline, and each cut the eraser made in it, closed paths of their own.
RDE_INTERNAL void fude_zoom_dxfw_fill(any _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _c) {
    fude_zoom_dxfw* _w     = (fude_zoom_dxfw*)_self;
    const u32       _layer = fude_zoom_dxfw_layer(_w, _c);
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _rings[_to] == _rings[_from]) {
            _to++;
        }
        if(_to - _from >= 3u) {
            fude_zoom_dxfw_path(_w, _layer, &_p[_from], _to - _from, true);
        }
        _from = _to;
    }
}

// A text: each line a TEXT (for engraving a label), its capitals about seven tenths of its letters' height; a line
// centred in its box's width or set against its right edge is justified so in the DXF, at the box's middle or right
// edge, as the screen and the SVG have it.
RDE_INTERNAL void fude_zoom_dxfw_text(any _self, fude_zoom_v2 _tl, f64 _px, f64 _w, f64 _h, u8 _style, rde_color _ink, rde_color _paper, const c8* const* _lines, u32 _count) {
    RDE_UNUSED(_h); RDE_UNUSED(_paper);
    fude_zoom_dxfw* _cw      = (fude_zoom_dxfw*)_self;
    const u32       _layer   = fude_zoom_dxfw_layer(_cw, _ink);
    const b8        _centred = (_style & FUDE_ZOOM_EXPORT_TEXT_CENTRED) != 0u, _right = (_style & FUDE_ZOOM_EXPORT_TEXT_RIGHT) != 0u;
    const f64       _pad     = (_style & ~FUDE_ZOOM_EXPORT_TEXT_FLAGS) == FUDE_ZOOM_TEXT_STICKY ? 0.6 * _px : 0.0;
    const f64       _x       = _centred ? _tl.x + _w * 0.5 : _right ? _tl.x + _w : _tl.x + _pad;
    const u8        _align   = _centred ? (u8)FUDE_ZOOM_DXF_ALIGN_CENTRE : _right ? (u8)FUDE_ZOOM_DXF_ALIGN_RIGHT : (u8)FUDE_ZOOM_DXF_ALIGN_LEFT;
    for(u32 _i = 0; _i < _count; _i++) {
        fude_zoom_dxf_text(_cw->dxf, _layer, _x * _cw->mm, (_tl.y - _pad - _px * 0.95 - (f64)_i * _px * 1.3) * _cw->mm, _px * 0.7 * _cw->mm, 0.0, _align, _lines[_i]);
    }
}

// See the header. The walk is a blade's (cutting): sheets, dimensions, guides and hatches are left out.
b8 fude_zoom_export_dxf(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, fude_bytes* _out, fude_zoom_export_check* _check) {
    fude_zoom_dxfw _w = { .dxf = fude_zoom_dxf_new(), .mm = _mm_per_point, .xy = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std()) };
    if(_w.dxf == NULL) {
        rde_arr_free(&_w.xy);
        return false;
    }
    const fude_zoom_export_sink _sink = { .self = &_w, .stroke = fude_zoom_dxfw_stroke, .shape = fude_zoom_dxfw_shape, .fill = fude_zoom_dxfw_fill,
                                          .text = fude_zoom_dxfw_text, .cutting = true };
    fude_zoom_export_walk_as(_s, _half, &_sink, fude_theme_get(FUDE_THEME_PAPER));   // the theme's ink as black, whatever the screen's theme
    if(_check != NULL) {
        const fude_zoom_dxf_check _c = fude_zoom_dxf_preflight(_w.dxf, 0.05);
        _check->open_paths    = _c.open_paths;
        _check->duplicates    = _c.duplicates;
        _check->tiny_segments = _c.tiny_segments;
        _check->paths         = _c.entities;
    }
    const b8 _ok = fude_zoom_dxf_finish(_w.dxf, _out);
    fude_zoom_dxf_free(_w.dxf);
    rde_arr_free(&_w.xy);
    return _ok;
}

// --- a template, printed at its true size --------------------------------------------------------

#define FUDE_ZOOM_TILE_MARGIN  10.0   // mm round each page a printer may not reach
#define FUDE_ZOOM_TILE_OVERLAP 10.0   // mm each page shares with the next, to line them up by
#define FUDE_ZOOM_TILE_MOST    64u    // pages at most

// A registration cross (page points), where pages are laid over each other.
RDE_INTERNAL void fude_zoom_tile_cross(fude_zoom_pdf* _pdf, f64 _x, f64 _y) {
    const f64 _r = 4.0 * FUDE_ZOOM_PDF_MM;
    fude_zoom_pdf_move(_pdf, _x - _r, _y);
    fude_zoom_pdf_line(_pdf, _x + _r, _y);
    fude_zoom_pdf_move(_pdf, _x, _y - _r);
    fude_zoom_pdf_line(_pdf, _x, _y + _r);
    fude_zoom_pdf_stroke(_pdf);
}

// See the header. Each page: the printer's error undone round its middle; the stretch of the view it holds, clipped
// to its inside; its corners' crosses and its shared edges dashed; its label and a map of the pages; and on the first
// page (when the square fits on it) the test square, with its note.
b8 fude_zoom_export_tiles(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, f64 _page_w_mm, f64 _page_h_mm, fude_zoom_v2 _printer,
                          const c8* _square_note, fude_bytes* _out, u32* _pages) {
    if(_pages != NULL) {
        *_pages = 0;
    }
    if(!(_mm_per_point > 0.0)) {
        return false;
    }
    // The view's size in millimetres, and the pages it takes (each printing its
    // inside, less the overlap it shares with the next).
    const f64 _w_mm = _half.x * 2.0 * _mm_per_point, _h_mm = _half.y * 2.0 * _mm_per_point;
    const f64 _in_w = _page_w_mm - 2.0 * FUDE_ZOOM_TILE_MARGIN, _in_h = _page_h_mm - 2.0 * FUDE_ZOOM_TILE_MARGIN;
    const f64 _step_w = _in_w - FUDE_ZOOM_TILE_OVERLAP, _step_h = _in_h - FUDE_ZOOM_TILE_OVERLAP;
    // Counted in doubles first: far out the view is kilometres across.
    const f64 _cols_f = rde_math_max_f64(1.0, rde_math_ceil_f64((_w_mm - FUDE_ZOOM_TILE_OVERLAP) / _step_w));
    const f64 _rows_f = rde_math_max_f64(1.0, rde_math_ceil_f64((_h_mm - FUDE_ZOOM_TILE_OVERLAP) / _step_h));
    if(!(_cols_f * _rows_f <= (f64)FUDE_ZOOM_TILE_MOST) || _rows_f > 26.0) {   // (rows go by letter)
        return false;
    }
    const u32 _cols = (u32)_cols_f, _rows = (u32)_rows_f;
    fude_zoom_pdfw _w = fude_zoom_pdfw_make((fude_zoom_v2){ 0.0, 0.0 });
    if(_w.pdf == NULL) {
        fude_zoom_pdfw_free(&_w);
        return false;
    }
    const fude_zoom_export_sink _sink = { .self = &_w, .stroke = fude_zoom_pdfw_stroke, .shape = fude_zoom_pdfw_shape, .fill = fude_zoom_pdfw_fill,
                                          .image = fude_zoom_pdfw_image, .text = fude_zoom_pdfw_text };
    const f64 _pw = _page_w_mm * FUDE_ZOOM_PDF_MM, _ph = _page_h_mm * FUDE_ZOOM_PDF_MM;
    const f64 _k  = _mm_per_point * FUDE_ZOOM_PDF_MM;   // page points a screen point
    const rde_color _ink = { 40, 40, 46, 255 };
    // The printer's own error undone: everything drawn smaller (or larger) by
    // what it prints too large (or small), round the page's middle.
    const f64 _fx = _printer.x > 0.5 && _printer.x < 2.0 ? 1.0 / _printer.x : 1.0;
    const f64 _fy = _printer.y > 0.5 && _printer.y < 2.0 ? 1.0 / _printer.y : 1.0;
    for(u32 _r = 0; _r < _rows; _r++) {
        for(u32 _c = 0; _c < _cols; _c++) {
            fude_zoom_pdf_page_begin(_w.pdf, _pw, _ph);
            fude_zoom_pdf_save(_w.pdf);
            if(_fx != 1.0 || _fy != 1.0) {
                fude_zoom_pdf_transform(_w.pdf, _fx, 0.0, 0.0, _fy, _pw * 0.5 * (1.0 - _fx), _ph * 0.5 * (1.0 - _fy));
            }
            // The drawing: the stretch of the view this page holds (rows from the top), clipped to its inside.
            const f64 _left_mm = (f64)_c * _step_w, _top_mm = (f64)_r * _step_h;   // from the view's top left
            fude_zoom_pdf_save(_w.pdf);
            fude_zoom_pdf_rect(_w.pdf, FUDE_ZOOM_TILE_MARGIN * FUDE_ZOOM_PDF_MM, FUDE_ZOOM_TILE_MARGIN * FUDE_ZOOM_PDF_MM, _in_w * FUDE_ZOOM_PDF_MM, _in_h * FUDE_ZOOM_PDF_MM);
            fude_zoom_pdf_clip(_w.pdf, false);
            // A screen point (centre origin, Y up) onto the page: the view's top left at the inside's top left,
            // less this page's offset.
            const f64 _ox = FUDE_ZOOM_TILE_MARGIN * FUDE_ZOOM_PDF_MM + (_half.x * _mm_per_point - _left_mm) * FUDE_ZOOM_PDF_MM;
            const f64 _oy = (_page_h_mm - FUDE_ZOOM_TILE_MARGIN) * FUDE_ZOOM_PDF_MM - (_half.y * _mm_per_point - _top_mm) * FUDE_ZOOM_PDF_MM;
            fude_zoom_pdf_transform(_w.pdf, _k, 0.0, 0.0, _k, _ox, _oy);
            fude_zoom_export_walk_as(_s, _half, &_sink, fude_theme_get(FUDE_THEME_PAPER));   // printed on white: the paper theme's ink
            fude_zoom_pdf_restore(_w.pdf);
            // What lines the pages up: the inside's corners crossed, its overlap shaded at the edges it shares.
            fude_zoom_pdf_stroke_color(_w.pdf, _ink);
            fude_zoom_pdf_line_width(_w.pdf, 0.4);
            const f64 _x0 = FUDE_ZOOM_TILE_MARGIN * FUDE_ZOOM_PDF_MM, _y0 = FUDE_ZOOM_TILE_MARGIN * FUDE_ZOOM_PDF_MM;
            const f64 _x1 = _x0 + _in_w * FUDE_ZOOM_PDF_MM, _y1 = _y0 + _in_h * FUDE_ZOOM_PDF_MM;
            fude_zoom_tile_cross(_w.pdf, _x0, _y0); fude_zoom_tile_cross(_w.pdf, _x1, _y0);
            fude_zoom_tile_cross(_w.pdf, _x0, _y1); fude_zoom_tile_cross(_w.pdf, _x1, _y1);
            const f64 _ov = FUDE_ZOOM_TILE_OVERLAP * FUDE_ZOOM_PDF_MM;
            const f64 _dash[2] = { 2.0, 2.0 };
            fude_zoom_pdf_dash(_w.pdf, _dash, 2u, 0.0);
            if(_c + 1u < _cols) { fude_zoom_pdf_move(_w.pdf, _x1 - _ov, _y0); fude_zoom_pdf_line(_w.pdf, _x1 - _ov, _y1); fude_zoom_pdf_stroke(_w.pdf); }
            if(_r + 1u < _rows) { fude_zoom_pdf_move(_w.pdf, _x0, _y0 + _ov); fude_zoom_pdf_line(_w.pdf, _x1, _y0 + _ov); fude_zoom_pdf_stroke(_w.pdf); }
            fude_zoom_pdf_dash(_w.pdf, NULL, 0u, 0.0);
            // Its label (rows by letter, columns by number), where it goes in the whole, and the test square.
            c8 _label[64];
            rde_snprintf(_label, sizeof _label, "%c%u  (%u x %u)", 'A' + (c8)_r, _c + 1u, _rows, _cols);
            fude_zoom_pdf_text(_w.pdf, _x0, _y1 + 3.0 * FUDE_ZOOM_PDF_MM, 9.0, _ink, _label);
            // A mini-map by it: the pages as a grid, this one filled (which way up the whole goes).
            if(_rows * _cols > 1u) {
                const f64 _cell = rde_math_min_f64(2.5, 22.0 / (f64)(_cols > _rows ? _cols : _rows)) * FUDE_ZOOM_PDF_MM;
                const f64 _mx = _x1 - (f64)_cols * _cell, _my = _y1 + 2.0 * FUDE_ZOOM_PDF_MM;
                fude_zoom_pdf_line_width(_w.pdf, 0.25);
                fude_zoom_pdf_fill_color(_w.pdf, _ink);
                for(u32 _mr = 0; _mr < _rows; _mr++) {
                    for(u32 _mc = 0; _mc < _cols; _mc++) {
                        fude_zoom_pdf_rect(_w.pdf, _mx + (f64)_mc * _cell, _my + (f64)(_rows - 1u - _mr) * _cell, _cell, _cell);
                        if(_mr == _r && _mc == _c) {
                            fude_zoom_pdf_fill_stroke(_w.pdf, false);
                        } else {
                            fude_zoom_pdf_stroke(_w.pdf);
                        }
                    }
                }
                fude_zoom_pdf_line_width(_w.pdf, 0.4);
            }
            // The test square and its note, where the square is: the first page, when it fits on it.
            const f64 _sq = 100.0 * FUDE_ZOOM_PDF_MM;
            if(_in_w > 105.0 && _in_h > 105.0 && _r == 0u && _c == 0u) {
                if(_square_note != NULL && _square_note[0] != 0) {
                    fude_zoom_pdf_text(_w.pdf, _x0, _y0 - 6.0 * FUDE_ZOOM_PDF_MM, 7.0, _ink, _square_note);
                }
                fude_zoom_pdf_rect(_w.pdf, _x1 - _sq - 3.0 * FUDE_ZOOM_PDF_MM, _y0 + 3.0 * FUDE_ZOOM_PDF_MM, _sq, _sq);
                fude_zoom_pdf_stroke(_w.pdf);
            }
            fude_zoom_pdf_restore(_w.pdf);
            fude_zoom_pdf_page_end(_w.pdf);
        }
    }
    const b8 _ok = fude_zoom_pdf_finish(_w.pdf, _out);
    fude_zoom_pdfw_free(&_w);
    if(_pages != NULL && _ok) {
        *_pages = _rows * _cols;
    }
    return _ok;
}

// --- the printer's check -----------------------------------------------------------------------

// A ruler's line (page points from _x, _y, _len long, across or down), a tick
// each centimetre, longer each five, and its length by its end.
RDE_INTERNAL void fude_zoom_check_ruler(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _len_mm, b8 _across, rde_color _ink) {
    const f64 _mm = FUDE_ZOOM_PDF_MM;
    fude_zoom_pdf_line_width(_pdf, 0.5);
    fude_zoom_pdf_move(_pdf, _x, _y);
    fude_zoom_pdf_line(_pdf, _across ? _x + _len_mm * _mm : _x, _across ? _y : _y + _len_mm * _mm);
    fude_zoom_pdf_stroke(_pdf);
    for(u32 _k = 0; (f64)_k <= _len_mm / 10.0 + 1e-9; _k++) {
        const f64 _t = (_k % 5u == 0u) ? 6.0 : 3.0;
        const f64 _at = (f64)_k * 10.0 * _mm;
        if(_across) { fude_zoom_pdf_move(_pdf, _x + _at, _y - _t * _mm); fude_zoom_pdf_line(_pdf, _x + _at, _y + _t * _mm); }
        else        { fude_zoom_pdf_move(_pdf, _x - _t * _mm, _y + _at); fude_zoom_pdf_line(_pdf, _x + _t * _mm, _y + _at); }
        fude_zoom_pdf_stroke(_pdf);
    }
    c8 _said[32];
    rde_snprintf(_said, sizeof _said, "%s  %.0f mm", _across ? "X" : "Y", _len_mm);
    if(_across) {
        fude_zoom_pdf_text(_pdf, _x, _y + 9.0 * _mm, 12.0, _ink, _said);
    } else {
        fude_zoom_pdf_text(_pdf, _x + 9.0 * _mm, _y + _len_mm * _mm - 4.0 * _mm, 12.0, _ink, _said);
    }
}

// See the header. The title and how to print it at the top left, the lines centred across near the top, and down the
// left from 20 mm up.
b8 fude_zoom_export_print_check(f64 _page_w_mm, f64 _page_h_mm, const c8* _title, const c8* _how, fude_bytes* _out) {
    fude_zoom_pdf* _pdf = fude_zoom_pdf_new();
    if(_pdf == NULL) {
        return false;
    }
    const f64 _mm = FUDE_ZOOM_PDF_MM;
    const rde_color _ink = { 40, 40, 46, 255 };
    fude_zoom_pdf_page_begin(_pdf, _page_w_mm * _mm, _page_h_mm * _mm);
    fude_zoom_pdf_stroke_color(_pdf, _ink);
    if(_title != NULL && _title[0] != 0) {
        fude_zoom_pdf_text(_pdf, 25.0 * _mm, (_page_h_mm - 25.0) * _mm, 16.0, _ink, _title);
    }
    if(_how != NULL && _how[0] != 0) {
        fude_zoom_pdf_text(_pdf, 25.0 * _mm, (_page_h_mm - 33.0) * _mm, 10.0, _ink, _how);
    }
    const f64 _x = FUDE_ZOOM_CHECK_X_MM, _y = FUDE_ZOOM_CHECK_Y_MM;
    fude_zoom_check_ruler(_pdf, (_page_w_mm - _x) * 0.5 * _mm, (_page_h_mm - 50.0) * _mm, _x, true, _ink);
    fude_zoom_check_ruler(_pdf, 35.0 * _mm, rde_math_min_f64(20.0, _page_h_mm - 62.0 - _y) * _mm, _y, false, _ink);   // (20 mm up: inside what any printer reaches)
    fude_zoom_pdf_page_end(_pdf);
    const b8 _ok = fude_zoom_pdf_finish(_pdf, _out);
    fude_zoom_pdf_free(_pdf);
    return _ok;
}

// --- the cut list --------------------------------------------------------------------------------

// A row of the cut list: a part, its sizes, grain and material, and how many of it.
typedef struct {
    c8  name[FUDE_ZOOM_BOARD_NAME];
    f64 length, width, thickness;   // mm, rounded to a tenth (what tells two parts the same)
    u8  grain;
    u8  material;                   // FUDE_ZOOM_MATERIAL_
    u32 count;
} fude_zoom_cutlist_row;

// rde_sort's order for the cut list's rows: by name, then material, then the thickest, longest and widest first, so
// the most alike lie together.
RDE_INTERNAL int fude_zoom_cutlist_order(const any _a, const any _b) {
    const fude_zoom_cutlist_row* _p = (const fude_zoom_cutlist_row*)_a;
    const fude_zoom_cutlist_row* _q = (const fude_zoom_cutlist_row*)_b;
    const int _byname = rde_strcmp(_p->name, _q->name);
    if(_byname != 0) {
        return _byname;
    }
    if(_p->material != _q->material) {
        return _p->material < _q->material ? -1 : 1;
    }
    return _p->thickness != _q->thickness ? (_p->thickness > _q->thickness ? -1 : 1) :
           _p->length != _q->length ? (_p->length > _q->length ? -1 : 1) : (_p->width > _q->width ? -1 : (_p->width < _q->width ? 1 : 0));
}

// A field of a CSV line: quoted when it holds a comma, a quote or a break (its quotes doubled).
RDE_INTERNAL void fude_zoom_csv_field(fude_bytes* _out, const c8* _text) {
    const b8 _quote = rde_strpbrk(_text, ",\"\r\n") != NULL;
    if(_quote) {
        rde_arr_add(_out, &(u8){ '"' });
    }
    for(const c8* _c = _text; *_c != 0; _c++) {
        if(*_c == '"') {
            rde_arr_add(_out, &(u8){ '"' });
        }
        rde_arr_add(_out, (any)_c);
    }
    if(_quote) {
        rde_arr_add(_out, &(u8){ '"' });
    }
}

// See the header. Hidden boards and those in frames not shown are left out. Parts are matched by a linear search of
// the rows found so far.
u32 fude_zoom_export_cutlist(const fude_zoom_scene* _s, f64 _mm_per_unit, const c8* const* _header, const c8* const* _grain_words,
                             const c8* const* _material_words, fude_bytes* _out) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _parts = rde_arr_new(sizeof(fude_zoom_cutlist_row), _heap);
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    u32 _boards = 0;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_BOARD || fude_zoom_scene_hides(_s, _o) ||
           !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _count = fude_zoom_scene_shape_numbers(_s, _i, _n, FUDE_ZOOM_SHAPE_NUMBERS);
        if(_count < 4u) {
            continue;
        }
        const f64 _mm = _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _mm_per_unit;
        fude_zoom_cutlist_row _p;
        rde_memset(&_p, 0, sizeof(_p));
        fude_zoom_board_name(_n, _count, _p.name, sizeof(_p.name));
        _p.length    = rde_math_round_f64(rde_math_abs_f64(_n[0]) * 2.0 * _mm * 10.0) / 10.0;
        _p.width     = rde_math_round_f64(rde_math_abs_f64(_n[1]) * 2.0 * _mm * 10.0) / 10.0;
        _p.thickness = rde_math_round_f64(_n[2] * 10.0) / 10.0;
        _p.grain     = fude_zoom_board_grain(_n, _count);
        _p.material  = fude_zoom_board_material(_n, _count);
        if(_p.width > _p.length) {
            // Length the longer side; the grain said against it.
            const f64 _t = _p.length; _p.length = _p.width; _p.width = _t;
            _p.grain ^= 1u;
        }
        _boards++;
        b8 _known = false;
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_parts) && !_known; _k++) {
            fude_zoom_cutlist_row* _q = &((fude_zoom_cutlist_row*)_parts.memory)[_k];
            if(rde_strcmp(_q->name, _p.name) == 0 && _q->length == _p.length && _q->width == _p.width && _q->thickness == _p.thickness && _q->grain == _p.grain &&
               _q->material == _p.material) {
                _q->count++;
                _known = true;
            }
        }
        if(!_known) {
            _p.count = 1u;
            rde_arr_add(&_parts, &_p);
        }
    }
    const u32 _rows = (u32)rde_arr_length(&_parts);
    rde_sort(_parts.memory, _rows, sizeof(fude_zoom_cutlist_row), fude_zoom_cutlist_order);
    // Part, quantity, length, width, thickness (mm), material, grain; then each part, the most alike together.
    for(u32 _c = 0; _c < 7u; _c++) {
        if(_c > 0) {
            rde_arr_add(_out, &(u8){ ',' });
        }
        fude_zoom_csv_field(_out, _header[_c]);
    }
    rde_arr_add(_out, &(u8){ '\r' });
    rde_arr_add(_out, &(u8){ '\n' });
    for(u32 _k = 0; _k < _rows; _k++) {
        const fude_zoom_cutlist_row* _p = &((const fude_zoom_cutlist_row*)_parts.memory)[_k];
        c8 _num[96];
        fude_zoom_csv_field(_out, _p->name);
        rde_snprintf(_num, sizeof _num, ",%u,%.1f,%.1f,%.1f,", _p->count, _p->length, _p->width, _p->thickness);
        for(const c8* _c = _num; *_c != 0; _c++) {
            rde_arr_add(_out, (any)_c);
        }
        fude_zoom_csv_field(_out, _material_words[_p->material]);
        rde_arr_add(_out, &(u8){ ',' });
        fude_zoom_csv_field(_out, _grain_words[_p->grain]);
        rde_arr_add(_out, &(u8){ '\r' });
        rde_arr_add(_out, &(u8){ '\n' });
    }
    rde_arr_free(&_parts);
    return _boards;
}
