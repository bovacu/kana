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
// See export.h.
//
// The file is the walk (fude_zoom_walk_*): it turns the scene into screen geometry, frame by frame and in the
// renderer's order, and hands it to a sink; the settings it reads (export.h's _only, _units, _formulas, _measure) are
// this file's statics. The walk's limits come first, then its state, the settings, and each kind of object walked (a
// stroke, a cut board, a symbol, a sheet, a dimension, a shape, a fill, a picture, a formula, a text), then a frame and
// the export's own walk (fude_zoom_export_walk, _walk_as). The writers, each a sink, are in export_formats.c.
// ===========================================================================

// The walk's limits, in screen points (named SVG's, the first export, but every export's).
#define FUDE_ZOOM_SVG_EXACT   0.25     // as the renderer: a transform's error under this many points
#define FUDE_ZOOM_SVG_MIN_PX  0.5      // a frame smaller than this on screen is not gone into
#define FUDE_ZOOM_SVG_SPECK   0.35     // a thing smaller than this on screen is left out

// --- the walk ----------------------------------------------------------------------------------------

// The walk's state: the scene and the sink, the screen's half size, scratch arrays kept from one object to the next
// (the objects a query found, a stroke's packed points, points, radii, rings), and the theme whose ink is "the
// theme's ink".
typedef struct {
    const fude_zoom_scene*        s;
    const fude_zoom_export_sink*  sink;
    fude_zoom_v2                  half;
    rde_arr TYPE(u32)              found;
    rde_arr TYPE(fude_zoom_qpoint) q;
    rde_arr TYPE(fude_zoom_v2)     pts;
    rde_arr TYPE(f64)              radii;
    rde_arr TYPE(u32)              rings;
    const fude_theme*              theme;   // whose ink "the theme's ink" is (a print's: the paper theme's)
} fude_zoom_walk_state;

// What an export of a selection keeps (fude_zoom_export_only; NULL: everything).
static const u8* fude_zoom_export_keep       = NULL;
static u32       fude_zoom_export_keep_count = 0;

// See the header.
void fude_zoom_export_only(const u8* _keep, u32 _count) {
    fude_zoom_export_keep       = _keep;
    fude_zoom_export_keep_count = _keep != NULL ? _count : 0u;
}

// Lengths as written (fude_zoom_export_units; 0: not set).
static f64                   fude_zoom_export_mm_per_unit = 0.0;
static fude_zoom_units_style fude_zoom_export_style;

// See the header. A NULL _units keeps the style as it was.
void fude_zoom_export_units(f64 _mm_per_unit, const fude_zoom_units_style* _units) {
    fude_zoom_export_mm_per_unit = _mm_per_unit;
    if(_units != NULL) {
        fude_zoom_export_style = *_units;
    }
}

// The formulas set as the page sets them (fude_zoom_export_formulas; NULL: written as their LaTeX).
static fude_zoom_formulas* fude_zoom_export_maths = NULL;

// How a note's words are measured (fude_zoom_export_measure): the app's fonts', else a guess.
RDE_INTERNAL fude_zoom_rich_measure_fn fude_zoom_export_measure_fn   = NULL;
RDE_INTERNAL any                       fude_zoom_export_measure_self = NULL;

// See the header.
void fude_zoom_export_measure(fude_zoom_rich_measure_fn _fn, any _self) {
    fude_zoom_export_measure_fn   = _fn;
    fude_zoom_export_measure_self = _self;
}

// The measure used until one is set: each letter half its height wide (a bold one a little more).
RDE_INTERNAL f32 fude_zoom_export_guess(any _self, const c8* _text, u32 _len, f32 _px, u8 _style) {
    RDE_UNUSED(_self);
    u32 _letters = 0;
    for(u32 _i = 0; _i < _len; _i++) {
        _letters += ((u8)_text[_i] & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return (f32)_letters * _px * ((_style & FUDE_ZOOM_RICH_BOLD) ? 0.55f : 0.5f);
}

// See the header.
void fude_zoom_export_formulas(struct fude_zoom_formulas* _formulas) {
    fude_zoom_export_maths = _formulas;
}

// Declared ahead: a symbol's label and a graph's legend use them, before the texts' part of the walk.
RDE_INTERNAL void fude_zoom_walk_tex(fude_zoom_walk_state* _w, const fude_zoom_tex_box* _box, fude_zoom_sim _em, rde_color _ink);
RDE_INTERNAL void fude_zoom_walk_rich_marks(fude_zoom_walk_state* _w, const fude_zoom_rich* _rich, const c8* _text, fude_zoom_sim _at, f64 _size, rde_color _ink);
RDE_INTERNAL void fude_zoom_walk_rich_lay(fude_zoom_rich* _rich, const c8* _text, u32 _len, f64 _width, f64 _size, u8 _align);

// Returns the colour to write for _c: the page's colour for the page's fill (FUDE_THEME_PAGE_FILL's sentinel), the
// walk's theme's ink for the theme's ink, else _c as it is.
RDE_INTERNAL rde_color fude_zoom_walk_resolve(const fude_zoom_walk_state* _w, rde_color _c) {
    if(_c.a == 1u && _c.r == 255u && _c.g == 254u && _c.b == 253u) {
        return _w->theme->page;   // (FUDE_THEME_PAGE_FILL: the page's)
    }
    return fude_theme_is_ink(_c) ? _w->theme->ink : _c;
}

// A stroke: its points and each one's half-width, on the screen through _to, handed to the sink's stroke.
RDE_INTERNAL void fude_zoom_walk_stroke(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    rde_arr_clear(&_w->q);
    rde_arr_add_n(&_w->q, _o->count);
    if(_o->count == 0 || _w->sink->stroke == NULL || !fude_zoom_scene_points(_w->s, _object, (fude_zoom_qpoint*)_w->q.memory)) {
        return;
    }
    const fude_zoom_qpoint* _q     = (const fude_zoom_qpoint*)_w->q.memory;
    const f64               _scale = fude_zoom_sim_scale(_to);
    rde_arr_clear(&_w->pts);
    rde_arr_clear(&_w->radii);
    fude_zoom_v2* _p = rde_arr_add_n(&_w->pts, _o->count);
    f64*          _r = rde_arr_add_n(&_w->radii, _o->count);
    for(u32 _i = 0; _i < _o->count; _i++) {
        _p[_i] = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_q[_i]));
        _r[_i] = fude_zoom_scene_radius_at(_o, &_q[_i]) * _scale;
    }
    _w->sink->stroke(_w->sink->self, _p, _r, _o->count, fude_zoom_walk_resolve(_w, _o->color));
}

// A cut board: its material as a fill (its holes cut from it), then each ring as a closed line.
RDE_INTERNAL b8 fude_zoom_walk_cut_board(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _num = rde_arr_new(sizeof(f64), _heap), _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _rings = rde_arr_new(sizeof(u32), _heap);
    const u32 _count = fude_zoom_scene_shape_numbers_all(_w->s, _object, &_num);
    if(!fude_zoom_board_is_cut((const f64*)_num.memory, _count)) {
        rde_arr_free(&_num); rde_arr_free(&_pts); rde_arr_free(&_rings);
        return false;
    }
    const u32 _n = fude_zoom_board_rings((const f64*)_num.memory, _count, &_pts, &_rings);
    const fude_zoom_sim _all = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
    fude_zoom_v2* _p = (fude_zoom_v2*)_pts.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        _p[_i] = fude_zoom_sim_apply(_all, _p[_i]);
    }
    const rde_color _line = fude_zoom_walk_resolve(_w, _o->color);
    if((_o->flags & FUDE_ZOOM_FLAG_FILLED) && _w->sink->fill != NULL) {
        _w->sink->fill(_w->sink->self, _p, (const u32*)_rings.memory, _n, (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_zoom_walk_resolve(_w, _o->fill) : _line);
    }
    if(_w->sink->shape != NULL) {
        const rde_color _none = { 0, 0, 0, 0 };
        for(u32 _from = 0; _from < _n;) {
            u32 _end = _from;
            while(_end < _n && ((const u32*)_rings.memory)[_end] == ((const u32*)_rings.memory)[_from]) {
                _end++;
            }
            if(_end - _from >= 3u) {
                _w->sink->shape(_w->sink->self, &_p[_from], _end - _from, true, (f64)_o->radius * _o->scale * fude_zoom_sim_scale(_to), _line, _none);
            }
            _from = _end;
        }
    }
    rde_arr_free(&_num); rde_arr_free(&_pts); rde_arr_free(&_rings);
    return true;
}

