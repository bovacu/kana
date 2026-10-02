#include "screens/chart.h"
#include "lang/ja/romaji.h"
#include "base/text.h"
#include "study/marks.h"
#include "widgets/draw.h"
#include "base/theme.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See chart.h.
// ===========================================================================

#define KANA_CHART_MARGIN    16.0f
#define KANA_CHART_CELL_MAX  88.0f
#define KANA_CHART_TITLE_H   48.0f
#define KANA_CHART_SUB_H     40.0f
#define KANA_CHART_GAP       20.0f


// The gojūon's columns: the chart's width in cells.
RDE_INTERNAL u32 kana_chart_columns(void) {
    u32 _n;
    kana_romaji_block(0u, &_n);
    return _n;
}

void kana_chart_init(kana_chart* _chart, const kana_kanji_db* _db) {
    memset(_chart, 0, sizeof(*_chart));
    _chart->db     = _db;
    _chart->tapped = -1;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _chart->list       = rde_arr_new(sizeof(u32), _heap);
    _chart->codepoints = rde_arr_new(sizeof(u32), _heap);
    _chart->romaji     = rde_arr_new(sizeof(const c8*), _heap);
    _chart->cells      = rde_arr_new(sizeof(kana_chart_cell), _heap);
    kana_glyph_init(&_chart->glyph, _db);

    if(_db == NULL) {
        return;
    }

    // The list, in the order the chart is laid out: section by section, block by
    // block, column by column, a to o. Kana the data lacks are left out.
    for(u32 _section = 0; _section < 2; _section++) {
        const u32 _shift = _section == KANA_CHART_KATAKANA ? KANA_ROMAJI_KATAKANA_SHIFT : 0u;
        for(u32 _block = 0; _block < 2; _block++) {
            u32                       _n;
            const kana_romaji_column* _cols = kana_romaji_block(_block, &_n);
            for(u32 _c = 0; _c < _n; _c++) {
                for(u32 _r = 0; _r < KANA_ROMAJI_ROWS; _r++) {
                    if(_cols[_c].cp[_r] == 0) {
                        continue;
                    }
                    u32 _cp     = _cols[_c].cp[_r] + _shift;
                    u32 _record = 0;
                    if(kana_kanji_find_index(_db, _cp, &_record)) {
                        rde_arr_add(&_chart->list, &_record);
                        rde_arr_add(&_chart->codepoints, &_cp);
                        rde_arr_add(&_chart->romaji, (any)&_cols[_c].romaji[_r]);
                    }
                }
            }
        }
    }
}

KANA_CHART_SECTION_ kana_chart_section_in_view(const kana_chart* _chart) {
    // Katakana once its title is in the top third of the view.
    const f32 _view = _chart->view_top - _chart->view_bottom;
    return _chart->scroller.offset + _view / 3.0f >= _chart->katakana_at && _chart->katakana_at > 0.0f ? KANA_CHART_KATAKANA : KANA_CHART_HIRAGANA;
}

void kana_chart_section_range(const kana_chart* _chart, KANA_CHART_SECTION_ _section, u32* _first, u32* _count) {
    const u32* _cps   = (const u32*)_chart->codepoints.memory;
    const u32  _total = kana_chart_count(_chart);
    u32        _split = 0;   // the first katakana
    while(_split < _total && _cps[_split] < 0x30A0u) {
        _split++;
    }
    *_first = _section == KANA_CHART_KATAKANA ? _split : 0u;
    *_count = _section == KANA_CHART_KATAKANA ? _total - _split : _split;
}

void kana_chart_destroy(kana_chart* _chart) {
    rde_arr* _arrays[] = { &_chart->list, &_chart->codepoints, &_chart->romaji, &_chart->cells };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    kana_glyph_destroy(&_chart->glyph);
    memset(_chart, 0, sizeof(*_chart));
}

void kana_chart_open(kana_chart* _chart) {
    if(rde_arr_length(&_chart->list) > 0) {
        _chart->open = true;
    }
}

void kana_chart_close(kana_chart* _chart) {
    _chart->open = false;
    kana_scroller_stop(&_chart->scroller);
}

