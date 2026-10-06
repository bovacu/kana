// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/export.h"
#include "zoom/symbol.h"
#include "zoom/shape.h"
#include "zoom/sheet.h"
#include "zoom/fill.h"
#include "zoom/dxf.h"
#include "zoom/pdf.h"
#include "zoom/cut.h"
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

// --- the walk ----------------------------------------------------------------------------------------

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

void fude_zoom_export_only(const u8* _keep, u32 _count) {
    fude_zoom_export_keep       = _keep;
    fude_zoom_export_keep_count = _keep != NULL ? _count : 0u;
}

// Lengths as written (fude_zoom_export_units; 0: not set).
static f64                   fude_zoom_export_mm_per_unit = 0.0;
static fude_zoom_units_style fude_zoom_export_style;

void fude_zoom_export_units(f64 _mm_per_unit, const fude_zoom_units_style* _units) {
    fude_zoom_export_mm_per_unit = _mm_per_unit;
    if(_units != NULL) {
        fude_zoom_export_style = *_units;
    }
}

RDE_INTERNAL rde_color fude_zoom_walk_resolve(const fude_zoom_walk_state* _w, rde_color _c) {
    if(_c.a == 1u && _c.r == 255u && _c.g == 254u && _c.b == 253u) {
        return _w->theme->page;   // (FUDE_THEME_PAGE_FILL: the page's)
    }
    return fude_theme_is_ink(_c) ? _w->theme->ink : _c;
}

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

// A symbol (symbol.h): each part a shape (filled as the shape is, or solid in its line's colour), the lines
// between a class's compartments, and its text in its boxes (its lines broken by an average letter's width).
RDE_INTERNAL void fude_zoom_walk_symbol(fude_zoom_walk_state* _w, u32 _object, fude_zoom_sim _to, f64 _size) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_w->s, _object);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_w->s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    const fude_zoom_symbol_info* _info = _count >= 4u ? fude_zoom_symbol_info_of((u32)_n[0]) : NULL;
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
    const u32 _np = fude_zoom_symbol_parts((u32)_n[0], _n[1], _n[2], fude_zoom_shape_segments(_size), &_pts, &_parts);
    fude_zoom_v2* _p = (fude_zoom_v2*)_pts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_pts); _i++) {
        _p[_i] = fude_zoom_sim_apply(_all, _p[_i]);
    }
    const fude_zoom_symbol_part* _pa = (const fude_zoom_symbol_part*)_parts.memory;
    for(u32 _i = 0; _i < _np && _w->sink->shape != NULL; _i++) {
        const b8  _solid = (_pa[_i].flags & FUDE_ZOOM_SYMBOL_SOLID) != 0u;
        rde_color _f     = _solid ? _line : ((_pa[_i].flags & FUDE_ZOOM_SYMBOL_FILLED) ? _fill : (rde_color){ 0, 0, 0, 0 });
        _w->sink->shape(_w->sink->self, &_p[_pa[_i].first], _pa[_i].count, (_pa[_i].flags & FUDE_ZOOM_SYMBOL_CLOSED) != 0u, _radius, _line, _f);
    }
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
    u32 _from[8], _len[8];
    const u32 _split = fude_zoom_symbol_text_split(_text, _from, _len, 8u);
    fude_zoom_box _boxes[8];
    const b8  _parts_text = _info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS;
    const u32 _nb = fude_zoom_symbol_text_boxes((u32)_n[0], _n[1], _n[2], _parts_text ? _split : 1u, _boxes, 8u);
    if(_parts_text && _info->boxes != 4u && _w->sink->shape != NULL) {
        for(u32 _i = 1; _i < _nb; _i++) {
            const fude_zoom_v2 _l[2] = { fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_n[1], _boxes[_i].max_y }), fude_zoom_sim_apply(_all, (fude_zoom_v2){ _n[1], _boxes[_i].max_y }) };
            _w->sink->shape(_w->sink->self, _l, 2u, false, _radius, _line, (rde_color){ 0, 0, 0, 0 });
        }
    }
    const f64 _px = _n[3] * _k;
    if(_w->sink->text != NULL && _text[0] != 0 && _px >= 3.0) {
        const u32 _shown = _parts_text ? (_split < _nb ? _split : _nb) : 1u;
        for(u32 _b = 0; _b < _shown; _b++) {
            const fude_zoom_box _box = fude_zoom_sim_box(_all, _boxes[_b]);
            c8 _part[FUDE_ZOOM_SYMBOL_TEXT];
            const u32 _l = _parts_text ? _len[_b] : (u32)strlen(_text);
            memcpy(_part, &_text[_parts_text ? _from[_b] : 0u], _l);
            _part[_l] = 0;
            // Its lines: its own breaks, and as many words as fit the box (an average letter half its height wide).
            const u32 _most = (u32)fmax(2.0, (_box.max_x - _box.min_x) / (0.5 * _px));
            const c8* _lines[24];
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
            if(_count_l == 0u) {
                continue;
            }
            const f64 _tall = _px * 1.3 * (f64)_count_l;
            const f64 _top  = (_parts_text && _b > 0u) || _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER ? _box.max_y : (_box.min_y + _box.max_y) * 0.5 + _tall * 0.5;
            const b8 _from_left = (_parts_text && _b > 0u) || _info->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER;   // (as the screen: a class's members, an area's name)
            _w->sink->text(_w->sink->self, (fude_zoom_v2){ _box.min_x, _top }, _px, _box.max_x - _box.min_x, _tall,
                           (u8)(FUDE_ZOOM_TEXT_PLAIN | (_from_left ? 0u : FUDE_ZOOM_EXPORT_TEXT_CENTRED)), _line, (rde_color){ 0, 0, 0, 0 },
                           _lines, _count_l);
        }
    }
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