// Hands each of a symbol's parts (_pa, _np of them, their points _p on the screen) to the sink as a shape: filled as
// its art says (solid in its line's colour _line, tinted, filled with _fill, or a joined ring, a hole, as the paper),
// its line its own or _line, its width its own (_k the screen's points a unit) or _radius.
RDE_INTERNAL void fude_zoom_walk_symbol_parts(fude_zoom_walk_state* _w, fude_zoom_v2* _p, const fude_zoom_symbol_part* _pa, u32 _np, rde_color _line,
                                              rde_color _fill, f64 _k, f64 _radius) {
    for(u32 _i = 0; _i < _np && _w->sink->shape != NULL; _i++) {
        // (its art's: its own fill and line colours and widths; a joined ring — a hole — filled as the paper is)
        const u8  _fl    = _pa[_i].flags;
        const b8  _solid = (_fl & FUDE_ZOOM_SYMBOL_SOLID) != 0u;
        rde_color _f     = _solid ? _line : ((_fl & FUDE_ZOOM_SYMBOL_TINTED) ? _pa[_i].fill : ((_fl & FUDE_ZOOM_SYMBOL_FILLED) ? _fill : (rde_color){ 0, 0, 0, 0 }));
        if((_fl & FUDE_ZOOM_SYMBOL_JOIN) && _f.a > 0u) {
            _f = _w->theme != NULL ? _w->theme->page : (rde_color){ 255, 255, 255, 255 };
        }
        rde_color _l = (_fl & FUDE_ZOOM_SYMBOL_NO_LINE) ? (rde_color){ 0, 0, 0, 0 } : (_pa[_i].line.a > 0u ? _pa[_i].line : _line);
        const f64 _r = _pa[_i].width > 0.0f ? (f64)_pa[_i].width * 0.5 * _k : _radius;
        _w->sink->shape(_w->sink->self, &_p[_pa[_i].first], _pa[_i].count, (_fl & FUDE_ZOOM_SYMBOL_CLOSED) != 0u, _r, _l, _f);
    }
}

// A graph's, axes', a number line's (plot.h, _info) lines and labels from its text and half sizes (_n[1], _n[2]),
// through _all onto the screen (_k its points a unit): its regions and bars filled, its curves and ticks as lines
// (_radius, _line), its legend set as the renderer sets it, and its ticks' numbers. Its scratch on _heap, freed.
RDE_INTERNAL void fude_zoom_walk_symbol_plot(fude_zoom_walk_state* _w, const fude_zoom_symbol_info* _info, const c8* _text, const f64* _n, fude_zoom_sim _all,
                                             f64 _k, f64 _radius, rde_color _line, rde_memory_allocator* _heap) {
    // (a graph's, axes', a number line's lines and ticks' numbers: plot.h)
    rde_arr _pp = rde_arr_new(sizeof(fude_zoom_v2), _heap), _qa = rde_arr_new(sizeof(fude_zoom_symbol_part), _heap), _pl = rde_arr_new(sizeof(fude_zoom_plot_label), _heap);
    fude_zoom_plot_lines(_info->id, _text, _n[1], _n[2], &_pp, &_qa, &_pl);
    static const rde_color _curve[6] = { { 70, 140, 255, 255 }, { 240, 80, 70, 255 }, { 60, 190, 110, 255 }, { 245, 160, 40, 255 }, { 170, 100, 240, 255 }, { 40, 190, 200, 255 } };
    fude_zoom_v2* _q = (fude_zoom_v2*)_pp.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_pp); _i++) {
        _q[_i] = fude_zoom_sim_apply(_all, _q[_i]);
    }
    const fude_zoom_symbol_part* _qp = (const fude_zoom_symbol_part*)_qa.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_qa); _i++) {
        const u8 _fl = _qp[_i].flags;
        if(_fl >= 64u) {
            // (a region an inequality holds, an area under a curve: its colour, faint, filled; a chart's bar or
            // slice, 128 + k: solid)
            rde_color _c = _fl >= 128u ? _curve[(_fl - 128u) % 6u] : _curve[(_fl - 64u) % 6u];
            _c.a = _fl >= 128u ? 255u : 56u;
            if(_w->sink->fill != NULL && _qp[_i].count >= 3u) {
                rde_arr_clear(&_w->rings);
                u32* _ring = (u32*)rde_arr_add_n(&_w->rings, _qp[_i].count);
                rde_memset(_ring, 0, sizeof(u32) * _qp[_i].count);
                _w->sink->fill(_w->sink->self, &_q[_qp[_i].first], _ring, _qp[_i].count, _c);
            }
            continue;
        }
        rde_color _c = _fl >= 32u ? _curve[((_fl - 32u) & 7u) % 6u] : _line;
        if(_fl < 32u && (_fl & FUDE_ZOOM_SYMBOL_DASHED)) {
            _c.a = (u8)((u32)_c.a * 30u / 100u);
        }
        _w->sink->shape(_w->sink->self, &_q[_qp[_i].first], _qp[_i].count, false, _fl >= 32u ? rde_math_max_f64(_radius * 1.3, 0.6) : rde_math_max_f64(_radius * 0.6, 0.25), _c, (rde_color){ 0, 0, 0, 0 });
    }
    const f64 _lpx = fude_zoom_plot_label_height(_info->id, _n[1], _n[2]) * _k;
    const fude_zoom_plot_label* _lb = (const fude_zoom_plot_label*)_pl.memory;
    // (its legend as the renderer sets it: each line maths in text style, stacked down from where its first is)
    f64 _row_y = 0.0, _row_x = 0.0;
    b8 _first_row = true;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_pl) && _lpx >= 3.0; _i++) {
        if(_lb[_i].align != 4u) {
            continue;
        }
        const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, _lb[_i].at);
        if(_first_row) {
            _row_x = _at.x;
            _row_y = _at.y + _lpx * 0.55;
            _first_row = false;
        }
        const f64 _em = _lpx * 1.25;
        const fude_zoom_tex_box* _box = fude_zoom_export_maths != NULL && _lb[_i].latex[0] != 0 ?
                                        fude_zoom_formulas_set_as(fude_zoom_export_maths, _lb[_i].latex, (u32)rde_strlen(_lb[_i].latex), false) : NULL;
        const f64 _up = rde_math_max_f64(_lpx * 0.72, _box != NULL ? _box->height * _em : 0.0), _down = rde_math_max_f64(_lpx * 0.25, _box != NULL ? _box->depth * _em : 0.0);
        const f64 _base = _row_y - _up;
        const rde_color _c = _lb[_i].curve > 0u ? _curve[(_lb[_i].curve - 1u) % 6u] : _line;
        if(_box != NULL) {
            fude_zoom_walk_tex(_w, _box, (fude_zoom_sim){ _em, 0.0, _row_x, _base }, _c);
        } else if(_w->sink->text != NULL) {
            const c8* const _lines[1] = { _lb[_i].text };
            _w->sink->text(_w->sink->self, (fude_zoom_v2){ _row_x, _base + _lpx * 0.75 }, _lpx, _lpx * 0.6 * (f64)rde_strlen(_lb[_i].text), _lpx * 1.3, 0u, _c,
                           (rde_color){ 0, 0, 0, 0 }, _lines, 1u);
        }
        _row_y = _base - _down - _lpx * 0.3;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_pl) && _w->sink->text != NULL && _lpx >= 3.0; _i++) {
        if(_lb[_i].align == 4u) {
            continue;   // (the legend: above)
        }
        const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, _lb[_i].at);
        const f64 _tw = _lpx * 0.6 * (f64)rde_strlen(_lb[_i].text);
        const fude_zoom_v2 _tl = { _lb[_i].align == 1u ? _at.x - _tw : (_lb[_i].align >= 3u ? _at.x : _at.x - _tw * 0.5), _lb[_i].align == 0u ? _at.y + _lpx * 0.3 : _at.y + _lpx * 0.6 };
        const c8* const _lines[1] = { _lb[_i].text };
        _w->sink->text(_w->sink->self, _tl, _lpx, _tw, _lpx * 1.3, 0u, _lb[_i].curve > 0u ? _curve[(_lb[_i].curve - 1u) % 6u] : _line, (rde_color){ 0, 0, 0, 0 }, _lines, 1u);
    }
    rde_arr_free(&_pp);
    rde_arr_free(&_qa);
    rde_arr_free(&_pl);
}

// Breaks _part into its lines in place (each ended where it breaks), into _lines (24 at most): its own breaks, and as
// many words as fit _most letters. Returns how many lines.
RDE_INTERNAL u32 fude_zoom_walk_symbol_break(c8* _part, u32 _most, const c8** _lines) {
    u32 _count_l = 0;
    c8* _c = _part;
    while(*_c != 0 && _count_l < 24u) {
        c8* _start = _c;
        c8* _space = NULL;
        u32 _letters = 0;
        while(*_c != 0 && *_c != '\n') {
            if(*_c == ' ') {
                _space = _c;
            }
            _letters += ((u8)*_c & 0xC0u) != 0x80u ? 1u : 0u;
            if(_letters > _most && _space != NULL) {
                _c = _space;
                break;
            }
            _c++;
        }
        const c8 _was = *_c;
        *_c = 0;
        _lines[_count_l++] = _start;
        if(_was != 0) {
            _c++;
        }
    }
    return _count_l;
}

