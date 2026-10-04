#include "study/screens/browse.h"
#include "drawing/base/text.h"
#include "study/models/marks.h"
#include "drawing/widgets/draw.h"
#include "lang/lang.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See browse.h.
// ===========================================================================

#define FUDE_BROWSE_MARGIN     16.0f     // screen units at the sides and bottom
#define FUDE_BROWSE_STATUS_H   34.0f     // the status line above the pad/grid
#define FUDE_BROWSE_CAPTION_PX 15.0f     // the line under each character
#define FUDE_BROWSE_PART_CELL  44.0f     // a part on the panel
#define FUDE_BROWSE_PANEL_H    270.0f    // the parts panel, at most
#define FUDE_BROWSE_MARKS      0x30000u  // code points the part marks cover (all of KanjiVG's)

RDE_INTERNAL int fude_browse_part_order(const void* _a, const void* _b) {
    const fude_browse_part* _x = (const fude_browse_part*)_a;
    const fude_browse_part* _y = (const fude_browse_part*)_b;
    if(_x->strokes != _y->strokes) { return _x->strokes < _y->strokes ? -1 : 1; }
    if(_x->uses != _y->uses)       { return _x->uses > _y->uses ? -1 : 1; }
    return _x->codepoint < _y->codepoint ? -1 : _x->codepoint > _y->codepoint ? 1 : 0;
}

// Every part worth offering: drawable (it has strokes of its own) and in at
// least FUDE_BROWSE_PART_USES characters Browse lists.
RDE_INTERNAL void fude_browse_build_parts(fude_browse* _browse) {
    if(_browse->db == NULL || !fude_kanji_has_parts(_browse->db)) {
        return;
    }

    u16* _uses = (u16*)calloc(FUDE_BROWSE_MARKS, sizeof(u16));
    if(_uses == NULL) {
        return;
    }
    const fude_catalog_entry* _entries = (const fude_catalog_entry*)_browse->catalog.entries.memory;
    for(u32 _e = 0; _e < (u32)rde_arr_length(&_browse->catalog.entries); _e++) {
        u32       _parts[FUDE_KANJI_MAX_PARTS];
        const u32 _n = fude_kanji_parts(_browse->db, _entries[_e].record, _parts, FUDE_KANJI_MAX_PARTS);
        for(u32 _i = 0; _i < _n; _i++) {
            if(_parts[_i] < FUDE_BROWSE_MARKS && _uses[_parts[_i]] < 65535u) {
                _uses[_parts[_i]]++;
            }
        }
    }

    for(u32 _cp = 0; _cp < FUDE_BROWSE_MARKS; _cp++) {
        fude_kanji_info _info;
        if(_uses[_cp] >= FUDE_BROWSE_PART_USES && fude_kanji_find(_browse->db, _cp, &_info)) {
            const fude_browse_part _part = { .codepoint = _cp, .uses = _uses[_cp], .strokes = _info.strokes, .usable = true };
            rde_arr_add(&_browse->parts, &_part);
        }
    }
    free(_uses);

    if(rde_arr_length(&_browse->parts) > 1) {
        qsort(_browse->parts.memory, rde_arr_length(&_browse->parts), sizeof(fude_browse_part), fude_browse_part_order);
    }
    _browse->_marks = (u8*)calloc(FUDE_BROWSE_MARKS, 1);
}

void fude_browse_init(fude_browse* _browse, const fude_kanji_db* _db) {
    memset(_browse, 0, sizeof(*_browse));
    _browse->db      = _db;
    _browse->filter  = FUDE_FILTER_ALL;
    _browse->sort    = FUDE_SORT_DEFAULT;
    _browse->dirty   = true;
    _browse->tapped  = -1;
    _browse->list    = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_catalog_init(&_browse->catalog, _db);
    fude_glyph_init(&_browse->glyph, _db);
    fude_ink_init(&_browse->pad);

    _browse->parts     = rde_arr_new(sizeof(fude_browse_part), rde_memory_allocator_get_default_std());
    _browse->part_hits = rde_arr_new(sizeof(fude_browse_part_hit), rde_memory_allocator_get_default_std());
    fude_browse_build_parts(_browse);
}

void fude_browse_destroy(fude_browse* _browse) {
    rde_arr* _arrays[] = { &_browse->list, &_browse->parts, &_browse->part_hits };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    free(_browse->_marks);
    fude_catalog_destroy(&_browse->catalog);
    fude_glyph_destroy(&_browse->glyph);
    fude_ink_destroy(&_browse->pad);
    memset(_browse, 0, sizeof(*_browse));
}

void fude_browse_language_changed(fude_browse* _browse) {
    if(_browse->db == NULL) {
        return;
    }
    fude_catalog_destroy(&_browse->catalog);
    fude_catalog_init(&_browse->catalog, _browse->db);
    _browse->dirty = true;
}

