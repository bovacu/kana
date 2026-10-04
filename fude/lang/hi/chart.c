#include "lang/hi/chart.h"
#include "lang/lang.h"
#include "drawing/base/text.h"
#include "study/models/marks.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See chart.h.
// ===========================================================================

#define FUDE_CHART_MARGIN    16.0f
#define FUDE_CHART_CELL_MAX  88.0f
#define FUDE_CHART_CELL_MIN  64.0f     // smaller, and a row wraps
#define FUDE_CHART_TITLE_H   48.0f
#define FUDE_CHART_GAP       20.0f
#define FUDE_CHART_COLUMNS   6u       // the widest row

#define FUDE_CHART_ROW_MAX   10u

// The chart's rows, section by section (0: an empty cell).
typedef struct {
    u8  section;
    u32 cp[FUDE_CHART_ROW_MAX];
} fude_chart_row;

static const fude_chart_row FUDE_CHART_ROWS[] = {
    { FUDE_CHART_VOWELS, { 0x0905, 0x0906, 0x0907, 0x0908, 0x0909, 0x090A } },   // अ आ इ ई उ ऊ
    { FUDE_CHART_VOWELS, { 0x090B, 0x090F, 0x0910, 0x0913, 0x0914, 0x0911 } },   // ऋ ए ऐ ओ औ ऑ
    { FUDE_CHART_CONSONANTS, { 0x0915, 0x0916, 0x0917, 0x0918, 0x0919 } },   // क ख ग घ ङ
    { FUDE_CHART_CONSONANTS, { 0x091A, 0x091B, 0x091C, 0x091D, 0x091E } },   // च छ ज झ ञ
    { FUDE_CHART_CONSONANTS, { 0x091F, 0x0920, 0x0921, 0x0922, 0x0923 } },   // ट ठ ड ढ ण
    { FUDE_CHART_CONSONANTS, { 0x0924, 0x0925, 0x0926, 0x0927, 0x0928 } },   // त थ द ध न
    { FUDE_CHART_CONSONANTS, { 0x092A, 0x092B, 0x092C, 0x092D, 0x092E } },   // प फ ब भ म
    { FUDE_CHART_CONSONANTS, { 0x092F, 0x0930, 0x0932, 0x0935 } },   // य र ल व
    { FUDE_CHART_CONSONANTS, { 0x0936, 0x0937, 0x0938, 0x0939 } },   // श ष स ह
    { FUDE_CHART_CONSONANTS, { 0x0958, 0x0959, 0x095A, 0x095B, 0x095E } },   // क़ ख़ ग़ ज़ फ़
    { FUDE_CHART_CONSONANTS, { 0x095C, 0x095D } },   // ड़ ढ़
    { FUDE_CHART_MARKS, { 0x093E, 0x093F, 0x0940, 0x0941, 0x0942, 0x0943 } },   // ◌ा ◌ि ◌ी ◌ु ◌ू ◌ृ
    { FUDE_CHART_MARKS, { 0x0947, 0x0948, 0x094B, 0x094C, 0x0949 } },   // ◌े ◌ै ◌ो ◌ौ ◌ॉ
    { FUDE_CHART_MARKS, { 0x0902, 0x0901, 0x0903, 0x093C, 0x094D } },   // ◌ं ◌ँ ◌ः ◌़ ◌्
    { FUDE_CHART_MARKS, { 0x093D, 0x0950, 0x0964 } },   // ऽ ॐ ।
    { FUDE_CHART_DIGITS, { 0x0966, 0x0967, 0x0968, 0x0969, 0x096A } },   // ० १ २ ३ ४
    { FUDE_CHART_DIGITS, { 0x096B, 0x096C, 0x096D, 0x096E, 0x096F } },   // ५ ६ ७ ८ ९
};

#define FUDE_CHART_COUNT(_t) ((u32)(sizeof(_t) / sizeof((_t)[0])))

