// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/sheet.h"
#include "lang/lang.h"
#include "drawing/base/kfile.h"
#include "drawing/base/text.h"   // (va_list comes with rde.h, which includes stdarg.h)

// ===========================================================================
// See sheet.h.
//
// The PDF 1.4 is written by hand. Objects 1 to 4 are the catalog, the page tree, Helvetica (one of the standard fonts
// every PDF reader has, so it is never embedded) and the document's info; from 5 on, each page has two objects, the
// page and its drawing (a content stream). The file holds the catalog, the font and the info first, then the pages,
// then the page tree, and last the cross-reference table, which gives every object's byte offset.
// Each page's drawing is built in its own buffer and copied into the file when the page is done, so its length is
// known for the stream's /Length.
// Units are PDF points (1/72 inch), with Y up from the page's bottom; the character data's strokes have Y down, so
// they are flipped when drawn.
// ===========================================================================

// The page and its layout, in points.
#define FUDE_SHEET_PAGE_W   595.28f   // A4.
#define FUDE_SHEET_PAGE_H   841.89f
#define FUDE_SHEET_MARGIN   36.0f
#define FUDE_SHEET_HEADER   30.0f     // The title line.
#define FUDE_SHEET_FOOTER   18.0f     // The credit and the page's number.
#define FUDE_SHEET_BOXES    10u       // Boxes in a row.
#define FUDE_SHEET_INFO     18.0f     // A character's line: its meanings, readings and strokes.
#define FUDE_SHEET_ORDER    22.0f     // The stroke order's boxes, at most; smaller when the strokes do not fit a row.
#define FUDE_SHEET_ORDER_GAP 2.0f
#define FUDE_SHEET_GAP      14.0f     // Between characters.
#define FUDE_SHEET_PER_PAGE 6u        // Characters on a page, at most.
#define FUDE_SHEET_TRACES   3u        // Faded copies to write over, in the first row.
#define FUDE_SHEET_INK      0.08f     // The model's grey (0 is black).
#define FUDE_SHEET_RED      0.80f, 0.22f, 0.20f   // The red of the stroke numbers and of the dots where strokes start.

// The objects with fixed numbers; the pages' objects follow, two for each page (the page, then its drawing).
#define FUDE_SHEET_OBJ_CATALOG 1u
#define FUDE_SHEET_OBJ_PAGES   2u
#define FUDE_SHEET_OBJ_FONT    3u
#define FUDE_SHEET_OBJ_INFO    4u
#define FUDE_SHEET_OBJ_FIRST   5u

