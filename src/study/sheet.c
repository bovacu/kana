#include "study/sheet.h"
#include "base/kfile.h"
#include "base/text.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See sheet.h. A PDF 1.4 written by hand: the catalog, the page tree, Helvetica
// (one of the fonts every reader has, never embedded), the document's info, then
// a page object and its drawing per page; the cross-reference table last. Units
// are points (1/72 inch), Y up from the page's bottom.
// ===========================================================================

#define KANA_SHEET_PAGE_W   595.28f   // A4
#define KANA_SHEET_PAGE_H   841.89f
#define KANA_SHEET_MARGIN   36.0f
#define KANA_SHEET_HEADER   30.0f     // the title line
#define KANA_SHEET_FOOTER   18.0f     // the credit and the page's number
#define KANA_SHEET_BOXES    10u       // boxes in a row
#define KANA_SHEET_INFO     18.0f     // a character's line: meaning, readings, strokes
#define KANA_SHEET_ORDER    22.0f     // its stroke order's boxes, at most
#define KANA_SHEET_ORDER_GAP 2.0f
#define KANA_SHEET_GAP      14.0f     // between characters
#define KANA_SHEET_PER_PAGE 6u        // characters a page, at most
#define KANA_SHEET_TRACES   3u        // faded ones to write over, the first row
#define KANA_SHEET_INK      0.08f     // the model's grey (0 black)
#define KANA_SHEET_RED      0.80f, 0.22f, 0.20f   // stroke numbers and starts

// The objects with fixed numbers; the pages' follow, two each (page, drawing).
#define KANA_SHEET_OBJ_CATALOG 1u
#define KANA_SHEET_OBJ_PAGES   2u
#define KANA_SHEET_OBJ_FONT    3u
#define KANA_SHEET_OBJ_INFO    4u
#define KANA_SHEET_OBJ_FIRST   5u

// Helvetica's advances (thousandths of the size) for WinAnsi 32..255.
static const u16 KANA_SHEET_HELVETICA[224] = {
     278,  278,  355,  556,  556,  889,  667,  191,  333,  333,  389,  584,  278,  333,  278,  278,
     556,  556,  556,  556,  556,  556,  556,  556,  556,  556,  278,  278,  584,  584,  584,  556,
    1015,  667,  667,  722,  722,  667,  611,  778,  722,  278,  500,  667,  556,  833,  722,  778,
     667,  778,  722,  667,  611,  722,  667,  944,  667,  667,  611,  278,  278,  278,  469,  556,
     333,  556,  556,  500,  556,  556,  278,  556,  556,  222,  222,  500,  222,  833,  556,  556,
     556,  556,  333,  500,  278,  556,  500,  722,  500,  500,  500,  334,  260,  334,  584,    0,
     556,    0,  222,  556,  333, 1000,  556,  556,  333, 1000,  667,  333, 1000,    0,  611,    0,
       0,  222,  222,  333,  333,  350,  556, 1000,  333, 1000,  500,  333,  944,    0,  500,  667,
     278,  333,  556,  556,  556,  556,  260,  556,  333,  737,  370,  556,  584,  333,  737,  552,
     400,  549,  333,  333,  333,  576,  537,  333,  333,  333,  365,  556,  834,  834,  834,  611,
     667,  667,  667,  667,  667,  667, 1000,  722,  667,  667,  667,  667,  278,  278,  278,  278,
     722,  722,  778,  778,  778,  778,  778,  584,  778,  722,  722,  722,  722,  667,  667,  611,
     556,  556,  556,  556,  556,  556,  889,  500,  556,  556,  556,  556,  278,  278,  278,  278,
     556,  556,  556,  556,  556,  556,  556,  549,  611,  556,  556,  556,  556,  500,  556,  500,
};

typedef struct {
    const kana_kanji_db* db;
    kana_bytes           out;       // the file
    kana_bytes           page;      // the page being drawn
    rde_arr TYPE(u32)    offsets;   // each object's place in the file, by number - 1
} kana_sheet_pdf;

// --- writing ----------------------------------------------------------------------------