// One line of text, its middle at _c (screen points), _px high: its width guessed (an export has no font: a
// digit's in Helvetica, about what most are).
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
        const f64 _tw  = 0.556 * _px * (f64)strlen(_t[_i].text);
        const f64 _out = fabs(_t[_i].in.x) * _tw * 0.5 + fabs(_t[_i].in.y) * _px * 0.5;
        fude_zoom_walk_word(_w, (fude_zoom_v2){ _t[_i].at.x + _t[_i].in.x * _out, _t[_i].at.y + _t[_i].in.y * _out }, _px, _t[_i].text, _line);
    }
    if(_unit.text[0] != 0) {
        c8 _say[64];
        if(_sheet.scale > 1.0) {
            c8 _sc[24];
            fude_zoom_sheet_number(_sheet.scale, _sc, sizeof(_sc));
            snprintf(_say, sizeof(_say), "%s \xC2\xB7 1:%s", _unit.text, _sc);
        } else {
            snprintf(_say, sizeof(_say), "%s", _unit.text);
        }
        const f64 _tw = 0.556 * 7.0 * (f64)strlen(_say);
        fude_zoom_walk_word(_w, (fude_zoom_v2){ _unit.at.x + _tw * 0.5 - 6.0, _unit.at.y }, 7.0, _say, _strong);
    }
    rde_arr_free(&_lines);
    rde_arr_free(&_labels);
}