// Helvetica's advance widths, in thousandths of the font size, for the WinAnsi bytes 32 to 255 (0 where WinAnsi has no
// character). They are needed to measure text, since the font is not in the file.
static const u16 FUDE_SHEET_HELVETICA[224] = {
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

// A PDF being written.
typedef struct {
    const fude_kanji_db* db;
    fude_bytes           out;       // The file so far.
    fude_bytes           page;      // The drawing of the page being drawn.
    rde_arr TYPE(u32)    offsets;   // Each object's byte offset in the file, at its number minus 1.
} fude_sheet_pdf;

// --- writing ----------------------------------------------------------------------------

// Formats _format with _args and appends it to _b. A line longer than 2047 bytes would be cut, which no line here is:
// they are the PDF's numbers and operators, and the longest, a run of text (fude_sheet_text), holds 400 bytes at most.
RDE_INTERNAL void fude_sheet_vput(fude_bytes* _b, const c8* _format, va_list _args) {
    c8        _line[2048];
    const int _n = rde_vsprintf(_line, sizeof(_line), _format, _args);
    if(_n > 0) {
        fude_put_data(_b, _line, (u32)(_n < (int)sizeof(_line) ? _n : (int)sizeof(_line) - 1));
    }
}

// Appends formatted text to the drawing of the page being drawn.
RDE_INTERNAL void fude_sheet_draw(fude_sheet_pdf* _pdf, const c8* _format, ...) {
    va_list _args;
    va_start(_args, _format);
    fude_sheet_vput(&_pdf->page, _format, _args);
    va_end(_args);
}

// Appends formatted text to the file.
RDE_INTERNAL void fude_sheet_put(fude_sheet_pdf* _pdf, const c8* _format, ...) {
    va_list _args;
    va_start(_args, _format);
    fude_sheet_vput(&_pdf->out, _format, _args);
    va_end(_args);
}

// Starts object _n here: notes its byte offset for the cross-reference table and writes its "n 0 obj" line. Objects
// may be written in any order, since the table lists them by number.
RDE_INTERNAL void fude_sheet_object(fude_sheet_pdf* _pdf, u32 _n) {
    while((u32)rde_arr_length(&_pdf->offsets) < _n) {
        const u32 _none = 0u;
        rde_arr_add(&_pdf->offsets, (any)&_none);
    }
    ((u32*)_pdf->offsets.memory)[_n - 1u] = fude_bytes_size(&_pdf->out);
    fude_sheet_put(_pdf, "%u 0 obj\n", _n);
}

// --- text ---------------------------------------------------------------------------------

// Returns a code point's byte in the WinAnsi encoding, which is what Helvetica draws here, or 0 when WinAnsi has no
// such character. Bytes 0x80 to 0x9F hold the characters listed in _high (the euro, curly quotes, dashes...).
RDE_INTERNAL u8 fude_sheet_winansi(u32 _cp) {
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

// A character of the language taught, inside a line of text, is drawn from its strokes in a box this many times the
// text size (the character data's box has room around a character), and takes this many times the size in width.
#define FUDE_SHEET_GLYPH_BOX     1.12f
#define FUDE_SHEET_GLYPH_ADVANCE 1.0f
// The model's strokes are numbered only when it has at most this many: more numbers crowd it, and the stroke order's
// boxes show the order anyway.
#define FUDE_SHEET_NUMBERED      6u      // Strokes, at most, for the model to be numbered.

// Returns how wide code point _cp is in text of size _px, and sets *_drawn to whether it can be drawn at all, in
// Helvetica or from the character data's strokes. A character neither can draw is 0 wide; an ideographic space takes
// room but draws nothing.
RDE_INTERNAL f32 fude_sheet_advance(const fude_sheet_pdf* _pdf, u32 _cp, f32 _px, b8* _drawn) {
    const u8 _b = fude_sheet_winansi(_cp);
    if(_b != 0u) {
        *_drawn = true;
        return (f32)FUDE_SHEET_HELVETICA[_b - 32u] * _px / 1000.0f;
    }
    if(_cp == 0x3000u) {
        *_drawn = false;   // An ideographic space takes room but draws nothing.
        return _px;
    }
    fude_kanji_info _info;
    *_drawn = fude_kanji_find(_pdf->db, _cp, &_info);
    return *_drawn ? _px * FUDE_SHEET_GLYPH_ADVANCE : 0.0f;
}

// Returns how wide the UTF-8 _text is at size _px, as fude_sheet_text would draw it.
RDE_INTERNAL f32 fude_sheet_text_width(const fude_sheet_pdf* _pdf, const c8* _text, f32 _px) {
    f32       _w = 0.0f;
    const c8* _p = _text;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0u; _cp = fude_utf8_next(&_p)) {
        b8 _drawn;
        _w += fude_sheet_advance(_pdf, _cp, _px, &_drawn);
    }
    return _w;
}

// Draws strokes [_from, _to) of character _info as PDF curves, in the current colour and line width, in a box whose
// top-left corner is (_x, _y) and whose side is _size. The strokes' Y, which grows downwards, is flipped.
RDE_INTERNAL void fude_sheet_strokes(fude_sheet_pdf* _pdf, const fude_kanji_info* _info, f32 _x, f32 _y, f32 _size, u32 _from, u32 _to) {
    const f32 _k = _size / FUDE_KANJI_BOX;
    for(u32 _s = _from; _s < _to; _s++) {
        fude_kanji_stroke _stroke;
        if(!fude_kanji_stroke_at(_pdf->db, _info, _s, &_stroke)) {
            continue;
        }
        fude_sheet_draw(_pdf, "%.2f %.2f m\n", _x + _stroke.start.x * _k, _y - _stroke.start.y * _k);
        for(u32 _g = 0; _g < _stroke.segments; _g++) {
            rde_vec_2F _c[3];
            fude_kanji_stroke_segment(&_stroke, _g, _c);
            fude_sheet_draw(_pdf, "%.2f %.2f %.2f %.2f %.2f %.2f c\n", _x + _c[0].x * _k, _y - _c[0].y * _k, _x + _c[1].x * _k, _y - _c[1].y * _k,
                            _x + _c[2].x * _k, _y - _c[2].y * _k);
        }
        fude_sheet_draw(_pdf, "S\n");
    }
}

// Draws the UTF-8 _text with its baseline starting at (_x, _y), at size _px, in grey _grey (0 is black). Characters
// Helvetica can draw are gathered into runs, each written as one PDF text string; the language's characters are drawn
// from their strokes between the runs. Returns how wide the text went.
RDE_INTERNAL f32 fude_sheet_text(fude_sheet_pdf* _pdf, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _grey) {
    c8        _run[400];
    u32       _run_n  = 0;
    f32       _run_x  = _x;
    f32       _at     = _x;
    const c8* _p      = _text;
    fude_sheet_draw(_pdf, "%.3f g %.3f G\n", _grey, _grey);
    for(u32 _cp = fude_utf8_next(&_p);; _cp = fude_utf8_next(&_p)) {
        // The run is written out when a character Helvetica cannot draw comes, at the end of the text, or when the
        // buffer is nearly full (an escaped byte takes up to 4).
        const u8 _b = _cp != 0u ? fude_sheet_winansi(_cp) : 0u;
        if((_b == 0u || _run_n + 4u >= sizeof(_run)) && _run_n > 0u) {
            _run[_run_n] = 0;
            fude_sheet_draw(_pdf, "BT /F1 %.2f Tf %.2f %.2f Td (%s) Tj ET\n", _px, _run_x, _y, _run);
            _run_n = 0;
        }
        if(_cp == 0u) {
            break;
        }
        b8        _drawn;
        const f32 _w = fude_sheet_advance(_pdf, _cp, _px, &_drawn);
        if(_b != 0u) {
            if(_run_n == 0u) {
                _run_x = _at;
            }
            // In a PDF string, parentheses and backslashes are escaped, and bytes above 127 are written in octal.
            if(_b == '(' || _b == ')' || _b == '\\') {
                _run[_run_n++] = '\\';
                _run[_run_n++] = (c8)_b;
            } else if(_b >= 0x80u) {
                _run_n += (u32)rde_snprintf(&_run[_run_n], sizeof(_run) - _run_n, "\\%03o", (unsigned)_b);
            } else {
                _run[_run_n++] = (c8)_b;
            }
        } else if(_drawn) {
            // A character from the character data: its strokes, centred in its advance, the box's top 0.8 of it above
            // the baseline.
            fude_kanji_info _info;
            fude_kanji_find(_pdf->db, _cp, &_info);
            const f32 _box = _px * FUDE_SHEET_GLYPH_BOX;
            fude_sheet_draw(_pdf, "%.2f w 1 J 1 j\n", rde_math_max_f32(0.35f, _px * 0.075f));
            fude_sheet_strokes(_pdf, &_info, _at + (_w - _box) * 0.5f, _y + _box * 0.80f, _box, 0u, _info.strokes);
        }
        _at += _w;
    }
    return _at - _x;
}

// --- drawing ------------------------------------------------------------------------------

// Fills a disc centred on (_x, _y) of radius _r, in the current fill colour, as four Bézier curves (0.5523 is the
// control distance that makes a quarter circle).
RDE_INTERNAL void fude_sheet_disc(fude_sheet_pdf* _pdf, f32 _x, f32 _y, f32 _r) {
    const f32 _c = _r * 0.5523f;
    fude_sheet_draw(_pdf, "%.2f %.2f m %.2f %.2f %.2f %.2f %.2f %.2f c %.2f %.2f %.2f %.2f %.2f %.2f c ", _x + _r, _y, _x + _r, _y + _c, _x + _c, _y + _r, _x, _y + _r,
                    _x - _c, _y + _r, _x - _r, _y + _c, _x - _r, _y);
    fude_sheet_draw(_pdf, "%.2f %.2f %.2f %.2f %.2f %.2f c %.2f %.2f %.2f %.2f %.2f %.2f c f\n", _x - _r, _y - _c, _x - _c, _y - _r, _x, _y - _r, _x + _c, _y - _r,
                    _x + _r, _y - _c, _x + _r, _y);
}

// Draws a character in a box whose top-left corner is (_x, _y) and whose side is _size, with 8% of padding: its
// strokes in grey _grey and, when _numbers is set and it has at most FUDE_SHEET_NUMBERED strokes, each stroke's number
// in red by where the stroke starts, kept inside the box.
RDE_INTERNAL void fude_sheet_character(fude_sheet_pdf* _pdf, const fude_kanji_info* _info, f32 _x, f32 _y, f32 _size, f32 _grey, b8 _numbers) {
    const f32 _pad  = _size * 0.08f;
    const f32 _draw = _size - 2.0f * _pad;
    fude_sheet_draw(_pdf, "%.3f G %.2f w 1 J 1 j\n", _grey, _draw * 0.045f);
    fude_sheet_strokes(_pdf, _info, _x + _pad, _y - _pad, _draw, 0u, _info->strokes);
    if(!_numbers || _info->strokes > FUDE_SHEET_NUMBERED) {
        return;
    }
    const f32 _k  = _draw / FUDE_KANJI_BOX;
    const f32 _px = rde_math_max_f32(4.5f, _size * 0.11f);
    fude_sheet_draw(_pdf, "%.3f %.3f %.3f rg\n", FUDE_SHEET_RED);
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        fude_kanji_stroke _stroke;
        if(!fude_kanji_stroke_at(_pdf->db, _info, _s, &_stroke)) {
            continue;
        }
        c8  _num[8];
        rde_snprintf(_num, sizeof(_num), "%u", _s + 1u);
        // A digit in Helvetica is 0.556 of the size wide. The number goes just above and to the left of the stroke's
        // start.
        const f32 _w  = (f32)rde_strlen(_num) * 0.556f * _px;
        const f32 _nx = rde_math_min_f32(rde_math_max_f32(_x + _pad + _stroke.start.x * _k - _w - _px * 0.15f, _x + 1.0f), _x + _size - _w - 1.0f);
        const f32 _ny = rde_math_min_f32(rde_math_max_f32(_y - _pad - _stroke.start.y * _k + _px * 0.15f, _y - _size + 1.5f), _y - _px);
        fude_sheet_draw(_pdf, "BT /F1 %.2f Tf %.2f %.2f Td (%s) Tj ET\n", _px, _nx, _ny, _num);
    }
}