b8 fude_browse_available(const fude_browse* _browse) {
    return _browse->db != NULL && rde_arr_length(&_browse->catalog.entries) > 0;
}

void fude_browse_open(fude_browse* _browse) {
    if(fude_browse_available(_browse)) {
        _browse->open  = true;
        _browse->dirty = true;
    }
}

void fude_browse_close(fude_browse* _browse) {
    _browse->open   = false;
    _browse->on_pad = false;
    fude_scroller_stop(&_browse->scroller);
}

RDE_INTERNAL void fude_browse_changed(fude_browse* _browse) {
    _browse->dirty           = true;
    _browse->scroller.offset = 0.0f;
    fude_scroller_stop(&_browse->scroller);
}

void fude_browse_set_filter(fude_browse* _browse, FUDE_FILTER_ _filter) {
    _browse->filter = _filter;
    fude_browse_changed(_browse);
}

void fude_browse_set_sort(fude_browse* _browse, FUDE_SORT_ _sort) {
    _browse->sort = _sort;
    fude_browse_changed(_browse);
}

void fude_browse_set_search(fude_browse* _browse, const c8* _text) {
    snprintf(_browse->search, sizeof(_browse->search), "%s", _text != NULL ? _text : "");
    fude_browse_changed(_browse);
}

void fude_browse_clear_pad(fude_browse* _browse) {
    fude_ink_destroy(&_browse->pad);
    fude_ink_init(&_browse->pad);
    fude_recognize_forget(&_browse->recognition);
    _browse->_recognize_wanted = false;
    fude_browse_changed(_browse);
}

void fude_browse_set_drawing(fude_browse* _browse, b8 _drawing) {
    _browse->drawing = _drawing;
    if(!_drawing) {
        fude_browse_clear_pad(_browse);
    } else {
        _browse->picking      = false;   // one search panel at a time
        _browse->picked_count = 0;
    }
    fude_browse_changed(_browse);
}

b8 fude_browse_parts_available(const fude_browse* _browse) {
    return rde_arr_length(&_browse->parts) > 0 && _browse->_marks != NULL;
}

void fude_browse_clear_parts(fude_browse* _browse) {
    _browse->picked_count = 0;
    fude_browse_changed(_browse);
}

void fude_browse_set_picking(fude_browse* _browse, b8 _picking) {
    _browse->picking      = _picking && fude_browse_parts_available(_browse);
    _browse->picked_count = 0;   // hidden picks would filter unseen
    if(_browse->picking && _browse->drawing) {
        _browse->drawing = false;
        fude_browse_clear_pad(_browse);
    }
    fude_scroller_stop(&_browse->parts_scroller);
    _browse->parts_scroller.offset = 0.0f;
    fude_browse_changed(_browse);
}

