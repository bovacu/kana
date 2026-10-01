#include "browse.h"
#include "text.h"
#include "marks.h"
#include "draw.h"
#include "chart.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See browse.h.
// ===========================================================================

#define KANA_BROWSE_MARGIN     16.0f     // screen units at the sides and bottom
#define KANA_BROWSE_STATUS_H   34.0f     // the status line above the pad/grid
#define KANA_BROWSE_CAPTION_PX 15.0f     // the line under each character
#define KANA_BROWSE_PART_CELL  44.0f     // a part on the panel
#define KANA_BROWSE_PANEL_H    270.0f    // the parts panel, at most
#define KANA_BROWSE_MARKS      0x30000u  // code points the part marks cover (all of KanjiVG's)

RDE_INTERNAL int kana_browse_part_order(const void* _a, const void* _b) {
    const kana_browse_part* _x = (const kana_browse_part*)_a;
    const kana_browse_part* _y = (const kana_browse_part*)_b;
    if(_x->strokes != _y->strokes) { return _x->strokes < _y->strokes ? -1 : 1; }
    if(_x->uses != _y->uses)       { return _x->uses > _y->uses ? -1 : 1; }
    return _x->codepoint < _y->codepoint ? -1 : _x->codepoint > _y->codepoint ? 1 : 0;
}

// Every part worth offering: drawable (it has strokes of its own) and in at
// least KANA_BROWSE_PART_USES characters Browse lists.
RDE_INTERNAL void kana_browse_build_parts(kana_browse* _browse) {
    if(_browse->db == NULL || !kana_kanji_has_parts(_browse->db)) {
        return;
    }

    u16* _uses = (u16*)calloc(KANA_BROWSE_MARKS, sizeof(u16));
    if(_uses == NULL) {
        return;
    }
    const kana_catalog_entry* _entries = (const kana_catalog_entry*)_browse->catalog.entries.memory;
    for(u32 _e = 0; _e < (u32)rde_arr_length(&_browse->catalog.entries); _e++) {
        u32       _parts[KANA_KANJI_MAX_PARTS];
        const u32 _n = kana_kanji_parts(_browse->db, _entries[_e].record, _parts, KANA_KANJI_MAX_PARTS);
        for(u32 _i = 0; _i < _n; _i++) {
            if(_parts[_i] < KANA_BROWSE_MARKS && _uses[_parts[_i]] < 65535u) {
                _uses[_parts[_i]]++;
            }
        }
    }

    for(u32 _cp = 0; _cp < KANA_BROWSE_MARKS; _cp++) {
        kana_kanji_info _info;
        if(_uses[_cp] >= KANA_BROWSE_PART_USES && kana_kanji_find(_browse->db, _cp, &_info)) {
            const kana_browse_part _part = { .codepoint = _cp, .uses = _uses[_cp], .strokes = _info.strokes, .usable = true };
            rde_arr_add(&_browse->parts, &_part);
        }
    }
    free(_uses);

    if(rde_arr_length(&_browse->parts) > 1) {
        qsort(_browse->parts.memory, rde_arr_length(&_browse->parts), sizeof(kana_browse_part), kana_browse_part_order);
    }
    _browse->_marks = (u8*)calloc(KANA_BROWSE_MARKS, 1);
}

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

    _browse->parts     = rde_arr_new(sizeof(kana_browse_part), rde_memory_allocator_get_default_std());
    _browse->part_hits = rde_arr_new(sizeof(kana_browse_part_hit), rde_memory_allocator_get_default_std());
    kana_browse_build_parts(_browse);
}

void kana_browse_destroy(kana_browse* _browse) {
    rde_arr* _arrays[] = { &_browse->list, &_browse->parts, &_browse->part_hits };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    free(_browse->_marks);
    kana_catalog_destroy(&_browse->catalog);
    kana_glyph_destroy(&_browse->glyph);
    kana_ink_destroy(&_browse->pad);
    memset(_browse, 0, sizeof(*_browse));
}

