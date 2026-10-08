// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/display.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// --- sized parts ---------------------------------------------------------------------------------
//
// Each made once for its kind, its size and its way round, kept (its pins and their names its own).

#define FZD_PINS 64u

typedef struct {
    fude_zoom_part        part;          // (first: what is handed out is it)
    const fude_zoom_part* template_of;
    u32                   cols, rows;
    b8                    anode;         // a panel's commons, a matrix's rows: anodes
    fude_zoom_pin         pins[FZD_PINS];
    c8                    names[FZD_PINS][6];
} fzd_made;

RDE_INTERNAL rde_arr TYPE(fzd_made*) fzd_parts;

RDE_INTERNAL const c8* const FZD_LCD_PINS[16] = { "VSS", "VDD", "V0", "RS", "RW", "E", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "A", "K" };
RDE_INTERNAL const c8* const FZD_SEGMENTS[8] = { "a", "b", "c", "d", "e", "f", "g", "dp" };

RDE_INTERNAL b8 fzd_sized(const fude_zoom_part* _p) {
    return _p != NULL && (_p->model == FUDE_ZOOM_MODEL_SEG_PANEL || _p->model == FUDE_ZOOM_MODEL_LED_MATRIX || _p->model == FUDE_ZOOM_MODEL_CHAR_LCD);
}

RDE_INTERNAL const fzd_made* fzd_of(const fude_zoom_part* _part) {
    if(_part == NULL || !rde_arr_is_inited(&fzd_parts)) {
        return NULL;
    }
    fzd_made* const* _m = (fzd_made* const*)fzd_parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fzd_parts); _i++) {
        if(&_m[_i]->part == _part) {
            return _m[_i];
        }
    }
    return NULL;
}

// Does _text hold the word _word (any case, not inside another word)?
RDE_INTERNAL b8 fzd_word(const c8* _text, const c8* _word) {
    const usize _n = strlen(_word);
    for(const c8* _p = _text; _p != NULL && *_p != 0; _p++) {
        if(_p != _text && ((_p[-1] | 0x20) >= 'a' && (_p[-1] | 0x20) <= 'z')) {
            continue;
        }
        usize _k = 0;
        while(_k < _n && _p[_k] != 0 && (_p[_k] | 0x20) == (_word[_k] | 0x20)) {
            _k++;
        }
        if(_k == _n && !((_p[_n] | 0x20) >= 'a' && (_p[_n] | 0x20) <= 'z')) {
            return true;
        }
    }
    return false;
}

// The first "A x B" in _text ("16x2", "8×8", "5*7"): A and B. False: none.
RDE_INTERNAL b8 fzd_by(const c8* _text, u32* _a, u32* _b) {
    for(const c8* _p = _text; _p != NULL && *_p != 0; _p++) {
        if(*_p < '0' || *_p > '9' || (_p != _text && _p[-1] >= '0' && _p[-1] <= '9')) {
            continue;
        }
        u32 _x = 0;
        const c8* _q = _p;
        while(*_q >= '0' && *_q <= '9' && _x < 1000u) {
            _x = _x * 10u + (u32)(*_q++ - '0');
        }
        while(*_q == ' ') {
            _q++;
        }
        if(*_q == 'x' || *_q == 'X' || *_q == '*') {
            _q++;
        } else if((u8)_q[0] == 0xC3 && (u8)_q[1] == 0x97) {
            _q += 2;   // ×
        } else {
            continue;
        }
        while(*_q == ' ') {
            _q++;
        }
        if(*_q < '0' || *_q > '9') {
            continue;
        }
        u32 _y = 0;
        while(*_q >= '0' && *_q <= '9' && _y < 1000u) {
            _y = _y * 10u + (u32)(*_q++ - '0');
        }
        *_a = _x;
        *_b = _y;
        return true;
    }
    return false;
}

// The first whole number in _text (0: none).
RDE_INTERNAL u32 fzd_number(const c8* _text) {
    for(const c8* _p = _text; _p != NULL && *_p != 0; _p++) {
        if(*_p >= '0' && *_p <= '9') {
            u32 _x = 0;
            while(*_p >= '0' && *_p <= '9' && _x < 1000u) {
                _x = _x * 10u + (u32)(*_p++ - '0');
            }
            return _x;
        }
    }
    return 0u;
}