RDE_INTERNAL b8 fude_browse_is_picked(const fude_browse* _browse, u32 _codepoint) {
    for(u32 _i = 0; _i < _browse->picked_count; _i++) {
        if(_browse->picked[_i] == _codepoint) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL void fude_browse_toggle_part(fude_browse* _browse, u32 _part) {
    const fude_browse_part* _p = &((const fude_browse_part*)_browse->parts.memory)[_part];
    for(u32 _i = 0; _i < _browse->picked_count; _i++) {
        if(_browse->picked[_i] == _p->codepoint) {
            _browse->picked[_i] = _browse->picked[--_browse->picked_count];
            fude_browse_changed(_browse);
            return;
        }
    }
    if(_p->usable && _browse->picked_count < FUDE_BROWSE_MAX_PICKED) {
        _browse->picked[_browse->picked_count++] = _p->codepoint;
        fude_browse_changed(_browse);
    }
}

const u32* fude_browse_list(const fude_browse* _browse) {
    return (const u32*)_browse->list.memory;
}

u32 fude_browse_count(const fude_browse* _browse) {
    return (u32)rde_arr_length(&_browse->list);
}

b8 fude_browse_take_tap(fude_browse* _browse, u32* _position) {
    if(_browse->tapped < 0) {
        return false;
    }
    *_position      = (u32)_browse->tapped;
    _browse->tapped = -1;
    return true;
}

// --- the list ----------------------------------------------------------------------

RDE_INTERNAL b8 fude_browse_pad_used(const fude_browse* _browse) {
    return _browse->drawing && fude_ink_alive_strokes(&_browse->pad) > 0;
}

RDE_INTERNAL void fude_browse_recompute(fude_browse* _browse) {
    rde_arr_clear(&_browse->list);

    if(fude_browse_pad_used(_browse)) {
        // Drawn: what ML Kit reads it as first, when it has (recognize.h: the
        // same as Check), then best resemblance — the filter applying to both.
        fude_match_result _matched[FUDE_BROWSE_MATCHES];
        fude_match_result _results[FUDE_BROWSE_MATCHES];
        const u32 _found = fude_match_rank(_browse->db, &_browse->catalog, _browse->filter, &_browse->pad, _matched, FUDE_BROWSE_MATCHES);
        const u32 _n     = fude_recognize_candidates(_browse->db, &_browse->recognition, 0, 1, &_browse->catalog, _browse->filter,
                                                     _matched, _found, _results, FUDE_BROWSE_MATCHES);
        for(u32 _i = 0; _i < _n; _i++) {
            rde_arr_add(&_browse->list, &_results[_i].record);
        }
    } else {
        const u32 _n = fude_catalog_query(&_browse->catalog, _browse->filter, _browse->sort, _browse->search);
        if(_n > 0) {
            memcpy(rde_arr_add_n(&_browse->list, _n), fude_catalog_results(&_browse->catalog), (usize)_n * sizeof(u32));
        }
    }

    if(_browse->picking && fude_browse_parts_available(_browse)) {
        // Only characters with every picked part (a character counts as its own part).
        u32* _list = (u32*)_browse->list.memory;
        u32  _kept = 0;
        memset(_browse->_marks, 0, FUDE_BROWSE_MARKS);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_browse->list); _i++) {
            u32             _parts[FUDE_KANJI_MAX_PARTS + 1];
            u32             _n = fude_kanji_parts(_browse->db, _list[_i], _parts, FUDE_KANJI_MAX_PARTS);
            fude_kanji_info _info;
            if(fude_kanji_at(_browse->db, _list[_i], &_info)) {
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
                if(_parts[_j] < FUDE_BROWSE_MARKS) {
                    _browse->_marks[_parts[_j]] = 1;   // what is left to pick
                }
            }
        }
        _browse->list.count = _kept;   // rde_arr has no truncate: rde_arr_clear's operation, to a length

        fude_browse_part* _parts = (fude_browse_part*)_browse->parts.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_browse->parts); _i++) {
            _parts[_i].usable = _browse->_marks[_parts[_i].codepoint] != 0 || fude_browse_is_picked(_browse, _parts[_i].codepoint);
        }
    }

    _browse->dirty = false;
}

// --- input -------------------------------------------------------------------------

RDE_INTERNAL b8 fude_browse_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y;
}

// Screen → pad-local (from its bottom-left corner), clamped to the pad.
RDE_INTERNAL rde_vec_2F fude_browse_pad_local(const fude_browse* _browse, rde_vec_2F _screen) {
    return (rde_vec_2F){
        rde_math_clamp_f32(_screen.x, _browse->pad_min.x, _browse->pad_max.x) - _browse->pad_min.x,
        rde_math_clamp_f32(_screen.y, _browse->pad_min.y, _browse->pad_max.y) - _browse->pad_min.y
    };
}

void fude_browse_pointer_down(fude_browse* _browse, rde_vec_2F _screen, b8 _pen, f64 _time) {
    _browse->on_pad = _browse->drawing && _pen && fude_browse_inside(_screen, _browse->pad_min, _browse->pad_max);

    if(_browse->on_pad) {
        _browse->pad.zoom = 1.0f;
        fude_ink_begin(&_browse->pad, fude_browse_pad_local(_browse, _screen), true, false);
        return;
    }

    _browse->on_parts = _browse->picking && fude_browse_inside(_screen, _browse->panel_min, _browse->panel_max);
    fude_scroller_down(_browse->on_parts ? &_browse->parts_scroller : &_browse->scroller, _screen, _time);
}

void fude_browse_pointer_moved(fude_browse* _browse, rde_vec_2F _screen, f64 _time) {
    if(_browse->on_pad) {
        fude_ink_extend(&_browse->pad, fude_browse_pad_local(_browse, _screen));
        return;
    }

    fude_scroller_moved(_browse->on_parts ? &_browse->parts_scroller : &_browse->scroller, _screen, _time);
}

