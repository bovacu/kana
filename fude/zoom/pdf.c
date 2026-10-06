// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/pdf.h"
#include "drawing/base/utf8.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See pdf.h. The pages' content streams and the images' samples are kept as
// they come, each in one buffer; the objects are only numbered and written at
// the end, when everything they refer to is known:
//
//   1 the catalog   2 the page tree   3 the resources (one dictionary every
//   page shares: all the alpha states, images and the font)   then the font,
//   the alpha states, the images (each followed by its mask) and, last, each
//   page with its content stream.
// ===========================================================================

#define FUDE_ZOOM_PDF_LIMIT  0x7FFFFFFFu   // content and samples together, bytes: the file must fit a fude_bytes
#define FUDE_ZOOM_PDF_DASHES 16u           // lengths in a dash pattern, at most
#define FUDE_ZOOM_PDF_SIDE   3.0           // the smallest page side PDF allows, points
#define FUDE_ZOOM_PDF_PLACES 4u            // decimals for coordinates and lengths
#define FUDE_ZOOM_PDF_SCALE  6u            // decimals for a matrix's scale and turn (small ones must not round away)
#define FUDE_ZOOM_PDF_COLOR  3u            // decimals for a colour channel: enough to give back its byte

typedef struct {
    f64 width;
    f64 height;
    u32 at;      // its content stream, in the document's content
    u32 size;
} fude_zoom_pdf_page;

typedef struct {
    u32 width;
    u32 height;
    u32 at;          // its samples (or the JPEG's bytes), in the document's data
    u32 size;
    u32 mask_at;     // its alpha; size 0: opaque, no mask
    u32 mask_size;
    u8  components;  // 1 gray, 3 RGB, 4 CMYK
    b8  jpeg;
    b8  inverted;    // an Adobe CMYK JPEG: its ink stored the other way round
} fude_zoom_pdf_picture;

// What the page's stream has set so far, so nothing is said twice.
typedef struct {
    rde_color fill;
    rde_color stroke;
    f64       width;
} fude_zoom_pdf_state;

struct fude_zoom_pdf {
    fude_bytes                          content;   // every page's content stream, one after another
    fude_bytes                          data;      // every image's bytes, one after another
    rde_arr TYPE(fude_zoom_pdf_page)    pages;
    rde_arr TYPE(fude_zoom_pdf_picture) images;
    rde_arr TYPE(fude_zoom_pdf_state)   saved;     // the states each open q put away
    fude_zoom_pdf_state                 state;
    b8                                  open;      // inside a page
    b8                                  text;      // Helvetica is used
    b8                                  full;      // outgrew the limit: finish fails
    b8                                  fill_alpha[256];     // the alphas fills have used (each an ExtGState)
    b8                                  stroke_alpha[256];
};

// Helvetica's advance widths (thousandths of the size) by WinAnsi code, from
// its standard AFM; 0 where WinAnsi has no character (never written).
static const u16 fude_zoom_pdf_helvetica[256] = {
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     278,  278,  355,  556,  556,  889,  667,  191,  333,  333,  389,  584,  278,  333,  278,  278,
     556,  556,  556,  556,  556,  556,  556,  556,  556,  556,  278,  278,  584,  584,  584,  556,
    1015,  667,  667,  722,  722,  667,  611,  778,  722,  278,  500,  667,  556,  833,  722,  778,
     667,  778,  722,  667,  611,  722,  667,  944,  667,  667,  611,  278,  278,  278,  469,  556,
     333,  556,  556,  500,  556,  556,  278,  556,  556,  222,  222,  500,  222,  833,  556,  556,
     556,  556,  333,  500,  278,  556,  500,  722,  500,  500,  500,  334,  260,  334,  584,    0,
     556,    0,  222,  556,  333, 1000,  556,  556,  333, 1000,  667,  333, 1000,    0,  611,    0,
       0,  222,  222,  333,  333,  350,  556, 1000,  333, 1000,  500,  333,  944,    0,  500,  667,
     278,  333,  556,  556,  556,  556,  260,  556,  333,  737,  370,  556,  584,  333,  737,  333,
     400,  584,  333,  333,  333,  556,  537,  278,  333,  333,  365,  556,  834,  834,  834,  611,
     667,  667,  667,  667,  667,  667, 1000,  722,  667,  667,  667,  667,  278,  278,  278,  278,
     722,  722,  778,  778,  778,  778,  778,  584,  778,  722,  722,  722,  722,  667,  667,  611,
     556,  556,  556,  556,  556,  556,  889,  500,  556,  556,  556,  556,  278,  278,  278,  278,
     556,  556,  556,  556,  556,  556,  556,  584,  611,  556,  556,  556,  556,  500,  556,  500,
};

// What WinAnsi puts at 0x80 to 0x9F, in place of Latin-1's control codes.
static const u16 fude_zoom_pdf_winansi_extra[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178,
};

static const fude_zoom_pdf_state fude_zoom_pdf_default = { { 0, 0, 0, 255 }, { 0, 0, 0, 255 }, 1.0 };

