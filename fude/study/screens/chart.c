// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/screens/chart.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/base/draw.h"
#include "study/models/marks.h"

// ===========================================================================
// See chart.h.
//
// The list (what the viewer walks) is built once, in fude_chart_init, and never changes: position_of maps each table
// cell (row * FUDE_LANG_CHART_COLUMNS + column) to its place in the list, or UINT32_MAX when the cell is empty or its
// letter is missing from the character data. The layout (cells and subtitles) is worked out again only when the width
// or the UI's direction changes, from the first fude_chart_render on; drawing and tapping then just read it.
//
// The layout's sizes below are in screen points.
// ===========================================================================

#define FUDE_CHART_MARGIN  16.0f   // The space from the screen's sides, inside its safe area.
#define FUDE_CHART_TITLE_H 48.0f   // The height of a section's title, over its first row.
#define FUDE_CHART_SUB_H   40.0f   // The height of a block's subtitle, over its first row, with half a gap above it.
#define FUDE_CHART_GAP     20.0f   // The chart's end has twice this under its last row.

// Copies row _r of the language's chart into _out, from the language's row function when it has one, else from its
// table. Returns false past the last row.
RDE_INTERNAL b8 fude_chart_row_at(const fude_lang_chart_def* _def, u32 _r, fude_lang_chart_row* _out) {
    if(_def->row != NULL) {
        return _def->row(_r, _out);
    }
    if(_r >= _def->row_count) {
        return false;
    }
    *_out = _def->rows[_r];
    return true;
}

// Counts the chart's rows by asking for them until there are no more, since a language with a row function does not
// say how many it has.
RDE_INTERNAL u32 fude_chart_row_count(const fude_lang_chart_def* _def) {
    fude_lang_chart_row _row;
    u32                 _n = 0;
    while(fude_chart_row_at(_def, _n, &_row)) {
        _n++;
    }
    return _n;
}

// Lists the cell at row _r, column _c, whose letter is code point _cp: when the character data has the letter, it
// gets the next place in the list, and position_of records that place.
RDE_INTERNAL void fude_chart_list_cell(fude_chart* _chart, u32 _r, u32 _c, u32 _cp) {
    u32 _record = 0;
    if(_cp == 0u || !fude_kanji_find_index(_chart->db, _cp, &_record)) {
        return;   // An empty cell, or a letter the data lacks, gets no place in the list.
    }
    ((u32*)_chart->position_of.memory)[_r * FUDE_LANG_CHART_COLUMNS + _c] = (u32)rde_arr_length(&_chart->list);
    rde_arr_add(&_chart->list, &_record);
    rde_arr_add(&_chart->codepoints, &_cp);
}

// --- The screen's row of buttons: one per section, made from the language's sections (below) -------------------------

// Makes the screen's rows of buttons from the language's sections, the first time it is called (defined below).
RDE_INTERNAL void fude_chart_buttons_make(const fude_lang_chart_def* _def);

// --- Life ------------------------------------------------------------------------------------------------------------