void kana_chart_jump(kana_chart* _chart, KANA_CHART_SECTION_ _section) {
    kana_scroller_stop(&_chart->scroller);
    _chart->scroller.offset = _section == KANA_CHART_KATAKANA ? _chart->katakana_at : 0.0f;
}

const u32* kana_chart_list(const kana_chart* _chart) {
    return (const u32*)_chart->list.memory;
}

u32 kana_chart_count(const kana_chart* _chart) {
    return (u32)rde_arr_length(&_chart->list);
}

b8 kana_chart_take_tap(kana_chart* _chart, u32* _position) {
    if(_chart->tapped < 0) {
        return false;
    }
    *_position     = (u32)_chart->tapped;
    _chart->tapped = -1;
    return true;
}

// --- layout ------------------------------------------------------------------------

// Positions every cell for a width; the list order is the layout order, so the
// n-th cell with a kana is the n-th in the list.
RDE_INTERNAL void kana_chart_layout(kana_chart* _chart, f32 _left, f32 _width) {
    rde_arr_clear(&_chart->cells);
    _chart->_laid_out_width = _width;

    const f32 _cell = fminf(KANA_CHART_CELL_MAX, _width / (f32)kana_chart_columns());
    u32       _pos  = 0;
    f32       _y    = 0.0f;

    for(u32 _section = 0; _section < 2; _section++) {
        const u32 _shift = _section == KANA_CHART_KATAKANA ? KANA_ROMAJI_KATAKANA_SHIFT : 0u;
        if(_section == KANA_CHART_KATAKANA) {
            _chart->katakana_at = _y;
        }
        _y += KANA_CHART_TITLE_H;

        for(u32 _block = 0; _block < 2; _block++) {
            if(_block == 1) {
                _y += KANA_CHART_GAP * 0.5f + KANA_CHART_SUB_H;
            }
            u32                       _n;
            const kana_romaji_column* _cols = kana_romaji_block(_block, &_n);
            for(u32 _c = 0; _c < _n; _c++) {
                for(u32 _r = 0; _r < KANA_ROMAJI_ROWS; _r++) {
                    if(_cols[_c].cp[_r] == 0 || _pos >= kana_chart_count(_chart) ||
                       ((const u32*)_chart->codepoints.memory)[_pos] != _cols[_c].cp[_r] + _shift) {
                        continue;   // empty, or a kana the data lacks
                    }
                    kana_chart_cell _cell_rect = { .x = _left + (f32)_c * _cell, .y = _y + (f32)_r * _cell, .size = _cell, .position = _pos++ };
                    rde_arr_add(&_chart->cells, &_cell_rect);
                }
            }
            _y += (f32)KANA_ROMAJI_ROWS * _cell;
        }
        _y += KANA_CHART_GAP * 2.0f;
    }

    _chart->content_height = _y;
}

// --- input -------------------------------------------------------------------------

void kana_chart_pointer_down(kana_chart* _chart, rde_vec_2F _screen, f64 _time) {
    kana_scroller_down(&_chart->scroller, _screen, _time);
}

void kana_chart_pointer_moved(kana_chart* _chart, rde_vec_2F _screen, f64 _time) {
    kana_scroller_moved(&_chart->scroller, _screen, _time);
}

void kana_chart_pointer_up(kana_chart* _chart, f64 _time) {
    kana_scroller_up(&_chart->scroller, _time);

    rde_vec_2F _at;
    if(!kana_scroller_take_tap(&_chart->scroller, &_at) || _at.y > _chart->view_top || _at.y < _chart->view_bottom) {
        return;
    }

    const f32              _content_y = _chart->view_top - _at.y + _chart->scroller.offset;
    const kana_chart_cell* _cells     = (const kana_chart_cell*)_chart->cells.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_chart->cells); _i++) {
        const kana_chart_cell* _c = &_cells[_i];
        if(_at.x >= _c->x && _at.x < _c->x + _c->size && _content_y >= _c->y && _content_y < _c->y + _c->size) {
            _chart->tapped = (i32)_c->position;
            return;
        }
    }
}

void kana_chart_update(kana_chart* _chart, f32 _dt) {
    if(_chart->open) {
        kana_scroller_update(&_chart->scroller, _dt, _chart->content_height, _chart->view_top - _chart->view_bottom);
    }
}