// A sized display's size and way round as its text says (each kind's own when it says none, or too much).
RDE_INTERNAL void fzd_size_of(const fude_zoom_part* _template, const c8* _text, u32* _cols, u32* _rows, b8* _anode) {
    u32 _a = 0, _b = 0;
    switch(_template->model) {
    case FUDE_ZOOM_MODEL_SEG_PANEL: {
        const u32 _d = fzd_number(_text);
        *_cols  = _d == 0u ? 4u : (_d < FUDE_ZOOM_DISPLAY_DIGITS ? _d : FUDE_ZOOM_DISPLAY_DIGITS);
        *_rows  = 1u;
        *_anode = fzd_word(_text, "CA") || fzd_word(_text, "anode");
        break;
    }
    case FUDE_ZOOM_MODEL_LED_MATRIX:
        if(fzd_by(_text, &_a, &_b) && _a >= 1u && _b >= 1u) {
            *_cols = _a < FUDE_ZOOM_DISPLAY_SIDE ? _a : FUDE_ZOOM_DISPLAY_SIDE;
            *_rows = _b < FUDE_ZOOM_DISPLAY_SIDE ? _b : FUDE_ZOOM_DISPLAY_SIDE;
        } else {
            *_cols = *_rows = 8u;
        }
        *_anode = !fzd_word(_text, "CC");   // (its rows anodes, as a 1088BS's; "CC": cathodes)
        break;
    default:   // (a character LCD: 8 to 40 columns, 1, 2 or 4 rows, 80 characters at most)
        if(fzd_by(_text, &_a, &_b) && _a >= 8u && _b >= 1u) {
            *_rows = _b >= 4u ? 4u : (_b >= 2u ? 2u : 1u);
            *_cols = _a > 80u / *_rows ? 80u / *_rows : _a;
            *_cols = *_cols > 40u ? 40u : *_cols;
        } else {
            *_cols = 16u;
            *_rows = 2u;
        }
        *_anode = false;
        break;
    }
}

// Its room (the catalogue's units): a panel's digits 60 apart; a matrix's dots 20 apart (its pins'); an LCD's
// characters 18 × 28, its 16 pins along its top.
RDE_INTERNAL void fzd_room(u8 _model, u32 _cols, u32 _rows, f64* _w, f64* _h) {
    switch(_model) {
    case FUDE_ZOOM_MODEL_SEG_PANEL:
        *_w = 60.0 * (f64)_cols + 60.0;
        *_h = 180.0;
        break;
    case FUDE_ZOOM_MODEL_LED_MATRIX:
        *_w = 20.0 * (f64)_cols + 40.0;
        *_h = 20.0 * (f64)_rows + 40.0;
        break;
    default:
        *_w = 20.0 * ceil(fmax(18.0 * (f64)_cols + 52.0, 340.0) / 20.0);
        *_h = 28.0 * (f64)_rows + 74.0;
        break;
    }
}