RDE_INTERNAL void kana_sheet_vput(kana_bytes* _b, const c8* _format, va_list _args) {
    c8        _line[2048];
    const int _n = vsnprintf(_line, sizeof(_line), _format, _args);
    if(_n > 0) {
        kana_put_data(_b, _line, (u32)(_n < (int)sizeof(_line) ? _n : (int)sizeof(_line) - 1));
    }
}

// Into the page's drawing.
RDE_INTERNAL void kana_sheet_draw(kana_sheet_pdf* _pdf, const c8* _format, ...) {
    va_list _args;
    va_start(_args, _format);
    kana_sheet_vput(&_pdf->page, _format, _args);
    va_end(_args);
}

// Into the file.
RDE_INTERNAL void kana_sheet_put(kana_sheet_pdf* _pdf, const c8* _format, ...) {
    va_list _args;
    va_start(_args, _format);
    kana_sheet_vput(&_pdf->out, _format, _args);
    va_end(_args);
}

// Object _n begins here (numbers come in any order: the table puts them right).
RDE_INTERNAL void kana_sheet_object(kana_sheet_pdf* _pdf, u32 _n) {
    while((u32)rde_arr_length(&_pdf->offsets) < _n) {
        const u32 _none = 0u;
        rde_arr_add(&_pdf->offsets, (any)&_none);
    }
    ((u32*)_pdf->offsets.memory)[_n - 1u] = kana_bytes_size(&_pdf->out);
    kana_sheet_put(_pdf, "%u 0 obj\n", _n);
}

// --- text ---------------------------------------------------------------------------------

// A code point's WinAnsi byte (what Helvetica draws here); 0 when it has none.
RDE_INTERNAL u8 kana_sheet_winansi(u32 _cp) {
    if((_cp >= 0x20u && _cp <= 0x7Eu) || (_cp >= 0xA0u && _cp <= 0xFFu)) {
        return (u8)_cp;
    }
    static const u32 _high[32] = { 0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
                                   0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178 };
    for(u32 _i = 0; _i < 32u; _i++) {
        if(_high[_i] != 0u && _high[_i] == _cp) {
            return (u8)(0x80u + _i);
        }
    }
    return 0u;
}

// A Japanese character in text: drawn from its strokes in a box this much of the
// size (KanjiVG's box has room around a character), this much of it apart.
#define KANA_SHEET_GLYPH_BOX     1.12f
#define KANA_SHEET_GLYPH_ADVANCE 1.0f
#define KANA_SHEET_NUMBERED      6u      // the model's strokes numbered up to this many (more crowd it: the order's boxes show them)

// How a code point goes in text at _px: its width; *_drawn true when it can be
// drawn at all (Helvetica, or the character data's strokes).
RDE_INTERNAL f32 kana_sheet_advance(const kana_sheet_pdf* _pdf, u32 _cp, f32 _px, b8* _drawn) {
    const u8 _b = kana_sheet_winansi(_cp);
    if(_b != 0u) {
        *_drawn = true;
        return (f32)KANA_SHEET_HELVETICA[_b - 32u] * _px / 1000.0f;
    }
    if(_cp == 0x3000u) {
        *_drawn = false;   // an ideographic space: room, nothing drawn
        return _px;
    }
    kana_kanji_info _info;
    *_drawn = kana_kanji_find(_pdf->db, _cp, &_info);
    return *_drawn ? _px * KANA_SHEET_GLYPH_ADVANCE : 0.0f;
}

RDE_INTERNAL f32 kana_sheet_text_width(const kana_sheet_pdf* _pdf, const c8* _text, f32 _px) {
    f32       _w = 0.0f;
    const c8* _p = _text;
    for(u32 _cp = kana_kanji_utf8_next(&_p); _cp != 0u; _cp = kana_kanji_utf8_next(&_p)) {
        b8 _drawn;
        _w += kana_sheet_advance(_pdf, _cp, _px, &_drawn);
    }
    return _w;
}