// --- drawing -----------------------------------------------------------------------

void kana_chart_render(kana_chart* _chart, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_chart->open) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_CHART_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_CHART_MARGIN;

    if(fabsf(_right - _left - _chart->_laid_out_width) > 0.5f) {
        kana_chart_layout(_chart, _left, _right - _left);   // first time, or the screen rotated
    }
    _chart->view_top    = _top;
    _chart->view_bottom = _bottom;
    if(_top <= _bottom) {
        return;
    }

    const f32 _scroll = _chart->scroller.offset;
    rde_rendering_begin_clipping_rect(_window,
                                      (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_top - _bottom) });

    // Titles: each section's, and its second block's.
    const f32 _cell = fminf(KANA_CHART_CELL_MAX, (_right - _left) / (f32)kana_chart_columns());
    for(u32 _section = 0; _section < 2; _section++) {
        const f32 _at  = _section == KANA_CHART_KATAKANA ? _chart->katakana_at : 0.0f;
        const f32 _y   = _top - (_at - _scroll);
        kana_draw_text(_font, _font_px, kana_text(_section == KANA_CHART_KATAKANA ? KANA_TEXT_KATAKANA : KANA_TEXT_HIRAGANA), _left, _y - 32.0f, 24.0f, kana_theme_active()->text);
        const f32 _sub = _y - KANA_CHART_TITLE_H - (f32)KANA_ROMAJI_ROWS * _cell - KANA_CHART_GAP * 0.5f;
        kana_draw_text(_font, _font_px, kana_text(KANA_TEXT_CHART_VOICED), _left, _sub - 26.0f, 15.0f, kana_theme_active()->text_soft);
    }

    const kana_chart_cell* _cells  = (const kana_chart_cell*)_chart->cells.memory;
    const u32*             _cps    = (const u32*)_chart->codepoints.memory;
    const c8* const*       _romaji = (const c8* const*)_chart->romaji.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_chart->cells); _i++) {
        const kana_chart_cell* _c = &_cells[_i];
        const f32              _y = _top - (_c->y - _scroll);    // the cell's top edge on screen
        if(_y - _c->size > _top || _y < _bottom) {
            continue;
        }

        const f32  _glyph  = _c->size * 0.58f;
        const u32  _record = ((const u32*)_chart->list.memory)[_c->position];
        const b8   _select = _chart->selection != NULL && _chart->selection->active;
        const b8   _ticked = _select && kana_selection_has(_chart->selection, _record);
        kana_draw_card((rde_vec_2F){ _c->x + 3.0f, _y - _c->size + 3.0f }, (rde_vec_2F){ _c->x + _c->size - 3.0f, _y - 3.0f }, 10.0f,
                       kana_theme_active()->surface, kana_theme_active()->outline);
        if(_select) {
            kana_selection_draw_behind(_ticked, (rde_vec_2F){ _c->x, _y }, _c->size);
        }

        kana_glyph_character(&_chart->glyph, _cps[_c->position], (rde_vec_2F){ _c->x + (_c->size - _glyph) * 0.5f, _y - _c->size * 0.08f }, _glyph, kana_theme_active()->ink);
        if(_romaji[_c->position] != NULL) {
            kana_draw_text(_font, _font_px, _romaji[_c->position], _c->x + 6.0f, _y - _c->size + 8.0f, 13.0f, kana_theme_active()->text_soft);
        }
        if(_select) {
            kana_selection_draw_tick(_ticked, (rde_vec_2F){ _c->x, _y }, _c->size);
        }
        kana_selection_draw_mark(kana_marks_get(_cps[_c->position]), (rde_vec_2F){ _c->x, _y }, _c->size, _select);
    }

    rde_rendering_end_clipping_rect();
}

// --- the screen (screen.h): its rows, and what they do ------------------------------------

#include "app/app.h"
#include "widgets/icons.h"

KANA_SCREEN_ADAPTERS(kana_chart, kana_chart)
KANA_SCREEN_RENDER(kana_chart, kana_chart)