// --- numbers and bytes -----------------------------------------------------------------------------

// A real as PDF writes it: at most _decimals places, its trailing zeros gone,
// never an exponent (PDF has none), -0 as 0, NaN and the infinities as 0, kept
// within a billion either way. Into _out (24 bytes at least); its length.
RDE_INTERNAL u32 fude_zoom_pdf_real(f64 _v, u32 _decimals, c8* _out) {
    static const f64 _scales[] = { 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0 };
    if(_decimals > 6u) {
        _decimals = 6u;
    }
    if(!isfinite(_v)) {
        _v = 0.0;
    }
    _v = fmin(fmax(_v, -1e9), 1e9);
    const i64 _n = llround(_v * _scales[_decimals]);
    u64       _m = (u64)(_n < 0 ? -_n : _n);
    // Its digits, the last first, at least one before the point.
    c8  _digits[24];
    u32 _k = 0;
    do {
        _digits[_k++] = (c8)('0' + (int)(_m % 10u));
        _m /= 10u;
    } while(_m > 0u || _k <= _decimals);
    u32 _zeros = 0;
    while(_zeros < _decimals && _digits[_zeros] == '0') {
        _zeros++;
    }
    u32 _len = 0;
    if(_n < 0) {
        _out[_len++] = '-';
    }
    for(u32 _i = _k; _i > _decimals; _i--) {
        _out[_len++] = _digits[_i - 1u];
    }
    if(_zeros < _decimals) {
        _out[_len++] = '.';
        for(u32 _i = _decimals; _i > _zeros; _i--) {
            _out[_len++] = _digits[_i - 1u];
        }
    }
    return _len;
}

// Can _n more bytes be kept? Once not, the document is spoilt (finish says so).
RDE_INTERNAL b8 fude_zoom_pdf_room(fude_zoom_pdf* _pdf, u64 _n) {
    if(_pdf->full || (u64)fude_bytes_size(&_pdf->content) + (u64)fude_bytes_size(&_pdf->data) + _n > FUDE_ZOOM_PDF_LIMIT) {
        _pdf->full = true;
        return false;
    }
    return true;
}

// Bytes onto the open page's content stream.
RDE_INTERNAL void fude_zoom_pdf_emit(fude_zoom_pdf* _pdf, const c8* _s, u32 _n) {
    if(_n > 0u && fude_zoom_pdf_room(_pdf, _n)) {
        fude_put_data(&_pdf->content, _s, _n);
    }
}

RDE_INTERNAL void fude_zoom_pdf_emit_str(fude_zoom_pdf* _pdf, const c8* _s) {
    fude_zoom_pdf_emit(_pdf, _s, (u32)strlen(_s));
}

// _n numbers (six at most), then the operator, one to a line.
RDE_INTERNAL void fude_zoom_pdf_op(fude_zoom_pdf* _pdf, const f64* _v, u32 _n, u32 _decimals, const c8* _op) {
    c8  _buf[192];
    u32 _len = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _len += fude_zoom_pdf_real(_v[_i], _decimals, _buf + _len);
        _buf[_len++] = ' ';
    }
    const u32 _k = (u32)strlen(_op);
    memcpy(_buf + _len, _op, _k);
    _len += _k;
    _buf[_len++] = '\n';
    fude_zoom_pdf_emit(_pdf, _buf, _len);
}

// Formatted text into the file being finished (names and integers only: no
// reals through printf, whose decimal point follows the locale).
RDE_INTERNAL void fude_zoom_pdf_put(fude_bytes* _out, const c8* _fmt, ...) {
    c8      _buf[256];
    va_list _args;
    va_start(_args, _fmt);
    const int _n = vsnprintf(_buf, sizeof _buf, _fmt, _args);
    va_end(_args);
    if(_n > 0) {
        fude_put_data(_out, _buf, (u32)((usize)_n < sizeof _buf ? (usize)_n : sizeof _buf - 1u));
    }
}

RDE_INTERNAL void fude_zoom_pdf_put_real(fude_bytes* _out, f64 _v, u32 _decimals) {
    c8 _buf[24];
    fude_put_data(_out, _buf, fude_zoom_pdf_real(_v, _decimals, _buf));
}

// --- the document ----------------------------------------------------------------------------------