const fude_zoom_part* fude_zoom_display_part(const fude_zoom_part* _template, const c8* _text) {
    const fzd_made* _was = fzd_of(_template);
    if(_was != NULL) {
        _template = _was->template_of;   // (one made already, of another size: its kind's)
    }
    if(!fzd_sized(_template)) {
        return _template;
    }
    u32 _cols, _rows;
    b8  _anode;
    fzd_size_of(_template, _text != NULL ? _text : "", &_cols, &_rows, &_anode);
    if(!rde_arr_is_inited(&fzd_parts)) {
        fzd_parts = rde_arr_new(sizeof(fzd_made*), rde_memory_allocator_get_default_std());
    }
    fzd_made* const* _m = (fzd_made* const*)fzd_parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fzd_parts); _i++) {
        if(_m[_i]->template_of == _template && _m[_i]->cols == _cols && _m[_i]->rows == _rows && _m[_i]->anode == _anode) {
            return &_m[_i]->part;
        }
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fzd_made* _new = (fzd_made*)_heap->calloc(_heap->allocator, 1, sizeof(fzd_made));
    _new->template_of = _template;
    _new->cols  = _cols;
    _new->rows  = _rows;
    _new->anode = _anode;
    f64 _w, _h;
    fzd_room(_template->model, _cols, _rows, &_w, &_h);
    const f64 _hw = _w * 0.5, _hh = _h * 0.5;
    u32 _n = 0;
    // Each pin where it is in its room, as u, v of its half sizes.
    #define FZD_PIN(_x, _y, _side, ...) do { snprintf(_new->names[_n], sizeof(_new->names[_n]), __VA_ARGS__); \
        _new->pins[_n] = (fude_zoom_pin){ (f32)((_x) / _hw), (f32)((_y) / _hh), _new->names[_n], FUDE_ZOOM_PIN_##_side }; _n++; } while(0)
    switch(_template->model) {
    case FUDE_ZOOM_MODEL_SEG_PANEL:
        // Its segments down its left (a … g, dp), each digit's common along its bottom, under it.
        for(u32 _k = 0; _k < 8u; _k++) {
            FZD_PIN(-_hw, 70.0 - 20.0 * (f64)_k, LEFT, "%s", FZD_SEGMENTS[_k]);
        }
        for(u32 _d = 0; _d < _cols; _d++) {
            FZD_PIN(-_hw + 80.0 + 60.0 * (f64)_d, -_hh, DOWN, "D%u", _d + 1u);
        }
        break;
    case FUDE_ZOOM_MODEL_LED_MATRIX:
        // Its rows down its left, its columns along its bottom.
        for(u32 _r = 0; _r < _rows; _r++) {
            FZD_PIN(-_hw, _hh - 20.0 - 20.0 * (f64)_r, LEFT, "R%u", _r + 1u);
        }
        for(u32 _c = 0; _c < _cols; _c++) {
            FZD_PIN(-_hw + 40.0 + 20.0 * (f64)_c, -_hh, DOWN, "C%u", _c + 1u);
        }
        break;
    default:
        // Its 16 pins along its top, from its left (VSS first: a module's pin 1).
        for(u32 _k = 0; _k < 16u; _k++) {
            FZD_PIN(-_hw + 20.0 + 20.0 * (f64)_k, _hh, UP, "%s", FZD_LCD_PINS[_k]);
        }
        break;
    }
    #undef FZD_PIN
    _new->part           = *_template;
    _new->part.pins      = _new->pins;
    _new->part.pin_count = (u16)_n;
    rde_arr_add(&fzd_parts, (any)&_new);
    return &_new->part;
}

b8 fude_zoom_display_made(const fude_zoom_part* _part) {
    return fzd_of(_part) != NULL;
}

u32 fude_zoom_display_cols(const fude_zoom_part* _part) {
    const fzd_made* _m = fzd_of(_part);
    return _m != NULL ? _m->cols : 0u;
}

u32 fude_zoom_display_rows(const fude_zoom_part* _part) {
    const fzd_made* _m = fzd_of(_part);
    return _m != NULL ? _m->rows : 0u;
}

b8 fude_zoom_display_room(const fude_zoom_part* _part, f64* _w, f64* _h) {
    const fzd_made* _m = fzd_of(_part);
    if(_m == NULL) {
        return false;
    }
    fzd_room(_part->model, _m->cols, _m->rows, _w, _h);
    return true;
}

void fude_zoom_display_label_inset(const fude_zoom_part* _part, f32* _u, f32* _v) {
    f64 _w, _h;
    if(!fude_zoom_display_room(_part, &_w, &_h)) {
        return;
    }
    // (just in from its edge, where its pins' leads end: 10 in)
    *_u = (f32)(1.0 - 10.0 / (_w * 0.5));
    *_v = (f32)(1.0 - 10.0 / (_h * 0.5));
}

// --- its LEDs ------------------------------------------------------------------------------------

u32 fude_zoom_display_lights(const fude_zoom_part* _part) {
    if(_part == NULL) {
        return 0u;
    }
    if(_part->model == FUDE_ZOOM_MODEL_BAR_GRAPH) {
        return 10u;
    }
    const fzd_made* _m = fzd_of(_part);
    if(_m == NULL) {
        return 0u;
    }
    if(_part->model == FUDE_ZOOM_MODEL_SEG_PANEL) {
        return 8u * _m->cols;
    }
    return _part->model == FUDE_ZOOM_MODEL_LED_MATRIX ? _m->cols * _m->rows : 0u;
}

b8 fude_zoom_display_led(const fude_zoom_part* _part, u32 _k, u32* _anode, u32* _cathode) {
    if(_k >= fude_zoom_display_lights(_part)) {
        return false;
    }
    if(_part->model == FUDE_ZOOM_MODEL_BAR_GRAPH) {
        *_anode   = _k;          // (A1–A10 down its left)
        *_cathode = 10u + _k;    // (K1–K10 down its right)
        return true;
    }
    const fzd_made* _m = fzd_of(_part);
    u32 _common, _other;
    if(_part->model == FUDE_ZOOM_MODEL_SEG_PANEL) {
        _common = 8u + _k / 8u;   // (its digit's)
        _other  = _k % 8u;        // (its segment's)
    } else {
        _common = _k / _m->cols;               // (its row's)
        _other  = _m->rows + _k % _m->cols;    // (its column's)
    }
    *_anode   = _m->anode ? _common : _other;
    *_cathode = _m->anode ? _other : _common;
    return true;
}

u8 fude_zoom_display_segments(c8 _c) {
    static const u8 _digits[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    if(_c >= '0' && _c <= '9') {
        return _digits[_c - '0'];
    }
    return _c == '-' ? 0x40u : 0u;
}

// --- a character LCD -----------------------------------------------------------------------------
//
// Its ROM's characters 0x20–0x7F (the A00's: ASCII but ¥ for the backslash, → and ← for ~ and DEL), 5 × 7, a column a
// byte (bit 0 its top), its eighth row the cursor's.

RDE_INTERNAL const u8 FZD_FONT[96][5] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x5F, 0x00, 0x00 }, { 0x00, 0x07, 0x00, 0x07, 0x00 }, { 0x14, 0x7F, 0x14, 0x7F, 0x14 },
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, { 0x23, 0x13, 0x08, 0x64, 0x62 }, { 0x36, 0x49, 0x55, 0x22, 0x50 }, { 0x00, 0x05, 0x03, 0x00, 0x00 },
    { 0x00, 0x1C, 0x22, 0x41, 0x00 }, { 0x00, 0x41, 0x22, 0x1C, 0x00 }, { 0x14, 0x08, 0x3E, 0x08, 0x14 }, { 0x08, 0x08, 0x3E, 0x08, 0x08 },
    { 0x00, 0x50, 0x30, 0x00, 0x00 }, { 0x08, 0x08, 0x08, 0x08, 0x08 }, { 0x00, 0x60, 0x60, 0x00, 0x00 }, { 0x20, 0x10, 0x08, 0x04, 0x02 },
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, { 0x00, 0x42, 0x7F, 0x40, 0x00 }, { 0x42, 0x61, 0x51, 0x49, 0x46 }, { 0x21, 0x41, 0x45, 0x4B, 0x31 },
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, { 0x27, 0x45, 0x45, 0x45, 0x39 }, { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, { 0x01, 0x71, 0x09, 0x05, 0x03 },
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, { 0x06, 0x49, 0x49, 0x29, 0x1E }, { 0x00, 0x36, 0x36, 0x00, 0x00 }, { 0x00, 0x56, 0x36, 0x00, 0x00 },
    { 0x08, 0x14, 0x22, 0x41, 0x00 }, { 0x14, 0x14, 0x14, 0x14, 0x14 }, { 0x00, 0x41, 0x22, 0x14, 0x08 }, { 0x02, 0x01, 0x51, 0x09, 0x06 },
    { 0x32, 0x49, 0x79, 0x41, 0x3E }, { 0x7E, 0x11, 0x11, 0x11, 0x7E }, { 0x7F, 0x49, 0x49, 0x49, 0x36 }, { 0x3E, 0x41, 0x41, 0x41, 0x22 },
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, { 0x7F, 0x49, 0x49, 0x49, 0x41 }, { 0x7F, 0x09, 0x09, 0x09, 0x01 }, { 0x3E, 0x41, 0x49, 0x49, 0x7A },
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, { 0x00, 0x41, 0x7F, 0x41, 0x00 }, { 0x20, 0x40, 0x41, 0x3F, 0x01 }, { 0x7F, 0x08, 0x14, 0x22, 0x41 },
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, { 0x7F, 0x04, 0x08, 0x10, 0x7F }, { 0x3E, 0x41, 0x41, 0x41, 0x3E },
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, { 0x3E, 0x41, 0x51, 0x21, 0x5E }, { 0x7F, 0x09, 0x19, 0x29, 0x46 }, { 0x46, 0x49, 0x49, 0x49, 0x31 },
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, { 0x3F, 0x40, 0x40, 0x40, 0x3F }, { 0x1F, 0x20, 0x40, 0x20, 0x1F }, { 0x3F, 0x40, 0x38, 0x40, 0x3F },
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, { 0x07, 0x08, 0x70, 0x08, 0x07 }, { 0x61, 0x51, 0x49, 0x45, 0x43 }, { 0x00, 0x7F, 0x41, 0x41, 0x00 },
    { 0x15, 0x16, 0x7C, 0x16, 0x15 }, { 0x00, 0x41, 0x41, 0x7F, 0x00 }, { 0x04, 0x02, 0x01, 0x02, 0x04 }, { 0x40, 0x40, 0x40, 0x40, 0x40 },
    { 0x00, 0x01, 0x02, 0x04, 0x00 }, { 0x20, 0x54, 0x54, 0x54, 0x78 }, { 0x7F, 0x48, 0x44, 0x44, 0x38 }, { 0x38, 0x44, 0x44, 0x44, 0x20 },
    { 0x38, 0x44, 0x44, 0x48, 0x7F }, { 0x38, 0x54, 0x54, 0x54, 0x18 }, { 0x08, 0x7E, 0x09, 0x01, 0x02 }, { 0x0C, 0x52, 0x52, 0x52, 0x3E },
    { 0x7F, 0x08, 0x04, 0x04, 0x78 }, { 0x00, 0x44, 0x7D, 0x40, 0x00 }, { 0x20, 0x40, 0x44, 0x3D, 0x00 }, { 0x7F, 0x10, 0x28, 0x44, 0x00 },
    { 0x00, 0x41, 0x7F, 0x40, 0x00 }, { 0x7C, 0x04, 0x18, 0x04, 0x78 }, { 0x7C, 0x08, 0x04, 0x04, 0x78 }, { 0x38, 0x44, 0x44, 0x44, 0x38 },
    { 0x7C, 0x14, 0x14, 0x14, 0x08 }, { 0x08, 0x14, 0x14, 0x18, 0x7C }, { 0x7C, 0x08, 0x04, 0x04, 0x08 }, { 0x48, 0x54, 0x54, 0x54, 0x20 },
    { 0x04, 0x3F, 0x44, 0x40, 0x20 }, { 0x3C, 0x40, 0x40, 0x20, 0x7C }, { 0x1C, 0x20, 0x40, 0x20, 0x1C }, { 0x3C, 0x40, 0x30, 0x40, 0x3C },
    { 0x44, 0x28, 0x10, 0x28, 0x44 }, { 0x0C, 0x50, 0x50, 0x50, 0x3C }, { 0x44, 0x64, 0x54, 0x4C, 0x44 }, { 0x00, 0x08, 0x36, 0x41, 0x00 },
    { 0x00, 0x00, 0x7F, 0x00, 0x00 }, { 0x00, 0x41, 0x36, 0x08, 0x00 }, { 0x08, 0x08, 0x2A, 0x1C, 0x08 }, { 0x08, 0x1C, 0x2A, 0x08, 0x08 },
};

void fude_zoom_lcd_power_on(fude_zoom_lcd* _l) {
    memset(_l, 0, sizeof(*_l));
    memset(_l->ddram, 0x20, sizeof(_l->ddram));
    _l->function = FUDE_ZOOM_LCD_DL;
    _l->entry    = FUDE_ZOOM_LCD_ID;
    _l->powered  = 1u;
    _l->fresh    = 1u;
}

// The characters' memory's index for address _ac (two lines: 0x00–0x27 and 0x40–0x67; one: 0x00–0x4F).
RDE_INTERNAL u32 fzd_dd_index(const fude_zoom_lcd* _l, u8 _ac) {
    if(_l->function & FUDE_ZOOM_LCD_N) {
        return _ac >= 0x40u ? 40u + ((u32)_ac - 0x40u) % 40u : (u32)_ac % 40u;
    }
    return (u32)_ac % 80u;
}

// Address _ac moved _by (each line running on into the other, the last into the first).
RDE_INTERNAL u8 fzd_dd_step(const fude_zoom_lcd* _l, u8 _ac, i32 _by) {
    const u32 _i = (u32)(((i32)fzd_dd_index(_l, _ac) + _by + 80) % 80);
    if(_l->function & FUDE_ZOOM_LCD_N) {
        return (u8)(_i >= 40u ? 0x40u + (_i - 40u) : _i);
    }
    return (u8)_i;
}

// The display's window moved _by (wrapped round a line's length).
RDE_INTERNAL i8 fzd_shifted(const fude_zoom_lcd* _l, i32 _by) {
    const i32 _len = (_l->function & FUDE_ZOOM_LCD_N) ? 40 : 80;
    return (i8)((((i32)_l->shift + _by) % _len + _len) % _len);
}

void fude_zoom_lcd_write(fude_zoom_lcd* _l, b8 _rs, u8 _b) {
    const i32 _way = (_l->entry & FUDE_ZOOM_LCD_ID) ? 1 : -1;
    _l->fresh = 0u;
    if(_rs) {
        if(_l->cg) {
            _l->cgram[_l->ac & 0x3Fu] = _b & 0x1Fu;
            _l->ac = (u8)(((i32)_l->ac + _way) & 0x3F);
        } else {
            _l->ddram[fzd_dd_index(_l, _l->ac)] = _b;
            _l->ac = fzd_dd_step(_l, _l->ac, _way);
            if(_l->entry & FUDE_ZOOM_LCD_S) {
                _l->shift = fzd_shifted(_l, _way);   // (the display moving with what is written: counting up, to the left)
            }
        }
        return;
    }
    if(_b & 0x80u) {            // set the characters' address
        _l->ac = _b & 0x7Fu;
        _l->cg = 0u;
    } else if(_b & 0x40u) {     // set CGRAM's address
        _l->ac = _b & 0x3Fu;
        _l->cg = 1u;
    } else if(_b & 0x20u) {     // function set: 8 or 4 bits, lines, dots
        _l->function = _b & 0x1Cu;
        _l->half     = 0u;
    } else if(_b & 0x10u) {     // cursor or display shift: S/C (8), right (4)
        const i32 _right = (_b & 0x04u) ? 1 : -1;
        if(_b & 0x08u) {
            _l->shift = fzd_shifted(_l, -_right);   // (the display moved right: its window left)
        } else {
            _l->ac = fzd_dd_step(_l, _l->ac, _right);
        }
    } else if(_b & 0x08u) {     // display control
        _l->control = _b & 0x07u;
    } else if(_b & 0x04u) {     // entry mode
        _l->entry = _b & 0x03u;
    } else if(_b & 0x02u) {     // home
        _l->ac    = 0u;
        _l->cg    = 0u;
        _l->shift = 0;
    } else if(_b & 0x01u) {     // clear
        memset(_l->ddram, 0x20, sizeof(_l->ddram));
        _l->ac    = 0u;
        _l->cg    = 0u;
        _l->shift = 0;
        _l->entry |= FUDE_ZOOM_LCD_ID;
    }
}

void fude_zoom_lcd_strobe(fude_zoom_lcd* _l, b8 _rs, u8 _pins) {
    if(_l->function & FUDE_ZOOM_LCD_DL) {
        fude_zoom_lcd_write(_l, _rs, _pins);
        return;
    }
    const u8 _high = (u8)(_pins >> 4);   // (D7–D4)
    if(!_l->half) {
        _l->nibble = _high;
        _l->half   = 1u;
        return;
    }
    _l->half = 0u;
    fude_zoom_lcd_write(_l, _rs, (u8)((_l->nibble << 4) | _high));
}

u8 fude_zoom_lcd_shown(const fude_zoom_lcd* _l, u32 _cols, u32 _rows, u32 _col, u32 _row, b8* _cursor) {
    if(_cursor != NULL) {
        *_cursor = false;
    }
    if(_col >= _cols || _row >= _rows) {
        return 0x20u;
    }
    u32 _index;
    if(_l->function & FUDE_ZOOM_LCD_N) {
        // Two lines: rows 0 and 2 the first's (row 2 on past what row 0 shows), 1 and 3 the second's.
        const u32 _at = ((_row >> 1) * _cols + _col + (u32)(u8)_l->shift) % 40u;
        _index = (_row & 1u) * 40u + _at;
    } else {
        if(_row & 1u) {
            return 0x20u;   // (one line: its commons drive the first row only — and a 4-row's third, on past it)
        }
        _index = ((_row >> 1) * _cols + _col + (u32)(u8)_l->shift) % 80u;
    }
    if(_cursor != NULL) {
        *_cursor = !_l->cg && fzd_dd_index(_l, _l->ac) == _index;
    }
    return _l->ddram[_index];
}

void fude_zoom_lcd_glyph(const fude_zoom_lcd* _l, u8 _code, u8* _rows) {
    memset(_rows, 0, 8u);
    if(_code < 16u) {
        // Its own (0–7, and 8–15 the same again): CGRAM's.
        for(u32 _r = 0; _r < 8u; _r++) {
            _rows[_r] = _l->cgram[(_code & 7u) * 8u + _r] & 0x1Fu;
        }
        return;
    }
    if(_code == 0xDFu) {   // °
        _rows[0] = 0x1C; _rows[1] = 0x14; _rows[2] = 0x1C;
        return;
    }
    if(_code < 0x20u || _code > 0x7Fu) {
        return;   // (its katakana and symbols: not drawn)
    }
    const u8* _c = FZD_FONT[_code - 0x20u];
    for(u32 _r = 0; _r < 7u; _r++) {
        for(u32 _x = 0; _x < 5u; _x++) {
            if((_c[_x] >> _r) & 1u) {
                _rows[_r] |= (u8)(0x10u >> _x);
            }
        }
    }
}

// --- a panel meter -------------------------------------------------------------------------------

fude_zoom_meter fude_zoom_display_meter(const c8* _text) {
    fude_zoom_meter _m;
    memset(&_m, 0, sizeof(_m));
    f64 _v = 0.0;
    _m.full  = fude_zoom_circuit_value(_text, 0u, &_v) && _v > 0.0 ? _v : 20.0;
    _m.amps  = _text != NULL && strchr(_text, 'A') != NULL;
    _m.times = _text != NULL && (strstr(_text, "mV") != NULL || strstr(_text, "mA") != NULL) ? 1000.0 : 1.0;
    // (3½ digits: 1999 at most — 20 V in hundredths, 200 mV in tenths, 2 A in thousandths)
    const f64 _d = floor(log10(2000.0 / (_m.full * _m.times)) + 1e-9);
    _m.decimals = _d < 0.0 ? 0u : (_d > 3.0 ? 3u : (u32)_d);
    return _m;
}

void fude_zoom_display_meter_show(const fude_zoom_meter* _m, f64 _value, fude_zoom_meter_shown* _out) {
    memset(_out, 0, sizeof(*_out));
    memset(_out->digit, ' ', sizeof(_out->digit));
    const f64 _counts = floor(fabs(_value) * _m->times * pow(10.0, (f64)_m->decimals) + 0.5);
    _out->minus = _value < 0.0 && _counts >= 1.0;
    if(_counts >= 2000.0) {
        _out->digit[0] = '1';   // (past its range: a 1 alone, as a real one shows)
        return;
    }
    const u32 _n = (u32)_counts;
    const u32 _units = 3u - _m->decimals;   // (the place its whole units are in: shown even when 0)
    const u32 _place[4] = { _n / 1000u, (_n / 100u) % 10u, (_n / 10u) % 10u, _n % 10u };
    b8 _lead = true;
    for(u32 _k = 0; _k < 4u; _k++) {
        if(_lead && _place[_k] == 0u && _k < _units) {
            continue;   // (a leading 0: blank)
        }
        _lead = false;
        _out->digit[_k] = (c8)('0' + _place[_k]);
    }
    if(_m->decimals > 0u) {
        _out->point[_units] = true;
    }
}

// --- drawn in Play -------------------------------------------------------------------------------

RDE_INTERNAL void fzd_quad(fude_zoom_sim _all, f64 _x0, f64 _y0, f64 _x1, f64 _y1, rde_color _c) {
    const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x0, _y0 }), _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x1, _y0 });
    const fude_zoom_v2 _d = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x0, _y1 }), _e = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x1, _y1 });
    rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_vec_2F){ (f32)_e.x, (f32)_e.y }, _c, NULL);
    rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_e.x, (f32)_e.y }, (rde_vec_2F){ (f32)_d.x, (f32)_d.y }, _c, NULL);
}