// Row _r of the chart (its letters in _out). False past the last.
RDE_INTERNAL b8 fude_chart_row_at(u32 _r, fude_chart_row* _out) {
    if(_r >= FUDE_CHART_COUNT(FUDE_CHART_ROWS)) {
        return false;
    }
    *_out = FUDE_CHART_ROWS[_r];
    return true;
}

void fude_chart_init(fude_chart* _chart, const fude_kanji_db* _db) {
    memset(_chart, 0, sizeof(*_chart));
    _chart->db     = _db;
    _chart->tapped = -1;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _chart->list       = rde_arr_new(sizeof(u32), _heap);
    _chart->codepoints = rde_arr_new(sizeof(u32), _heap);
    _chart->cells      = rde_arr_new(sizeof(fude_chart_cell), _heap);
    fude_glyph_init(&_chart->glyph, _db);

    if(_db == NULL) {
        return;
    }

    // The list, in the order the chart is laid out: row by row. Letters the data
    // lacks are left out.
    fude_chart_row _row;
    u32            _section = 0;
    for(u32 _r = 0; fude_chart_row_at(_r, &_row); _r++) {
        while(_section <= _row.section) {
            _chart->section_first[_section++] = (u32)rde_arr_length(&_chart->list);
        }
        for(u32 _c = 0; _c < FUDE_CHART_ROW_MAX; _c++) {
            u32 _record = 0;
            u32 _cp     = _row.cp[_c];
            if(_cp != 0u && fude_kanji_find_index(_db, _cp, &_record)) {
                rde_arr_add(&_chart->list, &_record);
                rde_arr_add(&_chart->codepoints, &_cp);
            }
        }
    }
    while(_section <= FUDE_CHART_SECTIONS) {
        _chart->section_first[_section++] = (u32)rde_arr_length(&_chart->list);
    }
}

FUDE_CHART_SECTION_ fude_chart_section_in_view(const fude_chart* _chart) {
    // The last section whose title is in the top third of the view.
    const f32 _view = _chart->view_top - _chart->view_bottom;
    u32       _in   = 0;
    for(u32 _s = 1; _s < FUDE_CHART_SECTIONS; _s++) {
        if(_chart->section_at[_s] > 0.0f && _chart->scroller.offset + _view / 3.0f >= _chart->section_at[_s]) {
            _in = _s;
        }
    }
    return (FUDE_CHART_SECTION_)_in;
}

void fude_chart_section_range(const fude_chart* _chart, FUDE_CHART_SECTION_ _section, u32* _first, u32* _count) {
    const u32 _s = (u32)_section < FUDE_CHART_SECTIONS ? (u32)_section : 0u;
    *_first = _chart->section_first[_s];
    *_count = _chart->section_first[_s + 1u] - _chart->section_first[_s];
}

void fude_chart_destroy(fude_chart* _chart) {
    rde_arr* _arrays[] = { &_chart->list, &_chart->codepoints, &_chart->cells };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    fude_glyph_destroy(&_chart->glyph);
    memset(_chart, 0, sizeof(*_chart));
}

void fude_chart_open(fude_chart* _chart) {
    if(rde_arr_length(&_chart->list) > 0) {
        _chart->open = true;
    }
}

void fude_chart_close(fude_chart* _chart) {
    _chart->open = false;
    fude_scroller_stop(&_chart->scroller);
}

void fude_chart_jump(fude_chart* _chart, FUDE_CHART_SECTION_ _section) {
    fude_scroller_stop(&_chart->scroller);
    _chart->scroller.offset = (u32)_section < FUDE_CHART_SECTIONS ? _chart->section_at[_section] : 0.0f;
}

const u32* fude_chart_list(const fude_chart* _chart) {
    return (const u32*)_chart->list.memory;
}

u32 fude_chart_count(const fude_chart* _chart) {
    return (u32)rde_arr_length(&_chart->list);
}