fude_zoom_pdf* fude_zoom_pdf_new(void) {
    fude_zoom_pdf* _pdf = (fude_zoom_pdf*)calloc(1u, sizeof(fude_zoom_pdf));
    if(_pdf == NULL) {
        return NULL;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _pdf->content = fude_bytes_new(4096u);
    _pdf->data    = fude_bytes_new(16u);
    _pdf->pages   = rde_arr_new(sizeof(fude_zoom_pdf_page), _heap);
    _pdf->images  = rde_arr_new(sizeof(fude_zoom_pdf_picture), _heap);
    _pdf->saved   = rde_arr_new(sizeof(fude_zoom_pdf_state), _heap);
    _pdf->state   = fude_zoom_pdf_default;
    return _pdf;
}

void fude_zoom_pdf_free(fude_zoom_pdf* _pdf) {
    if(_pdf == NULL) {
        return;
    }
    rde_arr_free(&_pdf->content);
    rde_arr_free(&_pdf->data);
    rde_arr_free(&_pdf->pages);
    rde_arr_free(&_pdf->images);
    rde_arr_free(&_pdf->saved);
    free(_pdf);
}

// --- pages -----------------------------------------------------------------------------------------

void fude_zoom_pdf_page_begin(fude_zoom_pdf* _pdf, f64 _width_pt, f64 _height_pt) {
    if(_pdf == NULL) {
        return;
    }
    if(_pdf->open) {
        fude_zoom_pdf_page_end(_pdf);
    }
    fude_zoom_pdf_page _page = {
        .width  = isfinite(_width_pt) && _width_pt >= FUDE_ZOOM_PDF_SIDE ? _width_pt : FUDE_ZOOM_PDF_SIDE,
        .height = isfinite(_height_pt) && _height_pt >= FUDE_ZOOM_PDF_SIDE ? _height_pt : FUDE_ZOOM_PDF_SIDE,
        .at     = fude_bytes_size(&_pdf->content),
    };
    rde_arr_add(&_pdf->pages, &_page);
    rde_arr_clear(&_pdf->saved);
    _pdf->state = fude_zoom_pdf_default;
    _pdf->open  = true;
}

void fude_zoom_pdf_page_end(fude_zoom_pdf* _pdf) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    // Every stream balanced on its own: some readers refuse a q left open.
    while(rde_arr_length(&_pdf->saved) > 0u) {
        fude_zoom_pdf_restore(_pdf);
    }
    fude_zoom_pdf_page* _page = (fude_zoom_pdf_page*)rde_arr_at(&_pdf->pages, rde_arr_length(&_pdf->pages) - 1u);
    _page->size = fude_bytes_size(&_pdf->content) - _page->at;
    _pdf->open  = false;
}

// --- graphics state --------------------------------------------------------------------------------

void fude_zoom_pdf_save(fude_zoom_pdf* _pdf) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    fude_zoom_pdf_emit_str(_pdf, "q\n");
    rde_arr_add(&_pdf->saved, &_pdf->state);
}

void fude_zoom_pdf_restore(fude_zoom_pdf* _pdf) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    const u32 _depth = (u32)rde_arr_length(&_pdf->saved);
    if(_depth == 0u) {
        return;
    }
    fude_zoom_pdf_emit_str(_pdf, "Q\n");
    _pdf->state = *(const fude_zoom_pdf_state*)rde_arr_at(&_pdf->saved, _depth - 1u);
    rde_arr_remove(&_pdf->saved, _depth - 1u);
}

void fude_zoom_pdf_transform(fude_zoom_pdf* _pdf, f64 _a, f64 _b, f64 _c, f64 _d, f64 _e, f64 _f) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    const f64 _turn[4]  = { _a, _b, _c, _d };
    const f64 _shift[2] = { _e, _f };
    c8  _buf[192];
    u32 _len = 0;
    for(u32 _i = 0; _i < 4u; _i++) {
        _len += fude_zoom_pdf_real(_turn[_i], FUDE_ZOOM_PDF_SCALE, _buf + _len);
        _buf[_len++] = ' ';
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        _len += fude_zoom_pdf_real(_shift[_i], FUDE_ZOOM_PDF_PLACES, _buf + _len);
        _buf[_len++] = ' ';
    }
    memcpy(_buf + _len, "cm\n", 3u);
    fude_zoom_pdf_emit(_pdf, _buf, _len + 3u);
}

// A colour for fills or strokes: its RGB when it changes, and its alpha by the
// shared ExtGState for that alpha when that changes.
RDE_INTERNAL void fude_zoom_pdf_color(fude_zoom_pdf* _pdf, rde_color _c, b8 _stroke) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    rde_color* _now = _stroke ? &_pdf->state.stroke : &_pdf->state.fill;
    if(_c.r != _now->r || _c.g != _now->g || _c.b != _now->b) {
        const f64 _rgb[3] = { _c.r / 255.0, _c.g / 255.0, _c.b / 255.0 };
        fude_zoom_pdf_op(_pdf, _rgb, 3u, FUDE_ZOOM_PDF_COLOR, _stroke ? "RG" : "rg");
    }
    if(_c.a != _now->a) {
        (_stroke ? _pdf->stroke_alpha : _pdf->fill_alpha)[_c.a] = true;
        c8 _buf[32];
        const int _n = snprintf(_buf, sizeof _buf, "/%s%u gs\n", _stroke ? "as" : "af", (unsigned)_c.a);
        fude_zoom_pdf_emit(_pdf, _buf, (u32)_n);
    }
    *_now = _c;
}

void fude_zoom_pdf_fill_color(fude_zoom_pdf* _pdf, rde_color _c) {
    fude_zoom_pdf_color(_pdf, _c, false);
}