// A dimension as the canvas draws it (render.c's fude_zoom_render_dimension): its extension lines, its line
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
    const f64           _len  = hypot(_b.x - _a.x, _b.y - _a.y);
    const rde_color     _line = fude_zoom_walk_resolve(_w, _o->color);
    const f64           _hwid = fmax((f64)_o->radius * _k, 0.35);
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
    if(fabs(_offset) > 3.0) {
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
        fude_zoom_units_format(hypot(_n[0], _n[1]) * _mm, &fude_zoom_export_style, _label, sizeof(_label));
        const f64 _px = 11.0;
        if(0.556 * _px * (f64)strlen(_label) + 12.0 <= _len || _len >= 40.0) {
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

// A circle's size and an angle's, as the canvas draws them (render.c) — not for a blade.
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
        const fude_zoom_v2 _u = { cos(_n[1]) * _n[0], sin(_n[1]) * _n[0] };
        const fude_zoom_v2 _c = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 }), _e = fude_zoom_sim_apply(_all, _u);
        const fude_zoom_v2 _f = _across ? fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_u.x, -_u.y }) : _c;
        const f64 _len = hypot(_e.x - _c.x, _e.y - _c.y);
        if(_len < 2.0) {
            return;
        }
        const fude_zoom_v2 _d = { (_e.x - _c.x) / _len, (_e.y - _c.y) / _len };
        fude_zoom_walk_segment(_w, _f, _e, fmax((f64)_o->radius * _k, 0.35), _line);
        fude_zoom_walk_head(_w, _e, _d, _line);
        if(_across) {
            fude_zoom_walk_head(_w, _f, (fude_zoom_v2){ -_d.x, -_d.y }, _line);
        }
        const f64 _mm = fude_zoom_walk_mm(_w, _o);
        if(_mm > 0.0) {
            c8 _v[FUDE_ZOOM_UNITS_TEXT], _label[FUDE_ZOOM_UNITS_TEXT + 8u];
            fude_zoom_units_format((_across ? 2.0 : 1.0) * _n[0] * _mm, &fude_zoom_export_style, _v, sizeof(_v));
            snprintf(_label, sizeof(_label), _across ? "\xC3\x98 %s" : "R %s", _v);
            const f64 _tw = 0.556 * 11.0 * (f64)strlen(_label);
            const f64 _out = _tw * 0.5 * fabs(_d.x) + 9.0 * fabs(_d.y) + 10.0;
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
    const f64 _from = _n[1] + atan2(_all.b, _all.a), _sweep = _n[2];
    const u32 _steps = (u32)fmin(fmax(fabs(_sweep) * _rad / 4.0, 4.0), 256.0);
    rde_arr_clear(&_w->pts);
    for(u32 _i = 0; _i <= _steps; _i++) {
        const f64 _a = _from + _sweep * (f64)_i / (f64)_steps;
        const fude_zoom_v2 _p = { _c.x + cos(_a) * _rad, _c.y + sin(_a) * _rad };
        rde_arr_add(&_w->pts, (any)&_p);
    }
    _w->sink->shape(_w->sink->self, (const fude_zoom_v2*)_w->pts.memory, _steps + 1u, false, 0.35, _line, (rde_color){ 0, 0, 0, 0 });
    const f64 _sg = _sweep >= 0.0 ? 1.0 : -1.0;
    if(fabs(_sweep) * _rad >= 22.0) {
        const f64 _a0 = _from, _a1 = _from + _sweep;
        fude_zoom_walk_head(_w, (fude_zoom_v2){ _c.x + cos(_a0) * _rad, _c.y + sin(_a0) * _rad }, (fude_zoom_v2){ sin(_a0) * _sg, -cos(_a0) * _sg }, _line);
        fude_zoom_walk_head(_w, (fude_zoom_v2){ _c.x + cos(_a1) * _rad, _c.y + sin(_a1) * _rad }, (fude_zoom_v2){ -sin(_a1) * _sg, cos(_a1) * _sg }, _line);
    }
    c8 _label[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format_angle(fabs(_sweep) * 180.0 / 3.14159265358979323846, 1u, _label, sizeof(_label));
    const f64 _mid = _from + _sweep * 0.5;
    fude_zoom_walk_word(_w, (fude_zoom_v2){ _c.x + cos(_mid) * (_rad + 16.0), _c.y + sin(_mid) * (_rad + 16.0) }, 11.0, _label, _line);
}

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
            _hw = fmin(_hw, 0.6);   // (a sheet's edge a fine line, as on the screen)
        }
        const u8  _style = (u8)_o->q;
        if(_style > FUDE_ZOOM_LINE_SOLID && _style < FUDE_ZOOM_LINE_STYLES && !_w->sink->cutting && _o->channels != FUDE_ZOOM_SHAPE_ARROW) {
            // Dashed or a centre line: its fill alone, then its pieces (a blade's file keeps it whole: it is cut all the same).
            if(_fill.a > 0) {
                _w->sink->shape(_w->sink->self, _p, _k, _closed, 0.0, (rde_color){ 0, 0, 0, 0 }, _fill);
            }
            rde_arr _dashes = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
            const u32 _pieces = fude_zoom_line_dashes(_p, _k, _closed, _style, fmax(_hw * 2.0, 0.7), (fude_zoom_box){ -_w->half.x * 2.0, -_w->half.y * 2.0, _w->half.x * 2.0, _w->half.y * 2.0 }, &_dashes);
            for(u32 _i = 0; _i < _pieces; _i++) {
                _w->sink->shape(_w->sink->self, &((const fude_zoom_v2*)_dashes.memory)[2u * _i], 2u, false, _hw, _line, (rde_color){ 0, 0, 0, 0 });
            }
            rde_arr_free(&_dashes);
        } else {
            _w->sink->shape(_w->sink->self, _p, _k, _closed, _hw, _line, _fill);
        }
        // A connector's heads (symbol.h: its ends' styles), as the canvas draws them.
        f64 _an[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _ac = _o->channels == FUDE_ZOOM_SHAPE_ARROW ? fude_zoom_scene_shape_numbers(_w->s, _object, _an, FUDE_ZOOM_SHAPE_NUMBERS) : 0u;
        const u32 _ak = _ac >= 1u ? (u32)_an[0] : 0u;
        if(_ak >= 2u && _k >= 2u && _ac >= 2u * _ak + 2u) {
            const u32 _heads = (u32)_an[1u + 2u * _ak];
            const f64 _wd    = fmax((f64)_o->radius * fude_zoom_sim_scale(_to), 0.75);
            const f64 _head  = fmax(_wd * 3.5 + 5.0, 8.0);
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
        const f64 _px = fmin(fmin(_n[0], _n[1]) * fude_zoom_sim_scale(_all) * 0.3, 14.0);
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

#define FUDE_ZOOM_EXPORT_LINES 64u

// A text's lines: its own breaks, and a note's words wrapped to its width — by
// an average letter's width (an export has no font to measure with).
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
    const f64               _k   = fude_zoom_sim_scale(_all);
    const f64               _px  = _size * _k;
    const u32 _most = _style == FUDE_ZOOM_TEXT_STICKY ? (u32)fmax(4.0, (_tw * _k - 1.2 * _px) / (0.5 * _px)) : 100000u;
    c8* _copy = (c8*)malloc((usize)_len + 1u);
    if(_copy == NULL) {
        return;
    }
    memcpy(_copy, _text, _len);
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
    free(_copy);
}

typedef struct {
    u64 z;
    u32 object;
} fude_zoom_walk_item;

RDE_INTERNAL int fude_zoom_walk_by_z(const void* _a, const void* _b) {
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
    fude_zoom_walk_item* _items = (fude_zoom_walk_item*)malloc((usize)_n * sizeof(fude_zoom_walk_item));
    if(_items == NULL) {
        return;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = ((const u32*)_w->found.memory)[_i];
        _items[_i] = (fude_zoom_walk_item){ fude_zoom_scene_draw_key(_w->s, fude_zoom_scene_object(_w->s, _o)), _o };
    }
    qsort(_items, _n, sizeof(_items[0]), fude_zoom_walk_by_z);
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
        const f64 _size = fmax(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * _scale;
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
    free(_items);
}

void fude_zoom_export_walk(const fude_zoom_scene* _s, fude_zoom_v2 _half, const fude_zoom_export_sink* _sink) {
    fude_zoom_export_walk_as(_s, _half, _sink, NULL);
}

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
        if((fabs(_up_sim.tx) + fabs(_up_sim.ty)) * 2.220446049250313e-16 >= FUDE_ZOOM_SVG_EXACT) {
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

// A stroke's width followed in runs: where each run ends (the next starts
// there), from _start. One run for an even pen.
RDE_INTERNAL u32 fude_zoom_export_run_end(const f64* _r, u32 _n, u32 _start) {
    const f64 _w0 = fmax(_r[_start], 0.5);
    u32 _end = _start + 1u;
    while(_end + 1u < _n && fabs(fmax(_r[_end], 0.5) - _w0) <= _w0 * FUDE_ZOOM_SVG_RUN) {
        _end++;
    }
    return _end;
}

// --- SVG ---------------------------------------------------------------------------------------------

typedef struct {
    fude_bytes*  out;
    fude_zoom_v2 half;
    u32          masks;      // fills' masks made so far (their ids)
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

// A colour as SVG wants it: "#rrggbb".
RDE_INTERNAL void fude_zoom_svg_color(c8* _out, usize _size, rde_color _c) {
    snprintf(_out, _size, "#%02x%02x%02x", _c.r, _c.g, _c.b);
}

// A point of the screen (centre origin, Y up) where the SVG has it (top-left, Y down).
RDE_INTERNAL void fude_zoom_svg_point(fude_zoom_svg* _w, const c8* _cmd, fude_zoom_v2 _p) {
    fude_zoom_svg_put(_w, "%s%.2f %.2f", _cmd, _p.x + _w->half.x, _w->half.y - _p.y);
}

// A stroke: each run its own path, their round ends meeting where they join.
RDE_INTERNAL void fude_zoom_svg_stroke(void* _self, const fude_zoom_v2* _p, const f64* _r, u32 _n, rde_color _c) {
    fude_zoom_svg* _w = (fude_zoom_svg*)_self;
    c8 _col[16];
    fude_zoom_svg_color(_col, sizeof _col, _c);
    c8 _alpha[32] = "";
    if(_c.a < 255u) {
        snprintf(_alpha, sizeof _alpha, " stroke-opacity=\"%.3f\"", (f64)_c.a / 255.0);
    }
    if(_n == 1u) {
        fude_zoom_svg_put(_w, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"%s\"%s/>\n", _p[0].x + _w->half.x, _w->half.y - _p[0].y, fmax(_r[0], 0.5), _col,
                          _c.a < 255u ? " fill-opacity=\"0.5\"" : "");
        return;
    }
    for(u32 _start = 0; _start + 1u < _n;) {
        const u32 _end = fude_zoom_export_run_end(_r, _n, _start);
        fude_zoom_svg_put(_w, "<path d=\"");
        for(u32 _i = _start; _i <= _end; _i++) {
            fude_zoom_svg_point(_w, _i == _start ? "M" : " L", _p[_i]);
        }
        fude_zoom_svg_put(_w, "\" fill=\"none\" stroke=\"%s\"%s stroke-width=\"%.2f\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n",
                          _col, _alpha, fmax(_r[_start], 0.5) * 2.0);
        _start = _end;
    }
}

RDE_INTERNAL void fude_zoom_svg_shape(void* _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill) {
    fude_zoom_svg* _w = (fude_zoom_svg*)_self;
    c8 _lc[16], _fc[16];
    fude_zoom_svg_color(_lc, sizeof _lc, _line);
    fude_zoom_svg_color(_fc, sizeof _fc, _fill);
    fude_zoom_svg_put(_w, "<path d=\"");
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_svg_point(_w, _i == 0 ? "M" : " L", _p[_i]);
    }
    fude_zoom_svg_put(_w, "%s\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n",
                      _closed ? " Z" : "", _fill.a > 0 ? _fc : "none", _lc, fmax(_radius, 0.5) * 2.0);
}

// A fill: its outline (even-odd), what the eraser cut from it masked out.
RDE_INTERNAL void fude_zoom_svg_fill(void* _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _c) {
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
        snprintf(_mask, sizeof _mask, " mask=\"url(#cut%u)\"", _id);
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
    fude_zoom_svg_put(_w, " Z\" fill=\"%s\" fill-rule=\"evenodd\"%s%s/>\n", _col, _mask, _c.a < 255u ? " fill-opacity=\"0.5\"" : "");
}

// A picture: itself, inside, placed by its transform (the SVG's Y runs down).
RDE_INTERNAL void fude_zoom_svg_image(void* _self, const u8* _bytes, u32 _size, f64 _hw, f64 _hh, fude_zoom_sim _m) {
    fude_zoom_svg* _w = (fude_zoom_svg*)_self;
    const b8    _png = _bytes[0] == 0x89 && _bytes[1] == 'P';
    const usize _len = rde_base64_encoded_size(_size);
    c8* _b64 = (c8*)malloc(_len + 1u);
    if(_b64 == NULL) {
        return;
    }
    const usize _n = rde_base64_encode(_bytes, _size, _b64, _len + 1u);
    fude_zoom_svg_put(_w, "<image x=\"%.4f\" y=\"%.4f\" width=\"%.4f\" height=\"%.4f\" preserveAspectRatio=\"none\" transform=\"matrix(%.6f %.6f %.6f %.6f %.2f %.2f)\" href=\"data:image/%s;base64,",
                      -_hw, -_hh, _hw * 2.0, _hh * 2.0, _m.a, -_m.b, _m.b, _m.a, _m.tx + _w->half.x, _w->half.y - _m.ty, _png ? "png" : "jpeg");
    fude_put_data(_w->out, _b64, (u32)_n);
    fude_zoom_svg_put(_w, "\"/>\n");
    free(_b64);
}

// A text: a note's paper, then each line, its characters escaped for XML.
RDE_INTERNAL void fude_zoom_svg_text(void* _self, fude_zoom_v2 _tl, f64 _px, f64 _w, f64 _h, u8 _style, rde_color _ink, rde_color _paper, const c8* const* _lines, u32 _count) {
    fude_zoom_svg* _sw = (fude_zoom_svg*)_self;
    c8 _col[16];
    const b8  _centred = (_style & FUDE_ZOOM_EXPORT_TEXT_CENTRED) != 0u;
    _style = (u8)(_style & ~FUDE_ZOOM_EXPORT_TEXT_CENTRED);
    const f64 _pad = _style == FUDE_ZOOM_TEXT_STICKY ? 0.6 * _px : 0.0;
    if(_style == FUDE_ZOOM_TEXT_STICKY) {
        fude_zoom_svg_color(_col, sizeof _col, _paper);
        fude_zoom_svg_put(_sw, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"%s\"/>\n", _tl.x + _sw->half.x, _sw->half.y - _tl.y, _w, _h, _col);
    }
    fude_zoom_svg_color(_col, sizeof _col, _ink);
    for(u32 _i = 0; _i < _count; _i++) {
        fude_zoom_svg_put(_sw, "<text x=\"%.2f\" y=\"%.2f\"%s font-family=\"sans-serif\" font-size=\"%.2f\" fill=\"%s\">",
                          (_centred ? _tl.x + _w * 0.5 : _tl.x + _pad) + _sw->half.x, _sw->half.y - _tl.y + _pad + _px * 0.95 + (f64)_i * _px * 1.3,
                          _centred ? " text-anchor=\"middle\"" : "", _px, _col);
        for(const c8* _c = _lines[_i]; *_c != 0; _c++) {
            if(*_c == '<')      { fude_zoom_svg_put(_sw, "&lt;"); }
            else if(*_c == '>') { fude_zoom_svg_put(_sw, "&gt;"); }
            else if(*_c == '&') { fude_zoom_svg_put(_sw, "&amp;"); }
            else                { fude_put_data(_sw->out, _c, 1u); }
        }
        fude_zoom_svg_put(_sw, "</text>\n");
    }
}

b8 fude_zoom_export_svg(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out) {
    fude_zoom_svg _w = { .out = _out, .half = _half };
    c8 _bg[16];
    fude_zoom_svg_color(_bg, sizeof _bg, _paper);
    fude_zoom_svg_put(&_w, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fude_zoom_svg_put(&_w, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.0f\" height=\"%.0f\" viewBox=\"0 0 %.0f %.0f\">\n",
                      _half.x * 2.0, _half.y * 2.0, _half.x * 2.0, _half.y * 2.0);
    fude_zoom_svg_put(&_w, "<rect width=\"100%%\" height=\"100%%\" fill=\"%s\"/>\n", _bg);
    const fude_zoom_export_sink _sink = { &_w, fude_zoom_svg_stroke, fude_zoom_svg_shape, fude_zoom_svg_fill, fude_zoom_svg_image, fude_zoom_svg_text };
    fude_zoom_export_walk(_s, _half, &_sink);
    fude_zoom_svg_put(&_w, "</svg>\n");
    return fude_bytes_size(_out) > 0;
}

// --- PDF ---------------------------------------------------------------------------------------------

typedef struct {
    fude_zoom_pdf* pdf;
    fude_zoom_v2   half;       // the page's middle (the screen's centre)
    rde_arr TYPE(f64)          xy;
    rde_arr TYPE(fude_zoom_v2) tris;
    struct { const u8* bytes; u32 id; } images[64];   // pictures put in once (copies share their bytes)
    u32            image_count;
} fude_zoom_pdfw;

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

RDE_INTERNAL void fude_zoom_pdfw_stroke(void* _self, const fude_zoom_v2* _p, const f64* _r, u32 _n, rde_color _c) {
    fude_zoom_pdfw* _w = (fude_zoom_pdfw*)_self;
    if(_n == 1u) {
        fude_zoom_pdf_fill_color(_w->pdf, _c);
        fude_zoom_pdfw_dot(_w, _p[0], fmax(_r[0], 0.5));
        return;
    }
    fude_zoom_pdf_stroke_color(_w->pdf, _c);
    fude_zoom_pdf_line_style(_w->pdf, 1u, 1u);
    for(u32 _start = 0; _start + 1u < _n;) {
        const u32 _end = fude_zoom_export_run_end(_r, _n, _start);
        fude_zoom_pdf_line_width(_w->pdf, fmax(_r[_start], 0.5) * 2.0);
        fude_zoom_pdfw_path(_w, &_p[_start], _end - _start + 1u, false);
        fude_zoom_pdf_stroke(_w->pdf);
        _start = _end;
    }
}

RDE_INTERNAL void fude_zoom_pdfw_shape(void* _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill) {
    fude_zoom_pdfw* _w = (fude_zoom_pdfw*)_self;
    fude_zoom_pdf_stroke_color(_w->pdf, _line);
    fude_zoom_pdf_line_style(_w->pdf, 1u, 1u);
    fude_zoom_pdf_line_width(_w->pdf, fmax(_radius, 0.5) * 2.0);
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
RDE_INTERNAL void fude_zoom_pdfw_fill(void* _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _c) {
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

// A picture: a JPEG as it is; a PNG read into pixels (its transparency kept).
// Drawn opaque (PDF paints pictures through the fill's alpha).
RDE_INTERNAL void fude_zoom_pdfw_image(void* _self, const u8* _bytes, u32 _size, f64 _hw, f64 _hh, fude_zoom_sim _m) {
    fude_zoom_pdfw* _w = (fude_zoom_pdfw*)_self;
    u32 _id = 0;
    for(u32 _i = 0; _i < _w->image_count; _i++) {
        if(_w->images[_i].bytes == _bytes) {
            _id = _w->images[_i].id;
        }
    }
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
        if(_w->image_count < 64u) {
            _w->images[_w->image_count].bytes = _bytes;
            _w->images[_w->image_count].id    = _id;
            _w->image_count++;
        }
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

// A text: a note's paper, then each line in Helvetica (the PDF's own font: near enough to the app's).
RDE_INTERNAL void fude_zoom_pdfw_text(void* _self, fude_zoom_v2 _tl, f64 _px, f64 _w, f64 _h, u8 _style, rde_color _ink, rde_color _paper, const c8* const* _lines, u32 _count) {
    fude_zoom_pdfw* _pw = (fude_zoom_pdfw*)_self;
    const b8  _centred = (_style & FUDE_ZOOM_EXPORT_TEXT_CENTRED) != 0u;
    _style = (u8)(_style & ~FUDE_ZOOM_EXPORT_TEXT_CENTRED);
    const f64 _x = _tl.x + _pw->half.x, _y = _tl.y + _pw->half.y;
    const f64 _pad = _style == FUDE_ZOOM_TEXT_STICKY ? 0.6 * _px : 0.0;
    if(_style == FUDE_ZOOM_TEXT_STICKY) {
        fude_zoom_pdf_fill_color(_pw->pdf, _paper);
        fude_zoom_pdf_rect(_pw->pdf, _x, _y - _h, _w, _h);
        fude_zoom_pdf_fill(_pw->pdf, false);
    }
    for(u32 _i = 0; _i < _count; _i++) {
        const f64 _lx = _centred ? _x + (_w - fude_zoom_pdf_text_width(_px, _lines[_i])) * 0.5 : _x + _pad;
        fude_zoom_pdf_text(_pw->pdf, _lx, _y - _pad - _px * 0.95 - (f64)_i * _px * 1.3, _px, _ink, _lines[_i]);
    }
}

b8 fude_zoom_export_pdf(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out) {
    fude_zoom_pdfw _w = {
        .pdf = fude_zoom_pdf_new(), .half = _half,
        .xy = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std()), .tris = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std()),
    };
    if(_w.pdf == NULL) {
        rde_arr_free(&_w.xy);
        rde_arr_free(&_w.tris);
        return false;
    }
    // A page the size of the screen, a point a point; the paper behind.
    fude_zoom_pdf_page_begin(_w.pdf, _half.x * 2.0, _half.y * 2.0);
    fude_zoom_pdf_fill_color(_w.pdf, _paper);
    fude_zoom_pdf_rect(_w.pdf, 0.0, 0.0, _half.x * 2.0, _half.y * 2.0);
    fude_zoom_pdf_fill(_w.pdf, false);
    const fude_zoom_export_sink _sink = { &_w, fude_zoom_pdfw_stroke, fude_zoom_pdfw_shape, fude_zoom_pdfw_fill, fude_zoom_pdfw_image, fude_zoom_pdfw_text };
    fude_zoom_export_walk(_s, _half, &_sink);
    fude_zoom_pdf_page_end(_w.pdf);
    const b8 _ok = fude_zoom_pdf_finish(_w.pdf, _out);
    fude_zoom_pdf_free(_w.pdf);
    rde_arr_free(&_w.xy);
    rde_arr_free(&_w.tris);
    return _ok;
}

b8 fude_zoom_export_pdf_views(fude_zoom_scene* _s, rde_color _paper, const fude_zoom_camera* _views, const fude_zoom_v2* _halves, u32 _n, fude_bytes* _out) {
    if(_n == 0u) {
        return false;
    }
    fude_zoom_pdfw _w = {
        .pdf = fude_zoom_pdf_new(), .half = _halves[0],
        .xy = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std()), .tris = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std()),
    };
    if(_w.pdf == NULL) {
        rde_arr_free(&_w.xy);
        rde_arr_free(&_w.tris);
        return false;
    }
    const fude_zoom_camera _was = _s->camera;
    const fude_zoom_export_sink _sink = { &_w, fude_zoom_pdfw_stroke, fude_zoom_pdfw_shape, fude_zoom_pdfw_fill, fude_zoom_pdfw_image, fude_zoom_pdfw_text };
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
    fude_zoom_pdf_free(_w.pdf);
    rde_arr_free(&_w.xy);
    rde_arr_free(&_w.tris);
    return _ok;
}

b8 fude_zoom_export_pdf_pages(fude_zoom_scene* _s, const fude_zoom_camera* _views, const fude_zoom_v2* _halves, u32 _n, fude_zoom_v2 _printer, fude_bytes* _out) {
    if(_n == 0u) {
        return false;
    }
    fude_zoom_pdfw _w = {
        .pdf = fude_zoom_pdf_new(), .half = _halves[0],
        .xy = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std()), .tris = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std()),
    };
    if(_w.pdf == NULL) {
        rde_arr_free(&_w.xy);
        rde_arr_free(&_w.tris);
        return false;
    }
    const fude_theme* _paper_theme = fude_theme_get(FUDE_THEME_PAPER);
    const fude_zoom_camera _was = _s->camera;
    const fude_zoom_export_sink _sink = { &_w, fude_zoom_pdfw_stroke, fude_zoom_pdfw_shape, fude_zoom_pdfw_fill, fude_zoom_pdfw_image, fude_zoom_pdfw_text };
    // The printer's own error undone (as the tiles' are): everything drawn smaller (or larger) by what it prints too large.
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
        fude_zoom_export_walk_as(_s, _w.half, &_sink, _paper_theme);
        fude_zoom_pdf_restore(_w.pdf);
        fude_zoom_pdf_page_end(_w.pdf);
    }
    _s->camera = _was;
    const b8 _ok = fude_zoom_pdf_finish(_w.pdf, _out);
    fude_zoom_pdf_free(_w.pdf);
    rde_arr_free(&_w.xy);
    rde_arr_free(&_w.tris);
    return _ok;
}