b8 fude_chart_take_tap(fude_chart* _chart, u32* _position) {
    if(_chart->tapped < 0) {
        return false;
    }
    *_position     = (u32)_chart->tapped;
    _chart->tapped = -1;
    return true;
}

// --- layout ------------------------------------------------------------------------

// Positions every cell for a width; the list order is the layout order, so the
// n-th cell with a letter is the n-th in the list.
RDE_INTERNAL void fude_chart_layout(fude_chart* _chart, f32 _left, f32 _width) {
    rde_arr_clear(&_chart->cells);
    _chart->_laid_out_width = _width;

    // As many to a line as fit at FUDE_CHART_CELL_MIN at least: a phone's rows wrap.
    const u32      _columns = (u32)fmaxf(1.0f, fminf((f32)FUDE_CHART_COLUMNS, floorf(_width / FUDE_CHART_CELL_MIN)));
    const f32      _cell    = fminf(FUDE_CHART_CELL_MAX, _width / (f32)_columns);
    const u32*     _cps     = (const u32*)_chart->codepoints.memory;
    u32            _pos     = 0;
    f32            _y       = 0.0f;
    i32            _section = -1;
    fude_chart_row _row;
    for(u32 _r = 0; fude_chart_row_at(_r, &_row); _r++) {
        if((i32)_row.section != _section) {
            if(_section >= 0) {
                _y += FUDE_CHART_GAP;
            }
            _section = (i32)_row.section;
            _chart->section_at[_section] = _y;
            _y += FUDE_CHART_TITLE_H;
        }
        u32 _at = 0;   // the cell's place on the line
        for(u32 _c = 0; _c < FUDE_CHART_ROW_MAX; _c++) {
            if(_row.cp[_c] == 0u || _pos >= fude_chart_count(_chart) || _cps[_pos] != _row.cp[_c]) {
                continue;   // empty, or a letter the data lacks
            }
            if(_at == _columns) {
                _y += _cell;   // the row goes on on the next line
                _at = 0;
            }
            fude_chart_cell _cell_rect = { .x = _left + (f32)_at * _cell, .y = _y, .size = _cell, .position = _pos++ };
            rde_arr_add(&_chart->cells, &_cell_rect);
            _at++;
        }
        _y += _cell;
    }
    _chart->content_height = _y + FUDE_CHART_GAP * 2.0f;
}

// --- input -------------------------------------------------------------------------

void fude_chart_pointer_down(fude_chart* _chart, rde_vec_2F _screen, f64 _time) {
    fude_scroller_down(&_chart->scroller, _screen, _time);
}

void fude_chart_pointer_moved(fude_chart* _chart, rde_vec_2F _screen, f64 _time) {
    fude_scroller_moved(&_chart->scroller, _screen, _time);
}

void fude_chart_pointer_up(fude_chart* _chart, f64 _time) {
    fude_scroller_up(&_chart->scroller, _time);

    rde_vec_2F _at;
    if(!fude_scroller_take_tap(&_chart->scroller, &_at) || _at.y > _chart->view_top || _at.y < _chart->view_bottom) {
        return;
    }

    const f32              _content_y = _chart->view_top - _at.y + _chart->scroller.offset;
    const fude_chart_cell* _cells     = (const fude_chart_cell*)_chart->cells.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_chart->cells); _i++) {
        const fude_chart_cell* _c = &_cells[_i];
        if(_at.x >= _c->x && _at.x < _c->x + _c->size && _content_y >= _c->y && _content_y < _c->y + _c->size) {
            _chart->tapped = (i32)_c->position;
            return;
        }
    }
}

void fude_chart_update(fude_chart* _chart, f32 _dt) {
    if(_chart->open) {
        fude_scroller_update(&_chart->scroller, _dt, _chart->content_height, _chart->view_top - _chart->view_bottom);
    }
}

// --- drawing -----------------------------------------------------------------------

