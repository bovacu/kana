#include "browse.h"
#include "chart.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See browse.h.
// ===========================================================================

#define KANA_BROWSE_MARGIN     16.0f     // screen units at the sides and bottom
#define KANA_BROWSE_STATUS_H   34.0f     // the status line above the pad/grid
#define KANA_BROWSE_CAPTION_PX 15.0f     // the line under each character

void kana_browse_init(kana_browse* _browse, const kana_kanji_db* _db) {
    memset(_browse, 0, sizeof(*_browse));
    _browse->db      = _db;
    _browse->filter  = KANA_FILTER_ALL;
    _browse->sort    = KANA_SORT_DEFAULT;
    _browse->dirty   = true;
    _browse->tapped  = -1;
    _browse->list    = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    kana_catalog_init(&_browse->catalog, _db);
    kana_glyph_init(&_browse->glyph, _db);
    kana_ink_init(&_browse->pad);
}

void kana_browse_destroy(kana_browse* _browse) {
    if(rde_arr_is_inited(&_browse->list)) {
        rde_arr_free(&_browse->list);
    }
    kana_catalog_destroy(&_browse->catalog);
    kana_glyph_destroy(&_browse->glyph);
    kana_ink_destroy(&_browse->pad);
    memset(_browse, 0, sizeof(*_browse));
}

b8 kana_browse_available(const kana_browse* _browse) {
    return _browse->db != NULL && rde_arr_length(&_browse->catalog.entries) > 0;
}

void kana_browse_open(kana_browse* _browse) {
    if(kana_browse_available(_browse)) {
        _browse->open  = true;
        _browse->dirty = true;
    }
}

void kana_browse_close(kana_browse* _browse) {
    _browse->open   = false;
    _browse->on_pad = false;
    kana_scroller_stop(&_browse->scroller);
}

RDE_INTERNAL void kana_browse_changed(kana_browse* _browse) {
    _browse->dirty           = true;
    _browse->scroller.offset = 0.0f;
    kana_scroller_stop(&_browse->scroller);
}

void kana_browse_set_filter(kana_browse* _browse, KANA_FILTER_ _filter) {
    _browse->filter = _filter;
    kana_browse_changed(_browse);
}

void kana_browse_set_sort(kana_browse* _browse, KANA_SORT_ _sort) {
    _browse->sort = _sort;
    kana_browse_changed(_browse);
}

void kana_browse_set_search(kana_browse* _browse, const c8* _text) {
    snprintf(_browse->search, sizeof(_browse->search), "%s", _text != NULL ? _text : "");
    kana_browse_changed(_browse);
}

void kana_browse_clear_pad(kana_browse* _browse) {
    kana_ink_destroy(&_browse->pad);
    kana_ink_init(&_browse->pad);
    kana_browse_changed(_browse);
}

void kana_browse_set_drawing(kana_browse* _browse, b8 _drawing) {
    _browse->drawing = _drawing;
    if(!_drawing) {
        kana_browse_clear_pad(_browse);
    }
    kana_browse_changed(_browse);
}

const u32* kana_browse_list(const kana_browse* _browse) {
    return (const u32*)_browse->list.memory;
}

u32 kana_browse_count(const kana_browse* _browse) {
    return (u32)rde_arr_length(&_browse->list);
}

b8 kana_browse_take_tap(kana_browse* _browse, u32* _position) {
    if(_browse->tapped < 0) {
        return false;
    }
    *_position      = (u32)_browse->tapped;
    _browse->tapped = -1;
    return true;
}

// --- the list ----------------------------------------------------------------------

RDE_INTERNAL b8 kana_browse_pad_used(const kana_browse* _browse) {
    return _browse->drawing && kana_ink_alive_strokes(&_browse->pad) > 0;
}

RDE_INTERNAL void kana_browse_recompute(kana_browse* _browse) {
    rde_arr_clear(&_browse->list);

    if(kana_browse_pad_used(_browse)) {
        // Drawn: best resemblance first (the filter still applies).
        kana_match_result _results[KANA_BROWSE_MATCHES];
        const u32 _n = kana_match_rank(_browse->db, &_browse->catalog, _browse->filter, &_browse->pad, _results, KANA_BROWSE_MATCHES);
        for(u32 _i = 0; _i < _n; _i++) {
            rde_arr_add(&_browse->list, &_results[_i].record);
        }
    } else {
        const u32 _n = kana_catalog_query(&_browse->catalog, _browse->filter, _browse->sort, _browse->search);
        if(_n > 0) {
            memcpy(rde_arr_add_n(&_browse->list, _n), kana_catalog_results(&_browse->catalog), (usize)_n * sizeof(u32));
        }
    }

    _browse->dirty = false;
}