// --- DXF -------------------------------------------------------------------------------------------

typedef struct {
    fude_zoom_dxf* dxf;
    f64            mm;          // millimetres a screen point
    rde_arr TYPE(f64) xy;
    struct { rde_color c; u32 layer; } layers[32];
    u32            layer_count;
} fude_zoom_cut;

// AutoCAD's colour nearest a colour: red, yellow, green, cyan, blue, magenta,
// black (7, shown white on a dark screen), the greys, orange.
RDE_INTERNAL u8 fude_zoom_cut_aci(rde_color _c) {
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
RDE_INTERNAL u32 fude_zoom_cut_layer(fude_zoom_cut* _w, rde_color _c) {
    for(u32 _i = 0; _i < _w->layer_count; _i++) {
        if(_w->layers[_i].c.r == _c.r && _w->layers[_i].c.g == _c.g && _w->layers[_i].c.b == _c.b) {
            return _w->layers[_i].layer;
        }
    }
    c8 _name[24];
    snprintf(_name, sizeof _name, "INK_%02X%02X%02X", _c.r, _c.g, _c.b);
    const u32 _layer = fude_zoom_dxf_layer(_w->dxf, _name, fude_zoom_cut_aci(_c));
    if(_w->layer_count < 32u) {
        _w->layers[_w->layer_count].c     = _c;
        _w->layers[_w->layer_count].layer = _layer;
        _w->layer_count++;
    }
    return _layer;
}

RDE_INTERNAL void fude_zoom_cut_path(fude_zoom_cut* _w, u32 _layer, const fude_zoom_v2* _p, u32 _n, b8 _closed) {
    rde_arr_clear(&_w->xy);
    f64* _xy = rde_arr_add_n(&_w->xy, _n * 2u);
    for(u32 _i = 0; _i < _n; _i++) {
        _xy[_i * 2u]      = _p[_i].x * _w->mm;
        _xy[_i * 2u + 1u] = _p[_i].y * _w->mm;
    }
    fude_zoom_dxf_polyline(_w->dxf, _layer, _xy, _n, _closed);
}

// A stroke: its centreline (where a blade or a beam goes).
RDE_INTERNAL void fude_zoom_cut_stroke(void* _self, const fude_zoom_v2* _p, const f64* _r, u32 _n, rde_color _c) {
    RDE_UNUSED(_r);
    fude_zoom_cut* _w = (fude_zoom_cut*)_self;
    if(_n >= 2u) {
        fude_zoom_cut_path(_w, fude_zoom_cut_layer(_w, _c), _p, _n, false);
    }
}

RDE_INTERNAL void fude_zoom_cut_shape(void* _self, const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _radius, rde_color _line, rde_color _fill) {
    RDE_UNUSED(_radius);
    RDE_UNUSED(_fill);
    fude_zoom_cut* _w = (fude_zoom_cut*)_self;
    fude_zoom_cut_path(_w, fude_zoom_cut_layer(_w, _line), _p, _n, _closed);
}

// A fill: its outline, and each cut the eraser made in it, closed paths of their own.
RDE_INTERNAL void fude_zoom_cut_fill(void* _self, const fude_zoom_v2* _p, const u32* _rings, u32 _n, rde_color _c) {
    fude_zoom_cut* _w     = (fude_zoom_cut*)_self;
    const u32      _layer = fude_zoom_cut_layer(_w, _c);
    for(u32 _from = 0; _from < _n;) {
        u32 _to = _from;
        while(_to < _n && _rings[_to] == _rings[_from]) {
            _to++;
        }
        if(_to - _from >= 3u) {
            fude_zoom_cut_path(_w, _layer, &_p[_from], _to - _from, true);
        }
        _from = _to;
    }
}

// A text: each line a TEXT (for engraving a label), its capitals about seven tenths of its letters' height.
RDE_INTERNAL void fude_zoom_cut_text(void* _self, fude_zoom_v2 _tl, f64 _px, f64 _w, f64 _h, u8 _style, rde_color _ink, rde_color _paper, const c8* const* _lines, u32 _count) {
    RDE_UNUSED(_w); RDE_UNUSED(_h); RDE_UNUSED(_paper);
    fude_zoom_cut* _cw    = (fude_zoom_cut*)_self;
    const u32      _layer = fude_zoom_cut_layer(_cw, _ink);
    const f64      _pad   = (_style & ~FUDE_ZOOM_EXPORT_TEXT_CENTRED) == FUDE_ZOOM_TEXT_STICKY ? 0.6 * _px : 0.0;
    for(u32 _i = 0; _i < _count; _i++) {
        fude_zoom_dxf_text(_cw->dxf, _layer, (_tl.x + _pad) * _cw->mm, (_tl.y - _pad - _px * 0.95 - (f64)_i * _px * 1.3) * _cw->mm, _px * 0.7 * _cw->mm, 0.0, _lines[_i]);
    }
}

b8 fude_zoom_export_dxf(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, fude_bytes* _out, fude_zoom_export_check* _check) {
    fude_zoom_cut _w = { .dxf = fude_zoom_dxf_new(), .mm = _mm_per_point, .xy = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std()) };
    if(_w.dxf == NULL) {
        rde_arr_free(&_w.xy);
        return false;
    }
    const fude_zoom_export_sink _sink = { &_w, fude_zoom_cut_stroke, fude_zoom_cut_shape, fude_zoom_cut_fill, NULL, fude_zoom_cut_text, true };
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

b8 fude_zoom_export_tiles(const fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _mm_per_point, f64 _page_w_mm, f64 _page_h_mm, fude_zoom_v2 _printer, fude_bytes* _out, u32* _pages) {
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
    const f64 _cols_f = fmax(1.0, ceil((_w_mm - FUDE_ZOOM_TILE_OVERLAP) / _step_w));
    const f64 _rows_f = fmax(1.0, ceil((_h_mm - FUDE_ZOOM_TILE_OVERLAP) / _step_h));
    if(!(_cols_f * _rows_f <= (f64)FUDE_ZOOM_TILE_MOST) || _rows_f > 26.0) {   // (rows go by letter)
        return false;
    }
    const u32 _cols = (u32)_cols_f, _rows = (u32)_rows_f;
    fude_zoom_pdfw _w = {
        .pdf = fude_zoom_pdf_new(), .half = { 0.0, 0.0 },
        .xy = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std()), .tris = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std()),
    };
    if(_w.pdf == NULL) {
        rde_arr_free(&_w.xy);
        rde_arr_free(&_w.tris);
        return false;
    }
    const fude_zoom_export_sink _sink = { &_w, fude_zoom_pdfw_stroke, fude_zoom_pdfw_shape, fude_zoom_pdfw_fill, fude_zoom_pdfw_image, fude_zoom_pdfw_text };
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
            // A screen point (centre origin, Y up) onto the page: the view's top left at the inside's top left, less this page's offset.
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
            snprintf(_label, sizeof _label, "%c%u  (%u x %u)", 'A' + (c8)_r, _c + 1u, _rows, _cols);
            fude_zoom_pdf_text(_w.pdf, _x0, _y1 + 3.0 * FUDE_ZOOM_PDF_MM, 9.0, _ink, _label);
            // A mini-map by it: the pages as a grid, this one filled (which way up the whole goes).
            if(_rows * _cols > 1u) {
                const f64 _cell = fmin(2.5, 22.0 / (f64)(_cols > _rows ? _cols : _rows)) * FUDE_ZOOM_PDF_MM;
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
            fude_zoom_pdf_text(_w.pdf, _x0, _y0 - 6.0 * FUDE_ZOOM_PDF_MM, 7.0, _ink, "1:1  Check the square: 100 mm a side");
            const f64 _sq = 100.0 * FUDE_ZOOM_PDF_MM;
            if(_in_w > 105.0 && _in_h > 105.0 && _r == 0u && _c == 0u) {
                fude_zoom_pdf_rect(_w.pdf, _x1 - _sq - 3.0 * FUDE_ZOOM_PDF_MM, _y0 + 3.0 * FUDE_ZOOM_PDF_MM, _sq, _sq);
                fude_zoom_pdf_stroke(_w.pdf);
            }
            fude_zoom_pdf_restore(_w.pdf);
            fude_zoom_pdf_page_end(_w.pdf);
        }
    }
    const b8 _ok = fude_zoom_pdf_finish(_w.pdf, _out);
    fude_zoom_pdf_free(_w.pdf);
    rde_arr_free(&_w.xy);
    rde_arr_free(&_w.tris);
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
    snprintf(_said, sizeof _said, "%s  %.0f mm", _across ? "X" : "Y", _len_mm);
    if(_across) {
        fude_zoom_pdf_text(_pdf, _x, _y + 9.0 * _mm, 12.0, _ink, _said);
    } else {
        fude_zoom_pdf_text(_pdf, _x + 9.0 * _mm, _y + _len_mm * _mm - 4.0 * _mm, 12.0, _ink, _said);
    }
}