// Draws a row of _n boxes of side _size, side by side, from the top-left corner (_x, _y): the dashed cross in each,
// then the row as one frame with lines between the boxes, so shared edges are not drawn twice.
RDE_INTERNAL void fude_sheet_boxes(fude_sheet_pdf* _pdf, f32 _x, f32 _y, f32 _size, u32 _n) {
    fude_sheet_draw(_pdf, "0.82 G 0.4 w [1.5 1.5] 0 d\n");
    for(u32 _i = 0; _i < _n; _i++) {
        const f32 _bx = _x + (f32)_i * _size;
        fude_sheet_draw(_pdf, "%.2f %.2f m %.2f %.2f l %.2f %.2f m %.2f %.2f l S\n", _bx + _size * 0.5f, _y, _bx + _size * 0.5f, _y - _size, _bx, _y - _size * 0.5f,
                        _bx + _size, _y - _size * 0.5f);
    }
    fude_sheet_draw(_pdf, "[] 0 d 0.55 G 0.6 w\n");
    fude_sheet_draw(_pdf, "%.2f %.2f %.2f %.2f re S\n", _x, _y - _size, _size * (f32)_n, _size);
    for(u32 _i = 1; _i < _n; _i++) {
        fude_sheet_draw(_pdf, "%.2f %.2f m %.2f %.2f l S\n", _x + (f32)_i * _size, _y, _x + (f32)_i * _size, _y - _size);
    }
}

