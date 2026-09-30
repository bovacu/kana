#include "chart.h"
#include "draw.h"
#include "theme.h"

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

#define KANA_CHART_ROWS      5u
#define KANA_CHART_KATAKANA_SHIFT 0x60u   // katakana = hiragana + this, cell for cell

// One column of the table: five rows, a i u e o. 0: no kana in that cell.
typedef struct {
    u32       cp[KANA_CHART_ROWS];
    const c8* romaji[KANA_CHART_ROWS];
} kana_chart_column;

// The gojūon: vowels first, then consonant by consonant.
static const kana_chart_column KANA_CHART_BASIC[] = {
    { { 0x3042, 0x3044, 0x3046, 0x3048, 0x304A }, { "a",  "i",   "u",   "e",  "o"  } },
    { { 0x304B, 0x304D, 0x304F, 0x3051, 0x3053 }, { "ka", "ki",  "ku",  "ke", "ko" } },
    { { 0x3055, 0x3057, 0x3059, 0x305B, 0x305D }, { "sa", "shi", "su",  "se", "so" } },
    { { 0x305F, 0x3061, 0x3064, 0x3066, 0x3068 }, { "ta", "chi", "tsu", "te", "to" } },
    { { 0x306A, 0x306B, 0x306C, 0x306D, 0x306E }, { "na", "ni",  "nu",  "ne", "no" } },
    { { 0x306F, 0x3072, 0x3075, 0x3078, 0x307B }, { "ha", "hi",  "fu",  "he", "ho" } },
    { { 0x307E, 0x307F, 0x3080, 0x3081, 0x3082 }, { "ma", "mi",  "mu",  "me", "mo" } },
    { { 0x3084, 0,      0x3086, 0,      0x3088 }, { "ya", NULL,  "yu",  NULL, "yo" } },
    { { 0x3089, 0x308A, 0x308B, 0x308C, 0x308D }, { "ra", "ri",  "ru",  "re", "ro" } },
    { { 0x308F, 0x3090, 0,      0x3091, 0x3092 }, { "wa", "wi",  NULL,  "we", "wo" } },
    { { 0x3093, 0,      0,      0,      0      }, { "n",  NULL,  NULL,  NULL, NULL } },
};

// The voiced columns (dakuten ゛, handakuten ゜), then the small kana.
static const kana_chart_column KANA_CHART_EXTRA[] = {
    { { 0x304C, 0x304E, 0x3050, 0x3052, 0x3054 }, { "ga", "gi",  "gu",  "ge", "go" } },
    { { 0x3056, 0x3058, 0x305A, 0x305C, 0x305E }, { "za", "ji",  "zu",  "ze", "zo" } },
    { { 0x3060, 0x3062, 0x3065, 0x3067, 0x3069 }, { "da", "ji",  "zu",  "de", "do" } },
    { { 0x3070, 0x3073, 0x3076, 0x3079, 0x307C }, { "ba", "bi",  "bu",  "be", "bo" } },
    { { 0x3071, 0x3074, 0x3077, 0x307A, 0x307D }, { "pa", "pi",  "pu",  "pe", "po" } },
    { { 0x3041, 0x3043, 0x3045, 0x3047, 0x3049 }, { "(a)", "(i)", "(u)", "(e)", "(o)" } },
    { { 0x3083, 0,      0x3085, 0,      0x3087 }, { "(ya)", NULL, "(yu)", NULL, "(yo)" } },
    { { 0x3063, 0x308E, 0x3094, 0x3095, 0x3096 }, { "(tsu)", "(wa)", "vu", "(ka)", "(ke)" } },
};