b8 fude_zoom_export_print_check(f64 _page_w_mm, f64 _page_h_mm, fude_bytes* _out) {
    fude_zoom_pdf* _pdf = fude_zoom_pdf_new();
    if(_pdf == NULL) {
        return false;
    }
    const f64 _mm = FUDE_ZOOM_PDF_MM;
    const rde_color _ink = { 40, 40, 46, 255 };
    fude_zoom_pdf_page_begin(_pdf, _page_w_mm * _mm, _page_h_mm * _mm);
    fude_zoom_pdf_stroke_color(_pdf, _ink);
    fude_zoom_pdf_text(_pdf, 25.0 * _mm, (_page_h_mm - 25.0) * _mm, 16.0, _ink, "Printer check");
    fude_zoom_pdf_text(_pdf, 25.0 * _mm, (_page_h_mm - 33.0) * _mm, 10.0, _ink, "Print at 100% (actual size, not fit to page). Measure both lines.");
    const f64 _x = FUDE_ZOOM_CHECK_X_MM, _y = FUDE_ZOOM_CHECK_Y_MM;
    fude_zoom_check_ruler(_pdf, (_page_w_mm - _x) * 0.5 * _mm, (_page_h_mm - 50.0) * _mm, _x, true, _ink);
    fude_zoom_check_ruler(_pdf, 35.0 * _mm, fmin(20.0, _page_h_mm - 62.0 - _y) * _mm, _y, false, _ink);   // (20 mm up: inside what any printer reaches)
    fude_zoom_pdf_page_end(_pdf);
    const b8 _ok = fude_zoom_pdf_finish(_pdf, _out);
    fude_zoom_pdf_free(_pdf);
    return _ok;
}