// A symbol's text (_text; _info its kind, _n its numbers, _all onto the screen, _k its points a unit) in its boxes: a
// class's compartments' lines between them (_radius, _line), then each box's part of it, a label with marks as the
// screen has it, else its lines broken by an average letter's width (its height _n[3]); none for a graph's (its
// legend is its plot's).
RDE_INTERNAL void fude_zoom_walk_symbol_text(fude_zoom_walk_state* _w, const fude_zoom_symbol_info* _info, const c8* _text, const f64* _n, fude_zoom_sim _all,
                                             f64 _k, f64 _radius, rde_color _line) {
    u32 _from[8], _len[8];
    const u32 _split = fude_zoom_symbol_text_split(_text, _from, _len, 8u);
    fude_zoom_box _boxes[8];
    const b8  _parts_text = _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS || _info->place == FUDE_ZOOM_SYMBOL_TEXT_REGIONS;   // (a part each in its box)
    const b8  _classes    = _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS;   // (compartments: lines between, the rest from the top left)
    // (its kind read from the four numbers at least the caller read)
    const u32 _nb = fude_zoom_symbol_text_boxes(fude_zoom_symbol_kind_of(_n, 4u), _n[1], _n[2], _parts_text ? _split : 1u, _boxes, 8u);
    if(_classes && _info->boxes != 4u && _w->sink->shape != NULL) {
        for(u32 _i = 1; _i < _nb; _i++) {
            const fude_zoom_v2 _l[2] = { fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_n[1], _boxes[_i].max_y }), fude_zoom_sim_apply(_all, (fude_zoom_v2){ _n[1], _boxes[_i].max_y }) };
            _w->sink->shape(_w->sink->self, _l, 2u, false, _radius, _line, (rde_color){ 0, 0, 0, 0 });
        }
    }
    const f64 _px = _n[3] * _k;
    if(_w->sink->text != NULL && _text[0] != 0 && _px >= 3.0 && !fude_zoom_plot_is(_info->id)) {   // (a graph's: its legend, above)
        const u32 _shown = _parts_text ? (_split < _nb ? _split : _nb) : 1u;
        for(u32 _b = 0; _b < _shown; _b++) {
            const fude_zoom_box _box = fude_zoom_sim_box(_all, _boxes[_b]);
            c8 _part[FUDE_ZOOM_SYMBOL_TEXT];
            const u32 _l = _parts_text ? _len[_b] : (u32)rde_strlen(_text);
            rde_memcpy(_part, &_text[_parts_text ? _from[_b] : 0u], _l);
            _part[_l] = 0;
            // A label with marks (rich.h): as the screen has it, in its box's middle.
            if(!_classes && fude_zoom_rich_marked(_part, (u32)rde_strlen(_part)) && _k > 0.0) {
                const u8 _side = _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_RIGHT || _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER ? 0u :
                                 _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT ? 2u : 1u;
                fude_zoom_rich _rich;
                fude_zoom_walk_rich_lay(&_rich, _part, (u32)rde_strlen(_part), (_box.max_x - _box.min_x) / _k, _n[3], _side);
                const f64 _down = _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER ? 0.0 : rde_math_max_f64(((_box.max_y - _box.min_y) - (f64)_rich.h * _k) * 0.5, 0.0);
                fude_zoom_walk_rich_marks(_w, &_rich, _part, (fude_zoom_sim){ _k, 0.0, _box.min_x, _box.max_y - _down }, _n[3], _line);
                fude_zoom_rich_free(&_rich);
                continue;
            }
            // Its lines: its own breaks, and as many words as fit the box (an average letter half its height wide).
            const u32 _most = (u32)rde_math_max_f64(2.0, (_box.max_x - _box.min_x) / (0.5 * _px));
            const c8* _lines[24];
            const u32 _count_l = fude_zoom_walk_symbol_break(_part, _most, _lines);
            if(_count_l == 0u) {
                continue;
            }
            const f64 _tall = _px * 1.3 * (f64)_count_l;
            const f64 _top  = (_classes && _b > 0u) || _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER ? _box.max_y : (_box.min_y + _box.max_y) * 0.5 + _tall * 0.5;
            // (as the screen: a class's members, an area's name from the left; a bracket's against it)
            const b8 _from_left = (_classes && _b > 0u) || _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER || _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_RIGHT;
            const u8 _align = _from_left ? 0u : _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT ? FUDE_ZOOM_EXPORT_TEXT_RIGHT : FUDE_ZOOM_EXPORT_TEXT_CENTRED;
            _w->sink->text(_w->sink->self, (fude_zoom_v2){ _box.min_x, _top }, _px, _box.max_x - _box.min_x, _tall,
                           (u8)(FUDE_ZOOM_TEXT_PLAIN | _align), _line, (rde_color){ 0, 0, 0, 0 },
                           _lines, _count_l);
        }
    }
}

// A symbol (symbol.h): each part a shape (filled as the shape is, or solid in its line's colour), the lines
// between a class's compartments, and its text in its boxes (its lines broken by an average letter's width).
RDE_INTERNAL void fude_zoom_walk_symbol(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to, f64 _size) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_w->s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    const u32 _kind = _count >= 4u ? fude_zoom_symbol_kind_of(_n, _count) : FUDE_ZOOM_NONE;
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL) {
        return;
    }
    const fude_zoom_sim _all    = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
    const f64           _k      = fude_zoom_sim_scale(_all);
    const rde_color     _line   = fude_zoom_walk_resolve(_w, _o->color);
    rde_color           _fill   = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_zoom_walk_resolve(_w, _o->fill) : _line;
    if(!(_o->flags & FUDE_ZOOM_FLAG_FILLED)) {
        _fill.a = 0;
    }
    const f64 _radius = (f64)_o->radius * _o->scale * fude_zoom_sim_scale(_to);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _parts = rde_arr_new(sizeof(fude_zoom_symbol_part), _heap);
    const u32 _np = fude_zoom_symbol_parts(_kind, _n[1], _n[2], fude_zoom_shape_segments(_size), &_pts, &_parts);
    fude_zoom_v2* _p = (fude_zoom_v2*)_pts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_pts); _i++) {
        _p[_i] = fude_zoom_sim_apply(_all, _p[_i]);
    }
    const fude_zoom_symbol_part* _pa = (const fude_zoom_symbol_part*)_parts.memory;
    fude_zoom_walk_symbol_parts(_w, _p, _pa, _np, _line, _fill, _k, _radius);
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
    if(fude_zoom_plot_is(_info->id) && _w->sink->shape != NULL && !_w->sink->cutting) {
        fude_zoom_walk_symbol_plot(_w, _info, _text, _n, _all, _k, _radius, _line, _heap);
    }
    fude_zoom_walk_symbol_text(_w, _info, _text, _n, _all, _k, _radius, _line);
    rde_arr_free(&_pts);
    rde_arr_free(&_parts);
}

// Millimetres one of an object's own units (0: lengths not set to be written).
RDE_INTERNAL f64 fude_zoom_walk_mm(const fude_zoom_walk_state* _w, const fude_zoom_object* _o) {
    const u32 _home = _w->s->home != FUDE_ZOOM_NONE ? _w->s->home : _w->s->root;
    return _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_w->s, _o->frame, _home)) * fude_zoom_export_mm_per_unit;
}

// A line of the screen's from _a to _b, _half_width points each side.
RDE_INTERNAL void fude_zoom_walk_segment(fude_zoom_walk_state* _w, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _half_width, rde_color _c) {
    const fude_zoom_v2 _p[2] = { _a, _b };
    _w->sink->shape(_w->sink->self, _p, 2u, false, _half_width, _c, (rde_color){ 0, 0, 0, 0 });
}

// One line of text, its middle at _c (screen points), _px high, centred. Its width is only guessed (a digit's in
// Helvetica, about what most of these labels are) and kept so: it sizes the box handed on and nothing else, since
// every writer that gets one centres the line on the box's middle by its own measure (SVG's text-anchor, the PDF's
// fonts' widths), and a blade's walk (DXF), whose text is not centred, never reaches here (sheets and dimensions).
RDE_INTERNAL void fude_zoom_walk_word(fude_zoom_walk_state* _w, fude_zoom_v2 _c, f64 _px, const c8* _text, rde_color _ink) {
    if(_w->sink->text == NULL || _text[0] == 0) {
        return;
    }
    u32 _letters = 0;
    for(const c8* _t = _text; *_t != 0; _t++) {
        _letters += ((u8)*_t & 0xC0u) != 0x80u ? 1u : 0u;
    }
    const f64 _tw = 0.556 * _px * (f64)_letters;
    const c8* const _lines[1] = { _text };
    _w->sink->text(_w->sink->self, (fude_zoom_v2){ _c.x - _tw * 0.5, _c.y + _px * 0.59 }, _px, _tw, _px * 1.3,
                   (u8)(FUDE_ZOOM_TEXT_PLAIN | FUDE_ZOOM_EXPORT_TEXT_CENTRED), _ink, (rde_color){ 0, 0, 0, 0 }, _lines, 1u);
}

