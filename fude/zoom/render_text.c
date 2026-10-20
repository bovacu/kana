// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/render_parts.h"

// ===========================================================================
// See render.h. The renderer, texts: formulas (formula.h: each glyph's triangles and its soft edge), a text read as a
// note (rich.h: its layout kept, by its words, fonts, sizes, width and alignment, and drawn with its marks), a text
// (its words upright in the app's font, its lines kept with its layout's slot while its size on the screen stays; a
// sticky note on its paper; a faint bar a line when too small to read), and text in a box (centred, or aligned, its
// letters smaller for a word too wide).
//
// The file goes: formulas, texts and notes (the layouts kept, a note drawn, a text drawn), then text in a box.
//
// Most drawing functions take _as and _extra: _as, when not NULL, draws the whole object in that colour (the
// selection's glow, or a faint thing), _extra points wider on each side.
//
// Part of the renderer: render_parts.h holds what the modules share and says which module holds what. Internal names
// start fude_zoom_render_ (fude_zoom_draw_ and fude_zoom_clip_ for the clipping). Main thread only, inside the app's 2D
// drawing block, as render.h says.
// ===========================================================================

// A closed ring of _n points through _to: a soft stroke 0.3 of a point across along it.
RDE_INTERNAL void fude_zoom_render_formula_edge(fude_zoom_renderer* _r, const fude_zoom_v2* _p, u32 _n, fude_zoom_sim _to, rde_color _ink) {
    if(_n < 2u) {
        return;
    }
    rde_arr_clear(&_r->screen);
    rde_arr_clear(&_r->radii);
    rde_vec_2F* _sp = (rde_vec_2F*)rde_arr_add_n(&_r->screen, _n + 1u);
    f32*        _sr = (f32*)rde_arr_add_n(&_r->radii, _n + 1u);
    for(u32 _i = 0; _i <= _n; _i++) {
        const fude_zoom_v2 _q = fude_zoom_sim_apply(_to, _p[_i % _n]);
        _sp[_i] = (rde_vec_2F){ (f32)_q.x, (f32)_q.y };
        _sr[_i] = 0.15f;
    }
    rde_rendering_2d_draw_stroke(_sp, _sr, _n + 1u, _ink);
}

// See render.h.
void fude_zoom_render_formula(fude_zoom_renderer* _r, const fude_zoom_tex_box* _box, fude_zoom_sim _em, rde_color _ink) {
    fude_zoom_formulas* _f = _r->formulas;
    if(_f == NULL || _box == NULL) {
        return;   // formulas not drawn by this renderer (render.h: formulas may be NULL)
    }
    const f64 _k = fude_zoom_sim_scale(_em);
    const f64 _unit = 1.0 / (f64)_f->font.units_per_em;
    const fude_zoom_tex_glyph* _g = (const fude_zoom_tex_glyph*)_box->glyphs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_box->glyphs); _i++) {
        const f64 _px = _k * _g[_i].size;
        if(_px < 0.8) {
            continue;
        }
        const fude_zoom_v2* _t = NULL;
        const u32 _n = fude_zoom_formulas_mesh(_f, _g[_i].glyph, _px, &_t);
        const fude_zoom_sim _glyph = fude_zoom_sim_compose(_em, (fude_zoom_sim){ _g[_i].size * _unit, 0.0, _g[_i].x, _g[_i].y });
        for(u32 _j = 0; _j < _n; _j++) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_glyph, _t[3u * _j]), _b = fude_zoom_sim_apply(_glyph, _t[3u * _j + 1u]),
                               _c = fude_zoom_sim_apply(_glyph, _t[3u * _j + 2u]);
            rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _ink, NULL);
        }
        // (its edge soft: each contour a stroke a few tenths of a point wide over it — a 2D draw has no MSAA, and a
        // stem thinner than a pixel would fall between pixels and vanish)
        const fude_zoom_v2* _ring = NULL;
        const u32*          _ends = NULL;
        const u32 _rings = fude_zoom_formulas_rings(_f, _g[_i].glyph, _px, &_ring, &_ends);
        u32 _from = 0;
        for(u32 _c = 0; _c < _rings; _c++) {
            fude_zoom_render_formula_edge(_r, _ring + _from, _ends[_c] - _from, _glyph, _ink);
            _from = _ends[_c];
        }
    }
    const fude_zoom_tex_rule* _rl = (const fude_zoom_tex_rule*)_box->rules.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_box->rules); _i++) {
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_em, (fude_zoom_v2){ _rl[_i].x, _rl[_i].y });
        const fude_zoom_v2 _b = fude_zoom_sim_apply(_em, (fude_zoom_v2){ _rl[_i].x + _rl[_i].w, _rl[_i].y });
        const fude_zoom_v2 _c = fude_zoom_sim_apply(_em, (fude_zoom_v2){ _rl[_i].x + _rl[_i].w, _rl[_i].y + _rl[_i].h });
        const fude_zoom_v2 _d = fude_zoom_sim_apply(_em, (fude_zoom_v2){ _rl[_i].x, _rl[_i].y + _rl[_i].h });
        rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _ink, NULL);
        rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, (rde_vec_2F){ (f32)_d.x, (f32)_d.y }, _ink, NULL);
        const fude_zoom_v2 _box[4] = { { _rl[_i].x, _rl[_i].y }, { _rl[_i].x + _rl[_i].w, _rl[_i].y }, { _rl[_i].x + _rl[_i].w, _rl[_i].y + _rl[_i].h },
                                       { _rl[_i].x, _rl[_i].y + _rl[_i].h } };
        fude_zoom_render_formula_edge(_r, _box, 4u, _em, _ink);
    }
}