// --- the cut list --------------------------------------------------------------------------------

typedef struct {
    c8  name[FUDE_ZOOM_BOARD_NAME];
    f64 length, width, thickness;   // mm, rounded to a tenth (what tells two parts the same)
    u8  grain;
    u8  material;                   // FUDE_ZOOM_MATERIAL_
    u32 count;
} fude_zoom_part;

RDE_INTERNAL int fude_zoom_part_order(const void* _a, const void* _b) {
    const fude_zoom_part* _p = (const fude_zoom_part*)_a;
    const fude_zoom_part* _q = (const fude_zoom_part*)_b;
    const int _byname = strcmp(_p->name, _q->name);
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
    const b8 _quote = strpbrk(_text, ",\"\r\n") != NULL;
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

u32 fude_zoom_export_cutlist(const fude_zoom_scene* _s, f64 _mm_per_unit, const c8* const* _header, const c8* const* _grain_words,
                             const c8* const* _material_words, fude_bytes* _out) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _parts = rde_arr_new(sizeof(fude_zoom_part), _heap);
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
        fude_zoom_part _p;
        memset(&_p, 0, sizeof(_p));
        fude_zoom_board_name(_n, _count, _p.name, sizeof(_p.name));
        _p.length    = round(fabs(_n[0]) * 2.0 * _mm * 10.0) / 10.0;
        _p.width     = round(fabs(_n[1]) * 2.0 * _mm * 10.0) / 10.0;
        _p.thickness = round(_n[2] * 10.0) / 10.0;
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
            fude_zoom_part* _q = &((fude_zoom_part*)_parts.memory)[_k];
            if(strcmp(_q->name, _p.name) == 0 && _q->length == _p.length && _q->width == _p.width && _q->thickness == _p.thickness && _q->grain == _p.grain &&
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
    qsort(_parts.memory, _rows, sizeof(fude_zoom_part), fude_zoom_part_order);
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
        const fude_zoom_part* _p = &((const fude_zoom_part*)_parts.memory)[_k];
        c8 _num[96];
        fude_zoom_csv_field(_out, _p->name);
        snprintf(_num, sizeof _num, ",%u,%.1f,%.1f,%.1f,", _p->count, _p->length, _p->width, _p->thickness);
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