// The stroke path of character _info, its box's top-left at (_x, _y) and _size
// square: strokes [_from, _to), stroked in the current colour and width.
RDE_INTERNAL void kana_sheet_strokes(kana_sheet_pdf* _pdf, const kana_kanji_info* _info, f32 _x, f32 _y, f32 _size, u32 _from, u32 _to) {
    const f32 _k = _size / KANA_KANJI_BOX;
    for(u32 _s = _from; _s < _to; _s++) {
        kana_kanji_stroke _stroke;
        if(!kana_kanji_stroke_at(_pdf->db, _info, _s, &_stroke)) {
            continue;
        }
        kana_sheet_draw(_pdf, "%.2f %.2f m\n", _x + _stroke.start.x * _k, _y - _stroke.start.y * _k);
        for(u32 _g = 0; _g < _stroke.segments; _g++) {
            rde_vec_2F _c[3];
            kana_kanji_stroke_segment(&_stroke, _g, _c);
            kana_sheet_draw(_pdf, "%.2f %.2f %.2f %.2f %.2f %.2f c\n", _x + _c[0].x * _k, _y - _c[0].y * _k, _x + _c[1].x * _k, _y - _c[1].y * _k,
                            _x + _c[2].x * _k, _y - _c[2].y * _k);
        }
        kana_sheet_draw(_pdf, "S\n");
    }
}

// _text at (_x, _y: its baseline) at _px in grey _grey: Helvetica runs, and
// Japanese drawn from strokes. How wide it went.
RDE_INTERNAL f32 kana_sheet_text(kana_sheet_pdf* _pdf, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _grey) {
    c8        _run[400];
    u32       _run_n  = 0;
    f32       _run_x  = _x;
    f32       _at     = _x;
    const c8* _p      = _text;
    kana_sheet_draw(_pdf, "%.3f g %.3f G\n", _grey, _grey);
    for(u32 _cp = kana_kanji_utf8_next(&_p);; _cp = kana_kanji_utf8_next(&_p)) {
        const u8 _b = _cp != 0u ? kana_sheet_winansi(_cp) : 0u;
        if((_b == 0u || _run_n + 4u >= sizeof(_run)) && _run_n > 0u) {
            _run[_run_n] = 0;
            kana_sheet_draw(_pdf, "BT /F1 %.2f Tf %.2f %.2f Td (%s) Tj ET\n", _px, _run_x, _y, _run);
            _run_n = 0;
        }
        if(_cp == 0u) {
            break;
        }
        b8        _drawn;
        const f32 _w = kana_sheet_advance(_pdf, _cp, _px, &_drawn);
        if(_b != 0u) {
            if(_run_n == 0u) {
                _run_x = _at;
            }
            if(_b == '(' || _b == ')' || _b == '\\') {
                _run[_run_n++] = '\\';
                _run[_run_n++] = (c8)_b;
            } else if(_b >= 0x80u) {
                _run_n += (u32)snprintf(&_run[_run_n], sizeof(_run) - _run_n, "\\%03o", (unsigned)_b);
            } else {
                _run[_run_n++] = (c8)_b;
            }
        } else if(_drawn) {
            kana_kanji_info _info;
            kana_kanji_find(_pdf->db, _cp, &_info);
            const f32 _box = _px * KANA_SHEET_GLYPH_BOX;
            kana_sheet_draw(_pdf, "%.2f w 1 J 1 j\n", fmaxf(0.35f, _px * 0.075f));
            kana_sheet_strokes(_pdf, &_info, _at + (_w - _box) * 0.5f, _y + _box * 0.80f, _box, 0u, _info.strokes);
        }
        _at += _w;
    }
    return _at - _x;
}

// --- drawing ------------------------------------------------------------------------------

// A disc at (_x, _y), _r: four curves.
RDE_INTERNAL void kana_sheet_disc(kana_sheet_pdf* _pdf, f32 _x, f32 _y, f32 _r) {
    const f32 _c = _r * 0.5523f;
    kana_sheet_draw(_pdf, "%.2f %.2f m %.2f %.2f %.2f %.2f %.2f %.2f c %.2f %.2f %.2f %.2f %.2f %.2f c ", _x + _r, _y, _x + _r, _y + _c, _x + _c, _y + _r, _x, _y + _r,
                    _x - _c, _y + _r, _x - _r, _y + _c, _x - _r, _y);
    kana_sheet_draw(_pdf, "%.2f %.2f %.2f %.2f %.2f %.2f c %.2f %.2f %.2f %.2f %.2f %.2f c f\n", _x - _r, _y - _c, _x - _c, _y - _r, _x, _y - _r, _x + _c, _y - _r,
                    _x + _r, _y - _c, _x + _r, _y);
}