// --- a text read as a note (rich.h) ---------------------------------------------------------------------

#define FUDE_ZOOM_RENDER_TEXT_LINE 1.3   // a text's lines apart (its letters' size)

// Its letters for a style: bold, italic, both (the plain font's where it has none).
RDE_INTERNAL rde_font* fude_zoom_render_font_of(const fude_zoom_renderer* _r, u8 _style) {
    const b8  _b = (_style & FUDE_ZOOM_RICH_BOLD) != 0u, _i = (_style & FUDE_ZOOM_RICH_ITALIC) != 0u;
    rde_font* _f = _b && _i ? _r->font_bold_italic : _b ? _r->font_bold : _i ? _r->font_italic : _r->font;
    return _f != NULL ? _f : _r->font;
}

// A text's bytes with a 0 after them, for the font's calls, which take an ended string: on the stack up to
// FUDE_ZOOM_RENDER_SHORT - 1 bytes, past that in an rde_arr on the std allocator (a text is the user's content, as long
// as they made it: coding_rules.md §3), so no text is ever cut. Made by fude_zoom_render_ended, let go by
// fude_zoom_render_ended_free, in the one call that uses it.
#define FUDE_ZOOM_RENDER_SHORT 512u
typedef struct {
    c8      small[FUDE_ZOOM_RENDER_SHORT];
    rde_arr long_copy;   // a longer one's (not made for a short one)
    c8*     s;           // the copy: small's or long_copy's
} fude_zoom_render_ended_text;

// Copies _len bytes of _text into _e, a 0 after them, and returns the copy (the caller's to change: it is its own).
RDE_INTERNAL c8* fude_zoom_render_ended(fude_zoom_render_ended_text* _e, const c8* _text, u32 _len) {
    _e->s = _e->small;
    if(_len >= sizeof(_e->small)) {
        _e->long_copy = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
        _e->s         = (c8*)rde_arr_add_n(&_e->long_copy, (usize)_len + 1u);
    }
    rde_memcpy(_e->s, _text, _len);
    _e->s[_len] = 0;
    return _e->s;
}

// Lets go of a copy fude_zoom_render_ended made (the long one's array; a short one has nothing to free).
RDE_INTERNAL void fude_zoom_render_ended_free(fude_zoom_render_ended_text* _e) {
    if(_e->s != _e->small) {
        rde_arr_free(&_e->long_copy);
    }
}

// See the header. The bytes are copied to end them with a 0 (fude_zoom_render_ended: however long).
f32 fude_zoom_render_measure(any _self, const c8* _text, u32 _len, f32 _px, u8 _style) {
    const fude_zoom_renderer*   _r = (const fude_zoom_renderer*)_self;
    fude_zoom_render_ended_text _e;
    const f32 _w = fude_draw_text_width(fude_zoom_render_font_of(_r, _style), _r->font_px, fude_zoom_render_ended(&_e, _text, _len), _px);
    fude_zoom_render_ended_free(&_e);
    return _w;
}

// See the header.
const struct fude_zoom_rich* fude_zoom_render_rich(fude_zoom_renderer* _r, const c8* _text, u32 _len, f64 _size, f64 _width) {
    return fude_zoom_render_rich_aligned(_r, _text, _len, _size, _width, 0u);
}