// Builds the list from the language's table (see the top of this file); the layout waits for the first render.
void fude_chart_init(fude_chart* _chart, const fude_kanji_db* _db) {
    rde_memset(_chart, 0, sizeof(*_chart));
    fude_kit_scroller_init(&_chart->scroller);
    _chart->db     = _db;
    _chart->def    = fude_lang_chart();
    _chart->tapped = -1;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _chart->list        = rde_arr_new(sizeof(u32), _heap);
    _chart->codepoints  = rde_arr_new(sizeof(u32), _heap);
    _chart->position_of = rde_arr_new(sizeof(u32), _heap);
    _chart->cells       = rde_arr_new(sizeof(fude_chart_cell), _heap);
    _chart->subtitles   = rde_arr_new(sizeof(fude_chart_subtitle), _heap);
    fude_glyph_init(&_chart->glyph, _db);
    fude_chart_buttons_make(_chart->def);

    if(_db == NULL || _chart->def == NULL) {
        return;
    }
    const fude_lang_chart_def* _def  = _chart->def;
    const u32              _rows = fude_chart_row_count(_def);
    rde_arr_resize(&_chart->position_of, (usize)_rows * FUDE_LANG_CHART_COLUMNS);
    // Every byte set to 0xFF makes every entry UINT32_MAX, which means the cell is not listed.
    rde_memset(_chart->position_of.memory, 0xFF, (usize)_rows * FUDE_LANG_CHART_COLUMNS * sizeof(u32));   // Not listed.

    // The list is built block by block, where a block is a section's rows up to the next subtitle. A block's letters
    // are listed along its rows, or down its columns when it is marked so.
    u32 _section = 0;
    u32 _r       = 0;
    fude_lang_chart_row _row;
    while(fude_chart_row_at(_def, _r, &_row)) {
        while(_section <= _row.section && _section < FUDE_LANG_CHART_SECTIONS) {
            _chart->section_first[_section++] = (u32)rde_arr_length(&_chart->list);
        }
        // Find where the block ends: it is this row and the rows after it in the same section, up to one that starts a
        // block of its own.
        u32 _end = _r + 1u;
        fude_lang_chart_row _next;
        while(fude_chart_row_at(_def, _end, &_next) && _next.section == _row.section && (_next.flags & FUDE_LANG_CHART_ROW_SUBTITLE) == 0u) {
            _end++;
        }
        if((_row.flags & FUDE_LANG_CHART_ROW_BY_COLUMN) != 0u) {
            for(u32 _c = 0; _c < FUDE_LANG_CHART_COLUMNS; _c++) {
                for(u32 _k = _r; _k < _end; _k++) {
                    fude_lang_chart_row _in;
                    fude_chart_row_at(_def, _k, &_in);
                    fude_chart_list_cell(_chart, _k, _c, _in.cp[_c]);
                }
            }
        } else {
            for(u32 _k = _r; _k < _end; _k++) {
                fude_lang_chart_row _in;
                fude_chart_row_at(_def, _k, &_in);
                for(u32 _c = 0; _c < FUDE_LANG_CHART_COLUMNS; _c++) {
                    fude_chart_list_cell(_chart, _k, _c, _in.cp[_c]);
                }
            }
        }
        _r = _end;
    }
    while(_section <= FUDE_LANG_CHART_SECTIONS) {
        _chart->section_first[_section++] = (u32)rde_arr_length(&_chart->list);
    }
}

// Frees the arrays that were made (none are when init was never called) and the glyph cache, and zeroes the chart.
void fude_chart_destroy(fude_chart* _chart) {
    rde_arr* _arrays[] = { &_chart->list, &_chart->codepoints, &_chart->position_of, &_chart->cells, &_chart->subtitles };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    fude_glyph_destroy(&_chart->glyph);
    rde_memset(_chart, 0, sizeof(*_chart));
}

// Opens the chart, but only when it lists at least one letter, so an empty chart never shows.
void fude_chart_open(fude_chart* _chart) {
    if(rde_arr_length(&_chart->list) > 0) {
        _chart->open = true;
    }
}

// Closes the chart and stops any scroll still gliding.
void fude_chart_close(fude_chart* _chart) {
    _chart->open = false;
    rde_ui_scroller_stop(&_chart->scroller);
}

// Scrolls straight to section _section's title (to the top for a section out of range); the scroller keeps the
// offset within the chart on its next update.
void fude_chart_jump(fude_chart* _chart, u32 _section) {
    rde_ui_scroller_stop(&_chart->scroller);
    _chart->scroller.offset.y = _section < FUDE_LANG_CHART_SECTIONS ? _chart->section_at[_section] : 0.0f;
}

// See the header: the list's records, owned by the chart.
const u32* fude_chart_list(const fude_chart* _chart) {
    return (const u32*)_chart->list.memory;
}

// See the header: how many letters the list has.
u32 fude_chart_count(const fude_chart* _chart) {
    return (u32)rde_arr_length(&_chart->list);
}

// Hands out the last tap once: tapped goes back to -1, which means no tap is waiting.
b8 fude_chart_take_tap(fude_chart* _chart, u32* _position) {
    if(_chart->tapped < 0) {
        return false;
    }
    *_position     = (u32)_chart->tapped;
    _chart->tapped = -1;
    return true;
}

// Returns the section mostly in view. The first section is the answer until a later title comes up into the top third
// of the view; a section whose title is at 0 has not been laid out yet and is skipped.
u32 fude_chart_section_in_view(const fude_chart* _chart) {
    // The view's height; the chart picks the last section whose title is in its top third.
    const f32 _view  = _chart->view_top - _chart->view_bottom;
    const u32 _count = _chart->def != NULL ? _chart->def->section_count : 0u;
    u32       _in    = 0;
    for(u32 _s = 1; _s < _count; _s++) {
        if(_chart->section_at[_s] > 0.0f && _chart->scroller.offset.y + _view / 3.0f >= _chart->section_at[_s]) {
            _in = _s;
        }
    }
    return _in;
}