void fude_browse_pointer_up(fude_browse* _browse, f64 _time) {
    if(_browse->on_pad) {
        fude_ink_end(&_browse->pad);
        _browse->on_pad          = false;
        _browse->dirty           = true;   // re-rank with the new stroke
        _browse->scroller.offset = 0.0f;
        fude_recognize_forget(&_browse->recognition);   // an answer for the old drawing no longer holds
        _browse->_recognize_wanted = true;
        return;
    }

    if(_browse->on_parts) {
        _browse->on_parts = false;
        fude_scroller_up(&_browse->parts_scroller, _time);

        rde_vec_2F _at;
        if(fude_scroller_take_tap(&_browse->parts_scroller, &_at) && fude_browse_inside(_at, _browse->panel_min, _browse->panel_max)) {
            const f32                   _content_y = _browse->panel_max.y - _at.y + _browse->parts_scroller.offset;
            const fude_browse_part_hit* _hits      = (const fude_browse_part_hit*)_browse->part_hits.memory;
            for(u32 _i = 0; _i < (u32)rde_arr_length(&_browse->part_hits); _i++) {
                if(_at.x >= _hits[_i].x && _at.x < _hits[_i].x + _hits[_i].size && _content_y >= _hits[_i].y && _content_y < _hits[_i].y + _hits[_i].size) {
                    fude_browse_toggle_part(_browse, _hits[_i].part);
                    break;
                }
            }
        }
        return;
    }

    fude_scroller_up(&_browse->scroller, _time);

    rde_vec_2F _at;
    if(fude_scroller_take_tap(&_browse->scroller, &_at) &&
       fude_browse_inside(_at, _browse->grid_min, _browse->grid_max) && _browse->cell > 0.0f) {
        const u32 _col = (u32)((_at.x - _browse->grid_min.x) / _browse->cell);
        const u32 _row = (u32)((_browse->grid_max.y - _at.y + _browse->scroller.offset) / _browse->cell);
        const u32 _i   = _row * _browse->columns + _col;
        if(_col < _browse->columns && _i < fude_browse_count(_browse)) {
            _browse->tapped = (i32)_i;
        }
    }
}

void fude_browse_update(fude_browse* _browse, f32 _dt) {
    if(!_browse->open) {
        return;
    }
    // Marks changed (the viewer's Study, an exam): the Studying and Known lists with them.
    if(fude_marks_revision() != _browse->_marks_seen) {
        _browse->_marks_seen = fude_marks_revision();
        _browse->dirty       = _browse->dirty || _browse->filter == fude_filter_studying() || _browse->filter == fude_filter_known();
    }
    // ML Kit on the pad: asked once a stroke ends; its answer ranks the list again.
    if(_browse->_recognize_wanted) {
        if(!fude_browse_pad_used(_browse) || !fude_recognize_available() || fude_recognize_start(&_browse->recognition, &_browse->pad)) {
            _browse->_recognize_wanted = false;
        }
    }
    if(fude_recognize_poll(&_browse->recognition)) {
        _browse->dirty = true;
    }
    if(_browse->dirty) {
        fude_browse_recompute(_browse);
    }

    const u32 _rows = _browse->columns > 0 ? (fude_browse_count(_browse) + _browse->columns - 1u) / _browse->columns : 0u;
    fude_scroller_update(&_browse->scroller, _dt, (f32)_rows * _browse->cell, _browse->grid_max.y - _browse->grid_min.y);
    if(_browse->picking) {
        fude_scroller_update(&_browse->parts_scroller, _dt, _browse->parts_height, _browse->panel_max.y - _browse->panel_min.y);
    }
}

// --- drawing -----------------------------------------------------------------------

RDE_INTERNAL void fude_browse_draw_pad(fude_browse* _browse, rde_vec_2F _tl, f32 _size, rde_vec_2F _screen_half) {
    fude_glyph_box(_tl, _size);   // the same writing square as Practice's and Check's

    _browse->pad_min = (rde_vec_2F){ _tl.x, _tl.y - _size };
    _browse->pad_max = (rde_vec_2F){ _tl.x + _size, _tl.y };

    // The pad's strokes are stored from its bottom-left corner.
    fude_ink_render(&_browse->pad, _browse->pad_min, 1.0f, _screen_half, rde_engine_get_time_now(), false);
}