// What a note's layout is made from besides its words, at the head of its key (fude_zoom_render_rich_aligned): the
// fonts its words are measured in and the size they are loaded at, its letters' size, its width and its alignment.
// Set to 0 before it is filled in, so its bytes are always alike for the same layout.
typedef struct {
    const rde_font* fonts[4];   // plain, bold, italic, bold italic (render.h: NULL uses the plain one)
    f64             size, width;
    f32             font_px;
    u32             align;
} fude_zoom_render_rich_how;

// Returns the kept entry of a text's layout (fude_zoom_render_rich_aligned's), or NULL when there is none to make (no
// font, no words). A layout is kept by the whole of what it is made from (fude_zoom_render_rich_how, then the words),
// in a slot of rich_lru: found, or laid out into a free slot or the one drawn longest ago. When that one was drawn in
// this draw too (more notes on the screen than FUDE_ZOOM_RENDER_RICH), it is laid out into the spare instead, so the
// notes kept stay kept and only those past the bound are laid out again each draw (all of them were once: every
// layout was let go past the bound). The entry is good until the next is laid out.
RDE_INTERNAL fude_zoom_render_rich_kept* fude_zoom_render_rich_entry(fude_zoom_renderer* _r, const c8* _text, u32 _len, f64 _size, f64 _width, u8 _align) {
    if(_r->font == NULL || _text == NULL || _len == 0u || !rde_lru_is_inited(&_r->rich_lru)) {
        return NULL;
    }
    fude_zoom_render_rich_how _how;
    rde_memset(&_how, 0, sizeof(_how));
    _how.fonts[0] = _r->font;
    _how.fonts[1] = _r->font_bold;
    _how.fonts[2] = _r->font_italic;
    _how.fonts[3] = _r->font_bold_italic;
    _how.size     = _size;
    _how.width    = _width;
    _how.font_px  = _r->font_px;
    _how.align    = _align;
    rde_arr_clear(&_r->rich_key);
    u8* _key = (u8*)rde_arr_add_n(&_r->rich_key, sizeof(_how) + _len);
    rde_memcpy(_key, &_how, sizeof(_how));
    rde_memcpy(_key + sizeof(_how), _text, _len);
    fude_zoom_render_rich_kept* _all = (fude_zoom_render_rich_kept*)_r->rich.memory;
    u32 _slot = rde_lru_find(&_r->rich_lru, _key, sizeof(_how) + _len);
    if(_slot != RDE_LRU_NONE) {
        _all[_slot].used = _r->draws;
        return &_all[_slot];
    }
    const u32 _oldest = rde_lru_oldest(&_r->rich_lru);
    _slot = FUDE_ZOOM_RENDER_RICH;   // (the spare)
    if(rde_lru_length(&_r->rich_lru) < FUDE_ZOOM_RENDER_RICH || _all[_oldest].used != _r->draws) {
        _slot = rde_lru_add(&_r->rich_lru, _key, sizeof(_how) + _len, NULL);
    }
    fude_zoom_render_rich_kept* _k = &_all[_slot];
    if(!rde_arr_is_inited(&_k->rich.runs)) {
        fude_zoom_rich_init(&_k->rich);
    }
    _k->used     = _r->draws;
    _k->plain_px = 0.0f;   // (another text's lines, if any: forgotten)
    _k->marked   = fude_zoom_rich_marked(_text, _len);
    if(_k->marked) {
        fude_zoom_rich_lay_aligned(&_k->rich, _text, _len, (f32)_width, (f32)_size, (f32)FUDE_ZOOM_RENDER_TEXT_LINE, _align, fude_zoom_render_measure, _r);
    }
    return _k;
}

// See the header.
const struct fude_zoom_rich* fude_zoom_render_rich_aligned(fude_zoom_renderer* _r, const c8* _text, u32 _len, f64 _size, f64 _width, u8 _align) {
    const fude_zoom_render_rich_kept* _k = fude_zoom_render_rich_entry(_r, _text, _len, _size, _width, _align);
    return _k != NULL && _k->marked ? &_k->rich : NULL;
}