// Gives where section _section's letters are in the list. A section out of range is taken as the first one.
void fude_chart_section_range(const fude_chart* _chart, u32 _section, u32* _first, u32* _count) {
    const u32 _s = _section < FUDE_LANG_CHART_SECTIONS ? _section : 0u;
    *_first = _chart->section_first[_s];
    *_count = _chart->section_first[_s + 1u] - _chart->section_first[_s];
}

// --- Layout ----------------------------------------------------------------------------------------------------------

// Places every cell and subtitle for a width (from _left, in screen x) and for the UI's direction, as chart.h's
// LAYOUT describes, and works out the chart's content height. _y runs down the content from 0 at its top.
RDE_INTERNAL void fude_chart_layout(fude_chart* _chart, f32 _left, f32 _width) {
    rde_arr_clear(&_chart->cells);
    rde_arr_clear(&_chart->subtitles);
    _chart->_laid_out_width = _width;
    _chart->_laid_out_rtl   = fude_draw_is_rtl();

    const fude_lang_chart_def* _def = _chart->def;
    // A line holds as many cells as fit at the language's smallest cell, so a phone's rows wrap. A language with no
    // smallest cell puts the widest row on each line, and its cells get smaller to fit.
    const u32  _columns   = _def->cell_min > 0.0f ? (u32)rde_math_max_f32(1.0f, rde_math_min_f32((f32)_def->columns, rde_math_floor_f32(_width / _def->cell_min)))
                                                  : _def->columns;
    const f32  _cell      = rde_math_min_f32(FUDE_CHART_CELL_MAX, _width / (f32)_columns);
    const u32* _positions = (const u32*)_chart->position_of.memory;
    f32        _y         = 0.0f;
    i32        _section   = -1;
    u32        _at        = 0;   // The next cell's place on its line, counted in cells.
    fude_lang_chart_row _row;
    for(u32 _r = 0; fude_chart_row_at(_def, _r, &_row); _r++) {
        if((i32)_row.section != _section) {
            if(_section >= 0) {
                // The gap before a new section. With flow, the last section's unfinished line is counted first.
                _y += (_def->flow && _at > 0u ? _cell : 0.0f) + _def->section_gap;   // Last line, then the gap.
            }
            _section = (i32)_row.section;
            if(_row.section < FUDE_LANG_CHART_SECTIONS) {
                _chart->section_at[_row.section] = _y;
            }
            _y += FUDE_CHART_TITLE_H;
            _at = 0u;
        }
        if((_row.flags & FUDE_LANG_CHART_ROW_SUBTITLE) != 0u) {
            _y += FUDE_CHART_GAP * 0.5f + FUDE_CHART_SUB_H;
            const fude_chart_subtitle _sub = { _row.subtitle, _y - FUDE_CHART_SUB_H };
            rde_arr_add(&_chart->subtitles, (any)&_sub);
        }
        if(!_def->flow) {
            _at = 0u;   // Without flow, each row starts at the beginning of its own line.
        }
        const u32 _first   = (u32)rde_arr_length(&_chart->cells);
        b8        _wrapped = false;
        for(u32 _c = 0; _c < FUDE_LANG_CHART_COLUMNS; _c++) {
            const u32 _position = _positions[_r * FUDE_LANG_CHART_COLUMNS + _c];
            if(_position == UINT32_MAX) {
                // An empty cell, or a letter the data lacks. When the language asks for it (keep_gaps), it keeps its
                // place, but only while more letters follow it in the row.
                if(_def->keep_gaps) {
                    b8 _more = false;
                    for(u32 _k = _c + 1u; _k < FUDE_LANG_CHART_COLUMNS; _k++) {
                        _more = _more || _row.cp[_k] != 0u;
                    }
                    _at += _more ? 1u : 0u;
                }
                continue;
            }
            if(_at >= _columns) {
                _y      += _cell;   // The line is full, so the row goes on at the start of the next line.
                _at      = 0u;
                _wrapped = true;
            }
            const fude_chart_cell _cell_rect = { .x = _left + (f32)_at * _cell, .y = _y, .size = _cell, .position = _position };
            rde_arr_add(&_chart->cells, (any)&_cell_rect);
            _at++;
        }
        // A right-to-left screen mirrors the whole chart (draw.h), but a mirrored section's rows are already laid out
        // from the right. Reversing such a row here lets the mirror put it back the right way round. A row that
        // wrapped, on a very narrow screen, is left as it is.
        if(_chart->_laid_out_rtl && !_wrapped && _row.section < _def->section_count && _def->sections[_row.section].mirrored) {
            fude_chart_cell* _cells = (fude_chart_cell*)_chart->cells.memory;
            for(u32 _k = _first; _k < (u32)rde_arr_length(&_chart->cells); _k++) {
                const f32 _col = rde_math_round_f32((_cells[_k].x - _left) / _cell);
                _cells[_k].x   = _left + ((f32)_at - 1.0f - _col) * _cell;
            }
        }
        if(!_def->flow) {
            _y += _cell;
        }
    }
    if(_def->flow) {
        _y += _at > 0u ? _cell : 0.0f;   // With flow, the last line still has to be counted.
    }
    _chart->content_height = _y + FUDE_CHART_GAP * 2.0f;
}