// --- input -------------------------------------------------------------------------

RDE_INTERNAL b8 kana_browse_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y;
}

// Screen → pad-local (from its bottom-left corner), clamped to the pad.
RDE_INTERNAL rde_vec_2F kana_browse_pad_local(const kana_browse* _browse, rde_vec_2F _screen) {
    return (rde_vec_2F){
        rde_math_clamp_f32(_screen.x, _browse->pad_min.x, _browse->pad_max.x) - _browse->pad_min.x,
        rde_math_clamp_f32(_screen.y, _browse->pad_min.y, _browse->pad_max.y) - _browse->pad_min.y
    };
}

void kana_browse_pointer_down(kana_browse* _browse, rde_vec_2F _screen, b8 _pen, f64 _time) {
    _browse->on_pad = _browse->drawing && _pen && kana_browse_inside(_screen, _browse->pad_min, _browse->pad_max);

    if(_browse->on_pad) {
        _browse->pad.zoom = 1.0f;
        kana_ink_begin(&_browse->pad, kana_browse_pad_local(_browse, _screen), true, false);
        return;
    }

    kana_scroller_down(&_browse->scroller, _screen, _time);
}

void kana_browse_pointer_moved(kana_browse* _browse, rde_vec_2F _screen, f64 _time) {
    if(_browse->on_pad) {
        kana_ink_extend(&_browse->pad, kana_browse_pad_local(_browse, _screen));
        return;
    }

    kana_scroller_moved(&_browse->scroller, _screen, _time);
}

void kana_browse_pointer_up(kana_browse* _browse, f64 _time) {
    if(_browse->on_pad) {
        kana_ink_end(&_browse->pad);
        _browse->on_pad          = false;
        _browse->dirty           = true;   // re-rank with the new stroke
        _browse->scroller.offset = 0.0f;
        return;
    }

    kana_scroller_up(&_browse->scroller, _time);

    rde_vec_2F _at;
    if(kana_scroller_take_tap(&_browse->scroller, &_at) &&
       kana_browse_inside(_at, _browse->grid_min, _browse->grid_max) && _browse->cell > 0.0f) {
        const u32 _col = (u32)((_at.x - _browse->grid_min.x) / _browse->cell);
        const u32 _row = (u32)((_browse->grid_max.y - _at.y + _browse->scroller.offset) / _browse->cell);
        const u32 _i   = _row * _browse->columns + _col;
        if(_col < _browse->columns && _i < kana_browse_count(_browse)) {
            _browse->tapped = (i32)_i;
        }
    }
}

void kana_browse_update(kana_browse* _browse, f32 _dt) {
    if(!_browse->open) {
        return;
    }
    if(_browse->dirty) {
        kana_browse_recompute(_browse);
    }

    const u32 _rows = _browse->columns > 0 ? (kana_browse_count(_browse) + _browse->columns - 1u) / _browse->columns : 0u;
    kana_scroller_update(&_browse->scroller, _dt, (f32)_rows * _browse->cell, _browse->grid_max.y - _browse->grid_min.y);
}

// --- drawing -----------------------------------------------------------------------

RDE_INTERNAL void kana_browse_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color) {
    const f32 _scale = _px / _font_px;
    rde_rendering_2d_draw_text_2(_font, _text, (rde_vec_3F){ _x, _y, 0.0f }, (rde_vec_2F){ _scale, _scale }, 0.0f, _color);
}

RDE_INTERNAL void kana_browse_line(rde_vec_2F _a, rde_vec_2F _b, f32 _radius, rde_color _color) {
    const rde_vec_2F _p[2] = { _a, _b };
    const f32        _r[2] = { _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 2, _color);
}