void fude_zoom_pdf_stroke_color(fude_zoom_pdf* _pdf, rde_color _c) {
    fude_zoom_pdf_color(_pdf, _c, true);
}

void fude_zoom_pdf_line_width(fude_zoom_pdf* _pdf, f64 _width) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    if(!isfinite(_width) || _width < 0.0) {
        _width = 0.0;   // PDF's thinnest line a device can draw
    }
    if(_width == _pdf->state.width) {
        return;
    }
    fude_zoom_pdf_op(_pdf, &_width, 1u, FUDE_ZOOM_PDF_PLACES, "w");
    _pdf->state.width = _width;
}

void fude_zoom_pdf_line_style(fude_zoom_pdf* _pdf, u8 _cap, u8 _join) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    c8 _buf[16];
    const int _n = snprintf(_buf, sizeof _buf, "%u J %u j\n", (unsigned)(_cap > 2u ? 2u : _cap), (unsigned)(_join > 2u ? 2u : _join));
    fude_zoom_pdf_emit(_pdf, _buf, (u32)_n);
}

void fude_zoom_pdf_dash(fude_zoom_pdf* _pdf, const f64* _pattern, u32 _count, f64 _phase) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    if(_pattern == NULL || _count > FUDE_ZOOM_PDF_DASHES) {
        _count = _pattern == NULL ? 0u : FUDE_ZOOM_PDF_DASHES;
    }
    // PDF calls a negative length, or a pattern of only zeros, an error: solid.
    b8 _any = false;
    for(u32 _i = 0; _i < _count; _i++) {
        if(!isfinite(_pattern[_i]) || _pattern[_i] < 0.0) {
            _count = 0u;
            break;
        }
        if(_pattern[_i] >= 0.00005) {
            _any = true;
        }
    }
    if(!_any) {
        _count = 0u;
    }
    c8  _buf[FUDE_ZOOM_PDF_DASHES * 24u + 40u];
    u32 _len = 0;
    _buf[_len++] = '[';
    for(u32 _i = 0; _i < _count; _i++) {
        if(_i > 0u) {
            _buf[_len++] = ' ';
        }
        _len += fude_zoom_pdf_real(_pattern[_i], FUDE_ZOOM_PDF_PLACES, _buf + _len);
    }
    _buf[_len++] = ']';
    _buf[_len++] = ' ';
    _len += fude_zoom_pdf_real(_count > 0u ? _phase : 0.0, FUDE_ZOOM_PDF_PLACES, _buf + _len);
    memcpy(_buf + _len, " d\n", 3u);
    fude_zoom_pdf_emit(_pdf, _buf, _len + 3u);
}

// --- paths -----------------------------------------------------------------------------------------

void fude_zoom_pdf_move(fude_zoom_pdf* _pdf, f64 _x, f64 _y) {
    if(_pdf != NULL && _pdf->open) {
        const f64 _v[2] = { _x, _y };
        fude_zoom_pdf_op(_pdf, _v, 2u, FUDE_ZOOM_PDF_PLACES, "m");
    }
}

void fude_zoom_pdf_line(fude_zoom_pdf* _pdf, f64 _x, f64 _y) {
    if(_pdf != NULL && _pdf->open) {
        const f64 _v[2] = { _x, _y };
        fude_zoom_pdf_op(_pdf, _v, 2u, FUDE_ZOOM_PDF_PLACES, "l");
    }
}

void fude_zoom_pdf_curve(fude_zoom_pdf* _pdf, f64 _x1, f64 _y1, f64 _x2, f64 _y2, f64 _x3, f64 _y3) {
    if(_pdf != NULL && _pdf->open) {
        const f64 _v[6] = { _x1, _y1, _x2, _y2, _x3, _y3 };
        fude_zoom_pdf_op(_pdf, _v, 6u, FUDE_ZOOM_PDF_PLACES, "c");
    }
}

void fude_zoom_pdf_close(fude_zoom_pdf* _pdf) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, "h\n");
    }
}

void fude_zoom_pdf_rect(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _w, f64 _h) {
    if(_pdf != NULL && _pdf->open) {
        const f64 _v[4] = { _x, _y, _w, _h };
        fude_zoom_pdf_op(_pdf, _v, 4u, FUDE_ZOOM_PDF_PLACES, "re");
    }
}

void fude_zoom_pdf_polyline(fude_zoom_pdf* _pdf, const f64* _xy, u32 _count, b8 _closed) {
    if(_pdf == NULL || !_pdf->open || _xy == NULL || _count == 0u) {
        return;
    }
    // In blocks, straight into a buffer: no call per number.
    c8  _buf[4096];
    u32 _len = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_len > sizeof _buf - 64u) {
            fude_zoom_pdf_emit(_pdf, _buf, _len);
            _len = 0;
        }
        _len += fude_zoom_pdf_real(_xy[2u * _i], FUDE_ZOOM_PDF_PLACES, _buf + _len);
        _buf[_len++] = ' ';
        _len += fude_zoom_pdf_real(_xy[2u * _i + 1u], FUDE_ZOOM_PDF_PLACES, _buf + _len);
        _buf[_len++] = ' ';
        _buf[_len++] = _i == 0u ? 'm' : 'l';
        _buf[_len++] = '\n';
    }
    if(_closed) {
        _buf[_len++] = 'h';
        _buf[_len++] = '\n';
    }
    fude_zoom_pdf_emit(_pdf, _buf, _len);
}