// --- Input -----------------------------------------------------------------------------------------------------------

// Starts a drag of the scroller; whether it is a tap is known only when the pointer is lifted.
void fude_chart_pointer_down(fude_chart* _chart, rde_vec_2F _screen, f64 _time) {
    rde_ui_scroller_pointer_down(&_chart->scroller, _screen, _time);
}

// Drags the scroller.
void fude_chart_pointer_moved(fude_chart* _chart, rde_vec_2F _screen, f64 _time) {
    rde_ui_scroller_pointer_moved(&_chart->scroller, _screen, _time);
}

// Ends the drag. When the scroller saw a tap inside the view, the cell under it (if any) is kept in tapped for
// fude_chart_take_tap.
void fude_chart_pointer_up(fude_chart* _chart, f64 _time) {
    rde_ui_scroller_pointer_up(&_chart->scroller, _time);

    rde_vec_2F _at;
    if(!rde_ui_scroller_take_tap(&_chart->scroller, &_at) || _at.y > _chart->view_top || _at.y < _chart->view_bottom) {
        return;
    }

    const f32              _content_y = _chart->view_top - _at.y + _chart->scroller.offset.y;
    const fude_chart_cell* _cells     = (const fude_chart_cell*)_chart->cells.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_chart->cells); _i++) {
        const fude_chart_cell* _c = &_cells[_i];
        if(_at.x >= _c->x && _at.x < _c->x + _c->size && _content_y >= _c->y && _content_y < _c->y + _c->size) {
            _chart->tapped = (i32)_c->position;
            return;
        }
    }
}

// Moves the scroll on (its glide after a fling, and its bounce at the ends) while the chart is open.
void fude_chart_update(fude_chart* _chart, f32 _dt) {
    if(_chart->open) {
        fude_kit_scroller_update(&_chart->scroller, _dt, _chart->content_height, _chart->view_top - _chart->view_bottom);
    }
}

// --- Drawing ---------------------------------------------------------------------------------------------------------

