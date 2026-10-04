// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/select.h"
#include "drawing/widgets/icons.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See select.h.
// ===========================================================================

void fude_selection_init(fude_selection* _selection, u32 _records) {
    memset(_selection, 0, sizeof(*_selection));
    _selection->_size  = _records;
    _selection->_marks = _records > 0 ? (u8*)rde_malloc(_records) : NULL;
    if(_selection->_marks != NULL) {
        memset(_selection->_marks, 0, _records);
    }
    _selection->order = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
}

void fude_selection_destroy(fude_selection* _selection) {
    rde_free(_selection->_marks);
    if(rde_arr_is_inited(&_selection->order)) {
        rde_arr_free(&_selection->order);
    }
    memset(_selection, 0, sizeof(*_selection));
}

b8 fude_selection_has(const fude_selection* _selection, u32 _record) {
    return _record < _selection->_size && _selection->_marks[_record] != 0;
}

void fude_selection_toggle(fude_selection* _selection, u32 _record) {
    if(_record >= _selection->_size) {
        return;
    }
    if(_selection->_marks[_record] == 0) {
        _selection->_marks[_record] = 1;
        rde_arr_add(&_selection->order, &_record);
        return;
    }

    // Unticked: out of the order, the rest keeping theirs.
    _selection->_marks[_record] = 0;
    u32*      _order = (u32*)_selection->order.memory;
    const u32 _n     = (u32)rde_arr_length(&_selection->order);
    u32       _kept  = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_order[_i] != _record) {
            _order[_kept++] = _order[_i];
        }
    }
    _selection->order.count = _kept;   // rde_arr has no truncate: rde_arr_clear's operation, to a length
}

void fude_selection_add(fude_selection* _selection, const u32* _records, u32 _count) {
    for(u32 _i = 0; _i < _count; _i++) {
        if(!fude_selection_has(_selection, _records[_i])) {
            fude_selection_toggle(_selection, _records[_i]);
        }
    }
}

void fude_selection_clear(fude_selection* _selection) {
    if(_selection->_marks != NULL) {
        memset(_selection->_marks, 0, _selection->_size);
    }
    rde_arr_clear(&_selection->order);
}

u32 fude_selection_count(const fude_selection* _selection) {
    return (u32)rde_arr_length(&_selection->order);
}

const u32* fude_selection_records(const fude_selection* _selection) {
    return (const u32*)_selection->order.memory;
}

void fude_selection_draw_behind(b8 _ticked, rde_vec_2F _tl, f32 _size) {
    if(_ticked) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(_tl.x + _size * 0.5f), _tl.y - _size * 0.5f }, (rde_vec_2F){ _size - 4.0f, _size - 4.0f },
                                        fude_theme_active()->select_fill);
    }
}

void fude_selection_draw_tick(b8 _ticked, rde_vec_2F _tl, f32 _size) {
    const fude_theme* _theme  = fude_theme_active();
    const f32         _r      = fmaxf(10.0f, _size * 0.11f);
    const rde_vec_2F  _center = { _tl.x + _size - _r - 6.0f, _tl.y - _r - 6.0f };
    if(!_ticked) {
        rde_rendering_2d_draw_circle_with_border(fude_draw_at(_center), _r, 24, _theme->surface, 1.5f, _theme->text_soft, NULL);
        return;
    }
    rde_rendering_2d_draw_circle(fude_draw_at(_center), _r, 24, _theme->accent, NULL);
    const f32 _u = _r * 0.5f;
    fude_draw_line((rde_vec_2F){ _center.x - _u, _center.y + _u * 0.05f }, (rde_vec_2F){ _center.x - _u * 0.25f, _center.y - _u * 0.7f }, fmaxf(1.2f, _r * 0.13f), _theme->on_accent);
    fude_draw_line((rde_vec_2F){ _center.x - _u * 0.25f, _center.y - _u * 0.7f }, (rde_vec_2F){ _center.x + _u, _center.y + _u * 0.75f }, fmaxf(1.2f, _r * 0.13f), _theme->on_accent);
}

// Studying: an amber star (still going); Known: a green seal with a tick. A
// disc of the colour with the icon in white on it, so it reads on any cell.
void fude_selection_draw_mark(FUDE_MARK_ _mark, rde_vec_2F _tl, f32 _size, b8 _left) {
    if(_mark == FUDE_MARK_NONE) {
        return;
    }
    const fude_theme* _theme = fude_theme_active();
    const f32         _r     = fminf(17.0f, fmaxf(8.0f, _size * 0.075f));
    const f32         _edge  = fmaxf(5.0f, _size * 0.035f);
    const rde_vec_2F  _c     = { _left ? _tl.x + _edge + _r : _tl.x + _size - _edge - _r, _tl.y - _edge - _r };
    const b8          _known = _mark == FUDE_MARK_KNOWN;
    rde_rendering_2d_draw_circle(fude_draw_at(_c), _r, 28, _known ? _theme->score_good : _theme->score_fair, NULL);
    fude_draw_icon_fill(_known ? FUDE_ICON_KNOWN : FUDE_ICON_STAR, _c, _r * 1.3f, _theme->on_accent);
}