// A character in a box (top-left _x, _y; _size square): its strokes in _grey,
// numbered (in red, by where each starts) when _numbers and not too many.
RDE_INTERNAL void kana_sheet_character(kana_sheet_pdf* _pdf, const kana_kanji_info* _info, f32 _x, f32 _y, f32 _size, f32 _grey, b8 _numbers) {
    const f32 _pad  = _size * 0.08f;
    const f32 _draw = _size - 2.0f * _pad;
    kana_sheet_draw(_pdf, "%.3f G %.2f w 1 J 1 j\n", _grey, _draw * 0.045f);
    kana_sheet_strokes(_pdf, _info, _x + _pad, _y - _pad, _draw, 0u, _info->strokes);
    if(!_numbers || _info->strokes > KANA_SHEET_NUMBERED) {
        return;
    }
    const f32 _k  = _draw / KANA_KANJI_BOX;
    const f32 _px = fmaxf(4.5f, _size * 0.11f);
    kana_sheet_draw(_pdf, "%.3f %.3f %.3f rg\n", KANA_SHEET_RED);
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        kana_kanji_stroke _stroke;
        if(!kana_kanji_stroke_at(_pdf->db, _info, _s, &_stroke)) {
            continue;
        }
        c8  _num[8];
        snprintf(_num, sizeof(_num), "%u", _s + 1u);
        const f32 _w  = (f32)strlen(_num) * 0.556f * _px;
        const f32 _nx = fminf(fmaxf(_x + _pad + _stroke.start.x * _k - _w - _px * 0.15f, _x + 1.0f), _x + _size - _w - 1.0f);
        const f32 _ny = fminf(fmaxf(_y - _pad - _stroke.start.y * _k + _px * 0.15f, _y - _size + 1.5f), _y - _px);
        kana_sheet_draw(_pdf, "BT /F1 %.2f Tf %.2f %.2f Td (%s) Tj ET\n", _px, _nx, _ny, _num);
    }
}

// A row of _n boxes from (_x, _y: top-left), each _size: the frame, the lines
// between, and the dashed cross in each.
RDE_INTERNAL void kana_sheet_boxes(kana_sheet_pdf* _pdf, f32 _x, f32 _y, f32 _size, u32 _n, f32 _gap) {
    kana_sheet_draw(_pdf, "0.82 G 0.4 w [1.5 1.5] 0 d\n");
    for(u32 _i = 0; _i < _n; _i++) {
        const f32 _bx = _x + (f32)_i * (_size + _gap);
        kana_sheet_draw(_pdf, "%.2f %.2f m %.2f %.2f l %.2f %.2f m %.2f %.2f l S\n", _bx + _size * 0.5f, _y, _bx + _size * 0.5f, _y - _size, _bx, _y - _size * 0.5f,
                        _bx + _size, _y - _size * 0.5f);
    }
    kana_sheet_draw(_pdf, "[] 0 d 0.55 G 0.6 w\n");
    for(u32 _i = 0; _i < _n; _i++) {
        if(_gap > 0.0f || _i == 0u) {
            kana_sheet_draw(_pdf, "%.2f %.2f %.2f %.2f re S\n", _x + (f32)_i * (_size + _gap), _y - _size, _gap > 0.0f ? _size : _size * (f32)_n, _size);
        } else {
            kana_sheet_draw(_pdf, "%.2f %.2f m %.2f %.2f l S\n", _x + (f32)_i * _size, _y, _x + (f32)_i * _size, _y - _size);
        }
    }
}

RDE_INTERNAL b8 kana_sheet_is_kana(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu);
}