RDE_INTERNAL void fzd_bar(fude_zoom_sim _all, f64 _x0, f64 _y0, f64 _x1, f64 _y1, f64 _width, rde_color _c) {
    const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x0, _y0 }), _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x1, _y1 });
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _c, (f32)fmax(_width * fude_zoom_sim_scale(_all), 1.5));
}

RDE_INTERNAL rde_color fzd_lit(rde_color _c, f64 _glow) {
    _c.a = (u8)(255.0 * fmin(fmax(_glow, 0.0), 1.0));
    return _c;
}

// How bright a light looks, of fully (the eye's: as the square root of what it gives — a multiplexed dot lit an eighth of
// the time not an eighth as bright to it).
RDE_INTERNAL f64 fzd_seen(f64 _glow) {
    return sqrt(fmin(fmax(_glow, 0.0), 1.0));
}

// A 7-segment digit's segment _g (a … g) from its middle (_cx, _cy), _w wide, _h high (its own units): its ends.
RDE_INTERNAL void fzd_segment(u32 _g, f64 _cx, f64 _cy, f64 _w, f64 _h, f64* _e) {
    static const f64 _seg[7][4] = { { -0.5, 1, 0.5, 1 }, { 0.5, 1, 0.5, 0 }, { 0.5, 0, 0.5, -1 }, { -0.5, -1, 0.5, -1 },
                                    { -0.5, 0, -0.5, -1 }, { -0.5, 1, -0.5, 0 }, { -0.5, 0, 0.5, 0 } };
    _e[0] = _cx + _seg[_g][0] * _w;
    _e[1] = _cy + _seg[_g][1] * _h * 0.5;
    _e[2] = _cx + _seg[_g][2] * _w;
    _e[3] = _cy + _seg[_g][3] * _h * 0.5;
}