void fude_zoom_pdf_fill(fude_zoom_pdf* _pdf, b8 _even_odd) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, _even_odd ? "f*\n" : "f\n");
    }
}

void fude_zoom_pdf_stroke(fude_zoom_pdf* _pdf) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, "S\n");
    }
}

void fude_zoom_pdf_fill_stroke(fude_zoom_pdf* _pdf, b8 _even_odd) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, _even_odd ? "B*\n" : "B\n");
    }
}

void fude_zoom_pdf_clip(fude_zoom_pdf* _pdf, b8 _even_odd) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, _even_odd ? "W* n\n" : "W n\n");
    }
}

// --- images ----------------------------------------------------------------------------------------

// A JPEG's size and colours from its frame header, and whether Adobe's marker
// (APP14) is there, which means a CMYK one is stored inverted. Only the frames
// DCTDecode reads (SOF0 baseline, SOF1 extended, SOF2 progressive; 8 bits).
RDE_INTERNAL b8 fude_zoom_pdf_jpeg_info(const u8* _b, u32 _size, fude_zoom_pdf_picture* _p) {
    if(_size < 4u || _b[0] != 0xFFu || _b[1] != 0xD8u) {
        return false;
    }
    u32 _i = 2u;
    while(_i < _size) {
        if(_b[_i] != 0xFFu) {
            return false;
        }
        while(_i < _size && _b[_i] == 0xFFu) {   // a marker may be padded with 0xFF
            _i++;
        }
        if(_i >= _size) {
            return false;
        }
        const u8 _m = _b[_i++];
        if(_m == 0x01u || (_m >= 0xD0u && _m <= 0xD8u)) {
            continue;   // markers with nothing after them
        }
        if(_m == 0xD9u || _m == 0xDAu || _i + 2u > _size) {
            return false;   // the end, or a scan, before any frame header
        }
        const u32 _len = ((u32)_b[_i] << 8) | (u32)_b[_i + 1u];
        if(_len < 2u || _len > _size - _i) {
            return false;
        }
        const u8* _seg = _b + _i + 2u;   // the segment after its length
        if(_m == 0xC0u || _m == 0xC1u || _m == 0xC2u) {
            if(_len < 8u) {
                return false;
            }
            _p->height     = ((u32)_seg[1] << 8) | (u32)_seg[2];
            _p->width      = ((u32)_seg[3] << 8) | (u32)_seg[4];
            _p->components = _seg[5];
            return _seg[0] == 8u && _p->width > 0u && _p->height > 0u
                && (_p->components == 1u || _p->components == 3u || _p->components == 4u)
                && _len >= 8u + 3u * (u32)_p->components;
        }
        if(_m >= 0xC3u && _m <= 0xCFu && _m != 0xC4u && _m != 0xC8u && _m != 0xCCu) {
            return false;   // lossless or arithmetic-coded: DCTDecode reads neither
        }
        if(_m == 0xEEu && _len >= 7u && memcmp(_seg, "Adobe", 5u) == 0) {
            _p->inverted = true;
        }
        _i += _len;
    }
    return false;
}

u32 fude_zoom_pdf_jpeg(fude_zoom_pdf* _pdf, const u8* _bytes, u32 _size) {
    if(_pdf == NULL || _bytes == NULL) {
        return 0u;
    }
    fude_zoom_pdf_picture _p;
    memset(&_p, 0, sizeof _p);
    if(!fude_zoom_pdf_jpeg_info(_bytes, _size, &_p)) {
        return 0u;
    }
    if((u64)fude_bytes_size(&_pdf->content) + (u64)fude_bytes_size(&_pdf->data) + _size > FUDE_ZOOM_PDF_LIMIT) {
        return 0u;
    }
    _p.jpeg     = true;
    _p.inverted = _p.inverted && _p.components == 4u;
    _p.at       = fude_bytes_size(&_pdf->data);
    _p.size     = _size;
    fude_put_data(&_pdf->data, _bytes, _size);
    rde_arr_add(&_pdf->images, &_p);
    return (u32)rde_arr_length(&_pdf->images);
}