// Returns the kept entry of text _object's layout (fude_zoom_render_rich_of's: a plain text's or a sticky note's, not a
// formula's), with where its words start in its own units in *_origin; NULL when there is none.
RDE_INTERNAL fude_zoom_render_rich_kept* fude_zoom_render_rich_entry_of(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_v2* _origin) {
    f64 _size = 0.0, _w = 0.0, _h = 0.0;
    u8  _style = 0;
    const c8* _text = NULL;
    u32 _len = 0;
    if(!fude_zoom_scene_text(_s, _object, &_size, &_w, &_h, &_style, &_text, &_len) || (_style != FUDE_ZOOM_TEXT_PLAIN && _style != FUDE_ZOOM_TEXT_STICKY)) {
        return NULL;
    }
    const f64 _pad = _style == FUDE_ZOOM_TEXT_STICKY ? _size * 0.6 : 0.0;
    if(_origin != NULL) {
        *_origin = (fude_zoom_v2){ _pad, -_pad };
    }
    return fude_zoom_render_rich_entry(_r, _text, _len, _size, _style == FUDE_ZOOM_TEXT_STICKY ? rde_math_max_f64(_w - 2.0 * _pad, _size) : 0.0, 0u);
}

// See the header.
const struct fude_zoom_rich* fude_zoom_render_rich_of(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_v2* _origin) {
    const fude_zoom_render_rich_kept* _k = fude_zoom_render_rich_entry_of(_r, _s, _object, _origin);
    return _k != NULL && _k->marked ? &_k->rich : NULL;
}

// A plain text's lines drawn as fude_draw_text_wrap draws them (its layout at _px, _width wide, each line _line below
// the one before, from (_x, _y)), the layout kept in _k (fude_zoom_render_rich_kept's plain_*): laid out again only
// when the size or the width on the screen is not the one it was laid out at (a zoom), so a still view lays nothing
// out. The same layout and the same calls, so the same pixels. _words is the caller's own copy, ended by a 0: each
// line is ended in place for the while (a line of any length drawn whole).
RDE_INTERNAL void fude_zoom_render_plain_lines(const fude_zoom_renderer* _r, fude_zoom_render_rich_kept* _k, c8* _words, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line,
                                               rde_color _ink) {
    if(_words[0] == 0) {
        return;
    }
    if(_k->plain_px != _px || _k->plain_width != _width || !rde_arr_is_inited(&_k->plain_spans)) {
        const u32 _n = fude_draw_text_wrap_lines(_r->font, _r->font_px, _words, _px, _width);
        fude_zoom_render_fresh(&_k->plain_spans, sizeof(u32));
        if(_n > 0u) {
            u32* _from = (u32*)rde_arr_add_n(&_k->plain_spans, 2u * (usize)_n);
            fude_draw_text_wrap_spans(_r->font, _r->font_px, _words, _px, _width, _from, _from + _n, _n);
        }
        _k->plain_lines = _n;
        _k->plain_px    = _px;
        _k->plain_width = _width;
    }
    const u32* _from = (const u32*)_k->plain_spans.memory;
    const u32* _to   = _from + _k->plain_lines;
    for(u32 _i = 0; _i < _k->plain_lines; _i++) {
        const c8 _after = _words[_to[_i]];
        _words[_to[_i]] = 0;
        fude_draw_text(_r->font, _r->font_px, &_words[_from[_i]], _x, _y - (f32)_i * _line, _px, _ink);
        _words[_to[_i]] = _after;
    }
}