// A sheet (sheet.h): its outline, its grid's lines, its rulers' ticks and numbers, its unit (and scale) by its
// first corner — as the canvas draws them at this zoom. Not for a blade: the stock, not a cut.
RDE_INTERNAL void fude_zoom_walk_sheet(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_w->s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    fude_zoom_sheet _sheet;
    const f64 _mm = fude_zoom_walk_mm(_w, _o);
    if(_w->sink->cutting || !fude_zoom_sheet_of(_n, _count, &_sheet) || !(_mm > 0.0)) {
        return;
    }
    const fude_zoom_sim _all  = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
    const rde_color     _line = fude_zoom_walk_resolve(_w, _o->color);
    const b8 _inch = fude_zoom_export_style.unit == FUDE_ZOOM_UNIT_IN || fude_zoom_export_style.unit == FUDE_ZOOM_UNIT_FT;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _lines = rde_arr_new(sizeof(fude_zoom_sheet_line), _heap), _labels = rde_arr_new(sizeof(fude_zoom_sheet_label), _heap);
    fude_zoom_sheet_label _unit;
    fude_zoom_sheet_marks(&_sheet, _all, _mm, _inch, (fude_zoom_box){ -_w->half.x, -_w->half.y, _w->half.x, _w->half.y }, &_lines, &_labels, &_unit);
    rde_color _grid = _line, _strong = _line;   // (lighter than on the screen: a print's lines are wider than its hairlines)
    _grid.a   = (u8)((u32)_line.a * 13u / 100u);
    _strong.a = (u8)((u32)_line.a * 28u / 100u);
    const fude_zoom_sheet_line* _l = (const fude_zoom_sheet_line*)_lines.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_lines); _i++) {
        fude_zoom_walk_segment(_w, _l[_i].a, _l[_i].b, _l[_i].weight == 0u ? 0.35 : 0.25, _l[_i].weight == 0u ? _line : _l[_i].weight == 2u ? _strong : _grid);
    }
    const f64 _px = 8.0;
    const fude_zoom_sheet_label* _t = (const fude_zoom_sheet_label*)_labels.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_labels); _i++) {
        const f64 _tw  = 0.556 * _px * (f64)rde_strlen(_t[_i].text);
        const f64 _out = rde_math_abs_f64(_t[_i].in.x) * _tw * 0.5 + rde_math_abs_f64(_t[_i].in.y) * _px * 0.5;
        fude_zoom_walk_word(_w, (fude_zoom_v2){ _t[_i].at.x + _t[_i].in.x * _out, _t[_i].at.y + _t[_i].in.y * _out }, _px, _t[_i].text, _line);
    }
    if(_unit.text[0] != 0) {
        c8 _say[64];
        if(_sheet.scale > 1.0) {
            c8 _sc[24];
            fude_zoom_sheet_number(_sheet.scale, _sc, sizeof(_sc));
            rde_snprintf(_say, sizeof(_say), "%s \xC2\xB7 1:%s", _unit.text, _sc);
        } else {
            rde_snprintf(_say, sizeof(_say), "%s", _unit.text);
        }
        const f64 _tw = 0.556 * 7.0 * (f64)rde_strlen(_say);
        fude_zoom_walk_word(_w, (fude_zoom_v2){ _unit.at.x + _tw * 0.5 - 6.0, _unit.at.y }, 7.0, _say, _strong);
    }
    rde_arr_free(&_lines);
    rde_arr_free(&_labels);
}

// A dimension as the canvas draws it (render_shape.c's fude_zoom_render_dimension): its extension lines, its line
// with an arrow at each end, and its length on it — not for a blade (it shows a size: it is no cut).
RDE_INTERNAL void fude_zoom_walk_dimension(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_w->s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    if(_count < 3u || _w->sink->cutting) {
        return;
    }
    const fude_zoom_sim _all  = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
    const fude_zoom_v2  _a    = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const fude_zoom_v2  _b    = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _n[0], _n[1] });
    const f64           _k    = fude_zoom_sim_scale(_all);
    const f64           _len  = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
    const rde_color     _line = fude_zoom_walk_resolve(_w, _o->color);
    const f64           _hwid = rde_math_max_f64((f64)_o->radius * _k, 0.35);
    if(_len < 1.0) {
        return;
    }
    const fude_zoom_v2 _d      = { (_b.x - _a.x) / _len, (_b.y - _a.y) / _len };
    const fude_zoom_v2 _left   = { -_d.y, _d.x };
    const f64          _offset = _n[2] * _k;
    const f64          _side   = _offset >= 0.0 ? 1.0 : -1.0;
    const fude_zoom_v2 _a2 = { _a.x + _left.x * _offset, _a.y + _left.y * _offset };
    const fude_zoom_v2 _b2 = { _b.x + _left.x * _offset, _b.y + _left.y * _offset };
    fude_zoom_walk_segment(_w, _a2, _b2, _hwid, _line);
    if(rde_math_abs_f64(_offset) > 3.0) {
        const f64 _gap = 3.0 * _side, _over = _offset + 5.0 * _side;
        fude_zoom_walk_segment(_w, (fude_zoom_v2){ _a.x + _left.x * _gap, _a.y + _left.y * _gap }, (fude_zoom_v2){ _a.x + _left.x * _over, _a.y + _left.y * _over }, 0.35, _line);
        fude_zoom_walk_segment(_w, (fude_zoom_v2){ _b.x + _left.x * _gap, _b.y + _left.y * _gap }, (fude_zoom_v2){ _b.x + _left.x * _over, _b.y + _left.y * _over }, 0.35, _line);
    }
    const f64 _al = 9.0, _aw = 3.5;
    const f64 _in = _len >= 2.5 * _al ? 1.0 : -1.0;
    for(u32 _end = 0; _end < 2u; _end++) {
        const fude_zoom_v2 _tip = _end == 0u ? _a2 : _b2;
        const f64          _s   = (_end == 0u ? 1.0 : -1.0) * _in;
        const fude_zoom_v2 _t[3] = {
            _tip,
            { _tip.x + _d.x * _al * _s + _left.x * _aw, _tip.y + _d.y * _al * _s + _left.y * _aw },
            { _tip.x + _d.x * _al * _s - _left.x * _aw, _tip.y + _d.y * _al * _s - _left.y * _aw },
        };
        _w->sink->shape(_w->sink->self, _t, 3u, true, 0.25, _line, _line);
    }
    const f64 _mm = fude_zoom_walk_mm(_w, _o);
    if(_mm > 0.0) {
        c8 _label[FUDE_ZOOM_UNITS_TEXT];
        fude_zoom_units_format(rde_math_hypot_f64(_n[0], _n[1]) * _mm, &fude_zoom_export_style, _label, sizeof(_label));
        const f64 _px = 11.0;
        if(0.556 * _px * (f64)rde_strlen(_label) + 12.0 <= _len || _len >= 40.0) {
            const f64 _lift = 10.0 * _side;
            fude_zoom_walk_word(_w, (fude_zoom_v2){ (_a2.x + _b2.x) * 0.5 + _left.x * _lift, (_a2.y + _b2.y) * 0.5 + _left.y * _lift }, _px, _label, _line);
        }
    }
}

// An arrowhead at _tip along _d (unit, screen), filled.
RDE_INTERNAL void fude_zoom_walk_head(fude_zoom_walk_state* _w, fude_zoom_v2 _tip, fude_zoom_v2 _d, rde_color _c) {
    const f64 _al = 9.0, _aw = 3.5;
    const fude_zoom_v2 _t[3] = { _tip, { _tip.x - _d.x * _al - _d.y * _aw, _tip.y - _d.y * _al + _d.x * _aw }, { _tip.x - _d.x * _al + _d.y * _aw, _tip.y - _d.y * _al - _d.x * _aw } };
    _w->sink->shape(_w->sink->self, _t, 3u, true, 0.25, _c, _c);
}