RDE_INTERNAL void kana_browse_draw_pad(kana_browse* _browse, rde_vec_2F _tl, f32 _size, rde_vec_2F _screen_half) {
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ _tl.x + _size * 0.5f, _tl.y - _size * 0.5f }, (rde_vec_2F){ _size, _size }, kana_theme_active()->sheet);

    const rde_vec_2F _box[5] = { _tl, { _tl.x + _size, _tl.y }, { _tl.x + _size, _tl.y - _size }, { _tl.x, _tl.y - _size }, _tl };
    const f32        _r[5]   = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    rde_rendering_2d_draw_stroke(_box, _r, 5, kana_theme_active()->sheet_outline);
    kana_browse_line((rde_vec_2F){ _tl.x + _size * 0.5f, _tl.y - 6.0f }, (rde_vec_2F){ _tl.x + _size * 0.5f, _tl.y - _size + 6.0f }, 0.6f, kana_theme_active()->sheet_guide);
    kana_browse_line((rde_vec_2F){ _tl.x + 6.0f, _tl.y - _size * 0.5f }, (rde_vec_2F){ _tl.x + _size - 6.0f, _tl.y - _size * 0.5f }, 0.6f, kana_theme_active()->sheet_guide);

    _browse->pad_min = (rde_vec_2F){ _tl.x, _tl.y - _size };
    _browse->pad_max = (rde_vec_2F){ _tl.x + _size, _tl.y };

    // The pad's strokes are stored from its bottom-left corner.
    kana_ink_render(&_browse->pad, _browse->pad_min, 1.0f, _screen_half, rde_engine_get_time_now(), false);
}

// What a cell shows under the character, depending on how the list is ordered.
RDE_INTERNAL void kana_browse_caption(kana_browse* _browse, const kana_kanji_info* _info, rde_font* _font, f32 _font_px,
                                      f32 _x, f32 _y, f32 _cell) {
    c8 _line[64] = "";

    if(kana_browse_pad_used(_browse)) {
        return;
    }

    const b8        _by_reading = _browse->sort == KANA_SORT_ON || _browse->sort == KANA_SORT_KUN;
    const c8* const _romaji     = kana_chart_romaji(_info->codepoint);

    if(_by_reading && _romaji == NULL) {
        // The reading it is sorted by, drawn in kana (first reading only). A kanji
        // without one — 校 has no kun, 込 no on — shows its other reading instead:
        // on readings are katakana and kun hiragana, so they never read as each other.
        const c8* _on  = kana_kanji_on(_browse->db, _info);
        const c8* _kun = kana_kanji_kun(_browse->db, _info);
        const c8* _all = _browse->sort == KANA_SORT_ON ? (_on[0] != 0 ? _on : _kun) : (_kun[0] != 0 ? _kun : _on);
        if(_all[0] != 0) {
            c8 _first[64];
            const c8* _sep = strstr(_all, "、");
            const usize _n = _sep != NULL ? (usize)(_sep - _all) : strlen(_all);
            snprintf(_first, sizeof(_first), "%.*s", (int)(_n < sizeof(_first) - 1 ? _n : sizeof(_first) - 1), _all);
            kana_glyph_reading(&_browse->glyph, _first, (rde_vec_2F){ _x + 6.0f, _y + 10.0f }, 14.0f, _x + _cell - 4.0f, kana_theme_active()->text_soft, kana_theme_active()->text_soft);
            return;
        }
        // No reading at all: its meaning, below.
    }

    if(_browse->sort == KANA_SORT_STROKES) {
        snprintf(_line, sizeof(_line), "%u", _info->strokes);
    } else if(_romaji != NULL) {
        snprintf(_line, sizeof(_line), "%s", _romaji);   // kana have no readings or meanings: their romaji
    } else if(_by_reading || _browse->sort == KANA_SORT_MEANING || _browse->search[0] != 0) {
        const c8* _m = kana_kanji_meanings(_browse->db, _info);
        const c8* _comma = strstr(_m, ", ");
        const usize _n = _comma != NULL ? (usize)(_comma - _m) : strlen(_m);
        const usize _fit = (usize)(_cell / (KANA_BROWSE_CAPTION_PX * 0.6f));   // roughly what fits
        snprintf(_line, sizeof(_line), "%.*s%s", (int)(_n < _fit ? _n : _fit), _m, _n > _fit ? "." : "");
    } else if(_info->jlpt_n != 0) {
        snprintf(_line, sizeof(_line), "N%u", _info->jlpt_n);
    }

    if(_line[0] != 0) {
        kana_browse_text(_font, _font_px, _line, _x + 6.0f, _y, KANA_BROWSE_CAPTION_PX, kana_theme_active()->text_soft);
    }
}