// Lays the chart out again when needed, remembers the view's band for tapping and scrolling, and draws the titles,
// subtitles and the cells in view, clipped to the band.
void fude_chart_render(fude_chart* _chart, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_chart->open || _chart->def == NULL) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // The order is left, top, right, bottom.
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_CHART_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_CHART_MARGIN;

    // The chart is laid out again the first time, after the screen rotates, and after the UI's direction changes.
    if(rde_math_abs_f32(_right - _left - _chart->_laid_out_width) > 0.5f || _chart->_laid_out_rtl != fude_draw_is_rtl()) {
        fude_chart_layout(_chart, _left, _right - _left);   // Lay it out again.
    }
    _chart->view_top    = _top;
    _chart->view_bottom = _bottom;
    if(_top <= _bottom) {
        return;
    }

    const f32 _scroll = _chart->scroller.offset.y;
    rde_rendering_begin_clipping_rect(_window,
                                      (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_top - _bottom) });

    // The sections' titles, then the blocks' subtitles.
    const fude_theme* _theme = fude_theme_active();
    for(u32 _s = 0; _s < _chart->def->section_count && _s < FUDE_LANG_CHART_SECTIONS; _s++) {
        const f32 _y = _top - (_chart->section_at[_s] - _scroll);
        fude_draw_text(_font, _font_px, fude_text((FUDE_TEXT_)_chart->def->sections[_s].title), _left, _y - 32.0f, 24.0f, _theme->text);
    }
    const fude_chart_subtitle* _subs = (const fude_chart_subtitle*)_chart->subtitles.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_chart->subtitles); _i++) {
        const f32 _y = _top - (_subs[_i].y - _scroll);
        fude_draw_text(_font, _font_px, fude_text((FUDE_TEXT_)_subs[_i].text), _left, _y - 26.0f, 15.0f, _theme->text_soft);
    }

    // The letters, each on its card: written from its strokes, with its Latin name under it, then its tick in Select
    // mode and its study mark. Cells outside the band are skipped.
    const fude_chart_cell* _cells = (const fude_chart_cell*)_chart->cells.memory;
    const u32*             _cps   = (const u32*)_chart->codepoints.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_chart->cells); _i++) {
        const fude_chart_cell* _c = &_cells[_i];
        const f32              _y = _top - (_c->y - _scroll);    // The cell's top edge on screen.
        if(_y - _c->size > _top || _y < _bottom) {
            continue;
        }

        const f32 _glyph  = _c->size * 0.58f;
        const u32 _record = ((const u32*)_chart->list.memory)[_c->position];
        const b8  _select = _chart->selection != NULL && _chart->selection->active;
        const b8  _ticked = _select && fude_selection_has(_chart->selection, _record);
        fude_draw_card((rde_vec_2F){ _c->x + 3.0f, _y - _c->size + 3.0f }, (rde_vec_2F){ _c->x + _c->size - 3.0f, _y - 3.0f }, 10.0f,
                       _theme->surface, _theme->outline);
        if(_select) {
            fude_selection_draw_behind(_ticked, (rde_vec_2F){ _c->x, _y }, _c->size);
        }

        fude_glyph_character(&_chart->glyph, _cps[_c->position], (rde_vec_2F){ _c->x + (_c->size - _glyph) * 0.5f, _y - _c->size * 0.08f }, _glyph, _theme->ink);
        const c8* _latin = fude_lang_latin(_cps[_c->position]);
        if(_latin != NULL) {   // Drawn whole, never cut: a long name (Thai's "tho montho") is drawn smaller to fit.
            fude_draw_text_whole(_font, _font_px, _latin, _c->x + 6.0f, _y - _c->size + 8.0f + 13.0f * 0.38f, 13.0f, _c->size - 12.0f, 1u, _theme->text_soft);
        }
        if(_select) {
            fude_selection_draw_tick(_ticked, (rde_vec_2F){ _c->x, _y }, _c->size);
        }
        fude_selection_draw_mark(fude_marks_get(_cps[_c->position]), (rde_vec_2F){ _c->x, _y }, _c->size, _select);
    }

    rde_rendering_end_clipping_rect();
}

// --- The screen (screen.h): its rows of buttons, and what they do ----------------------------------------------------

#include "study/app/study.h"
#include "drawing/base/icons.h"

// The screen's is_open, close, pointer and render functions, which call the chart's own (screen.h).
FUDE_SCREEN_ADAPTERS(fude_chart, fude_chart)
FUDE_SCREEN_RENDER(fude_chart, fude_chart)

// Moves the scroll on, then hands on a tapped letter: it opens the viewer, able to walk the whole chart, or in Select
// mode it is ticked (or unticked) instead.
RDE_INTERNAL void fude_chart_screen_update(fude_app* _app, any _self, f32 _dt) {
    fude_study* _study = FUDE_STUDY(_app);
    fude_chart* _chart = (fude_chart*)_self;
    fude_chart_update(_chart, _dt);
    u32 _position = 0;
    if(fude_chart_take_tap(_chart, &_position)) {
        if(_study->selection->active) { fude_selection_toggle(_study->selection, fude_chart_list(_chart)[_position]); }
        else                          { fude_study_view(_app, fude_chart_list(_chart), fude_chart_count(_chart), _position); }
    }
}

// What is in view (screen.h: in_view): the section in view, in chart order. *_records points into the chart's list.
RDE_INTERNAL u32 fude_chart_screen_in_view(any _self, const u32** _records) {
    const fude_chart* _chart = (const fude_chart*)_self;
    u32 _first = 0, _count = 0;
    fude_chart_section_range(_chart, fude_chart_section_in_view(_chart), &_first, &_count);
    *_records = &fude_chart_list(_chart)[_first];
    return _count;
}

// Close closes the chart.
FUDE_ROW_CALL(fude_chart_row_close, fude_chart, fude_chart_close)