// The parts panel: the sheet, and on it every offered part by stroke count — a
// small stroke number where a new count starts — picked ones marked, ones no
// listed character has dimmed. Scrolls on its own.
RDE_INTERNAL void fude_browse_draw_parts(fude_browse* _browse, rde_window* _window, rde_font* _font, f32 _font_px,
                                         f32 _left, f32 _right, f32 _top, f32 _height) {
    const fude_theme* _theme = fude_theme_active();
    _browse->panel_min = (rde_vec_2F){ _left, _top - _height };
    _browse->panel_max = (rde_vec_2F){ _right, _top };

    fude_draw_card((rde_vec_2F){ _left, _top - _height }, (rde_vec_2F){ _right, _top }, 14.0f, _theme->surface, _theme->outline);

    rde_arr_clear(&_browse->part_hits);
    const f32 _cell   = FUDE_BROWSE_PART_CELL;
    const f32 _inner  = _left + 6.0f;
    const u32 _cols   = (u32)fmaxf(1.0f, floorf((_right - _left - 12.0f) / _cell));
    const f32 _scroll = _browse->parts_scroller.offset;

    rde_rendering_begin_clipping_rect(_window,
                                      (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)(_top - _height * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left - 2.0f), (u32)(_height - 2.0f) });

    const fude_browse_part* _parts   = (const fude_browse_part*)_browse->parts.memory;
    u32                     _slot    = 0;
    u32                     _strokes = 0;
    c8                      _label[8];
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_browse->parts); _i++) {
        const fude_browse_part* _p = &_parts[_i];

        if(_p->strokes != _strokes) {   // a new stroke count: its number, in a cell of its own
            _strokes = _p->strokes;
            const f32 _cy = 4.0f + (f32)(_slot / _cols) * _cell;
            const f32 _y  = _top - (_cy - _scroll);
            if(_y > _top - _height - _cell && _y - _cell < _top) {
                snprintf(_label, sizeof(_label), "%u", _strokes);
                fude_draw_text(_font, _font_px, _label, _inner + (f32)(_slot % _cols) * _cell + 12.0f, _y - _cell * 0.62f, 14.0f, _theme->select);
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

        const fude_browse_part_hit _hit = { _x, _cy, _cell, _i };
        rde_arr_add(&_browse->part_hits, &_hit);

        if(fude_browse_is_picked(_browse, _p->codepoint)) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x + _cell * 0.5f, _y - _cell * 0.5f }, (rde_vec_2F){ _cell - 4.0f, _cell - 4.0f }, _theme->select_fill);
            fude_draw_outline((rde_vec_2F){ _x + 2.0f, _y - _cell + 2.0f }, (rde_vec_2F){ _x + _cell - 2.0f, _y - 2.0f }, 1.2f, _theme->select);
        }
        const f32 _glyph = _cell * 0.7f;
        fude_glyph_character(&_browse->glyph, _p->codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - (_cell - _glyph) * 0.5f }, _glyph,
                             _p->usable ? _theme->ink : _theme->ghost);
    }
    _browse->parts_height = 8.0f + (f32)((_slot + _cols - 1u) / _cols) * _cell;

    rde_rendering_end_clipping_rect();
}

// What a cell shows under the character, depending on how the list is ordered.
RDE_INTERNAL void fude_browse_caption(fude_browse* _browse, const fude_kanji_info* _info, rde_font* _font, f32 _font_px,
                                      f32 _x, f32 _y, f32 _cell) {
    c8 _line[64] = "";

    if(fude_browse_pad_used(_browse)) {
        return;
    }

    const b8        _by_reading = _browse->sort >= fude_sort_reading(0u) && _browse->sort < fude_sort_meaning();
    const c8* const _romaji     = fude_lang_latin(_info->codepoint);

    if(_by_reading && _romaji == NULL) {
        // The reading it is sorted by (first reading only). One without — 校 has
        // no kun, 込 no on — shows its other kind instead (Japanese writes on in
        // katakana and kun in hiragana, so they never read as each other).
        const u32 _kind  = _browse->sort - fude_sort_reading(0u);
        const c8* _this  = fude_kanji_reading(_browse->db, _info, _kind);
        const c8* _other = fude_kanji_reading(_browse->db, _info, 1u - _kind);
        const c8* _all   = _this[0] != 0 ? _this : _other;
        if(_all[0] != 0) {
            c8 _first[64];
            const c8* _sep = strstr(_all, "、");
            const usize _n = _sep != NULL ? (usize)(_sep - _all) : strlen(_all);
            snprintf(_first, sizeof(_first), "%.*s", (int)(_n < sizeof(_first) - 1 ? _n : sizeof(_first) - 1), _all);
            // Hangul packs its letters in one square, too small written from its
            // strokes at a caption's size: in the text font, as the captions beside it.
            const c8* _p  = _first;
            const u32 _cp = fude_utf8_next(&_p);
            if(_cp >= 0xAC00u && _cp <= 0xD7A3u) {
                fude_draw_text(_font, _font_px, _first, _x + 6.0f, _y, FUDE_BROWSE_CAPTION_PX, fude_theme_active()->text_soft);
            } else {
                fude_glyph_reading(&_browse->glyph, _first, (rde_vec_2F){ _x + 6.0f, _y + 10.0f }, 14.0f, _x + _cell - 4.0f, fude_theme_active()->text_soft, fude_theme_active()->text_soft);
            }
            return;
        }
        // No reading at all: its meaning, below.
    }

    if(_browse->sort == FUDE_SORT_STROKES) {
        snprintf(_line, sizeof(_line), "%u", _info->strokes);
    } else if(_romaji != NULL) {
        snprintf(_line, sizeof(_line), "%s", _romaji);   // a script's letters have no readings or meanings: their Latin names
    } else if(_by_reading || _browse->sort == fude_sort_meaning() || _browse->search[0] != 0) {
        const c8* _m = fude_kanji_meanings(_browse->db, _info);
        const c8* _comma = strstr(_m, ", ");
        const usize _n = _comma != NULL ? (usize)(_comma - _m) : strlen(_m);
        snprintf(_line, sizeof(_line), "%.*s", (int)_n, _m);   // its first meaning, whole (shrunk to fit, below)
    } else if(_info->level != 0) {
        fude_lang_level_name(_info->level, false, _line, sizeof(_line));
    }

    if(_line[0] != 0) {
        // Whole: a long one (Thai's "pho samphao", a meaning) smaller, never into the next cell.
        fude_draw_text_whole(_font, _font_px, _line, _x + 6.0f, _y + FUDE_BROWSE_CAPTION_PX * 0.38f, FUDE_BROWSE_CAPTION_PX, _cell - 10.0f, 1u,
                             fude_theme_active()->text_soft);
    }
}