void kana_browse_render(kana_browse* _browse, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top) {
    if(!_browse->open) {
        return;
    }
    if(_browse->dirty) {
        kana_browse_recompute(_browse);
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _hw     = (f32)_size.x * 0.5f;
    const f32        _hh     = (f32)_size.y * 0.5f;
    const f32        _left   = -_hw + (f32)_insets.x + KANA_BROWSE_MARGIN;
    const f32        _right  = _hw - (f32)_insets.z - KANA_BROWSE_MARGIN;
    const f32        _bottom = -_hh + (f32)_insets.w + KANA_BROWSE_MARGIN;

    // --- status ------------------------------------------------------------------------
    c8 _status[160];
    const u32 _count = kana_browse_count(_browse);
    if(kana_browse_pad_used(_browse)) {
        snprintf(_status, sizeof(_status), "Best matches for your drawing: %u", _count);
    } else if(_browse->drawing) {
        snprintf(_status, sizeof(_status), "Write a character in the box with the pen: every stroke re-ranks the list");
    } else {
        snprintf(_status, sizeof(_status), "%u character%s", _count, _count == 1 ? "" : "s");
    }
    kana_browse_text(_font, _font_px, _status, _left, _top - 24.0f, 17.0f, kana_theme_active()->text);

    f32 _grid_top = _top - KANA_BROWSE_STATUS_H;

    // --- the pad -------------------------------------------------------------------------
    if(_browse->drawing) {
        const f32 _pad = fminf(KANA_BROWSE_PAD, _right - _left);
        kana_browse_draw_pad(_browse, (rde_vec_2F){ _left, _grid_top - 4.0f }, _pad, (rde_vec_2F){ _hw, _hh });
        _grid_top -= _pad + 20.0f;
    } else {
        _browse->pad_min = _browse->pad_max = (rde_vec_2F){ 0.0f, 0.0f };
    }

    // --- the grid ------------------------------------------------------------------------
    const f32 _width   = _right - _left;
    const u32 _columns = (u32)fmaxf(1.0f, floorf(_width / KANA_BROWSE_CELL_MIN));
    const f32 _cell    = _width / (f32)_columns;
    _browse->columns   = _columns;
    _browse->cell      = _cell;
    _browse->grid_min  = (rde_vec_2F){ _left, _bottom };
    _browse->grid_max  = (rde_vec_2F){ _right, _grid_top };

    const f32 _view = _grid_top - _bottom;
    if(_view <= 0.0f || _count == 0) {
        return;
    }

    rde_rendering_begin_clipping_rect(_window,
                                      (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_grid_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)_width, (u32)_view });

    const f32 _scroll    = _browse->scroller.offset;
    const u32 _first_row = (u32)fmaxf(0.0f, floorf(_scroll / _cell));
    const u32 _last_row  = (u32)ceilf((_scroll + _view) / _cell);
    const f32 _glyph     = _cell * 0.6f;
    const u32* _list     = kana_browse_list(_browse);

    for(u32 _row = _first_row; _row <= _last_row; _row++) {
        const f32 _y = _grid_top - ((f32)_row * _cell - _scroll);   // the row's top edge

        for(u32 _col = 0; _col < _columns; _col++) {
            const u32 _i = _row * _columns + _col;
            if(_i >= _count) {
                break;
            }

            kana_kanji_info _info;
            if(!kana_kanji_at(_browse->db, _list[_i], &_info)) {
                continue;
            }

            const f32 _x = _left + (f32)_col * _cell;
            kana_browse_line((rde_vec_2F){ _x + 4.0f, _y - _cell }, (rde_vec_2F){ _x + _cell - 4.0f, _y - _cell }, 0.5f, kana_theme_active()->line);
            kana_glyph_character(&_browse->glyph, _info.codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - _cell * 0.08f }, _glyph, kana_theme_active()->ink);
            kana_browse_caption(_browse, &_info, _font, _font_px, _x, _y - _cell + 8.0f, _cell);
        }
    }

    rde_rendering_end_clipping_rect();
}