#define KANA_CHART_BASIC_COLUMNS (sizeof(KANA_CHART_BASIC) / sizeof(KANA_CHART_BASIC[0]))
#define KANA_CHART_EXTRA_COLUMNS (sizeof(KANA_CHART_EXTRA) / sizeof(KANA_CHART_EXTRA[0]))

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
        const u32 _shift = _section == KANA_CHART_KATAKANA ? KANA_CHART_KATAKANA_SHIFT : 0u;
        for(u32 _block = 0; _block < 2; _block++) {
            const kana_chart_column* _cols = _block == 0 ? KANA_CHART_BASIC : KANA_CHART_EXTRA;
            const u32                _n    = _block == 0 ? (u32)KANA_CHART_BASIC_COLUMNS : (u32)KANA_CHART_EXTRA_COLUMNS;
            for(u32 _c = 0; _c < _n; _c++) {
                for(u32 _r = 0; _r < KANA_CHART_ROWS; _r++) {
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

// Characters of the kana (and CJK symbol) blocks the chart leaves out, named
// short enough for a Browse caption.
static const struct { u32 cp; const c8* name; } KANA_CHART_OTHERS[] = {
    { 0x309B, "tenten" }, { 0x309C, "maru" },          // ゛ ゜ on their own
    { 0x309D, "repeat" }, { 0x309E, "repeat+" },        // ゝ ゞ: repeat the kana before (voiced)
    { 0x30FD, "repeat" }, { 0x30FE, "repeat+" },        // ヽ ヾ
    { 0x30F7, "va" }, { 0x30F8, "vi" }, { 0x30F9, "ve" }, { 0x30FA, "vo" },
    { 0x30FB, "dot" }, { 0x30FC, "long" },              // ・ ー
    { 0x3005, "repeat" }, { 0x3006, "shime" },          // 々: repeat the kanji before; 〆
    { 0x3001, "comma" }, { 0x3002, "period" },          // 、 。
};

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

const c8* kana_chart_romaji(u32 _codepoint) {
    for(u32 _i = 0; _i < sizeof(KANA_CHART_OTHERS) / sizeof(KANA_CHART_OTHERS[0]); _i++) {
        if(KANA_CHART_OTHERS[_i].cp == _codepoint) {
            return KANA_CHART_OTHERS[_i].name;
        }
    }
    if(_codepoint >= 0x30A1u && _codepoint <= 0x30F6u) {
        _codepoint -= KANA_CHART_KATAKANA_SHIFT;
    }
    for(u32 _block = 0; _block < 2; _block++) {
        const kana_chart_column* _cols = _block == 0 ? KANA_CHART_BASIC : KANA_CHART_EXTRA;
        const u32                _n    = _block == 0 ? (u32)KANA_CHART_BASIC_COLUMNS : (u32)KANA_CHART_EXTRA_COLUMNS;
        for(u32 _c = 0; _c < _n; _c++) {
            for(u32 _r = 0; _r < KANA_CHART_ROWS; _r++) {
                if(_cols[_c].cp[_r] == _codepoint) {
                    return _cols[_c].romaji[_r];
                }
            }
        }
    }
    return NULL;
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

    const f32 _cell = fminf(KANA_CHART_CELL_MAX, _width / (f32)KANA_CHART_BASIC_COLUMNS);
    u32       _pos  = 0;
    f32       _y    = 0.0f;

    for(u32 _section = 0; _section < 2; _section++) {
        const u32 _shift = _section == KANA_CHART_KATAKANA ? KANA_CHART_KATAKANA_SHIFT : 0u;
        if(_section == KANA_CHART_KATAKANA) {
            _chart->katakana_at = _y;
        }
        _y += KANA_CHART_TITLE_H;

        for(u32 _block = 0; _block < 2; _block++) {
            if(_block == 1) {
                _y += KANA_CHART_GAP * 0.5f + KANA_CHART_SUB_H;
            }
            const kana_chart_column* _cols = _block == 0 ? KANA_CHART_BASIC : KANA_CHART_EXTRA;
            const u32                _n    = _block == 0 ? (u32)KANA_CHART_BASIC_COLUMNS : (u32)KANA_CHART_EXTRA_COLUMNS;
            for(u32 _c = 0; _c < _n; _c++) {
                for(u32 _r = 0; _r < KANA_CHART_ROWS; _r++) {
                    if(_cols[_c].cp[_r] == 0 || _pos >= kana_chart_count(_chart) ||
                       ((const u32*)_chart->codepoints.memory)[_pos] != _cols[_c].cp[_r] + _shift) {
                        continue;   // empty, or a kana the data lacks
                    }
                    kana_chart_cell _cell_rect = { .x = _left + (f32)_c * _cell, .y = _y + (f32)_r * _cell, .size = _cell, .position = _pos++ };
                    rde_arr_add(&_chart->cells, &_cell_rect);
                }
            }
            _y += (f32)KANA_CHART_ROWS * _cell;
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
    const f32 _cell = fminf(KANA_CHART_CELL_MAX, (_right - _left) / (f32)KANA_CHART_BASIC_COLUMNS);
    for(u32 _section = 0; _section < 2; _section++) {
        const f32 _at  = _section == KANA_CHART_KATAKANA ? _chart->katakana_at : 0.0f;
        const f32 _y   = _top - (_at - _scroll);
        kana_draw_text(_font, _font_px, _section == KANA_CHART_KATAKANA ? "Katakana" : "Hiragana", _left, _y - 32.0f, 26.0f, kana_theme_active()->text);
        const f32 _sub = _y - KANA_CHART_TITLE_H - (f32)KANA_CHART_ROWS * _cell - KANA_CHART_GAP * 0.5f;
        kana_draw_text(_font, _font_px, "Voiced (dakuten, handakuten) and small kana", _left, _sub - 26.0f, 17.0f, kana_theme_active()->text_soft);
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
        if(_select) {
            kana_selection_draw_behind(_ticked, (rde_vec_2F){ _c->x, _y }, _c->size);
        }
        kana_draw_outline((rde_vec_2F){ _c->x + 2.0f, _y - _c->size + 2.0f }, (rde_vec_2F){ _c->x + _c->size - 2.0f, _y - 2.0f }, 0.5f, kana_theme_active()->line);

        kana_glyph_character(&_chart->glyph, _cps[_c->position], (rde_vec_2F){ _c->x + (_c->size - _glyph) * 0.5f, _y - _c->size * 0.08f }, _glyph, kana_theme_active()->ink);
        if(_romaji[_c->position] != NULL) {
            kana_draw_text(_font, _font_px, _romaji[_c->position], _c->x + 6.0f, _y - _c->size + 8.0f, 13.0f, kana_theme_active()->text_soft);
        }
        if(_select) {
            kana_selection_draw_tick(_ticked, (rde_vec_2F){ _c->x, _y }, _c->size);
        }
    }

    rde_rendering_end_clipping_rect();
}