void kana_browse_language_changed(kana_browse* _browse) {
    if(_browse->db == NULL) {
        return;
    }
    kana_catalog_destroy(&_browse->catalog);
    kana_catalog_init(&_browse->catalog, _browse->db);
    _browse->dirty = true;
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
    kana_recognize_forget(&_browse->recognition);
    _browse->_recognize_wanted = false;
    kana_browse_changed(_browse);
}

void kana_browse_set_drawing(kana_browse* _browse, b8 _drawing) {
    _browse->drawing = _drawing;
    if(!_drawing) {
        kana_browse_clear_pad(_browse);
    } else {
        _browse->picking      = false;   // one search panel at a time
        _browse->picked_count = 0;
    }
    kana_browse_changed(_browse);
}

b8 kana_browse_parts_available(const kana_browse* _browse) {
    return rde_arr_length(&_browse->parts) > 0 && _browse->_marks != NULL;
}

void kana_browse_clear_parts(kana_browse* _browse) {
    _browse->picked_count = 0;
    kana_browse_changed(_browse);
}

void kana_browse_set_picking(kana_browse* _browse, b8 _picking) {
    _browse->picking      = _picking && kana_browse_parts_available(_browse);
    _browse->picked_count = 0;   // hidden picks would filter unseen
    if(_browse->picking && _browse->drawing) {
        _browse->drawing = false;
        kana_browse_clear_pad(_browse);
    }
    kana_scroller_stop(&_browse->parts_scroller);
    _browse->parts_scroller.offset = 0.0f;
    kana_browse_changed(_browse);
}

RDE_INTERNAL b8 kana_browse_is_picked(const kana_browse* _browse, u32 _codepoint) {
    for(u32 _i = 0; _i < _browse->picked_count; _i++) {
        if(_browse->picked[_i] == _codepoint) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL void kana_browse_toggle_part(kana_browse* _browse, u32 _part) {
    const kana_browse_part* _p = &((const kana_browse_part*)_browse->parts.memory)[_part];
    for(u32 _i = 0; _i < _browse->picked_count; _i++) {
        if(_browse->picked[_i] == _p->codepoint) {
            _browse->picked[_i] = _browse->picked[--_browse->picked_count];
            kana_browse_changed(_browse);
            return;
        }
    }
    if(_p->usable && _browse->picked_count < KANA_BROWSE_MAX_PICKED) {
        _browse->picked[_browse->picked_count++] = _p->codepoint;
        kana_browse_changed(_browse);
    }
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
        // Drawn: what ML Kit reads it as first, when it has (recognize.h: the
        // same as Check), then best resemblance — the filter applying to both.
        kana_match_result _matched[KANA_BROWSE_MATCHES];
        kana_match_result _results[KANA_BROWSE_MATCHES];
        const u32 _found = kana_match_rank(_browse->db, &_browse->catalog, _browse->filter, &_browse->pad, _matched, KANA_BROWSE_MATCHES);
        const u32 _n     = kana_recognize_candidates(_browse->db, &_browse->recognition, 0, 1, &_browse->catalog, _browse->filter,
                                                     _matched, _found, _results, KANA_BROWSE_MATCHES);
        for(u32 _i = 0; _i < _n; _i++) {
            rde_arr_add(&_browse->list, &_results[_i].record);
        }
    } else {
        const u32 _n = kana_catalog_query(&_browse->catalog, _browse->filter, _browse->sort, _browse->search);
        if(_n > 0) {
            memcpy(rde_arr_add_n(&_browse->list, _n), kana_catalog_results(&_browse->catalog), (usize)_n * sizeof(u32));
        }
    }

    if(_browse->picking && kana_browse_parts_available(_browse)) {
        // Only characters with every picked part (a character counts as its own part).
        u32* _list = (u32*)_browse->list.memory;
        u32  _kept = 0;
        memset(_browse->_marks, 0, KANA_BROWSE_MARKS);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_browse->list); _i++) {
            u32             _parts[KANA_KANJI_MAX_PARTS + 1];
            u32             _n = kana_kanji_parts(_browse->db, _list[_i], _parts, KANA_KANJI_MAX_PARTS);
            kana_kanji_info _info;
            if(kana_kanji_at(_browse->db, _list[_i], &_info)) {
                _parts[_n++] = _info.codepoint;
            }

            b8 _all = true;
            for(u32 _k = 0; _k < _browse->picked_count && _all; _k++) {
                b8 _has = false;
                for(u32 _j = 0; _j < _n && !_has; _j++) {
                    _has = _parts[_j] == _browse->picked[_k];
                }
                _all = _has;
            }
            if(!_all) {
                continue;
            }

            _list[_kept++] = _list[_i];
            for(u32 _j = 0; _j < _n; _j++) {
                if(_parts[_j] < KANA_BROWSE_MARKS) {
                    _browse->_marks[_parts[_j]] = 1;   // what is left to pick
                }
            }
        }
        _browse->list.count = _kept;   // rde_arr has no truncate: rde_arr_clear's operation, to a length

        kana_browse_part* _parts = (kana_browse_part*)_browse->parts.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_browse->parts); _i++) {
            _parts[_i].usable = _browse->_marks[_parts[_i].codepoint] != 0 || kana_browse_is_picked(_browse, _parts[_i].codepoint);
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

    _browse->on_parts = _browse->picking && kana_browse_inside(_screen, _browse->panel_min, _browse->panel_max);
    kana_scroller_down(_browse->on_parts ? &_browse->parts_scroller : &_browse->scroller, _screen, _time);
}