// A circle's size and an angle's, as the canvas draws them (render_shape.c) — not for a blade.
RDE_INTERNAL void fude_zoom_walk_radial(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_w->s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    if(_count < 3u || _w->sink->cutting) {
        return;
    }
    const fude_zoom_sim _all  = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
    const rde_color     _line = fude_zoom_walk_resolve(_w, _o->color);
    const f64           _k    = fude_zoom_sim_scale(_all);
    if(_o->channels == FUDE_ZOOM_SHAPE_RADIAL) {
        const b8 _across = _n[2] >= 0.5;
        const fude_zoom_v2 _u = { rde_math_cos_f64(_n[1]) * _n[0], rde_math_sin_f64(_n[1]) * _n[0] };
        const fude_zoom_v2 _c = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 }), _e = fude_zoom_sim_apply(_all, _u);
        const fude_zoom_v2 _f = _across ? fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_u.x, -_u.y }) : _c;
        const f64 _len = rde_math_hypot_f64(_e.x - _c.x, _e.y - _c.y);
        if(_len < 2.0) {
            return;
        }
        const fude_zoom_v2 _d = { (_e.x - _c.x) / _len, (_e.y - _c.y) / _len };
        fude_zoom_walk_segment(_w, _f, _e, rde_math_max_f64((f64)_o->radius * _k, 0.35), _line);
        fude_zoom_walk_head(_w, _e, _d, _line);
        if(_across) {
            fude_zoom_walk_head(_w, _f, (fude_zoom_v2){ -_d.x, -_d.y }, _line);
        }
        const f64 _mm = fude_zoom_walk_mm(_w, _o);
        if(_mm > 0.0) {
            c8 _v[FUDE_ZOOM_UNITS_TEXT], _label[FUDE_ZOOM_UNITS_TEXT + 8u];
            fude_zoom_units_format((_across ? 2.0 : 1.0) * _n[0] * _mm, &fude_zoom_export_style, _v, sizeof(_v));
            rde_snprintf(_label, sizeof(_label), _across ? "\xC3\x98 %s" : "R %s", _v);
            const f64 _tw = 0.556 * 11.0 * (f64)rde_strlen(_label);
            const f64 _out = _tw * 0.5 * rde_math_abs_f64(_d.x) + 9.0 * rde_math_abs_f64(_d.y) + 10.0;
            fude_zoom_walk_word(_w, (fude_zoom_v2){ _e.x + _d.x * _out, _e.y + _d.y * _out }, 11.0, _label, _line);
        }
        return;
    }
    // An angle: its arc in short lines, its heads, its degrees.
    const f64 _rad = _n[0] * _k;
    if(_rad < 6.0) {
        return;
    }
    const fude_zoom_v2 _c = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const f64 _from = _n[1] + rde_math_atan2_f64(_all.b, _all.a), _sweep = _n[2];
    const u32 _steps = (u32)rde_math_min_f64(rde_math_max_f64(rde_math_abs_f64(_sweep) * _rad / 4.0, 4.0), 256.0);
    rde_arr_clear(&_w->pts);
    for(u32 _i = 0; _i <= _steps; _i++) {
        const f64 _a = _from + _sweep * (f64)_i / (f64)_steps;
        const fude_zoom_v2 _p = { _c.x + rde_math_cos_f64(_a) * _rad, _c.y + rde_math_sin_f64(_a) * _rad };
        rde_arr_add(&_w->pts, (any)&_p);
    }
    _w->sink->shape(_w->sink->self, (const fude_zoom_v2*)_w->pts.memory, _steps + 1u, false, 0.35, _line, (rde_color){ 0, 0, 0, 0 });
    const f64 _sg = _sweep >= 0.0 ? 1.0 : -1.0;
    if(rde_math_abs_f64(_sweep) * _rad >= 22.0) {
        const f64 _a0 = _from, _a1 = _from + _sweep;
        fude_zoom_walk_head(_w, (fude_zoom_v2){ _c.x + rde_math_cos_f64(_a0) * _rad, _c.y + rde_math_sin_f64(_a0) * _rad }, (fude_zoom_v2){ rde_math_sin_f64(_a0) * _sg, -rde_math_cos_f64(_a0) * _sg }, _line);
        fude_zoom_walk_head(_w, (fude_zoom_v2){ _c.x + rde_math_cos_f64(_a1) * _rad, _c.y + rde_math_sin_f64(_a1) * _rad }, (fude_zoom_v2){ -rde_math_sin_f64(_a1) * _sg, rde_math_cos_f64(_a1) * _sg }, _line);
    }
    c8 _label[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format_angle(rde_math_abs_f64(_sweep) * 180.0 / RDE_PI, 1u, _label, sizeof(_label));
    const f64 _mid = _from + _sweep * 0.5;
    fude_zoom_walk_word(_w, (fude_zoom_v2){ _c.x + rde_math_cos_f64(_mid) * (_rad + 16.0), _c.y + rde_math_sin_f64(_mid) * (_rad + 16.0) }, 11.0, _label, _line);
}