u32 fude_zoom_pdf_rgba(fude_zoom_pdf* _pdf, const u8* _rgba, u32 _width, u32 _height) {
    if(_pdf == NULL || _rgba == NULL || _width == 0u || _height == 0u) {
        return 0u;
    }
    const u64 _n = (u64)_width * (u64)_height;
    if(_n > FUDE_ZOOM_PDF_LIMIT) {
        return 0u;
    }
    b8 _opaque = true;
    for(u64 _i = 0; _i < _n && _opaque; _i++) {
        _opaque = _rgba[_i * 4u + 3u] == 255u;
    }
    if((u64)fude_bytes_size(&_pdf->content) + (u64)fude_bytes_size(&_pdf->data) + _n * (_opaque ? 3u : 4u) > FUDE_ZOOM_PDF_LIMIT) {
        return 0u;
    }
    fude_zoom_pdf_picture _p;
    memset(&_p, 0, sizeof _p);
    _p.width      = _width;
    _p.height     = _height;
    _p.components = 3u;
    _p.at         = fude_bytes_size(&_pdf->data);
    _p.size       = (u32)(_n * 3u);
    u8* _rgb = (u8*)rde_arr_add_n(&_pdf->data, (usize)_p.size);
    for(u64 _i = 0; _i < _n; _i++) {
        _rgb[_i * 3u]      = _rgba[_i * 4u];
        _rgb[_i * 3u + 1u] = _rgba[_i * 4u + 1u];
        _rgb[_i * 3u + 2u] = _rgba[_i * 4u + 2u];
    }
    if(!_opaque) {
        _p.mask_at   = fude_bytes_size(&_pdf->data);
        _p.mask_size = (u32)_n;
        u8* _alpha = (u8*)rde_arr_add_n(&_pdf->data, (usize)_n);
        for(u64 _i = 0; _i < _n; _i++) {
            _alpha[_i] = _rgba[_i * 4u + 3u];
        }
    }
    rde_arr_add(&_pdf->images, &_p);
    return (u32)rde_arr_length(&_pdf->images);
}

void fude_zoom_pdf_image(fude_zoom_pdf* _pdf, u32 _id, f64 _a, f64 _b, f64 _c, f64 _d, f64 _e, f64 _f) {
    if(_pdf == NULL || !_pdf->open || _id == 0u || _id > (u32)rde_arr_length(&_pdf->images)) {
        return;
    }
    fude_zoom_pdf_save(_pdf);
    fude_zoom_pdf_transform(_pdf, _a, _b, _c, _d, _e, _f);
    c8 _buf[32];
    const int _n = snprintf(_buf, sizeof _buf, "/Im%u Do\n", (unsigned)_id);
    fude_zoom_pdf_emit(_pdf, _buf, (u32)_n);
    fude_zoom_pdf_restore(_pdf);
}

// --- text ------------------------------------------------------------------------------------------

// A code point as WinAnsiEncoding has it; '?' for one it has not.
RDE_INTERNAL u8 fude_zoom_pdf_winansi(u32 _cp) {
    if((_cp >= 0x20u && _cp <= 0x7Eu) || (_cp >= 0xA0u && _cp <= 0xFFu)) {
        return (u8)_cp;
    }
    for(u32 _i = 0; _i < 32u; _i++) {
        if(fude_zoom_pdf_winansi_extra[_i] != 0u && fude_zoom_pdf_winansi_extra[_i] == _cp) {
            return (u8)(0x80u + _i);
        }
    }
    return (u8)'?';
}

void fude_zoom_pdf_text(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _size, rde_color _c, const c8* _utf8) {
    if(_pdf == NULL || !_pdf->open || _utf8 == NULL || _utf8[0] == '\0' || !isfinite(_size) || _size <= 0.0) {
        return;
    }
    _pdf->text = true;
    fude_zoom_pdf_save(_pdf);
    fude_zoom_pdf_fill_color(_pdf, _c);
    c8  _buf[512];
    u32 _len = 0;
    memcpy(_buf, "BT\n/F1 ", 7u);
    _len = 7u;
    _len += fude_zoom_pdf_real(_size, FUDE_ZOOM_PDF_PLACES, _buf + _len);
    memcpy(_buf + _len, " Tf\n", 4u);
    _len += 4u;
    const f64 _at[2] = { _x, _y };
    for(u32 _i = 0; _i < 2u; _i++) {
        _len += fude_zoom_pdf_real(_at[_i], FUDE_ZOOM_PDF_PLACES, _buf + _len);
        _buf[_len++] = ' ';
    }
    memcpy(_buf + _len, "Td\n(", 4u);
    _len += 4u;
    // The bytes as they are (a literal string may hold any), but for the three
    // the string itself is made of.
    const c8* _s = _utf8;
    for(u32 _cp = fude_utf8_next(&_s); _cp != 0u; _cp = fude_utf8_next(&_s)) {
        if(_len > sizeof _buf - 8u) {
            fude_zoom_pdf_emit(_pdf, _buf, _len);
            _len = 0;
        }
        const u8 _byte = fude_zoom_pdf_winansi(_cp);
        if(_byte == '(' || _byte == ')' || _byte == '\\') {
            _buf[_len++] = '\\';
        }
        _buf[_len++] = (c8)_byte;
    }
    memcpy(_buf + _len, ") Tj\nET\n", 8u);
    fude_zoom_pdf_emit(_pdf, _buf, _len + 8u);
    fude_zoom_pdf_restore(_pdf);
}