// A note's layout drawn: its top left at (_ox, _oy) on the screen, a unit of it _k points (its Y down, the screen's
// up), its letters _size; its marks under its words.
void fude_zoom_render_rich_draw(const fude_zoom_renderer* _r, const fude_zoom_rich* _rich, const c8* _text, f64 _ox, f64 _oy, f64 _k, f64 _size, rde_color _ink) {
    const f32 _lw = (f32)rde_math_max_f64(1.0, _k * _size * 0.07);
    rde_color _soft = _ink, _ground = _ink;
    _soft.a   = (u8)((u32)_ink.a * 35u / 100u);
    _ground.a = (u8)((u32)_ink.a * 10u / 100u);
    #define FZRN_AT(_x, _y) (rde_vec_2F){ (f32)(_ox + (f64)(_x) * _k), (f32)(_oy - (f64)(_y) * _k) }
    const fude_zoom_rich_mark* _m = (const fude_zoom_rich_mark*)_rich->marks.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_rich->marks); _i++) {
        const fude_zoom_rich_mark _q = _m[_i];
        if(_q.kind == FUDE_ZOOM_RICH_MARK_GROUND) {
            const rde_vec_2F _a = FZRN_AT(_q.x0, _q.y0), _b = FZRN_AT(_q.x1, _q.y1);
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_a.x + _b.x) * 0.5f, (_a.y + _b.y) * 0.5f }, (rde_vec_2F){ rde_math_abs_f32(_b.x - _a.x), rde_math_abs_f32(_b.y - _a.y) }, _ground);
        } else if(_q.kind == FUDE_ZOOM_RICH_MARK_LINE) {
            rde_rendering_2d_draw_line_1(FZRN_AT(_q.x0, _q.y0), FZRN_AT(_q.x1, _q.y1), _q.soft ? _soft : _ink, _lw);
        } else {
            // A task's box, its corners a little round; ticked: a tick in it.
            const rde_vec_2F _c[4] = { FZRN_AT(_q.x0, _q.y0), FZRN_AT(_q.x1, _q.y0), FZRN_AT(_q.x1, _q.y1), FZRN_AT(_q.x0, _q.y1) };
            for(u32 _e = 0; _e < 4u; _e++) {
                rde_rendering_2d_draw_line_1(_c[_e], _c[(_e + 1u) % 4u], _ink, _lw);
            }
            if(_q.kind == FUDE_ZOOM_RICH_MARK_TICKED) {
                const f32 _w = _q.x1 - _q.x0, _h = _q.y1 - _q.y0;
                const rde_vec_2F _t0 = FZRN_AT(_q.x0 + 0.2f * _w, _q.y0 + 0.52f * _h), _t1 = FZRN_AT(_q.x0 + 0.42f * _w, _q.y0 + 0.78f * _h),
                                 _t2 = FZRN_AT(_q.x0 + 0.82f * _w, _q.y0 + 0.22f * _h);
                rde_rendering_2d_draw_line_1(_t0, _t1, _ink, _lw * 1.4f);
                rde_rendering_2d_draw_line_1(_t1, _t2, _ink, _lw * 1.4f);
            }
        }
    }
    // Each run whole, however long (a word wider than its line is one run: fude_zoom_render_ended).
    const fude_zoom_rich_run* _run = (const fude_zoom_rich_run*)_rich->runs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_rich->runs); _i++) {
        const fude_zoom_rich_run _q = _run[_i];
        const f64 _px = (f64)_q.px * _k;
        if(_px < 3.0 || _px > 3000.0) {
            continue;
        }
        const c8* _from = (_q.style & FUDE_ZOOM_RICH_OWN) ? FUDE_ZOOM_RICH_BULLET : &_text[_q.from];
        rde_color _c = (_q.style & FUDE_ZOOM_RICH_LINK) ? _r->link_color : _ink;
        if(_q.style & FUDE_ZOOM_RICH_SOFT) {
            _c.a = (u8)((u32)_c.a * 55u / 100u);
        }
        const rde_vec_2F _at = FZRN_AT(_q.x, _q.y);
        fude_zoom_render_ended_text _e;
        fude_draw_text_placed(fude_zoom_render_font_of(_r, _q.style), fude_zoom_render_ended(&_e, _from, _q.len), (rde_vec_3F){ _at.x, _at.y, 0.0f },
                              (rde_vec_2F){ (f32)_px / _r->font_px, (f32)_px / _r->font_px }, 0.0f, _c);
        fude_zoom_render_ended_free(&_e);
    }
    #undef FZRN_AT
}