void fude_chart_render(fude_chart* _chart, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_chart->open) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_CHART_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_CHART_MARGIN;

    if(fabsf(_right - _left - _chart->_laid_out_width) > 0.5f) {
        fude_chart_layout(_chart, _left, _right - _left);   // first time, or the screen rotated
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

    // The sections' titles.
    static const FUDE_TEXT_ _titles[FUDE_CHART_SECTIONS] = { FUDE_TEXT_HI_VOWELS, FUDE_TEXT_HI_CONSONANTS, FUDE_TEXT_HI_SIGNS, FUDE_TEXT_HI_DIGITS };
    for(u32 _s = 0; _s < FUDE_CHART_SECTIONS; _s++) {
        const f32 _y = _top - (_chart->section_at[_s] - _scroll);
        fude_draw_text(_font, _font_px, fude_text(_titles[_s]), _left, _y - 32.0f, 24.0f, fude_theme_active()->text);
    }

    const fude_chart_cell* _cells = (const fude_chart_cell*)_chart->cells.memory;
    const u32*             _cps   = (const u32*)_chart->codepoints.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_chart->cells); _i++) {
        const fude_chart_cell* _c = &_cells[_i];
        const f32              _y = _top - (_c->y - _scroll);    // the cell's top edge on screen
        if(_y - _c->size > _top || _y < _bottom) {
            continue;
        }

        const f32  _glyph  = _c->size * 0.58f;
        const u32  _record = ((const u32*)_chart->list.memory)[_c->position];
        const b8   _select = _chart->selection != NULL && _chart->selection->active;
        const b8   _ticked = _select && fude_selection_has(_chart->selection, _record);
        fude_draw_card((rde_vec_2F){ _c->x + 3.0f, _y - _c->size + 3.0f }, (rde_vec_2F){ _c->x + _c->size - 3.0f, _y - 3.0f }, 10.0f,
                       fude_theme_active()->surface, fude_theme_active()->outline);
        if(_select) {
            fude_selection_draw_behind(_ticked, (rde_vec_2F){ _c->x, _y }, _c->size);
        }

        fude_glyph_character(&_chart->glyph, _cps[_c->position], (rde_vec_2F){ _c->x + (_c->size - _glyph) * 0.5f, _y - _c->size * 0.08f }, _glyph, fude_theme_active()->ink);
        const c8* _latin = fude_lang_latin(_cps[_c->position]);
        if(_latin != NULL) {
            fude_draw_text_whole(_font, _font_px, _latin, _c->x + 6.0f, _y - _c->size + 8.0f + 13.0f * 0.38f, 13.0f, _c->size - 12.0f, 1u, fude_theme_active()->text_soft);
        }
        if(_select) {
            fude_selection_draw_tick(_ticked, (rde_vec_2F){ _c->x, _y }, _c->size);
        }
        fude_selection_draw_mark(fude_marks_get(_cps[_c->position]), (rde_vec_2F){ _c->x, _y }, _c->size, _select);
    }

    rde_rendering_end_clipping_rect();
}

// --- the screen (screen.h): its rows, and what they do ------------------------------------

#include "study/app/study.h"
#include "drawing/widgets/icons.h"

FUDE_SCREEN_ADAPTERS(fude_chart, fude_chart)
FUDE_SCREEN_RENDER(fude_chart, fude_chart)

// A tapped letter opens the viewer, walking the chart; in Select mode it is
// ticked (or unticked) instead.
RDE_INTERNAL void fude_chart_screen_update(fude_app* _app, void* _self, f32 _dt) {
    fude_study* _study = FUDE_STUDY(_app);
    fude_chart* _chart = (fude_chart*)_self;
    fude_chart_update(_chart, _dt);
    u32 _position = 0;
    if(fude_chart_take_tap(_chart, &_position)) {
        if(_study->selection->active) { fude_selection_toggle(_study->selection, fude_chart_list(_chart)[_position]); }
        else                        { fude_study_view(_app, fude_chart_list(_chart), fude_chart_count(_chart), _position); }
    }
}