// Shortens a list such as "a, b, c" in place, dropping items from its end at the separator _comma, until it fits
// _width at size _px. The first item always stays, even when it alone does not fit.
RDE_INTERNAL void fude_sheet_shorten(const fude_sheet_pdf* _pdf, c8* _list, const c8* _comma, f32 _px, f32 _width) {
    while(fude_sheet_text_width(_pdf, _list, _px) > _width) {
        c8* _last = NULL;
        for(c8* _c = rde_strstr(_list, _comma); _c != NULL; _c = rde_strstr(_c + 1, _comma)) {
            _last = _c;
        }
        if(_last == NULL) {
            return;
        }
        *_last = 0;
    }
}

// Draws one character's block downwards from _top: its line of meanings, readings, level and strokes; its stroke
// order, one box a step; then _rows rows of boxes of side _box to write in.
RDE_INTERNAL void fude_sheet_block(fude_sheet_pdf* _pdf, const fude_kanji_info* _info, f32 _top, u32 _rows, f32 _box) {
    const f32 _left  = FUDE_SHEET_MARGIN;
    const f32 _right = FUDE_SHEET_PAGE_W - FUDE_SHEET_MARGIN;
    const f32 _width = _right - _left;

    // The line: the meanings, then the readings, on the left; the level and the stroke count on the right.
    c8 _label[96];
    c8 _strokes[512];
    FUDE_TEXTF(_strokes, FUDE_TEXT_STROKES_N, FUDE_TN(_info->strokes));
    if(_info->level != 0u) {
        c8 _level[32];
        fude_lang_level_name(_info->level, false, _level, sizeof(_level));
        rde_snprintf(_label, sizeof(_label), "%s \xC2\xB7 %s", _level, _strokes);   // A middle dot (·) between them.
    } else {
        rde_snprintf(_label, sizeof(_label), "%s", _strokes);
    }
    const f32 _base  = _top - 12.0f;
    const f32 _lw    = fude_sheet_text_width(_pdf, _label, 8.0f);
    fude_sheet_text(_pdf, _label, _right - _lw, _base, 8.0f, 0.45f);

    // A script's letter shows its group's name; any other character its meanings and its two kinds of readings
    // (lang.h; Japanese: on, then kun).
    c8 _meanings[512];
    c8 _readings[512];
    if(fude_lang_is_letter(_info->codepoint)) {   // a script's letter (Japanese: a kana) has no readings or meanings
        rde_snprintf(_meanings, sizeof(_meanings), "%s", fude_text((FUDE_TEXT_)fude_lang_group_name(fude_lang_group(_info->codepoint))));
        _readings[0] = 0;
    } else {
        rde_snprintf(_meanings, sizeof(_meanings), "%s", fude_kanji_meanings(_pdf->db, _info));
        const c8* _first  = fude_kanji_reading(_pdf->db, _info, 0u);
        const c8* _second = fude_kanji_reading(_pdf->db, _info, 1u);
        rde_snprintf(_readings, sizeof(_readings), "%s%s%s", _first, _first[0] != 0 && _second[0] != 0 ? "\xE3\x80\x80" : "", _second);   // An ideographic space between.
    }
    const f32 _room = _width - _lw - 16.0f;
    f32       _px   = 10.0f;
    // The meanings may take up to 55% of the room when there are readings, and the readings what is left; each list
    // is cut at a whole item. If both are still too long, the text is made smaller, down to 70% of its size.
    fude_sheet_shorten(_pdf, _meanings, ", ", _px, _room * (_readings[0] != 0 ? 0.55f : 1.0f));
    const f32 _mw = fude_sheet_text_width(_pdf, _meanings, _px);
    fude_sheet_shorten(_pdf, _readings, "\xE3\x80\x81", _px * 0.9f, _room - _mw - 14.0f);   // An ideographic comma (、).
    const f32 _total = _mw + (_readings[0] != 0 ? 14.0f + fude_sheet_text_width(_pdf, _readings, _px * 0.9f) : 0.0f);
    if(_total > _room) {
        _px *= rde_math_max_f32(0.7f, _room / _total);
    }
    const f32 _mx = fude_sheet_text(_pdf, _meanings, _left, _base, _px, 0.12f);
    if(_readings[0] != 0) {
        fude_sheet_text(_pdf, _readings, _left + _mx + 14.0f * _px / 10.0f, _base, _px * 0.9f, 0.38f);
    }

    // The stroke order: a box for each step, with the new stroke dark, those before it grey, and a red dot where the
    // new one starts. The boxes shrink so that every stroke fits one row.
    const f32 _ot   = _top - FUDE_SHEET_INFO;
    const f32 _gap  = FUDE_SHEET_ORDER_GAP;
    const u32 _n    = _info->strokes > 0u ? _info->strokes : 1u;
    const f32 _os   = rde_math_min_f32(FUDE_SHEET_ORDER, (_width + _gap) / (f32)_n - _gap);
    fude_sheet_draw(_pdf, "0.80 G 0.4 w\n");
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        fude_sheet_draw(_pdf, "%.2f %.2f %.2f %.2f re S\n", _left + (f32)_s * (_os + _gap), _ot - _os, _os, _os);
    }
    const f32 _pad  = _os * 0.1f;
    const f32 _draw = _os - 2.0f * _pad;
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        const f32 _x = _left + (f32)_s * (_os + _gap) + _pad;
        const f32 _y = _ot - _pad;
        fude_sheet_draw(_pdf, "%.2f w 1 J 1 j 0.70 G\n", _draw * 0.05f);
        fude_sheet_strokes(_pdf, _info, _x, _y, _draw, 0u, _s);
        fude_sheet_draw(_pdf, "%.3f G\n", FUDE_SHEET_INK);
        fude_sheet_strokes(_pdf, _info, _x, _y, _draw, _s, _s + 1u);
        fude_kanji_stroke _stroke;
        if(fude_kanji_stroke_at(_pdf->db, _info, _s, &_stroke)) {
            const f32 _k = _draw / FUDE_KANJI_BOX;
            fude_sheet_draw(_pdf, "%.3f %.3f %.3f rg\n", FUDE_SHEET_RED);
            fude_sheet_disc(_pdf, _x + _stroke.start.x * _k, _y - _stroke.start.y * _k, rde_math_max_f32(0.8f, _os * 0.055f));
        }
    }

    // The rows: the first has the numbered model, then FUDE_SHEET_TRACES copies fading out; each other row starts with
    // one faded copy. The rest of each row is empty.
    const f32 _rt = _ot - FUDE_SHEET_ORDER - 6.0f;
    for(u32 _r = 0; _r < _rows; _r++) {
        const f32 _y = _rt - (f32)_r * _box;
        fude_sheet_boxes(_pdf, _left, _y, _box, FUDE_SHEET_BOXES);
        if(_r == 0u) {
            fude_sheet_character(_pdf, _info, _left, _y, _box, FUDE_SHEET_INK, true);
            static const f32 _greys[FUDE_SHEET_TRACES] = { 0.72f, 0.80f, 0.87f };
            for(u32 _t = 0; _t < FUDE_SHEET_TRACES; _t++) {
                fude_sheet_character(_pdf, _info, _left + (f32)(_t + 1u) * _box, _y, _box, _greys[_t], false);
            }
        } else {
            fude_sheet_character(_pdf, _info, _left, _y, _box, 0.80f, false);
        }
    }
}