// A text: its words in the app's font at its size on the screen, upright (the
// font is drawn from curves, which do not turn); a note on its own paper. Too
// small to read, a note is still its paper and a text a faint bar a line. A
// formula is set and drawn as one (formula.h); a text with marks as a note.
void fude_zoom_render_text(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, const rde_color* _as) {
    f64 _size = 0.0, _w = 0.0, _h = 0.0;
    u8  _style = 0;
    const c8* _text = NULL;
    u32 _len = 0;
    if(!fude_zoom_scene_text(_s, _object, &_size, &_w, &_h, &_style, &_text, &_len)) {
        return;
    }
    const fude_zoom_object* _o   = fude_zoom_scene_object(_s, _object);
    const fude_zoom_sim     _all = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64               _k   = fude_zoom_sim_scale(_all);
    const fude_zoom_v2      _tl  = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const f64 _bw = _w * _k, _bh = _h * _k, _px = _size * _k;
    if(_bw < 0.5 || _bh < 0.5 || _bw > 1e6 || _bh > 1e6) {
        return;
    }
    const rde_color _ink = _as != NULL ? *_as : fude_theme_resolve(_o->color);
    if(_style == FUDE_ZOOM_TEXT_FORMULA) {
        // (a formula: set, its baseline its height below its box's top, the pad round it)
        const fude_zoom_tex_box* _box = _r->formulas != NULL ? fude_zoom_formulas_set(_r->formulas, _text, _len) : NULL;
        if(_box != NULL) {
            const fude_zoom_sim _em = fude_zoom_sim_compose(_all, (fude_zoom_sim){ _size, 0.0, FUDE_ZOOM_FORMULA_PAD * _size,
                                                                                  -(FUDE_ZOOM_FORMULA_PAD + _box->height) * _size });
            fude_zoom_render_formula(_r, _box, _em, _ink);
        }
        return;
    }
    if(_style == FUDE_ZOOM_TEXT_STICKY) {
        const rde_vec_2F _c = { (f32)(_tl.x + _bw * 0.5), (f32)(_tl.y - _bh * 0.5) };
        rde_color _shadow = { 0, 0, 0, 40 };
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _c.x + (f32)(_px * 0.12), _c.y - (f32)(_px * 0.16) }, (rde_vec_2F){ (f32)_bw, (f32)_bh }, _shadow);
        rde_rendering_2d_draw_rectangle(_c, (rde_vec_2F){ (f32)_bw, (f32)_bh }, _as != NULL ? *_as : _o->fill);
    }
    const f64 _pad  = _style == FUDE_ZOOM_TEXT_STICKY ? _size * 0.6 * _k : 0.0;
    const f64 _line = _px * 1.3;
    if(_px < 3.0 || _r->font == NULL) {
        // Too small to read: a faint bar for each line it fills.
        rde_color _faint = _ink;
        _faint.a = (u8)(_faint.a / 3u);
        const u32 _lines = (u32)rde_math_max_f64(1.0, rde_math_floor_f64((_bh - 2.0 * _pad) / rde_math_max_f64(_line, 1e-9)));
        for(u32 _i = 0; _i < _lines && _i < 64u; _i++) {
            const f64 _y = _tl.y - _pad - _line * ((f64)_i + 0.5);
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (f32)(_tl.x + _bw * 0.5), (f32)_y }, (rde_vec_2F){ (f32)rde_math_max_f64(_bw - 2.0 * _pad, 0.5), (f32)rde_math_max_f64(_px * 0.5, 0.5) }, _faint);
        }
        return;
    }
    if(_px > 3000.0) {
        return;   // a few letters filling far more than the screen: nothing readable to draw
    }
    // Read as a note (its marks shown): laid out once, kept. Else its plain lines, kept while the view is still.
    fude_zoom_v2 _origin;
    fude_zoom_render_rich_kept* _kept = fude_zoom_render_rich_entry_of(_r, _s, _object, &_origin);
    if(_kept != NULL && _kept->marked) {
        fude_zoom_render_rich_draw(_r, &_kept->rich, _text, _tl.x + _pad, _tl.y - _pad, _k, _size, _ink);
        return;
    }
    fude_zoom_render_ended_text _e;
    c8* _words = fude_zoom_render_ended(&_e, _text, _len);
    const f32 _x = (f32)(_tl.x + _pad), _y = (f32)(_tl.y - _pad - _px * 0.95), _width = (f32)rde_math_max_f64(_bw - 2.0 * _pad, 1.0);
    if(_kept != NULL) {
        fude_zoom_render_plain_lines(_r, _kept, _words, _x, _y, (f32)_px, _width, (f32)_line, _ink);
    } else {
        fude_draw_text_wrap(_r->font, _r->font_px, _words, _x, _y, (f32)_px, _width, (f32)_line, _ink);
    }
    fude_zoom_render_ended_free(&_e);
}

// See the header: centred.
void fude_zoom_render_text_in(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, rde_color _color) {
    fude_zoom_render_text_at(_r, _text, _len, _box, _px, _top, false, _color);
}

// See the header.
void fude_zoom_render_text_at(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, b8 _left, rde_color _color) {
    fude_zoom_render_text_aligned(_r, _text, _len, _box, _px, _top, _left ? FUDE_ZOOM_ALIGN_LEFT : FUDE_ZOOM_ALIGN_CENTRE, _color);
}