// The section in view, in chart order.
RDE_INTERNAL u32 fude_chart_screen_in_view(void* _self, const u32** _records) {
    const fude_chart* _chart = (const fude_chart*)_self;
    u32 _first = 0, _count = 0;
    fude_chart_section_range(_chart, fude_chart_section_in_view(_chart), &_first, &_count);
    *_records = &fude_chart_list(_chart)[_first];
    return _count;
}

FUDE_ROW_CALL(fude_chart_row_close, fude_chart, fude_chart_close)

// Consonants, Vowels, Syllables: a jump to its section (_arg).
RDE_INTERNAL void fude_chart_row_jump(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app);
    fude_chart_jump((fude_chart*)_self, (FUDE_CHART_SECTION_)_arg);
}

RDE_INTERNAL void fude_chart_row_select(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    FUDE_STUDY(_app)->selection->active = true;
}

RDE_INTERNAL void fude_chart_row_practice(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    const u32* _records;
    const u32  _n = fude_chart_screen_in_view(_self, &_records);
    fude_study_practice_set(_app, _records, _n);
}

// Its own row; in Select mode, Select's (app.h).
enum { FUDE_CHART_ROW_MAIN = 0, FUDE_CHART_ROW_SELECT };
static const fude_row_button FUDE_CHART_BUTTONS[] = {
    { FUDE_TEXT_HI_VOWELS, "\xE0\xA4\x85", fude_chart_row_jump, FUDE_CHART_VOWELS, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_HI_CONSONANTS, "\xE0\xA4\x95", fude_chart_row_jump, FUDE_CHART_CONSONANTS, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_HI_SIGNS, "\xE0\xA4\xBE", fude_chart_row_jump, FUDE_CHART_MARKS, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_HI_DIGITS, "\xE0\xA5\xA7", fude_chart_row_jump, FUDE_CHART_DIGITS, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_SELECT,           FUDE_ICON_SELECT, fude_chart_row_select, 0,                     FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE,         FUDE_ICON_PEN,  fude_chart_row_practice, 0,                     FUDE_ROW_PRIMARY, false, NULL },
    { FUDE_TEXT_CLOSE,            FUDE_ICON_CLOSE, fude_chart_row_close,   0,                     FUDE_ROW_QUIET,   false, NULL },
};
static const fude_row_def FUDE_CHART_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_CHART_BUTTONS), { FUDE_STUDY_SELECT_BUTTONS, FUDE_STUDY_SELECT_COUNT } };

RDE_INTERNAL u32 fude_chart_screen_row(const void* _self) {
    const fude_chart* _chart = (const fude_chart*)_self;
    return _chart->selection != NULL && _chart->selection->active ? FUDE_CHART_ROW_SELECT : FUDE_CHART_ROW_MAIN;
}

RDE_INTERNAL void fude_chart_screen_faces(const void* _self, u32 _row, fude_row_face* _faces) {
    if(_row == FUDE_CHART_ROW_SELECT) {
        fude_study_select_faces(((const fude_chart*)_self)->selection, _faces);
    }
}

const fude_screen FUDE_CHART_SCREEN = {
    .name = "chart", .input = FUDE_SCREEN_INPUT_POINT, .selects = true,
    .is_open = fude_chart_screen_is_open, .close = fude_chart_screen_close,
    .update = fude_chart_screen_update, .render = fude_chart_screen_render,
    .pointer_down = fude_chart_screen_down, .pointer_moved = fude_chart_screen_moved, .pointer_up = fude_chart_screen_up,
    .rows = FUDE_CHART_BUTTON_ROWS, .row_count = 2u, .row = fude_chart_screen_row, .faces = fude_chart_screen_faces,
    .field_hint = FUDE_TEXT_COUNT, .in_view = fude_chart_screen_in_view,
};