// A shape as the canvas draws it (_size: how big it is on the screen, for its outline's segments). Symbols,
// dimensions, sizes and angles go to their own; a sheet (as stock) is left out of a blade's file, attributes and
// guides out of every file. Else its outline (a cut board's rings), dashed when its style says (not for a blade),
// its hatch, a connector's heads, a sheet's marks, and a board's name.
RDE_INTERNAL void fude_zoom_walk_shape(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to, f64 _size) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    if(_w->sink->shape == NULL) {
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_SYMBOL) {
        fude_zoom_walk_symbol(_w, _object, _to, _size);
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_DIMENSION) {
        fude_zoom_walk_dimension(_w, _object, _to);
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_RADIAL || _o->channels == FUDE_ZOOM_SHAPE_ANGLE) {
        fude_zoom_walk_radial(_w, _object, _to);
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_SHEET && _w->sink->cutting) {
        return;   // (the stock, not a cut)
    }
    if(fude_zoom_shape_is_attribute(_o->channels)) {
        return;
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_GUIDE) {
        return;   // (a guide: drawing help, never printed or cut)
    }
    const b8 _cut = _o->channels == FUDE_ZOOM_SHAPE_BOARD && fude_zoom_walk_cut_board(_w, _object, _to);
    const rde_color _line = fude_zoom_walk_resolve(_w, _o->color);
    if(!_cut) {
        b8 _closed = false;
        rde_arr_clear(&_w->pts);
        fude_zoom_scene_shape_outline(_w->s, _object, fude_zoom_shape_segments(_size), &_w->pts, &_closed);
        const u32 _k = (u32)rde_arr_length(&_w->pts);
        if(_k < 2u) {
            return;
        }
        fude_zoom_v2* _p = (fude_zoom_v2*)_w->pts.memory;
        for(u32 _i = 0; _i < _k; _i++) {
            _p[_i] = fude_zoom_sim_apply(_to, _p[_i]);
        }
        const b8  _filled = _closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED);
        rde_color _fill   = (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? fude_zoom_walk_resolve(_w, _o->fill) : _line;
        if(!_filled) {
            _fill.a = 0;
        }
        f64 _hw = (f64)_o->radius * _o->scale * fude_zoom_sim_scale(_to);
        if(_o->channels == FUDE_ZOOM_SHAPE_SHEET) {
            _hw = rde_math_min_f64(_hw, 0.6);   // (a sheet's edge a fine line, as on the screen)
        }
        const u8  _style = fude_zoom_line_style_of(_o->q);
        if(_style > FUDE_ZOOM_LINE_SOLID && _style < FUDE_ZOOM_LINE_STYLES && !_w->sink->cutting && _o->channels != FUDE_ZOOM_SHAPE_ARROW) {
            // Dashed or a centre line: its fill alone, then its pieces (a blade's file keeps it whole: it
            // is cut all the same).
            if(_fill.a > 0) {
                _w->sink->shape(_w->sink->self, _p, _k, _closed, 0.0, (rde_color){ 0, 0, 0, 0 }, _fill);
            }
            rde_arr _dashes = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
            const u32 _pieces = fude_zoom_line_dashes(_p, _k, _closed, _style, rde_math_max_f64(_hw * 2.0, 0.7), (fude_zoom_box){ -_w->half.x * 2.0, -_w->half.y * 2.0, _w->half.x * 2.0, _w->half.y * 2.0 }, &_dashes);
            for(u32 _i = 0; _i < _pieces; _i++) {
                _w->sink->shape(_w->sink->self, &((const fude_zoom_v2*)_dashes.memory)[2u * _i], 2u, false, _hw, _line, (rde_color){ 0, 0, 0, 0 });
            }
            rde_arr_free(&_dashes);
        } else {
            _w->sink->shape(_w->sink->self, _p, _k, _closed, _hw, _line, _fill);
        }
        // Hatched: its lines across its inside, half its line's width (a blade's file leaves them out: not cuts).
        const u8 _hatch = fude_zoom_hatch_of(_o->q);
        if(_closed && _hatch != FUDE_ZOOM_HATCH_NONE && !_w->sink->cutting && _hw > 0.0) {
            rde_arr _lines = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
            const fude_zoom_box _all_view = { -1e300, -1e300, 1e300, 1e300 };
            fude_zoom_hatch_lines(_p, _k, RDE_PI * 0.25, FUDE_ZOOM_HATCH_APART * 2.0 * _hw, _all_view, &_lines);
            if(_hatch == FUDE_ZOOM_HATCH_CROSS) {
                fude_zoom_hatch_lines(_p, _k, 2.356194490192345, FUDE_ZOOM_HATCH_APART * 2.0 * _hw, _all_view, &_lines);
            }
            for(u32 _i = 0; _i + 1u < (u32)rde_arr_length(&_lines); _i += 2u) {
                _w->sink->shape(_w->sink->self, &((const fude_zoom_v2*)_lines.memory)[_i], 2u, false, _hw * 0.5, _line, (rde_color){ 0, 0, 0, 0 });
            }
            rde_arr_free(&_lines);
        }
        // A connector's heads (symbol.h: its ends' styles), as the canvas draws them.
        f64 _an[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _ac = _o->channels == FUDE_ZOOM_SHAPE_ARROW ? fude_zoom_scene_shape_numbers(_w->s, _object, _an, FUDE_ZOOM_SHAPE_NUMBERS) : 0u;
        const u32 _ak = fude_zoom_arrow_points(_an, _ac);   // (0 for a damaged file's count, its heads none)
        if(_ak >= 2u && _k >= 2u && _ac >= 2u * _ak + 2u) {
            const u32 _heads = fude_zoom_arrow_heads(_an, _ac, _ak);
            const f64 _wd    = rde_math_max_f64((f64)_o->radius * fude_zoom_sim_scale(_to), 0.75);
            const f64 _head  = rde_math_max_f64(_wd * 3.5 + 5.0, 8.0);
            rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
            rde_arr _hp = rde_arr_new(sizeof(fude_zoom_v2), _heap), _hs = rde_arr_new(sizeof(fude_zoom_symbol_part), _heap);
            for(u32 _end = 0; _end < 2u; _end++) {
                if(!(_heads & (_end == 0u ? FUDE_ZOOM_ARROW_END : FUDE_ZOOM_ARROW_START))) {
                    continue;
                }
                const fude_zoom_v2 _tip  = _end == 0u ? _p[_k - 1u] : _p[0];
                const fude_zoom_v2 _from = _end == 0u ? _p[_k - 2u] : _p[1];
                fude_zoom_symbol_head(_end == 0u ? FUDE_ZOOM_ARROW_END_STYLE(_heads) : FUDE_ZOOM_ARROW_START_STYLE(_heads), (_heads & FUDE_ZOOM_ARROW_CIRCLE) != 0u,
                                      _tip, _from, _head, &_hp, &_hs);
                const fude_zoom_v2*          _hq = (const fude_zoom_v2*)_hp.memory;
                const fude_zoom_symbol_part* _hh = (const fude_zoom_symbol_part*)_hs.memory;
                for(u32 _i = 0; _i < (u32)rde_arr_length(&_hs); _i++) {
                    const b8  _solid = (_hh[_i].flags & FUDE_ZOOM_SYMBOL_SOLID) != 0u;
                    const rde_color _hf = _solid ? _line : ((_hh[_i].flags & FUDE_ZOOM_SYMBOL_FILLED) ? _w->theme->page : (rde_color){ 0, 0, 0, 0 });
                    _w->sink->shape(_w->sink->self, &_hq[_hh[_i].first], _hh[_i].count, (_hh[_i].flags & FUDE_ZOOM_SYMBOL_CLOSED) != 0u, _wd, _line, _hf);
                }
            }
            rde_arr_free(&_hp);
            rde_arr_free(&_hs);
        }
    }
    if(_o->channels == FUDE_ZOOM_SHAPE_SHEET) {
        fude_zoom_walk_sheet(_w, _object, _to);
    }
    // A board's name in its middle (what a cut list and the workshop call it).
    if(_o->channels == FUDE_ZOOM_SHAPE_BOARD && _w->sink->text != NULL) {
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _count = fude_zoom_scene_shape_numbers(_w->s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
        c8 _name[FUDE_ZOOM_BOARD_NAME];
        fude_zoom_board_name(_n, _count, _name, sizeof(_name));
        const fude_zoom_sim _all = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
        const f64 _px = rde_math_min_f64(rde_math_min_f64(_n[0], _n[1]) * fude_zoom_sim_scale(_all) * 0.3, 14.0);
        if(_name[0] != 0 && _px >= 3.0) {
            const fude_zoom_v2 _c = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
            u32 _letters = 0;
            for(const c8* _c = _name; *_c != 0; _c++) {
                _letters += ((u8)*_c & 0xC0u) != 0x80u ? 1u : 0u;
            }
            const f64 _tw = 0.5 * _px * (f64)_letters;   // (an average letter's width: an export has no font)
            const c8* const _lines[1] = { _name };
            _w->sink->text(_w->sink->self, (fude_zoom_v2){ _c.x - _tw * 0.5, _c.y + _px * 0.65 }, _px, _tw, _px * 1.3, FUDE_ZOOM_TEXT_PLAIN, _line, _o->fill, _lines, 1u);
        }
    }
}

// A fill (the Fill tool's): its points on the screen, each one's ring (kept in its packed points' time), handed to the
// sink's fill. Fewer than three points: nothing.
RDE_INTERNAL void fude_zoom_walk_fill(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    rde_arr_clear(&_w->q);
    rde_arr_add_n(&_w->q, _o->count);
    if(_o->count < 3u || _w->sink->fill == NULL || !fude_zoom_scene_points(_w->s, _object, (fude_zoom_qpoint*)_w->q.memory)) {
        return;
    }
    const fude_zoom_qpoint* _q = (const fude_zoom_qpoint*)_w->q.memory;
    rde_arr_clear(&_w->pts);
    rde_arr_clear(&_w->rings);
    fude_zoom_v2* _p = rde_arr_add_n(&_w->pts, _o->count);
    u32*          _r = rde_arr_add_n(&_w->rings, _o->count);
    for(u32 _i = 0; _i < _o->count; _i++) {
        _p[_i] = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_q[_i]));
        _r[_i] = _q[_i].time;
    }
    _w->sink->fill(_w->sink->self, _p, _r, _o->count, fude_zoom_walk_resolve(_w, _o->color));
}

// A picture: its file's bytes, its half size and its place on the screen, handed to the sink's image.
RDE_INTERNAL void fude_zoom_walk_image(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    f64       _hw, _hh;
    const u8* _bytes = NULL;
    u32       _size  = 0;
    if(_w->sink->image == NULL || !fude_zoom_scene_image(_w->s, _object, &_hw, &_hh, &_bytes, &_size) || _size < 4u) {
        return;
    }
    _w->sink->image(_w->sink->self, _bytes, _size, _hw, _hh, fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o)));
}

// A text's lines written at most (more are left off).
#define FUDE_ZOOM_EXPORT_LINES 64u
#define FUDE_ZOOM_EXPORT_GLYPH  0.02   // a formula's glyphs' curves no further out than this many points (a PDF zoomed into)

// A set formula's glyphs and rules, its ems on the screen through _em: each glyph its outline filled (a cutter's: each
// contour a path), each rule a box.
RDE_INTERNAL void fude_zoom_walk_tex(fude_zoom_walk_state* _w, const fude_zoom_tex_box* _box, fude_zoom_sim _em, rde_color _ink) {
    fude_zoom_formulas* _f = fude_zoom_export_maths;
    if(_f == NULL || _w->sink->fill == NULL) {
        return;
    }
    const f64           _px   = fude_zoom_sim_scale(_em);
    const f64           _upem = (f64)_f->font.units_per_em;
    const fude_zoom_tex_glyph* _g = (const fude_zoom_tex_glyph*)_box->glyphs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_box->glyphs); _i++) {
        const f64 _gpx = _px * _g[_i].size;
        if(_gpx < FUDE_ZOOM_SVG_SPECK) {
            continue;
        }
        rde_arr_clear(&_w->pts);
        rde_arr_clear(&_w->rings);
        const u32 _n = fude_zoom_formulas_outline(_f, _g[_i].glyph, rde_math_max_f64(FUDE_ZOOM_EXPORT_GLYPH * _upem / _gpx, 0.25), _w->sink->cutting, &_w->pts,
                                                  &_w->rings);
        if(_n < 3u) {
            continue;
        }
        const fude_zoom_sim _to = fude_zoom_sim_compose(_em, (fude_zoom_sim){ _g[_i].size / _upem, 0.0, _g[_i].x, _g[_i].y });
        fude_zoom_v2* _p = (fude_zoom_v2*)_w->pts.memory;
        for(u32 _k = 0; _k < _n; _k++) {
            _p[_k] = fude_zoom_sim_apply(_to, _p[_k]);
        }
        _w->sink->fill(_w->sink->self, _p, (const u32*)_w->rings.memory, _n, _ink);
    }
    const fude_zoom_tex_rule* _r = (const fude_zoom_tex_rule*)_box->rules.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_box->rules); _i++) {
        const fude_zoom_v2 _q[4] = {
            fude_zoom_sim_apply(_em, (fude_zoom_v2){ _r[_i].x, _r[_i].y }),
            fude_zoom_sim_apply(_em, (fude_zoom_v2){ _r[_i].x + _r[_i].w, _r[_i].y }),
            fude_zoom_sim_apply(_em, (fude_zoom_v2){ _r[_i].x + _r[_i].w, _r[_i].y + _r[_i].h }),
            fude_zoom_sim_apply(_em, (fude_zoom_v2){ _r[_i].x, _r[_i].y + _r[_i].h }),
        };
        const u32 _rings[4] = { 0, 0, 0, 0 };
        _w->sink->fill(_w->sink->self, _q, _rings, 4u, _ink);
    }
}