void fude_browse_render(fude_browse* _browse, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom_edge) {
    if(!_browse->open) {
        return;
    }
    if(_browse->dirty) {
        fude_browse_recompute(_browse);
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _hw     = (f32)_size.x * 0.5f;
    const f32        _hh     = (f32)_size.y * 0.5f;
    const f32        _left   = -_hw + (f32)_insets.x + FUDE_BROWSE_MARGIN;
    const f32        _right  = _hw - (f32)_insets.z - FUDE_BROWSE_MARGIN;
    const f32        _bottom = fmaxf(_bottom_edge, -_hh + (f32)_insets.w + FUDE_BROWSE_MARGIN);   // above the bottom row

    // --- status ------------------------------------------------------------------------
    c8 _status[160];
    const u32 _count = fude_browse_count(_browse);
    if(fude_browse_pad_used(_browse)) {
        FUDE_TEXTF(_status, FUDE_TEXT_BROWSE_BEST, FUDE_TN(_count));
    } else if(_browse->picking && _browse->picked_count > 0) {
        FUDE_TEXTF(_status, _browse->picked_count == 1 ? FUDE_TEXT_BROWSE_PARTS_ONE : FUDE_TEXT_BROWSE_PARTS_MANY, FUDE_TN(_count));
    } else if(_browse->picking) {
        snprintf(_status, sizeof(_status), "%s", fude_text(FUDE_TEXT_BROWSE_PARTS_HINT));
    } else if(_browse->drawing) {
        snprintf(_status, sizeof(_status), "%s", fude_text(FUDE_TEXT_BROWSE_DRAW_HINT));
    } else {
        FUDE_TEXTF(_status, FUDE_TEXT_CHARACTERS_N, FUDE_TN(_count));
    }
    fude_draw_text(_font, _font_px, _status, _left, _top - 24.0f, 15.0f, fude_theme_active()->text);

    f32 _grid_top = _top - FUDE_BROWSE_STATUS_H;

    // --- the pad -------------------------------------------------------------------------
    if(_browse->drawing) {
        const f32 _pad = fminf(FUDE_BROWSE_PAD, _right - _left);
        fude_browse_draw_pad(_browse, (rde_vec_2F){ _left, _grid_top - 4.0f }, _pad, (rde_vec_2F){ _hw, _hh });
        _grid_top -= _pad + 20.0f;
    } else {
        _browse->pad_min = _browse->pad_max = (rde_vec_2F){ 0.0f, 0.0f };
    }

    // --- the parts panel -----------------------------------------------------------------
    if(_browse->picking) {
        const f32 _panel = fminf(FUDE_BROWSE_PANEL_H, (_grid_top - _bottom) * 0.5f);
        fude_browse_draw_parts(_browse, _window, _font, _font_px, _left, _right, _grid_top - 4.0f, _panel);
        _grid_top -= _panel + 16.0f;
    } else {
        _browse->panel_min = _browse->panel_max = (rde_vec_2F){ 0.0f, 0.0f };
    }

    // --- the grid ------------------------------------------------------------------------
    const f32 _width   = _right - _left;
    const u32 _columns = (u32)fmaxf(1.0f, floorf(_width / FUDE_BROWSE_CELL_MIN));
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
    const u32* _list     = fude_browse_list(_browse);

    for(u32 _row = _first_row; _row <= _last_row; _row++) {
        const f32 _y = _grid_top - ((f32)_row * _cell - _scroll);   // the row's top edge

        for(u32 _col = 0; _col < _columns; _col++) {
            const u32 _i = _row * _columns + _col;
            if(_i >= _count) {
                break;
            }

            fude_kanji_info _info;
            if(!fude_kanji_at(_browse->db, _list[_i], &_info)) {
                continue;
            }

            const f32 _x      = _left + (f32)_col * _cell;
            const b8  _select = _browse->selection != NULL && _browse->selection->active;
            const b8  _ticked = _select && fude_selection_has(_browse->selection, _list[_i]);
            if(_select) {
                fude_selection_draw_behind(_ticked, (rde_vec_2F){ _x, _y }, _cell);
            }
            fude_draw_line((rde_vec_2F){ _x + 4.0f, _y - _cell }, (rde_vec_2F){ _x + _cell - 4.0f, _y - _cell }, 0.5f, fude_theme_active()->line);
            fude_glyph_character(&_browse->glyph, _info.codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - _cell * 0.08f }, _glyph, fude_theme_active()->ink);
            fude_browse_caption(_browse, &_info, _font, _font_px, _x, _y - _cell + 8.0f, _cell);
            if(_select) {
                fude_selection_draw_tick(_ticked, (rde_vec_2F){ _x, _y }, _cell);
            }
            fude_selection_draw_mark(fude_marks_get(_info.codepoint), (rde_vec_2F){ _x, _y }, _cell, _select);
        }
    }

    rde_rendering_end_clipping_rect();
}