// A section's button: jumps to section _arg.
RDE_INTERNAL void fude_chart_row_jump(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app);
    fude_chart_jump((fude_chart*)_self, _arg);
}

// Select: turns on the app's Select mode, which swaps in Select's row.
RDE_INTERNAL void fude_chart_row_select(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    FUDE_STUDY(_app)->selection->active = true;
}

// Practice: opens Practice on the section in view, as a set.
RDE_INTERNAL void fude_chart_row_practice(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    const u32* _records;
    const u32  _n = fude_chart_screen_in_view(_self, &_records);
    fude_study_practice_set(_app, _records, _n);
}

// The chart's own row (a button per section, then Select, Practice and Close), and in Select mode, Select's row
// (study.h). They are made once, from the language's sections, the first time a chart is made, since every chart of
// an app is of the app's one language. The array has room for every section plus the three other buttons.
enum { FUDE_CHART_ROW_MAIN = 0, FUDE_CHART_ROW_SELECT };
RDE_INTERNAL fude_row_button fude_chart_buttons[FUDE_LANG_CHART_SECTIONS + 3u];
RDE_INTERNAL fude_row_def    fude_chart_button_rows[2];

// Fills the rows above, once: a later call (another chart) finds them made and returns. _def may be NULL (no chart),
// and then the row has only Select, Practice and Close.
RDE_INTERNAL void fude_chart_buttons_make(const fude_lang_chart_def* _def) {
    if(fude_chart_button_rows[FUDE_CHART_ROW_MAIN].buttons != NULL) {
        return;
    }
    u32 _n = 0;
    for(u32 _s = 0; _def != NULL && _s < _def->section_count && _s < FUDE_LANG_CHART_SECTIONS; _s++) {
        fude_chart_buttons[_n++] = (fude_row_button){ _def->sections[_s].button, _def->sections[_s].glyph, fude_chart_row_jump, _s, FUDE_ROW_QUIET, false, NULL };
    }
    fude_chart_buttons[_n++] = (fude_row_button){ FUDE_TEXT_SELECT,   FUDE_ICON_SELECT, fude_chart_row_select,   0, FUDE_ROW_QUIET,   false, NULL };
    fude_chart_buttons[_n++] = (fude_row_button){ FUDE_TEXT_PRACTICE, FUDE_ICON_PEN,    fude_chart_row_practice, 0, FUDE_ROW_PRIMARY, false, NULL };
    fude_chart_buttons[_n++] = (fude_row_button){ FUDE_TEXT_CLOSE,    FUDE_ICON_CLOSE,  fude_chart_row_close,    0, FUDE_ROW_QUIET,   false, NULL };
    fude_chart_button_rows[FUDE_CHART_ROW_MAIN]   = (fude_row_def){ fude_chart_buttons, _n };
    fude_chart_button_rows[FUDE_CHART_ROW_SELECT] = (fude_row_def){ FUDE_STUDY_SELECT_BUTTONS, FUDE_STUDY_SELECT_COUNT };
}

// Which row is up: Select's while the app is in Select mode, else the chart's own.
RDE_INTERNAL u32 fude_chart_screen_row(const any _self) {
    const fude_chart* _chart = (const fude_chart*)_self;
    return _chart->selection != NULL && _chart->selection->active ? FUDE_CHART_ROW_SELECT : FUDE_CHART_ROW_MAIN;
}

// How the buttons show: Select's row shows the ticks (study.h); the chart's own row shows as declared.
RDE_INTERNAL void fude_chart_screen_faces(const any _self, u32 _row, fude_row_face* _faces) {
    if(_row == FUDE_CHART_ROW_SELECT) {
        fude_study_select_faces(((const fude_chart*)_self)->selection, _faces);
    }
}

const fude_screen FUDE_CHART_SCREEN = {
    .name = "chart", .input = FUDE_SCREEN_INPUT_POINT, .selects = true,
    .is_open = fude_chart_screen_is_open, .close = fude_chart_screen_close,
    .update = fude_chart_screen_update, .render = fude_chart_screen_render,
    .pointer_down = fude_chart_screen_down, .pointer_moved = fude_chart_screen_moved, .pointer_up = fude_chart_screen_up,
    .rows = fude_chart_button_rows, .row_count = 2u, .row = fude_chart_screen_row, .faces = fude_chart_screen_faces,
    .field_hint = FUDE_TEXT_COUNT, .in_view = fude_chart_screen_in_view,
};