// A formula as the renderer draws it (render_text.c). False: no font to set it with (written as its words, then).
RDE_INTERNAL b8 fude_zoom_walk_formula(fude_zoom_walk_state* _w, const fude_zoom_object* _o, fude_zoom_sim _all, f64 _size, const c8* _text, u32 _len) {
    fude_zoom_formulas*      _f   = fude_zoom_export_maths;
    const fude_zoom_tex_box* _box = _f != NULL ? fude_zoom_formulas_set(_f, _text, _len) : NULL;
    if(_box == NULL) {
        return false;
    }
    const fude_zoom_sim _em = fude_zoom_sim_compose(_all, (fude_zoom_sim){ _size, 0.0, FUDE_ZOOM_FORMULA_PAD * _size,
                                                                          -(FUDE_ZOOM_FORMULA_PAD + _box->height) * _size });
    fude_zoom_walk_tex(_w, _box, _em, fude_zoom_walk_resolve(_w, _o->color));
    return true;
}

// A note's layout (rich.h) written: its marks as lines and boxes, each run of its words a text of its own (bold,
// italic; a link in the links' blue, a task done softer). _at: its units (Y down from its words' top left, its Y
// turned up) onto the screen; _size its letters' size in its units.
RDE_INTERNAL void fude_zoom_walk_rich_marks(fude_zoom_walk_state* _w, const fude_zoom_rich* _rich, const c8* _text, fude_zoom_sim _at, f64 _size, rde_color _ink) {
    const f64 _k  = fude_zoom_sim_scale(_at);
    const f64 _lw = rde_math_max_f64(0.25, _k * _size * 0.035);
    rde_color _soft = _ink, _ground = _ink;
    _soft.a   = (u8)((u32)_ink.a * 35u / 100u);
    _ground.a = (u8)((u32)_ink.a * 10u / 100u);
    const rde_color _none = { 0, 0, 0, 0 };
    // A point of the layout (Y down) onto the screen through _at.
    #define FUDE_ZOOM_WALK_AT(_x, _y) fude_zoom_sim_apply(_at, (fude_zoom_v2){ (f64)(_x), -(f64)(_y) })
    const fude_zoom_rich_mark* _m = (const fude_zoom_rich_mark*)_rich->marks.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_rich->marks) && _w->sink->shape != NULL; _i++) {
        const fude_zoom_rich_mark _q = _m[_i];
        if(_q.kind == FUDE_ZOOM_RICH_MARK_LINE) {
            const fude_zoom_v2 _l[2] = { FUDE_ZOOM_WALK_AT(_q.x0, _q.y0), FUDE_ZOOM_WALK_AT(_q.x1, _q.y1) };
            _w->sink->shape(_w->sink->self, _l, 2u, false, _lw, _q.soft ? _soft : _ink, _none);
            continue;
        }
        const fude_zoom_v2 _box[4] = { FUDE_ZOOM_WALK_AT(_q.x0, _q.y0), FUDE_ZOOM_WALK_AT(_q.x1, _q.y0), FUDE_ZOOM_WALK_AT(_q.x1, _q.y1), FUDE_ZOOM_WALK_AT(_q.x0, _q.y1) };
        if(_q.kind == FUDE_ZOOM_RICH_MARK_GROUND) {
            _w->sink->shape(_w->sink->self, _box, 4u, true, 0.0, _none, _ground);
            continue;
        }
        _w->sink->shape(_w->sink->self, _box, 4u, true, _lw, _ink, _none);
        if(_q.kind == FUDE_ZOOM_RICH_MARK_TICKED) {
            const f32 _bw = _q.x1 - _q.x0, _bh = _q.y1 - _q.y0;
            const fude_zoom_v2 _tick[3] = { FUDE_ZOOM_WALK_AT(_q.x0 + 0.2f * _bw, _q.y0 + 0.52f * _bh), FUDE_ZOOM_WALK_AT(_q.x0 + 0.42f * _bw, _q.y0 + 0.78f * _bh),
                                            FUDE_ZOOM_WALK_AT(_q.x0 + 0.82f * _bw, _q.y0 + 0.22f * _bh) };
            _w->sink->shape(_w->sink->self, _tick, 3u, false, _lw * 1.4, _ink, _none);
        }
    }
    const fude_zoom_rich_run* _run = (const fude_zoom_rich_run*)_rich->runs.memory;
    rde_arr _row = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());   // (a run NUL-ended, whole)
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_rich->runs); _i++) {
        const fude_zoom_rich_run _q = _run[_i];
        const f64 _px = (f64)_q.px * _k;
        rde_arr_resize(&_row, (usize)_q.len + 1u);
        c8* _words = (c8*)_row.memory;
        rde_memcpy(_words, (_q.style & FUDE_ZOOM_RICH_OWN) ? FUDE_ZOOM_RICH_BULLET : &_text[_q.from], _q.len);
        _words[_q.len] = 0;
        rde_color _c = (_q.style & FUDE_ZOOM_RICH_LINK) ? (rde_color){ 70, 130, 230, 255 } : _ink;
        if(_q.style & FUDE_ZOOM_RICH_SOFT) {
            _c.a = (u8)((u32)_c.a * 55u / 100u);
        }
        const u8 _flags = (u8)(((_q.style & FUDE_ZOOM_RICH_BOLD) ? FUDE_ZOOM_EXPORT_TEXT_BOLD : 0u) | ((_q.style & FUDE_ZOOM_RICH_ITALIC) ? FUDE_ZOOM_EXPORT_TEXT_ITALIC : 0u));
        const fude_zoom_v2 _base = FUDE_ZOOM_WALK_AT(_q.x, _q.y);
        const c8* const _line[1] = { _words };
        _w->sink->text(_w->sink->self, (fude_zoom_v2){ _base.x, _base.y + _px * 0.95 }, _px, (f64)_q.w * _k, _px * 1.3, (u8)(FUDE_ZOOM_TEXT_PLAIN | _flags), _c, _none, _line, 1u);
    }
    rde_arr_free(&_row);
    #undef FUDE_ZOOM_WALK_AT
}

// _text (_len bytes) laid out as a note (rich.h): _width wide (0: as long as its lines are), its letters _size, its
// plain lines to the side _align says, measured as the app measures (fude_zoom_export_measure). Freed by the caller.
RDE_INTERNAL void fude_zoom_walk_rich_lay(fude_zoom_rich* _rich, const c8* _text, u32 _len, f64 _width, f64 _size, u8 _align) {
    fude_zoom_rich_init(_rich);
    fude_zoom_rich_lay_aligned(_rich, _text, _len, (f32)_width, (f32)_size, 1.3f, _align,
                               fude_zoom_export_measure_fn != NULL ? fude_zoom_export_measure_fn : fude_zoom_export_guess, fude_zoom_export_measure_self);
}