f64 fude_zoom_pdf_text_width(f64 _size, const c8* _utf8) {
    if(_utf8 == NULL || !isfinite(_size)) {
        return 0.0;
    }
    u64       _units = 0;
    const c8* _s     = _utf8;
    for(u32 _cp = fude_utf8_next(&_s); _cp != 0u; _cp = fude_utf8_next(&_s)) {
        _units += fude_zoom_pdf_helvetica[fude_zoom_pdf_winansi(_cp)];
    }
    return (f64)_units * _size / 1000.0;
}

// --- the file --------------------------------------------------------------------------------------

// Where object _n starts (from the file's first byte), and its first line.
RDE_INTERNAL void fude_zoom_pdf_object(fude_bytes* _out, u32 _base, u32* _offsets, u32 _n) {
    _offsets[_n] = fude_bytes_size(_out) - _base;
    fude_zoom_pdf_put(_out, "%u 0 obj\n", (unsigned)_n);
}

RDE_INTERNAL void fude_zoom_pdf_stream(fude_bytes* _out, const u8* _data, u32 _size) {
    fude_zoom_pdf_put(_out, "/Length %u >>\nstream\n", (unsigned)_size);
    fude_put_data(_out, _data, _size);
    fude_zoom_pdf_put(_out, "\nendstream\nendobj\n");
}