// --- the screen (screen.h): its bar and rows, and what they do -----------------------------

#include "study/app/study.h"
#include "drawing/widgets/icons.h"
#include "drawing/widgets/filterbar.h"

FUDE_SCREEN_ADAPTERS_PEN(fude_browse, fude_browse)

// Under its bar (the frame's top is its edge), over its row.
RDE_INTERNAL void fude_browse_screen_render(void* _self, const fude_screen_frame* _f) {
    fude_browse_render((fude_browse*)_self, _f->window, _f->font, _f->font_px, _f->top, _f->bottom);
}

// A tapped character opens the viewer, walking the grid's list; in Select mode
// it is ticked (or unticked) instead.
RDE_INTERNAL void fude_browse_screen_update(fude_app* _app, void* _self, f32 _dt) {
    fude_study* _study = FUDE_STUDY(_app);
    fude_browse* _browse = (fude_browse*)_self;
    fude_browse_update(_browse, _dt);
    u32 _position = 0;
    if(fude_browse_take_tap(_browse, &_position)) {
        if(_study->selection->active) { fude_selection_toggle(_study->selection, fude_browse_list(_browse)[_position]); }
        else                        { fude_study_view(_app, fude_browse_list(_browse), fude_browse_count(_browse), _position); }
    }
}

// Its list as it is filtered, sorted and searched, in that order.
RDE_INTERNAL u32 fude_browse_screen_in_view(void* _self, const u32** _records) {
    *_records = fude_browse_list((const fude_browse*)_self);
    return fude_browse_count((const fude_browse*)_self);
}

// --- its bar (filterbar.h) ---

// The filters' chips (catalog.h's order): All, the language's groups, its levels
// (their short names, the same in every language: "N5"), Studying, Known.
RDE_INTERNAL const c8* fude_browse_filter_label(u32 _i) {
    static c8 _levels[FUDE_LANG_LEVELS][16];
    const u32 _groups = fude_lang_group_count();
    if(_i == FUDE_FILTER_ALL) {
        return fude_text(FUDE_TEXT_ALL);
    }
    if(_i <= _groups) {
        return fude_text((FUDE_TEXT_)fude_lang_group_name(_i - 1u));
    }
    if(_i < fude_filter_studying()) {
        const u32 _level = _i - 1u - _groups;
        fude_lang_level_name(fude_lang_level_value(_level), false, _levels[_level], sizeof(_levels[_level]));
        return _levels[_level];
    }
    return fude_text(_i == fude_filter_studying() ? FUDE_TEXT_STUDYING : FUDE_TEXT_KNOWN);
}
// The sorts' chips: Default, Strokes, each of the language's readings, Meaning.
RDE_INTERNAL const c8* fude_browse_sort_label(u32 _i) {
    if(_i == FUDE_SORT_DEFAULT)  { return fude_text(FUDE_TEXT_SORT_DEFAULT); }
    if(_i == FUDE_SORT_STROKES)  { return fude_text(FUDE_TEXT_SORT_STROKES); }
    if(_i < fude_sort_meaning()) { return fude_text((FUDE_TEXT_)fude_lang_reading_name(_i - fude_sort_reading(0u))); }
    return fude_text(FUDE_TEXT_SORT_MEANING);
}
RDE_INTERNAL u32       fude_browse_filter_chosen(const void* _self)   { return (u32)((const fude_browse*)_self)->filter; }
RDE_INTERNAL u32       fude_browse_sort_chosen(const void* _self)     { return (u32)((const fude_browse*)_self)->sort; }
RDE_INTERNAL void      fude_browse_filter_choose(void* _self, u32 _i) { fude_browse_set_filter((fude_browse*)_self, (FUDE_FILTER_)_i); }
RDE_INTERNAL void      fude_browse_sort_choose(void* _self, u32 _i)   { fude_browse_set_sort((fude_browse*)_self, (FUDE_SORT_)_i); }
RDE_INTERNAL void      fude_browse_search(void* _self, const c8* _text) { fude_browse_set_search((fude_browse*)_self, _text); }
RDE_INTERNAL b8        fude_browse_drawing_on(const void* _self)      { return ((const fude_browse*)_self)->drawing; }
RDE_INTERNAL b8        fude_browse_picking_on(const void* _self)      { return ((const fude_browse*)_self)->picking; }
RDE_INTERNAL b8        fude_browse_has_parts(const void* _self)       { return fude_browse_parts_available((const fude_browse*)_self); }   // data baked before parts: none
// Draw: the drawing pad; Parts: the panel of parts instead (one or the other).
RDE_INTERNAL void fude_browse_toggle_drawing(void* _self) { fude_browse* _b = (fude_browse*)_self; fude_browse_set_drawing(_b, !_b->drawing); }
RDE_INTERNAL void fude_browse_toggle_picking(void* _self) { fude_browse* _b = (fude_browse*)_self; fude_browse_set_picking(_b, !_b->picking); }
// Clear: the drawing, the typed search and the picked parts.
RDE_INTERNAL void fude_browse_clear_all(void* _self) {
    fude_browse* _browse = (fude_browse*)_self;
    fude_browse_set_search(_browse, "");
    fude_browse_clear_pad(_browse);
    fude_browse_clear_parts(_browse);
}