void kana_browse_pointer_moved(kana_browse* _browse, rde_vec_2F _screen, f64 _time) {
    if(_browse->on_pad) {
        kana_ink_extend(&_browse->pad, kana_browse_pad_local(_browse, _screen));
        return;
    }

    kana_scroller_moved(_browse->on_parts ? &_browse->parts_scroller : &_browse->scroller, _screen, _time);
}

void kana_browse_pointer_up(kana_browse* _browse, f64 _time) {
    if(_browse->on_pad) {
        kana_ink_end(&_browse->pad);
        _browse->on_pad          = false;
        _browse->dirty           = true;   // re-rank with the new stroke
        _browse->scroller.offset = 0.0f;
        kana_recognize_forget(&_browse->recognition);   // an answer for the old drawing no longer holds
        _browse->_recognize_wanted = true;
        return;
    }

    if(_browse->on_parts) {
        _browse->on_parts = false;
        kana_scroller_up(&_browse->parts_scroller, _time);

        rde_vec_2F _at;
        if(kana_scroller_take_tap(&_browse->parts_scroller, &_at) && kana_browse_inside(_at, _browse->panel_min, _browse->panel_max)) {
            const f32                   _content_y = _browse->panel_max.y - _at.y + _browse->parts_scroller.offset;
            const kana_browse_part_hit* _hits      = (const kana_browse_part_hit*)_browse->part_hits.memory;
            for(u32 _i = 0; _i < (u32)rde_arr_length(&_browse->part_hits); _i++) {
                if(_at.x >= _hits[_i].x && _at.x < _hits[_i].x + _hits[_i].size && _content_y >= _hits[_i].y && _content_y < _hits[_i].y + _hits[_i].size) {
                    kana_browse_toggle_part(_browse, _hits[_i].part);
                    break;
                }
            }
        }
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
    // Marks changed (the viewer's Study, an exam): the Studying and Known lists with them.
    if(kana_marks_revision() != _browse->_marks_seen) {
        _browse->_marks_seen = kana_marks_revision();
        _browse->dirty       = _browse->dirty || _browse->filter == KANA_FILTER_STUDYING || _browse->filter == KANA_FILTER_KNOWN;
    }
    // ML Kit on the pad: asked once a stroke ends; its answer ranks the list again.
    if(_browse->_recognize_wanted) {
        if(!kana_browse_pad_used(_browse) || !kana_recognize_available() || kana_recognize_start(&_browse->recognition, &_browse->pad)) {
            _browse->_recognize_wanted = false;
        }
    }
    if(kana_recognize_poll(&_browse->recognition)) {
        _browse->dirty = true;
    }
    if(_browse->dirty) {
        kana_browse_recompute(_browse);
    }

    const u32 _rows = _browse->columns > 0 ? (kana_browse_count(_browse) + _browse->columns - 1u) / _browse->columns : 0u;
    kana_scroller_update(&_browse->scroller, _dt, (f32)_rows * _browse->cell, _browse->grid_max.y - _browse->grid_min.y);
    if(_browse->picking) {
        kana_scroller_update(&_browse->parts_scroller, _dt, _browse->parts_height, _browse->panel_max.y - _browse->panel_min.y);
    }
}

// --- drawing -----------------------------------------------------------------------

RDE_INTERNAL void kana_browse_draw_pad(kana_browse* _browse, rde_vec_2F _tl, f32 _size, rde_vec_2F _screen_half) {
    kana_glyph_box(_tl, _size);   // the same writing square as Practice's and Check's

    _browse->pad_min = (rde_vec_2F){ _tl.x, _tl.y - _size };
    _browse->pad_max = (rde_vec_2F){ _tl.x + _size, _tl.y };

    // The pad's strokes are stored from its bottom-left corner.
    kana_ink_render(&_browse->pad, _browse->pad_min, 1.0f, _screen_half, rde_engine_get_time_now(), false);
}

// The parts panel: the sheet, and on it every offered part by stroke count — a
// small stroke number where a new count starts — picked ones marked, ones no
// listed character has dimmed. Scrolls on its own.
RDE_INTERNAL void kana_browse_draw_parts(kana_browse* _browse, rde_window* _window, rde_font* _font, f32 _font_px,
                                         f32 _left, f32 _right, f32 _top, f32 _height) {
    const kana_theme* _theme = kana_theme_active();
    _browse->panel_min = (rde_vec_2F){ _left, _top - _height };
    _browse->panel_max = (rde_vec_2F){ _right, _top };

    kana_draw_card((rde_vec_2F){ _left, _top - _height }, (rde_vec_2F){ _right, _top }, 14.0f, _theme->surface, _theme->outline);

    rde_arr_clear(&_browse->part_hits);
    const f32 _cell   = KANA_BROWSE_PART_CELL;
    const f32 _inner  = _left + 6.0f;
    const u32 _cols   = (u32)fmaxf(1.0f, floorf((_right - _left - 12.0f) / _cell));
    const f32 _scroll = _browse->parts_scroller.offset;

    rde_rendering_begin_clipping_rect(_window,
                                      (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)(_top - _height * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left - 2.0f), (u32)(_height - 2.0f) });

    const kana_browse_part* _parts   = (const kana_browse_part*)_browse->parts.memory;
    u32                     _slot    = 0;
    u32                     _strokes = 0;
    c8                      _label[8];
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_browse->parts); _i++) {
        const kana_browse_part* _p = &_parts[_i];

        if(_p->strokes != _strokes) {   // a new stroke count: its number, in a cell of its own
            _strokes = _p->strokes;
            const f32 _cy = 4.0f + (f32)(_slot / _cols) * _cell;
            const f32 _y  = _top - (_cy - _scroll);
            if(_y > _top - _height - _cell && _y - _cell < _top) {
                snprintf(_label, sizeof(_label), "%u", _strokes);
                kana_draw_text(_font, _font_px, _label, _inner + (f32)(_slot % _cols) * _cell + 12.0f, _y - _cell * 0.62f, 14.0f, _theme->select);
            }
            _slot++;
        }

        const f32 _x  = _inner + (f32)(_slot % _cols) * _cell;
        const f32 _cy = 4.0f + (f32)(_slot / _cols) * _cell;
        const f32 _y  = _top - (_cy - _scroll);
        _slot++;
        if(_y - _cell > _top || _y < _top - _height) {
            continue;
        }

        const kana_browse_part_hit _hit = { _x, _cy, _cell, _i };
        rde_arr_add(&_browse->part_hits, &_hit);

        if(kana_browse_is_picked(_browse, _p->codepoint)) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x + _cell * 0.5f, _y - _cell * 0.5f }, (rde_vec_2F){ _cell - 4.0f, _cell - 4.0f }, _theme->select_fill);
            kana_draw_outline((rde_vec_2F){ _x + 2.0f, _y - _cell + 2.0f }, (rde_vec_2F){ _x + _cell - 2.0f, _y - 2.0f }, 1.2f, _theme->select);
        }
        const f32 _glyph = _cell * 0.7f;
        kana_glyph_character(&_browse->glyph, _p->codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - (_cell - _glyph) * 0.5f }, _glyph,
                             _p->usable ? _theme->ink : _theme->ghost);
    }
    _browse->parts_height = 8.0f + (f32)((_slot + _cols - 1u) / _cols) * _cell;

    rde_rendering_end_clipping_rect();
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
        kana_draw_text(_font, _font_px, _line, _x + 6.0f, _y, KANA_BROWSE_CAPTION_PX, kana_theme_active()->text_soft);
    }
}