b8 fude_zoom_pdf_finish(fude_zoom_pdf* _pdf, fude_bytes* _out) {
    if(_pdf == NULL || _out == NULL) {
        return false;
    }
    if(_pdf->open) {
        fude_zoom_pdf_page_end(_pdf);
    }
    const u32 _pages  = (u32)rde_arr_length(&_pdf->pages);
    const u32 _images = (u32)rde_arr_length(&_pdf->images);
    if(_pages == 0u || _pdf->full) {
        return false;
    }
    const fude_zoom_pdf_page*    _page = (const fude_zoom_pdf_page*)_pdf->pages.memory;
    const fude_zoom_pdf_picture* _pic  = (const fude_zoom_pdf_picture*)_pdf->images.memory;
    u32 _states = 0, _masks = 0;
    for(u32 _i = 0; _i < 256u; _i++) {
        _states += (u32)_pdf->fill_alpha[_i] + (u32)_pdf->stroke_alpha[_i];
    }
    for(u32 _i = 0; _i < _images; _i++) {
        _masks += _pic[_i].mask_size > 0u ? 1u : 0u;
    }
    // Numbering: 1 catalog, 2 pages, 3 resources, then what they refer to.
    u32       _next  = 4u;
    const u32 _font  = _pdf->text ? _next++ : 0u;
    const u32 _state = _next;
    _next += _states;
    const u32 _image = _next;
    _next += _images + _masks;
    const u32 _first = _next;   // each page, then its content
    _next += 2u * _pages;
    const u32 _count = _next;
    // Everything but the streams is small: a generous few hundred bytes an object.
    const u64 _room = (u64)fude_bytes_size(&_pdf->content) + (u64)fude_bytes_size(&_pdf->data) + (u64)_count * 400u + 1024u;
    if((u64)fude_bytes_size(_out) + _room > 0xFFFFFFFFu) {
        return false;
    }
    u32* _offsets = (u32*)calloc((usize)_count + (usize)_images + 1u, sizeof(u32));
    if(_offsets == NULL) {
        return false;
    }
    u32* _image_obj = _offsets + _count;   // each image's object (its mask the next)
    for(u32 _i = 0, _n = _image; _i < _images; _i++) {
        _image_obj[_i] = _n;
        _n += _pic[_i].mask_size > 0u ? 2u : 1u;
    }
    const u32 _base = fude_bytes_size(_out);
    // The second line's high bytes tell transfer tools the file is binary.
    fude_put_data(_out, "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n", 15u);

    fude_zoom_pdf_object(_out, _base, _offsets, 1u);
    fude_zoom_pdf_put(_out, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    fude_zoom_pdf_object(_out, _base, _offsets, 2u);
    fude_zoom_pdf_put(_out, "<< /Type /Pages /Kids [");
    for(u32 _i = 0; _i < _pages; _i++) {
        fude_zoom_pdf_put(_out, _i == 0u ? "%u 0 R" : " %u 0 R", (unsigned)(_first + 2u * _i));
    }
    fude_zoom_pdf_put(_out, "] /Count %u >>\nendobj\n", (unsigned)_pages);

    fude_zoom_pdf_object(_out, _base, _offsets, 3u);
    fude_zoom_pdf_put(_out, "<< /ProcSet [/PDF /Text /ImageB /ImageC /ImageI]");
    if(_states > 0u) {
        fude_zoom_pdf_put(_out, "\n/ExtGState <<");
        u32 _n = _state;
        for(u32 _k = 0; _k < 2u; _k++) {
            const b8* _used = _k == 0u ? _pdf->fill_alpha : _pdf->stroke_alpha;
            for(u32 _i = 0; _i < 256u; _i++) {
                if(_used[_i]) {
                    fude_zoom_pdf_put(_out, " /%s%u %u 0 R", _k == 0u ? "af" : "as", (unsigned)_i, (unsigned)_n++);
                }
            }
        }
        fude_zoom_pdf_put(_out, " >>");
    }
    if(_images > 0u) {
        fude_zoom_pdf_put(_out, "\n/XObject <<");
        for(u32 _i = 0; _i < _images; _i++) {
            fude_zoom_pdf_put(_out, " /Im%u %u 0 R", (unsigned)(_i + 1u), (unsigned)_image_obj[_i]);
        }
        fude_zoom_pdf_put(_out, " >>");
    }
    if(_font != 0u) {
        fude_zoom_pdf_put(_out, "\n/Font << /F1 %u 0 R >>", (unsigned)_font);
    }
    fude_zoom_pdf_put(_out, " >>\nendobj\n");

    if(_font != 0u) {
        fude_zoom_pdf_object(_out, _base, _offsets, _font);
        fude_zoom_pdf_put(_out, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n");
    }

    u32 _n = _state;
    for(u32 _k = 0; _k < 2u; _k++) {
        const b8* _used = _k == 0u ? _pdf->fill_alpha : _pdf->stroke_alpha;
        for(u32 _i = 0; _i < 256u; _i++) {
            if(_used[_i]) {
                fude_zoom_pdf_object(_out, _base, _offsets, _n++);
                fude_zoom_pdf_put(_out, "<< /Type /ExtGState /%s ", _k == 0u ? "ca" : "CA");
                fude_zoom_pdf_put_real(_out, _i / 255.0, FUDE_ZOOM_PDF_COLOR);
                fude_zoom_pdf_put(_out, " >>\nendobj\n");
            }
        }
    }

    static const c8* const _spaces[5] = { "", "/DeviceGray", "", "/DeviceRGB", "/DeviceCMYK" };
    for(u32 _i = 0; _i < _images; _i++) {
        const fude_zoom_pdf_picture* _p = &_pic[_i];
        fude_zoom_pdf_object(_out, _base, _offsets, _image_obj[_i]);
        fude_zoom_pdf_put(_out, "<< /Type /XObject /Subtype /Image /Width %u /Height %u /ColorSpace %s /BitsPerComponent 8",
                          (unsigned)_p->width, (unsigned)_p->height, _spaces[_p->components]);
        if(_p->jpeg) {
            fude_zoom_pdf_put(_out, " /Filter /DCTDecode");
        }
        if(_p->inverted) {
            fude_zoom_pdf_put(_out, " /Decode [1 0 1 0 1 0 1 0]");
        }
        if(_p->mask_size > 0u) {
            fude_zoom_pdf_put(_out, " /SMask %u 0 R", (unsigned)(_image_obj[_i] + 1u));
        }
        fude_zoom_pdf_put(_out, " ");
        fude_zoom_pdf_stream(_out, _pdf->data.memory + _p->at, _p->size);
        if(_p->mask_size > 0u) {
            fude_zoom_pdf_object(_out, _base, _offsets, _image_obj[_i] + 1u);
            fude_zoom_pdf_put(_out, "<< /Type /XObject /Subtype /Image /Width %u /Height %u /ColorSpace /DeviceGray /BitsPerComponent 8 ",
                              (unsigned)_p->width, (unsigned)_p->height);
            fude_zoom_pdf_stream(_out, _pdf->data.memory + _p->mask_at, _p->mask_size);
        }
    }

    for(u32 _i = 0; _i < _pages; _i++) {
        const u32 _obj = _first + 2u * _i;
        fude_zoom_pdf_object(_out, _base, _offsets, _obj);
        fude_zoom_pdf_put(_out, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ");
        fude_zoom_pdf_put_real(_out, _page[_i].width, FUDE_ZOOM_PDF_PLACES);
        fude_zoom_pdf_put(_out, " ");
        fude_zoom_pdf_put_real(_out, _page[_i].height, FUDE_ZOOM_PDF_PLACES);
        fude_zoom_pdf_put(_out, "] /Resources 3 0 R /Contents %u 0 R >>\nendobj\n", (unsigned)(_obj + 1u));
        fude_zoom_pdf_object(_out, _base, _offsets, _obj + 1u);
        fude_zoom_pdf_put(_out, "<< ");
        fude_zoom_pdf_stream(_out, _pdf->content.memory + _page[_i].at, _page[_i].size);
    }

    // Each entry exactly 20 bytes, its end of line two of them.
    const u32 _xref = fude_bytes_size(_out) - _base;
    fude_zoom_pdf_put(_out, "xref\n0 %u\n0000000000 65535 f\r\n", (unsigned)_count);
    for(u32 _i = 1u; _i < _count; _i++) {
        fude_zoom_pdf_put(_out, "%010u 00000 n\r\n", (unsigned)_offsets[_i]);
    }
    fude_zoom_pdf_put(_out, "trailer\n<< /Size %u /Root 1 0 R >>\nstartxref\n%u\n%%%%EOF\n", (unsigned)_count, (unsigned)_xref);
    free(_offsets);
    return true;
}