static const fude_filterbar_def FUDE_BROWSE_BAR = {
    .chips = { { 0u, fude_filter_count, fude_browse_filter_label, fude_browse_filter_chosen, fude_browse_filter_choose },
               { 0u, fude_sort_count,   fude_browse_sort_label,   fude_browse_sort_chosen,   fude_browse_sort_choose } },
    .chip_rows   = 2u,
    .search_hint = FUDE_TEXT_SEARCH_HINT,
    .search      = fude_browse_search,
    .toggles = { { FUDE_TEXT_BROWSE_DRAW,  FUDE_ICON_SCRIBBLE, 15.0f, fude_browse_drawing_on, fude_browse_toggle_drawing, NULL,                  false },
                 { FUDE_TEXT_BROWSE_PARTS, "\xE9\x83\xA8",     13.0f, fude_browse_picking_on, fude_browse_toggle_picking, fude_browse_has_parts, false },   // 部
                 { FUDE_TEXT_CLEAR,        FUDE_ICON_CLOSE,    14.0f, NULL,                   fude_browse_clear_all,      NULL,                  true } },
    .toggle_count = 3u,
};

// --- its rows ---

FUDE_ROW_CALL(fude_browse_row_close, fude_browse, fude_browse_close)

RDE_INTERNAL void fude_browse_row_select(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    FUDE_STUDY(_app)->selection->active = true;
}

// Practice: its list as a set.
RDE_INTERNAL void fude_browse_row_practice(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    const u32* _records;
    const u32  _n = fude_browse_screen_in_view(_self, &_records);
    fude_study_practice_set(_app, _records, _n);
}

// Its own row; in Select mode, Select's (app.h).
enum { FUDE_BROWSE_ROW_MAIN = 0, FUDE_BROWSE_ROW_SELECT };
static const fude_row_button FUDE_BROWSE_BUTTONS[] = {
    { FUDE_TEXT_SELECT,   FUDE_ICON_SELECT, fude_browse_row_select,   0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE, FUDE_ICON_PEN,    fude_browse_row_practice, 0, FUDE_ROW_PRIMARY, false, NULL },
    { FUDE_TEXT_CLOSE,    FUDE_ICON_CLOSE,  fude_browse_row_close,    0, FUDE_ROW_QUIET,   false, NULL },
};
static const fude_row_def FUDE_BROWSE_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_BROWSE_BUTTONS), { FUDE_STUDY_SELECT_BUTTONS, FUDE_STUDY_SELECT_COUNT } };

RDE_INTERNAL u32 fude_browse_screen_row(const void* _self) {
    const fude_browse* _browse = (const fude_browse*)_self;
    return _browse->selection != NULL && _browse->selection->active ? FUDE_BROWSE_ROW_SELECT : FUDE_BROWSE_ROW_MAIN;
}

RDE_INTERNAL void fude_browse_screen_faces(const void* _self, u32 _row, fude_row_face* _faces) {
    if(_row == FUDE_BROWSE_ROW_SELECT) {
        fude_study_select_faces(((const fude_browse*)_self)->selection, _faces);
    }
}

const fude_screen FUDE_BROWSE_SCREEN = {
    .name = "browse", .input = FUDE_SCREEN_INPUT_POINT, .selects = true,
    .is_open = fude_browse_screen_is_open, .close = fude_browse_screen_close,
    .update = fude_browse_screen_update, .render = fude_browse_screen_render,
    .pointer_down = fude_browse_screen_down, .pointer_moved = fude_browse_screen_moved, .pointer_up = fude_browse_screen_up,
    .rows = FUDE_BROWSE_BUTTON_ROWS, .row_count = 2u, .row = fude_browse_screen_row, .faces = fude_browse_screen_faces,
    .bar = &FUDE_BROWSE_BAR,
    .field_hint = FUDE_TEXT_COUNT, .in_view = fude_browse_screen_in_view,
};