void kana_browse_render(kana_browse* _browse, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom_edge) {
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
    const f32        _bottom = fmaxf(_bottom_edge, -_hh + (f32)_insets.w + KANA_BROWSE_MARGIN);   // above the bottom row

    // --- status ------------------------------------------------------------------------
    c8 _status[160];
    const u32 _count = kana_browse_count(_browse);
    if(kana_browse_pad_used(_browse)) {
        KANA_TEXTF(_status, KANA_TEXT_BROWSE_BEST, KANA_TN(_count));
    } else if(_browse->picking && _browse->picked_count > 0) {
        KANA_TEXTF(_status, _browse->picked_count == 1 ? KANA_TEXT_BROWSE_PARTS_ONE : KANA_TEXT_BROWSE_PARTS_MANY, KANA_TN(_count));
    } else if(_browse->picking) {
        snprintf(_status, sizeof(_status), "%s", kana_text(KANA_TEXT_BROWSE_PARTS_HINT));
    } else if(_browse->drawing) {
        snprintf(_status, sizeof(_status), "%s", kana_text(KANA_TEXT_BROWSE_DRAW_HINT));
    } else {
        KANA_TEXTF(_status, KANA_TEXT_CHARACTERS_N, KANA_TN(_count));
    }
    kana_draw_text(_font, _font_px, _status, _left, _top - 24.0f, 15.0f, kana_theme_active()->text);

    f32 _grid_top = _top - KANA_BROWSE_STATUS_H;

    // --- the pad -------------------------------------------------------------------------
    if(_browse->drawing) {
        const f32 _pad = fminf(KANA_BROWSE_PAD, _right - _left);
        kana_browse_draw_pad(_browse, (rde_vec_2F){ _left, _grid_top - 4.0f }, _pad, (rde_vec_2F){ _hw, _hh });
        _grid_top -= _pad + 20.0f;
    } else {
        _browse->pad_min = _browse->pad_max = (rde_vec_2F){ 0.0f, 0.0f };
    }

    // --- the parts panel -----------------------------------------------------------------
    if(_browse->picking) {
        const f32 _panel = fminf(KANA_BROWSE_PANEL_H, (_grid_top - _bottom) * 0.5f);
        kana_browse_draw_parts(_browse, _window, _font, _font_px, _left, _right, _grid_top - 4.0f, _panel);
        _grid_top -= _panel + 16.0f;
    } else {
        _browse->panel_min = _browse->panel_max = (rde_vec_2F){ 0.0f, 0.0f };
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

            const f32 _x      = _left + (f32)_col * _cell;
            const b8  _select = _browse->selection != NULL && _browse->selection->active;
            const b8  _ticked = _select && kana_selection_has(_browse->selection, _list[_i]);
            if(_select) {
                kana_selection_draw_behind(_ticked, (rde_vec_2F){ _x, _y }, _cell);
            }
            kana_draw_line((rde_vec_2F){ _x + 4.0f, _y - _cell }, (rde_vec_2F){ _x + _cell - 4.0f, _y - _cell }, 0.5f, kana_theme_active()->line);
            kana_glyph_character(&_browse->glyph, _info.codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - _cell * 0.08f }, _glyph, kana_theme_active()->ink);
            kana_browse_caption(_browse, &_info, _font, _font_px, _x, _y - _cell + 8.0f, _cell);
            if(_select) {
                kana_selection_draw_tick(_ticked, (rde_vec_2F){ _x, _y }, _cell);
            }
            kana_selection_draw_mark(kana_marks_get(_info.codepoint), (rde_vec_2F){ _x, _y }, _cell, _select);
        }
    }

    rde_rendering_end_clipping_rect();
}