// Shortens a list ("a, b, c") from its end, at its commas, until it fits _width
// at _px (the first item always stays).
RDE_INTERNAL void kana_sheet_shorten(const kana_sheet_pdf* _pdf, c8* _list, const c8* _comma, f32 _px, f32 _width) {
    while(kana_sheet_text_width(_pdf, _list, _px) > _width) {
        c8* _last = NULL;
        for(c8* _c = strstr(_list, _comma); _c != NULL; _c = strstr(_c + 1, _comma)) {
            _last = _c;
        }
        if(_last == NULL) {
            return;
        }
        *_last = 0;
    }
}

// One character's block from _top: its line, its stroke order, then _rows rows
// of boxes (each _box).
RDE_INTERNAL void kana_sheet_block(kana_sheet_pdf* _pdf, const kana_kanji_info* _info, f32 _top, u32 _rows, f32 _box) {
    const f32 _left  = KANA_SHEET_MARGIN;
    const f32 _right = KANA_SHEET_PAGE_W - KANA_SHEET_MARGIN;
    const f32 _width = _right - _left;

    // The line: meanings, then readings; JLPT and strokes at the right.
    c8 _label[96];
    c8 _strokes[64];
    KANA_TEXTF(_strokes, KANA_TEXT_STROKES_N, KANA_TN(_info->strokes));
    if(_info->jlpt_n != 0u) {
        snprintf(_label, sizeof(_label), "N%u \xC2\xB7 %s", _info->jlpt_n, _strokes);   // ·
    } else {
        snprintf(_label, sizeof(_label), "%s", _strokes);
    }
    const f32 _base  = _top - 12.0f;
    const f32 _lw    = kana_sheet_text_width(_pdf, _label, 8.0f);
    kana_sheet_text(_pdf, _label, _right - _lw, _base, 8.0f, 0.45f);

    c8 _meanings[512];
    c8 _readings[512];
    if(kana_sheet_is_kana(_info->codepoint)) {
        snprintf(_meanings, sizeof(_meanings), "%s", kana_text(_info->codepoint < 0x30A0u ? KANA_TEXT_HIRAGANA : KANA_TEXT_KATAKANA));
        _readings[0] = 0;
    } else {
        snprintf(_meanings, sizeof(_meanings), "%s", kana_kanji_meanings(_pdf->db, _info));
        const c8* _on  = kana_kanji_on(_pdf->db, _info);
        const c8* _kun = kana_kanji_kun(_pdf->db, _info);
        snprintf(_readings, sizeof(_readings), "%s%s%s", _on, _on[0] != 0 && _kun[0] != 0 ? "\xE3\x80\x80" : "", _kun);   // an ideographic space between
    }
    const f32 _room = _width - _lw - 16.0f;
    f32       _px   = 10.0f;
    // The meanings may take most of it; the readings what is left, each cut at a
    // whole one; then both a little smaller if still too long.
    kana_sheet_shorten(_pdf, _meanings, ", ", _px, _room * (_readings[0] != 0 ? 0.55f : 1.0f));
    const f32 _mw = kana_sheet_text_width(_pdf, _meanings, _px);
    kana_sheet_shorten(_pdf, _readings, "\xE3\x80\x81", _px * 0.9f, _room - _mw - 14.0f);   // 、
    const f32 _total = _mw + (_readings[0] != 0 ? 14.0f + kana_sheet_text_width(_pdf, _readings, _px * 0.9f) : 0.0f);
    if(_total > _room) {
        _px *= fmaxf(0.7f, _room / _total);
    }
    const f32 _mx = kana_sheet_text(_pdf, _meanings, _left, _base, _px, 0.12f);
    if(_readings[0] != 0) {
        kana_sheet_text(_pdf, _readings, _left + _mx + 14.0f * _px / 10.0f, _base, _px * 0.9f, 0.38f);
    }

    // The stroke order: each step a box, the new stroke dark, those before grey,
    // a red dot where it starts.
    const f32 _ot   = _top - KANA_SHEET_INFO;
    const f32 _gap  = KANA_SHEET_ORDER_GAP;
    const u32 _n    = _info->strokes > 0u ? _info->strokes : 1u;
    const f32 _os   = fminf(KANA_SHEET_ORDER, (_width + _gap) / (f32)_n - _gap);
    kana_sheet_draw(_pdf, "0.80 G 0.4 w\n");
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        kana_sheet_draw(_pdf, "%.2f %.2f %.2f %.2f re S\n", _left + (f32)_s * (_os + _gap), _ot - _os, _os, _os);
    }
    const f32 _pad  = _os * 0.1f;
    const f32 _draw = _os - 2.0f * _pad;
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        const f32 _x = _left + (f32)_s * (_os + _gap) + _pad;
        const f32 _y = _ot - _pad;
        kana_sheet_draw(_pdf, "%.2f w 1 J 1 j 0.70 G\n", _draw * 0.05f);
        kana_sheet_strokes(_pdf, _info, _x, _y, _draw, 0u, _s);
        kana_sheet_draw(_pdf, "%.3f G\n", KANA_SHEET_INK);
        kana_sheet_strokes(_pdf, _info, _x, _y, _draw, _s, _s + 1u);
        kana_kanji_stroke _stroke;
        if(kana_kanji_stroke_at(_pdf->db, _info, _s, &_stroke)) {
            const f32 _k = _draw / KANA_KANJI_BOX;
            kana_sheet_draw(_pdf, "%.3f %.3f %.3f rg\n", KANA_SHEET_RED);
            kana_sheet_disc(_pdf, _x + _stroke.start.x * _k, _y - _stroke.start.y * _k, fmaxf(0.8f, _os * 0.055f));
        }
    }

    // The rows: the first with the model and faded ones, the rest one faded each.
    const f32 _rt = _ot - KANA_SHEET_ORDER - 6.0f;
    for(u32 _r = 0; _r < _rows; _r++) {
        const f32 _y = _rt - (f32)_r * _box;
        kana_sheet_boxes(_pdf, _left, _y, _box, KANA_SHEET_BOXES, 0.0f);
        if(_r == 0u) {
            kana_sheet_character(_pdf, _info, _left, _y, _box, KANA_SHEET_INK, true);
            static const f32 _greys[KANA_SHEET_TRACES] = { 0.72f, 0.80f, 0.87f };
            for(u32 _t = 0; _t < KANA_SHEET_TRACES; _t++) {
                kana_sheet_character(_pdf, _info, _left + (f32)(_t + 1u) * _box, _y, _box, _greys[_t], false);
            }
        } else {
            kana_sheet_character(_pdf, _info, _left, _y, _box, 0.80f, false);
        }
    }
}