// A tapped kana opens the viewer, walking the chart; in Select mode it is ticked
// (or unticked) instead.
RDE_INTERNAL void kana_chart_screen_update(kana_app* _app, void* _self, f32 _dt) {
    kana_chart* _chart = (kana_chart*)_self;
    kana_chart_update(_chart, _dt);
    u32 _position = 0;
    if(kana_chart_take_tap(_chart, &_position)) {
        if(_app->selection->active) { kana_selection_toggle(_app->selection, kana_chart_list(_chart)[_position]); }
        else                        { kana_app_view(_app, kana_chart_list(_chart), kana_chart_count(_chart), _position); }
    }
}

// The section in view, in chart order.
RDE_INTERNAL u32 kana_chart_screen_in_view(void* _self, const u32** _records) {
    const kana_chart* _chart = (const kana_chart*)_self;
    u32 _first = 0, _count = 0;
    kana_chart_section_range(_chart, kana_chart_section_in_view(_chart), &_first, &_count);
    *_records = &kana_chart_list(_chart)[_first];
    return _count;
}

KANA_ROW_CALL(kana_chart_row_close, kana_chart, kana_chart_close)

// Hiragana, Katakana: a jump to its section (_arg).
RDE_INTERNAL void kana_chart_row_jump(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app);
    kana_chart_jump((kana_chart*)_self, (KANA_CHART_SECTION_)_arg);
}

RDE_INTERNAL void kana_chart_row_select(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    _app->selection->active = true;
}

RDE_INTERNAL void kana_chart_row_practice(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    const u32* _records;
    const u32  _n = kana_chart_screen_in_view(_self, &_records);
    kana_app_practice_set(_app, _records, _n);
}

// Its own row; in Select mode, Select's (app.h).
enum { KANA_CHART_ROW_MAIN = 0, KANA_CHART_ROW_SELECT };
static const kana_row_button KANA_CHART_BUTTONS[] = {
    { KANA_TEXT_HIRAGANA, "\xE3\x81\x82",   kana_chart_row_jump,     KANA_CHART_HIRAGANA, KANA_ROW_QUIET,   false, NULL },   // あ
    { KANA_TEXT_KATAKANA, "\xE3\x82\xA2",   kana_chart_row_jump,     KANA_CHART_KATAKANA, KANA_ROW_QUIET,   false, NULL },   // ア
    { KANA_TEXT_SELECT,   KANA_ICON_SELECT, kana_chart_row_select,   0,                   KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_PRACTICE, KANA_ICON_PEN,    kana_chart_row_practice, 0,                   KANA_ROW_PRIMARY, false, NULL },
    { KANA_TEXT_CLOSE,    KANA_ICON_CLOSE,  kana_chart_row_close,    0,                   KANA_ROW_QUIET,   false, NULL },
};
static const kana_row_def KANA_CHART_BUTTON_ROWS[] = { KANA_ROW_DEF(KANA_CHART_BUTTONS), { KANA_APP_SELECT_BUTTONS, KANA_APP_SELECT_COUNT } };

RDE_INTERNAL u32 kana_chart_screen_row(const void* _self) {
    const kana_chart* _chart = (const kana_chart*)_self;
    return _chart->selection != NULL && _chart->selection->active ? KANA_CHART_ROW_SELECT : KANA_CHART_ROW_MAIN;
}

RDE_INTERNAL void kana_chart_screen_faces(const void* _self, u32 _row, kana_row_face* _faces) {
    if(_row == KANA_CHART_ROW_SELECT) {
        kana_app_select_faces(((const kana_chart*)_self)->selection, _faces);
    }
}

const kana_screen KANA_CHART_SCREEN = {
    .name = "chart", .input = KANA_SCREEN_INPUT_POINT,
    .is_open = kana_chart_screen_is_open, .close = kana_chart_screen_close,
    .update = kana_chart_screen_update, .render = kana_chart_screen_render,
    .pointer_down = kana_chart_screen_down, .pointer_moved = kana_chart_screen_moved, .pointer_up = kana_chart_screen_up,
    .rows = KANA_CHART_BUTTON_ROWS, .row_count = 2u, .row = kana_chart_screen_row, .faces = kana_chart_screen_faces,
    .field_hint = KANA_TEXT_COUNT, .in_view = kana_chart_screen_in_view,
};