// Returns how wide the widest word of _words (_len bytes and a 0 after them: the caller's own copy) is at _px. Each
// word is measured where it is, ended by a 0 for the while and given its byte back, so a word of any length counts.
RDE_INTERNAL f32 fude_zoom_render_widest_word(const fude_zoom_renderer* _r, c8* _words, u32 _len, f64 _px) {
    f32 _widest = 0.0f;
    for(u32 _i = 0; _i < _len;) {
        while(_i < _len && (_words[_i] == ' ' || _words[_i] == '\n')) {
            _i++;
        }
        u32 _j = _i;
        while(_j < _len && _words[_j] != ' ' && _words[_j] != '\n') {
            _j++;
        }
        if(_j > _i) {
            const c8 _after = _words[_j];
            _words[_j] = 0;
            _widest = rde_math_max_f32(_widest, fude_draw_text_width(_r->font, _r->font_px, &_words[_i], (f32)_px));
            _words[_j] = _after;
        }
        _i = _j;
    }
    return _widest;
}

// How many lines text in a box keeps its spans for on the stack; a text of more has them on the std allocator.
#define FUDE_ZOOM_RENDER_BOX_LINES 48u

// See the header. A word wider than the box makes the letters smaller, to two thirds at the least. Every line is drawn
// whole, however many there are and however long (each ended by a 0 in place for the while, in the text's own copy).
void fude_zoom_render_text_aligned(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, u8 _align, rde_color _color) {
    if(_r->font == NULL || _len == 0u || !(_px >= 3.0)) {
        return;
    }
    fude_zoom_render_ended_text _e;
    c8*       _words = fude_zoom_render_ended(&_e, _text, _len);
    const f32 _width = (f32)rde_math_max_f64(_box.max_x - _box.min_x, _px);
    // (a word wider than the box: the letters smaller, to two thirds — never a word cut in two)
    const f32 _widest = fude_zoom_render_widest_word(_r, _words, _len, _px);
    if(_widest > _width) {
        _px = rde_math_max_f64(_px * (f64)(_width / _widest), _px * 0.66);
    }
    // Its lines' spans: on the stack, or for a text of more lines on the std allocator.
    u32       _from_small[FUDE_ZOOM_RENDER_BOX_LINES], _to_small[FUDE_ZOOM_RENDER_BOX_LINES];
    u32*      _from  = _from_small;
    u32*      _to    = _to_small;
    const u32 _count = fude_draw_text_wrap_lines(_r->font, _r->font_px, _words, (f32)_px, _width);
    rde_arr   _spans;
    rde_memset(&_spans, 0, sizeof(_spans));
    if(_count > FUDE_ZOOM_RENDER_BOX_LINES) {
        _spans = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        _from  = (u32*)rde_arr_add_n(&_spans, 2u * (usize)_count);
        _to    = _from + _count;
    }
    const u32 _lines = fude_draw_text_wrap_spans(_r->font, _r->font_px, _words, (f32)_px, _width, _from, _to, _from == _from_small ? FUDE_ZOOM_RENDER_BOX_LINES : _count);
    const f64 _step  = _px * 1.25;
    const f64 _cx    = (_box.min_x + _box.max_x) * 0.5;
    f64       _y     = _top ? _box.max_y - _px * 1.05 : (_box.min_y + _box.max_y) * 0.5 + _step * (f64)_lines * 0.5 - _px * 1.0;
    for(u32 _i = 0; _i < _lines; _i++, _y -= _step) {
        const c8 _after = _words[_to[_i]];
        _words[_to[_i]] = 0;
        const c8* _line = &_words[_from[_i]];
        const f32 _w = fude_draw_text_width(_r->font, _r->font_px, _line, (f32)_px);
        const f32 _x = _align == FUDE_ZOOM_ALIGN_LEFT ? (f32)_box.min_x : _align == FUDE_ZOOM_ALIGN_RIGHT ? (f32)_box.max_x - _w : (f32)(_cx - _w * 0.5);
        fude_draw_text_placed(_r->font, _line, (rde_vec_3F){ _x, (f32)_y, 0.0f },
                              (rde_vec_2F){ (f32)_px / _r->font_px, (f32)_px / _r->font_px }, 0.0f, _color);
        _words[_to[_i]] = _after;
    }
    if(_from != _from_small) {
        rde_arr_free(&_spans);
    }
    fude_zoom_render_ended_free(&_e);
}