// A page done: its object, its drawing (both numbered by _page), and the header
// and footer drawn into it first.
RDE_INTERNAL void kana_sheet_end_page(kana_sheet_pdf* _pdf, u32 _page, u32 _pages, const c8* _title, const c8* _date) {
    const f32 _left  = KANA_SHEET_MARGIN;
    const f32 _right = KANA_SHEET_PAGE_W - KANA_SHEET_MARGIN;
    const f32 _top   = KANA_SHEET_PAGE_H - KANA_SHEET_MARGIN;
    kana_sheet_text(_pdf, _title, _left, _top - 16.0f, 15.0f, 0.10f);
    kana_sheet_text(_pdf, _date, _right - kana_sheet_text_width(_pdf, _date, 9.0f), _top - 15.0f, 9.0f, 0.45f);
    kana_sheet_draw(_pdf, "0.75 G 0.5 w %.2f %.2f m %.2f %.2f l S\n", _left, _top - KANA_SHEET_HEADER + 6.0f, _right, _top - KANA_SHEET_HEADER + 6.0f);
    c8 _number[32];
    snprintf(_number, sizeof(_number), "%u / %u", _page + 1u, _pages);
    const f32 _foot = KANA_SHEET_MARGIN - 4.0f;
    const f32 _nw   = kana_sheet_text_width(_pdf, _number, 7.5f);
    c8        _credit[256];
    snprintf(_credit, sizeof(_credit), "%s", kana_text(KANA_TEXT_SHEET_CREDIT));
    const f32 _cw   = kana_sheet_text_width(_pdf, _credit, 7.0f);
    const f32 _room = _right - _left - _nw - 12.0f;
    kana_sheet_text(_pdf, _credit, _left, _foot, _cw > _room ? 7.0f * _room / _cw : 7.0f, 0.50f);
    kana_sheet_text(_pdf, _number, _right - _nw, _foot, 7.5f, 0.50f);

    const u32 _object = KANA_SHEET_OBJ_FIRST + 2u * _page;
    kana_sheet_object(_pdf, _object);
    kana_sheet_put(_pdf, "<< /Type /Page /Parent %u 0 R /MediaBox [0 0 %.2f %.2f] /Resources << /Font << /F1 %u 0 R >> >> /Contents %u 0 R >>\nendobj\n",
                   KANA_SHEET_OBJ_PAGES, KANA_SHEET_PAGE_W, KANA_SHEET_PAGE_H, KANA_SHEET_OBJ_FONT, _object + 1u);
    kana_sheet_object(_pdf, _object + 1u);
    kana_sheet_put(_pdf, "<< /Length %u >>\nstream\n", kana_bytes_size(&_pdf->page));
    kana_put_data(&_pdf->out, _pdf->page.memory, kana_bytes_size(&_pdf->page));
    kana_sheet_put(_pdf, "\nendstream\nendobj\n");
    rde_arr_clear(&_pdf->page);
}