// Finishes page _page (counting from 0) of _pages: draws its header (the title, the date, a rule under them) and its
// footer (the credit, shrunk to fit if need be, and "page of pages") into its drawing, then writes the page's two
// objects into the file and empties the drawing for the next page.
RDE_INTERNAL void fude_sheet_end_page(fude_sheet_pdf* _pdf, u32 _page, u32 _pages, const c8* _title, const c8* _date) {
    const f32 _left  = FUDE_SHEET_MARGIN;
    const f32 _right = FUDE_SHEET_PAGE_W - FUDE_SHEET_MARGIN;
    const f32 _top   = FUDE_SHEET_PAGE_H - FUDE_SHEET_MARGIN;
    fude_sheet_text(_pdf, _title, _left, _top - 16.0f, 15.0f, 0.10f);
    fude_sheet_text(_pdf, _date, _right - fude_sheet_text_width(_pdf, _date, 9.0f), _top - 15.0f, 9.0f, 0.45f);
    fude_sheet_draw(_pdf, "0.75 G 0.5 w %.2f %.2f m %.2f %.2f l S\n", _left, _top - FUDE_SHEET_HEADER + 6.0f, _right, _top - FUDE_SHEET_HEADER + 6.0f);
    c8 _number[512];
    FUDE_TEXTF(_number, FUDE_TEXT_OF_N, FUDE_TN(_page + 1u), FUDE_TN(_pages));
    const f32 _foot = FUDE_SHEET_MARGIN - 4.0f;
    const f32 _nw   = fude_sheet_text_width(_pdf, _number, 7.5f);
    c8        _credit[512];
    rde_snprintf(_credit, sizeof(_credit), "%s", fude_text(FUDE_TEXT_SHEET_CREDIT));
    const f32 _cw   = fude_sheet_text_width(_pdf, _credit, 7.0f);
    const f32 _room = _right - _left - _nw - 12.0f;
    fude_sheet_text(_pdf, _credit, _left, _foot, _cw > _room ? 7.0f * _room / _cw : 7.0f, 0.50f);
    fude_sheet_text(_pdf, _number, _right - _nw, _foot, 7.5f, 0.50f);

    const u32 _object = FUDE_SHEET_OBJ_FIRST + 2u * _page;
    fude_sheet_object(_pdf, _object);
    fude_sheet_put(_pdf, "<< /Type /Page /Parent %u 0 R /MediaBox [0 0 %.2f %.2f] /Resources << /Font << /F1 %u 0 R >> >> /Contents %u 0 R >>\nendobj\n",
                   FUDE_SHEET_OBJ_PAGES, FUDE_SHEET_PAGE_W, FUDE_SHEET_PAGE_H, FUDE_SHEET_OBJ_FONT, _object + 1u);
    fude_sheet_object(_pdf, _object + 1u);
    fude_sheet_put(_pdf, "<< /Length %u >>\nstream\n", fude_bytes_size(&_pdf->page));
    fude_put_data(&_pdf->out, _pdf->page.memory, fude_bytes_size(&_pdf->page));
    fude_sheet_put(_pdf, "\nendstream\nendobj\n");
    rde_arr_clear(&_pdf->page);
}