// A text read as a note (rich.h), as the screen has it: a sticky note's paper, then its marks and words.
RDE_INTERNAL void fude_zoom_walk_rich(fude_zoom_walk_state* _w, const fude_zoom_object* _o, fude_zoom_sim _all, u8 _style, f64 _size, f64 _tw, f64 _th,
                                      const c8* _text, u32 _len) {
    const f64       _k   = fude_zoom_sim_scale(_all);
    const rde_color _ink = fude_zoom_walk_resolve(_w, _o->color);
    const f64       _pad = _style == FUDE_ZOOM_TEXT_STICKY ? _size * 0.6 : 0.0;
    if(_style == FUDE_ZOOM_TEXT_STICKY) {
        _w->sink->text(_w->sink->self, fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 }), _size * _k, _tw * _k, _th * _k, FUDE_ZOOM_TEXT_STICKY, _ink, _o->fill, NULL, 0u);
    }
    fude_zoom_rich _rich;
    fude_zoom_walk_rich_lay(&_rich, _text, _len, _style == FUDE_ZOOM_TEXT_STICKY ? rde_math_max_f64(_tw - 2.0 * _pad, _size) : 0.0, _size, 0u);
    fude_zoom_walk_rich_marks(_w, &_rich, _text, fude_zoom_sim_compose(_all, (fude_zoom_sim){ 1.0, 0.0, _pad, -_pad }), _size, _ink);
    fude_zoom_rich_free(&_rich);
}

// A text: a formula set as the renderer sets it, a note with marks laid out as one, else its lines (a sticky note's
// broken to its width by an average letter's) handed to the sink's text.
RDE_INTERNAL void fude_zoom_walk_text(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to) {
    f64 _size = 0.0, _tw = 0.0, _th = 0.0;
    u8  _style = 0;
    const c8* _text = NULL;
    u32 _len = 0;
    if(_w->sink->text == NULL || !fude_zoom_scene_text(_w->s, _object, &_size, &_tw, &_th, &_style, &_text, &_len)) {
        return;
    }
    const fude_zoom_object* _o   = fude_zoom_scene_object(_w->s, _object);
    const fude_zoom_sim     _all = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
    if(_style == FUDE_ZOOM_TEXT_FORMULA && fude_zoom_walk_formula(_w, _o, _all, _size, _text, _len)) {
        return;
    }
    if((_style == FUDE_ZOOM_TEXT_PLAIN || _style == FUDE_ZOOM_TEXT_STICKY) && fude_zoom_rich_marked(_text, _len)) {
        fude_zoom_walk_rich(_w, _o, _all, _style, _size, _tw, _th, _text, _len);
        return;
    }
    const f64               _k   = fude_zoom_sim_scale(_all);
    const f64               _px  = _size * _k;
    const u32 _most = _style == FUDE_ZOOM_TEXT_STICKY ? (u32)rde_math_max_f64(4.0, (_tw * _k - 1.2 * _px) / (0.5 * _px)) : 100000u;
    rde_arr _copy_arr = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_copy_arr, (usize)_len + 1u);
    c8* _copy = (c8*)_copy_arr.memory;
    rde_memcpy(_copy, _text, _len);
    _copy[_len] = 0;
    const c8* _lines[FUDE_ZOOM_EXPORT_LINES];
    u32 _count = 0;
    c8* _p = _copy;
    while(*_p != 0 && _count < FUDE_ZOOM_EXPORT_LINES) {
        // One line: up to its break, or as many words as fit.
        c8* _start = _p;
        c8* _space = NULL;
        u32 _n = 0;
        while(*_p != 0 && *_p != '\n') {
            if(*_p == ' ') {
                _space = _p;
            }
            if(((u8)*_p & 0xC0u) != 0x80u) {
                _n++;
            }
            if(_n > _most && _space != NULL) {
                _p = _space;
                break;
            }
            _p++;
        }
        const c8 _was = *_p;
        *_p = 0;
        _lines[_count++] = _start;
        if(_was != 0) {
            _p++;
        }
    }
    if(_count == 0) {
        _lines[_count++] = _copy;
    }
    _w->sink->text(_w->sink->self, fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 }), _px, _tw * _k, _th * _k, _style,
                   fude_zoom_walk_resolve(_w, _o->color), _o->fill, _lines, _count);
    rde_arr_free(&_copy_arr);
}

// An object the walk is about to hand on, with the key it is drawn in order by.
typedef struct {
    u64 z;
    u32 object;
} fude_zoom_walk_item;

// rde_sort's order for the walk's items: by their draw key, ascending.
RDE_INTERNAL int fude_zoom_walk_by_z(const any _a, const any _b) {
    const u64 _x = ((const fude_zoom_walk_item*)_a)->z, _y = ((const fude_zoom_walk_item*)_b)->z;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// A frame's things on screen, in order (markers first, as the renderer draws
// them), the frames in it in their places.
RDE_INTERNAL void fude_zoom_walk_frame(fude_zoom_walk_state* _w, u32 _frame, fude_zoom_sim _to, u32 _depth) {
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
    rde_arr _items_arr = rde_arr_new(sizeof(fude_zoom_walk_item), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_items_arr, _n);
    fude_zoom_walk_item* _items = (fude_zoom_walk_item*)_items_arr.memory;   // (sized once: it stays put)
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = ((const u32*)_w->found.memory)[_i];
        _items[_i] = (fude_zoom_walk_item){ fude_zoom_scene_draw_key(_w->s, fude_zoom_scene_object(_w->s, _o)), _o };
    }
    rde_sort(_items, _n, sizeof(_items[0]), fude_zoom_walk_by_z);
    const f64 _scale = fude_zoom_sim_scale(_to);
    // In order, markers too: a highlight over what was there before it, under what came after (as drawn).
    for(u32 _i = 0; _i < _n; _i++) {
        const u32               _object = _items[_i].object;
        const fude_zoom_object* _o      = fude_zoom_scene_object(_w->s, _object);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || (_o->kind != FUDE_ZOOM_KIND_FRAME && fude_zoom_scene_hides(_w->s, _o))) {
            continue;   // (a hidden layer's are left out)
        }
        if(fude_zoom_export_keep != NULL && (_object >= fude_zoom_export_keep_count || fude_zoom_export_keep[_object] == 0u)) {
            continue;   // (exporting what the lasso holds: not this)
        }
        const f64 _size = rde_math_max_f64(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * _scale;
        if(_size < FUDE_ZOOM_SVG_SPECK) {
            continue;
        }
        if(_w->sink->object != NULL && _o->kind != FUDE_ZOOM_KIND_FRAME) {
            _w->sink->object(_w->sink->self, _w->s, _object);
        }
        if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
            fude_zoom_walk_stroke(_w, _object, _to);
        } else if(_o->kind == FUDE_ZOOM_KIND_FILL) {
            fude_zoom_walk_fill(_w, _object, _to);
        } else if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
            fude_zoom_walk_shape(_w, _object, _to, _size);
        } else if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
            fude_zoom_walk_image(_w, _object, _to);
        } else if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
            fude_zoom_walk_text(_w, _object, _to);
        } else if(_o->kind == FUDE_ZOOM_KIND_FRAME && _size >= FUDE_ZOOM_SVG_MIN_PX) {
            const fude_zoom_frame* _c = fude_zoom_scene_frame(_w->s, _o->child);
            if(!_c->removed) {
                // Its own query list: the parent's is still being walked.
                rde_arr _kept = _w->found;
                _w->found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
                fude_zoom_walk_frame(_w, _o->child, fude_zoom_sim_compose(_to, fude_zoom_sim_from_xform(_c->xf)), _depth + 1u);
                rde_arr_free(&_w->found);
                _w->found = _kept;
            }
        }
    }
    rde_arr_free(&_items_arr);
}

// See the header: the walk in the active theme's ink.
void fude_zoom_export_walk(const fude_zoom_scene* _s, fude_zoom_v2 _half, const fude_zoom_export_sink* _sink) {
    fude_zoom_export_walk_as(_s, _half, _sink, NULL);
}

// See the header. The walk starts at most three frames up from the camera's, as long as that frame's place on the
// screen is still exact in a double (its error under FUDE_ZOOM_SVG_EXACT), as the renderer does.
void fude_zoom_export_walk_as(const fude_zoom_scene* _s, fude_zoom_v2 _half, const fude_zoom_export_sink* _sink, const fude_theme* _theme) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_walk_state _w = {
        .s = _s, .sink = _sink, .half = _half, .theme = _theme != NULL ? _theme : fude_theme_active(),
        .found = rde_arr_new(sizeof(u32), _heap), .q = rde_arr_new(sizeof(fude_zoom_qpoint), _heap), .pts = rde_arr_new(sizeof(fude_zoom_v2), _heap),
        .radii = rde_arr_new(sizeof(f64), _heap), .rings = rde_arr_new(sizeof(u32), _heap),
    };
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
        if((rde_math_abs_f64(_up_sim.tx) + rde_math_abs_f64(_up_sim.ty)) * 2.220446049250313e-16 >= FUDE_ZOOM_SVG_EXACT) {
            break;
        }
        _top     = _up;
        _top_sim = _up_sim;
    }
    fude_zoom_walk_frame(&_w, _top, _top_sim, 0u);
    rde_arr_free(&_w.found);
    rde_arr_free(&_w.q);
    rde_arr_free(&_w.pts);
    rde_arr_free(&_w.radii);
    rde_arr_free(&_w.rings);
}