// _text as a PDF text string in UTF-16 (the info's title): <FEFF...>.
RDE_INTERNAL void kana_sheet_put_utf16(kana_sheet_pdf* _pdf, const c8* _text) {
    kana_sheet_put(_pdf, "<FEFF");
    const c8* _p = _text;
    for(u32 _cp = kana_kanji_utf8_next(&_p); _cp != 0u; _cp = kana_kanji_utf8_next(&_p)) {
        if(_cp >= 0x10000u) {
            _cp -= 0x10000u;
            kana_sheet_put(_pdf, "%04X%04X", 0xD800u + (_cp >> 10), 0xDC00u + (_cp & 0x3FFu));
        } else {
            kana_sheet_put(_pdf, "%04X", _cp);
        }
    }
    kana_sheet_put(_pdf, ">");
}

b8 kana_sheet_write(const kana_kanji_db* _db, const u32* _records, u32 _count, const c8* _path, kana_sheet_info* _info) {
    if(_info != NULL) {
        memset(_info, 0, sizeof(*_info));
    }
    // The characters that can be drawn, the first KANA_SHEET_MAX.
    static kana_kanji_info _chars[KANA_SHEET_MAX];
    u32 _n = 0;
    for(u32 _i = 0; _i < _count && _n < KANA_SHEET_MAX; _i++) {
        if(kana_kanji_at(_db, _records[_i], &_chars[_n]) && _chars[_n].strokes > 0u) {
            _n++;
        }
    }
    if(_db == NULL || _n == 0u || _path == NULL) {
        return false;
    }

    // How many rows each, so that a few characters fill their page.
    const f32 _box     = (KANA_SHEET_PAGE_W - 2.0f * KANA_SHEET_MARGIN) / (f32)KANA_SHEET_BOXES;
    const f32 _content = KANA_SHEET_PAGE_H - 2.0f * KANA_SHEET_MARGIN - KANA_SHEET_HEADER - KANA_SHEET_FOOTER + KANA_SHEET_GAP;
    const f32 _fixed   = KANA_SHEET_INFO + KANA_SHEET_ORDER + 6.0f + KANA_SHEET_GAP;
    const u32 _most    = (u32)fminf((f32)KANA_SHEET_PER_PAGE, floorf(_content / (_fixed + _box)));
    const u32 _share   = _n < _most ? _n : _most;
    const u32 _rows    = (u32)fmaxf(1.0f, floorf((_content / (f32)_share - _fixed) / _box));
    const f32 _block   = _fixed + (f32)_rows * _box;
    const u32 _per     = (u32)fmaxf(1.0f, fminf((f32)KANA_SHEET_PER_PAGE, floorf(_content / _block)));
    const u32 _pages   = (_n + _per - 1u) / _per;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    kana_sheet_pdf        _pdf  = { .db = _db, .out = kana_bytes_new(64u * 1024u), .page = kana_bytes_new(64u * 1024u),
                                    .offsets = rde_arr_new(sizeof(u32), _heap) };
    c8 _title[128];
    c8 _date[64];
    snprintf(_title, sizeof(_title), "%s", kana_text(KANA_TEXT_SHEET_TITLE));
    kana_text_date(_date, sizeof(_date), (u64)time(NULL));

    kana_sheet_put(&_pdf, "%%PDF-1.4\n%%\xE2\xE3\xCF\xD3\n");
    kana_sheet_object(&_pdf, KANA_SHEET_OBJ_CATALOG);
    kana_sheet_put(&_pdf, "<< /Type /Catalog /Pages %u 0 R >>\nendobj\n", KANA_SHEET_OBJ_PAGES);
    kana_sheet_object(&_pdf, KANA_SHEET_OBJ_FONT);
    kana_sheet_put(&_pdf, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n");
    kana_sheet_object(&_pdf, KANA_SHEET_OBJ_INFO);
    kana_sheet_put(&_pdf, "<< /Title ");
    kana_sheet_put_utf16(&_pdf, _title);
    kana_sheet_put(&_pdf, " /Creator (Kana) /Producer (Kana) >>\nendobj\n");

    for(u32 _page = 0; _page < _pages; _page++) {
        f32 _top = KANA_SHEET_PAGE_H - KANA_SHEET_MARGIN - KANA_SHEET_HEADER;
        for(u32 _i = _page * _per; _i < _n && _i < (_page + 1u) * _per; _i++) {
            kana_sheet_block(&_pdf, &_chars[_i], _top, _rows, _box);
            _top -= _block;
        }
        kana_sheet_end_page(&_pdf, _page, _pages, _title, _date);
    }

    kana_sheet_object(&_pdf, KANA_SHEET_OBJ_PAGES);
    kana_sheet_put(&_pdf, "<< /Type /Pages /Count %u /Kids [", _pages);
    for(u32 _page = 0; _page < _pages; _page++) {
        kana_sheet_put(&_pdf, "%s%u 0 R", _page > 0u ? " " : "", KANA_SHEET_OBJ_FIRST + 2u * _page);
    }
    kana_sheet_put(&_pdf, "] >>\nendobj\n");

    // The table: each object's place, ten digits, twenty bytes a line.
    const u32 _objects = (u32)rde_arr_length(&_pdf.offsets);
    const u32 _xref    = kana_bytes_size(&_pdf.out);
    kana_sheet_put(&_pdf, "xref\n0 %u\n0000000000 65535 f \n", _objects + 1u);
    for(u32 _o = 0; _o < _objects; _o++) {
        kana_sheet_put(&_pdf, "%010u 00000 n \n", ((const u32*)_pdf.offsets.memory)[_o]);
    }
    kana_sheet_put(&_pdf, "trailer\n<< /Size %u /Root %u 0 R /Info %u 0 R >>\nstartxref\n%u\n%%%%EOF\n", _objects + 1u, KANA_SHEET_OBJ_CATALOG, KANA_SHEET_OBJ_INFO,
                   _xref);

    const u32 _size = kana_bytes_size(&_pdf.out);
    FILE*     _f    = fopen(_path, "wb");
    b8        _ok   = _f != NULL && fwrite(_pdf.out.memory, 1u, _size, _f) == _size;
    if(_f != NULL && fclose(_f) != 0) {
        _ok = false;
    }
    if(!_ok) {
        remove(_path);
        rde_log_level(RDE_LOG_LEVEL_ERROR, "kana: the practice sheet could not be written to %s", _path);
    }
    rde_arr_free(&_pdf.out);
    rde_arr_free(&_pdf.page);
    rde_arr_free(&_pdf.offsets);
    if(_ok && _info != NULL) {
        *_info = (kana_sheet_info){ .characters = _n, .pages = _pages, .bytes = _size };
    }
    return _ok;
}