// Writes the UTF-8 _text as a PDF text string in UTF-16 big-endian with its byte order mark, <FEFF...>, so that the
// document's info (its title, the app's name) shows in any language. Characters past U+FFFF become surrogate pairs.
RDE_INTERNAL void fude_sheet_put_utf16(fude_sheet_pdf* _pdf, const c8* _text) {
    fude_sheet_put(_pdf, "<FEFF");
    const c8* _p = _text;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0u; _cp = fude_utf8_next(&_p)) {
        if(_cp >= 0x10000u) {
            _cp -= 0x10000u;
            fude_sheet_put(_pdf, "%04X%04X", 0xD800u + (_cp >> 10), 0xDC00u + (_cp & 0x3FFu));
        } else {
            fude_sheet_put(_pdf, "%04X", _cp);
        }
    }
    fude_sheet_put(_pdf, ">");
}

// Writes the sheet (see the header): picks the characters, works out how many rows each gets and how many share a
// page, builds the whole PDF in memory, and writes it in one go.
b8 fude_sheet_write(const fude_kanji_db* _db, const u32* _records, u32 _count, const c8* _path, const c8* _creator, fude_sheet_info* _info) {
    if(_info != NULL) {
        rde_memset(_info, 0, sizeof(*_info));
    }
    if(_db == NULL || _path == NULL) {
        return false;
    }
    // The first FUDE_SHEET_MAX characters that can be drawn, those with strokes. The array is static so it is not
    // on the stack.
    static fude_kanji_info _chars[FUDE_SHEET_MAX];
    u32 _n = 0;
    for(u32 _i = 0; _i < _count && _n < FUDE_SHEET_MAX; _i++) {
        if(fude_kanji_at(_db, _records[_i], &_chars[_n]) && _chars[_n].strokes > 0u) {
            _n++;
        }
    }
    if(_n == 0u) {
        return false;
    }

    // How many rows each character gets, so that a few characters fill their page: the boxes fill the width, then the
    // characters that share a page (at most FUDE_SHEET_PER_PAGE, and no more than fit one row each) split its height.
    // The page's height counts one extra gap, since the last block on a page needs none.
    const f32 _box     = (FUDE_SHEET_PAGE_W - 2.0f * FUDE_SHEET_MARGIN) / (f32)FUDE_SHEET_BOXES;
    const f32 _content = FUDE_SHEET_PAGE_H - 2.0f * FUDE_SHEET_MARGIN - FUDE_SHEET_HEADER - FUDE_SHEET_FOOTER + FUDE_SHEET_GAP;
    const f32 _fixed   = FUDE_SHEET_INFO + FUDE_SHEET_ORDER + 6.0f + FUDE_SHEET_GAP;
    const u32 _most    = (u32)rde_math_min_f32((f32)FUDE_SHEET_PER_PAGE, rde_math_floor_f32(_content / (_fixed + _box)));
    const u32 _share   = _n < _most ? _n : _most;
    const u32 _rows    = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32((_content / (f32)_share - _fixed) / _box));
    const f32 _block   = _fixed + (f32)_rows * _box;
    const u32 _per     = (u32)rde_math_max_f32(1.0f, rde_math_min_f32((f32)FUDE_SHEET_PER_PAGE, rde_math_floor_f32(_content / _block)));
    const u32 _pages   = (_n + _per - 1u) / _per;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_sheet_pdf        _pdf  = { .db = _db, .out = fude_bytes_new(64u * 1024u), .page = fude_bytes_new(64u * 1024u),
                                    .offsets = rde_arr_new(sizeof(u32), _heap) };
    c8 _title[512];
    c8 _date[64];
    rde_snprintf(_title, sizeof(_title), "%s", fude_text(FUDE_TEXT_SHEET_TITLE));
    fude_text_date(_date, sizeof(_date), (u64)rde_date_now_unix_seconds());

    // The header line, then a comment of four bytes above 127, which tells file transfers that the file is binary.
    fude_sheet_put(&_pdf, "%%PDF-1.4\n%%\xE2\xE3\xCF\xD3\n");
    fude_sheet_object(&_pdf, FUDE_SHEET_OBJ_CATALOG);
    fude_sheet_put(&_pdf, "<< /Type /Catalog /Pages %u 0 R >>\nendobj\n", FUDE_SHEET_OBJ_PAGES);
    fude_sheet_object(&_pdf, FUDE_SHEET_OBJ_FONT);
    fude_sheet_put(&_pdf, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n");
    fude_sheet_object(&_pdf, FUDE_SHEET_OBJ_INFO);
    fude_sheet_put(&_pdf, "<< /Title ");
    fude_sheet_put_utf16(&_pdf, _title);
    // The app that made the file, shown in a PDF viewer's document properties.
    fude_sheet_put(&_pdf, " /Creator ");
    fude_sheet_put_utf16(&_pdf, _creator != NULL ? _creator : "");
    fude_sheet_put(&_pdf, " /Producer ");
    fude_sheet_put_utf16(&_pdf, _creator != NULL ? _creator : "");
    fude_sheet_put(&_pdf, " >>\nendobj\n");

    for(u32 _page = 0; _page < _pages; _page++) {
        f32 _top = FUDE_SHEET_PAGE_H - FUDE_SHEET_MARGIN - FUDE_SHEET_HEADER;
        for(u32 _i = _page * _per; _i < _n && _i < (_page + 1u) * _per; _i++) {
            fude_sheet_block(&_pdf, &_chars[_i], _top, _rows, _box);
            _top -= _block;
        }
        fude_sheet_end_page(&_pdf, _page, _pages, _title, _date);
    }

    fude_sheet_object(&_pdf, FUDE_SHEET_OBJ_PAGES);
    fude_sheet_put(&_pdf, "<< /Type /Pages /Count %u /Kids [", _pages);
    for(u32 _page = 0; _page < _pages; _page++) {
        fude_sheet_put(&_pdf, "%s%u 0 R", _page > 0u ? " " : "", FUDE_SHEET_OBJ_FIRST + 2u * _page);
    }
    fude_sheet_put(&_pdf, "] >>\nendobj\n");

    // The cross-reference table: each object's byte offset in ten digits, every line exactly twenty bytes as the PDF
    // format requires, after the entry for the free object 0.
    const u32 _objects = (u32)rde_arr_length(&_pdf.offsets);
    const u32 _xref    = fude_bytes_size(&_pdf.out);
    fude_sheet_put(&_pdf, "xref\n0 %u\n0000000000 65535 f \n", _objects + 1u);
    for(u32 _o = 0; _o < _objects; _o++) {
        fude_sheet_put(&_pdf, "%010u 00000 n \n", ((const u32*)_pdf.offsets.memory)[_o]);
    }
    fude_sheet_put(&_pdf, "trailer\n<< /Size %u /Root %u 0 R /Info %u 0 R >>\nstartxref\n%u\n%%%%EOF\n", _objects + 1u, FUDE_SHEET_OBJ_CATALOG, FUDE_SHEET_OBJ_INFO,
                   _xref);

    const u32 _size = fude_bytes_size(&_pdf.out);
    const b8  _ok   = fude_file_write(_path, _pdf.out.memory, _size);   // A failed write leaves no partial file.
    if(!_ok) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "sheet: the practice sheet could not be written to %s", _path);
    }
    rde_arr_free(&_pdf.out);
    rde_arr_free(&_pdf.page);
    rde_arr_free(&_pdf.offsets);
    if(_ok && _info != NULL) {
        *_info = (fude_sheet_info){ .characters = _n, .pages = _pages, .bytes = _size };
    }
    return _ok;
}