void fude_zoom_display_render(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _q, fude_zoom_sim _all, f64 _hw, f64 _hh, rde_color _led,
                              f64 _time) {
    const fude_zoom_part* _part = _q->part;
    const u8* _store = fude_zoom_circuit_store_of(_c, _q);
    f64 _w = _hw * 2.0, _h = _hh * 2.0;
    fude_zoom_display_room(_part, &_w, &_h);
    // (its room's units — what its layout is in — to its own)
    const fude_zoom_sim _room = fude_zoom_sim_compose(_all, (fude_zoom_sim){ _hh / (_h * 0.5), 0.0, 0.0, 0.0 });
    const f64 _rw = _w * 0.5, _rh = _h * 0.5;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_SEG_PANEL: {
        const f32* _glow = (const f32*)_store;
        const u32 _digits = fude_zoom_display_cols(_part);
        for(u32 _d = 0; _store != NULL && _d < _digits; _d++) {
            const f64 _cx = -_rw + 80.0 + 60.0 * (f64)_d, _cy = 5.0;
            for(u32 _g = 0; _g < 7u; _g++) {
                const f64 _b = _glow[_d * 8u + _g];
                if(_b > 0.02) {
                    f64 _e[4];
                    fzd_segment(_g, _cx, _cy, 32.0, 84.0, _e);
                    fzd_bar(_room, _e[0], _e[1], _e[2], _e[3], 7.0, fzd_lit(_led, 0.25 + 0.75 * fzd_seen(_b)));
                }
            }
            const f64 _dp = _glow[_d * 8u + 7u];
            if(_dp > 0.02) {
                const fude_zoom_v2 _at = fude_zoom_sim_apply(_room, (fude_zoom_v2){ _cx + 24.0, _cy - 42.0 });
                rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_at.x, (f32)_at.y }, (f32)(4.5 * fude_zoom_sim_scale(_room)), 12u, fzd_lit(_led, 0.25 + 0.75 * fzd_seen(_dp)), NULL);
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_LED_MATRIX: {
        const f32* _glow = (const f32*)_store;
        const u32 _cols = fude_zoom_display_cols(_part), _rows = fude_zoom_display_rows(_part);
        const f64 _k = fude_zoom_sim_scale(_room);
        for(u32 _r = 0; _store != NULL && _r < _rows; _r++) {
            for(u32 _cc = 0; _cc < _cols; _cc++) {
                const f64 _b = _glow[_r * _cols + _cc];
                if(_b > 0.02) {
                    const fude_zoom_v2 _at = fude_zoom_sim_apply(_room, (fude_zoom_v2){ -_rw + 40.0 + 20.0 * (f64)_cc, _rh - 20.0 - 20.0 * (f64)_r });
                    rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_at.x, (f32)_at.y }, (f32)(8.5 * _k), 12u, fzd_lit(_led, 0.25 * fzd_seen(_b)), NULL);
                    rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_at.x, (f32)_at.y }, (f32)(6.0 * _k), 12u, fzd_lit(_led, 0.3 + 0.7 * fzd_seen(_b)), NULL);
                }
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_BAR_GRAPH: {
        // (its own units: its bars down it, as drawn — circuit.c's)
        const f32* _glow = (const f32*)_store;
        for(u32 _k = 0; _store != NULL && _k < 10u; _k++) {
            if(_glow[_k] > 0.02) {
                const f64 _y = _hh * (1.0 - 2.0 * (f64)(_k + 1u) / 11.0);
                fzd_quad(_all, -0.28 * _hw, _y - 0.06 * _hh, 0.28 * _hw, _y + 0.06 * _hh, fzd_lit(_led, 0.3 + 0.7 * fzd_seen(_glow[_k])));
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_PANEL_METER: {
        // Its LCD (its own units: its window as drawn — circuit.c's), its digits dark on it while it has its reading.
        fude_zoom_meter _m;
        memset(&_m, 0, sizeof(_m));
        _m.full     = _q->value[0];
        _m.amps     = _q->value[1] > 0.5;
        _m.times    = _q->value[2] > 0.0 ? _q->value[2] : 1.0;
        _m.decimals = (u32)_q->value[3];
        fude_zoom_meter_shown _s;
        fude_zoom_display_meter_show(&_m, _q->shown, &_s);
        const rde_color _glass = { 196, 206, 170, 255 }, _ink = { 28, 34, 24, 255 };
        fzd_quad(_all, -0.78 * _hw, -0.62 * _hh, 0.78 * _hw, 0.62 * _hh, _glass);
        const f64 _dw = 0.28 * _hw, _dh = 0.8 * _hh, _gap = 0.36 * _hw;
        if(_s.minus) {
            fzd_bar(_all, -0.72 * _hw, 0.0, -0.6 * _hw, 0.0, 0.06 * _hh, _ink);
        }
        for(u32 _k = 0; _k < 4u; _k++) {
            const f64 _cx = -0.42 * _hw + _gap * (f64)_k;
            const u8 _mask = fude_zoom_display_segments(_s.digit[_k]);
            for(u32 _g = 0; _g < 7u; _g++) {
                if(_mask & (1u << _g)) {
                    f64 _e[4];
                    fzd_segment(_g, _cx, 0.0, _dw * 0.6, _dh, _e);
                    fzd_bar(_all, _e[0], _e[1], _e[2], _e[3], 0.08 * _hh, _ink);
                }
            }
            if(_s.point[_k]) {
                const fude_zoom_v2 _at = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _cx + _gap * 0.5, -_dh * 0.5 });
                rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_at.x, (f32)_at.y }, (f32)fmax(0.05 * _hh * fude_zoom_sim_scale(_all), 1.0), 8u, _ink, NULL);
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_CHAR_LCD: {
        if(_store == NULL) {
            break;
        }
        const fude_zoom_lcd* _l = (const fude_zoom_lcd*)_store;
        const u32 _cols = fude_zoom_display_cols(_part), _rows = fude_zoom_display_rows(_part);
        const b8 _blue = strcmp(_q->color, "blue") == 0;   // (white on blue; else dark on yellow-green)
        const f64 _light = fmin(fmax(_q->shown, 0.0), 1.0);
        // Its glass: lit as bright as its backlight; its dots as dark (or as light, a blue one's) as its contrast.
        const rde_color _dark = _blue ? (rde_color){ 18, 26, 70, 255 } : (rde_color){ 92, 104, 64, 255 };
        const rde_color _lit  = _blue ? (rde_color){ 40, 92, 230, 255 } : (rde_color){ 170, 204, 56, 255 };
        const rde_color _glass = { (u8)(_dark.r + (_lit.r - _dark.r) * _light), (u8)(_dark.g + (_lit.g - _dark.g) * _light), (u8)(_dark.b + (_lit.b - _dark.b) * _light), 255 };
        const f64 _sx = -9.0 * (f64)_cols, _sy = _rh - 34.0 - 12.0;   // (its characters' top left, its room's units)
        fzd_quad(_room, _sx - 10.0, _sy + 10.0, -_sx + 10.0, _sy - 28.0 * (f64)_rows - 10.0, _glass);
        const b8 _on = _l->powered && (_l->control & FUDE_ZOOM_LCD_D);
        const b8 _boxes = _l->powered && _l->fresh;   // (powered, told nothing yet: its first row dark blocks)
        const f64 _contrast = fmin(fmax(_q->state[0], 0.0), 1.0);
        rde_color _dot = _blue ? (rde_color){ 236, 242, 255, 255 } : (rde_color){ 24, 30, 20, 255 };
        rde_color _faint = _dot;
        _dot.a   = (u8)(255.0 * _contrast);
        _faint.a = (u8)(18.0 * _contrast);
        const b8 _blink = fmod(_time, 1.0 / 1.9) < 0.5 / 1.9;
        for(u32 _r = 0; _r < _rows; _r++) {
            for(u32 _cc = 0; _cc < _cols; _cc++) {
                b8 _cursor = false;
                const u8 _code = _on ? fude_zoom_lcd_shown(_l, _cols, _rows, _cc, _r, &_cursor) : 0x20u;
                u8 _g[8];
                fude_zoom_lcd_glyph(_l, _code, _g);
                if(_on && _cursor && (_l->control & FUDE_ZOOM_LCD_C)) {
                    _g[7] = 0x1F;   // (its cursor: its eighth row)
                }
                const b8 _block = (_on && _cursor && (_l->control & FUDE_ZOOM_LCD_B) && _blink) || (_boxes && _r == 0u);
                const f64 _x0 = _sx + 18.0 * (f64)_cc + 1.0, _y0 = _sy - 28.0 * (f64)_r - 1.0;
                for(u32 _y = 0; _y < 8u; _y++) {
                    for(u32 _x = 0; _x < 5u; _x++) {
                        const b8 _set = _block || ((_g[_y] >> (4u - _x)) & 1u);
                        const f64 _px = _x0 + 3.2 * (f64)_x, _py = _y0 - 3.2 * (f64)_y;
                        fzd_quad(_room, _px, _py, _px + 2.8, _py - 2.8, _set && (_on || _boxes) ? _dot : _faint);
                    }
                }
            }
        }
        break;
    }
    default:
        break;
    }
}
