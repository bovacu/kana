#include "select.h"
#include "icons.h"
#include "draw.h"
#include "theme.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See select.h.
// ===========================================================================

void kana_selection_init(kana_selection* _selection, u32 _records) {
    memset(_selection, 0, sizeof(*_selection));
    _selection->_size  = _records;
    _selection->_marks = _records > 0 ? (u8*)rde_malloc(_records) : NULL;
    if(_selection->_marks != NULL) {
        memset(_selection->_marks, 0, _records);
    }
    _selection->order = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
}

void kana_selection_destroy(kana_selection* _selection) {
    rde_free(_selection->_marks);
    if(rde_arr_is_inited(&_selection->order)) {
        rde_arr_free(&_selection->order);
    }
    memset(_selection, 0, sizeof(*_selection));
}

b8 kana_selection_has(const kana_selection* _selection, u32 _record) {
    return _record < _selection->_size && _selection->_marks[_record] != 0;
}

void kana_selection_toggle(kana_selection* _selection, u32 _record) {
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

void kana_selection_add(kana_selection* _selection, const u32* _records, u32 _count) {
    for(u32 _i = 0; _i < _count; _i++) {
        if(!kana_selection_has(_selection, _records[_i])) {
            kana_selection_toggle(_selection, _records[_i]);
        }
    }
}

void kana_selection_clear(kana_selection* _selection) {
    if(_selection->_marks != NULL) {
        memset(_selection->_marks, 0, _selection->_size);
    }
    rde_arr_clear(&_selection->order);
}

u32 kana_selection_count(const kana_selection* _selection) {
    return (u32)rde_arr_length(&_selection->order);
}

const u32* kana_selection_records(const kana_selection* _selection) {
    return (const u32*)_selection->order.memory;
}

void kana_selection_draw_behind(b8 _ticked, rde_vec_2F _tl, f32 _size) {
    if(_ticked) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _tl.x + _size * 0.5f, _tl.y - _size * 0.5f }, (rde_vec_2F){ _size - 4.0f, _size - 4.0f },
                                        kana_theme_active()->select_fill);
    }
}

void kana_selection_draw_tick(b8 _ticked, rde_vec_2F _tl, f32 _size) {
    const kana_theme* _theme  = kana_theme_active();
    const f32         _r      = fmaxf(10.0f, _size * 0.11f);
    const rde_vec_2F  _center = { _tl.x + _size - _r - 6.0f, _tl.y - _r - 6.0f };
    if(!_ticked) {
        rde_rendering_2d_draw_circle_with_border(_center, _r, 24, _theme->surface, 1.5f, _theme->text_soft, NULL);
        return;
    }
    rde_rendering_2d_draw_circle(_center, _r, 24, _theme->accent, NULL);
    const f32 _u = _r * 0.5f;
    kana_draw_line((rde_vec_2F){ _center.x - _u, _center.y + _u * 0.05f }, (rde_vec_2F){ _center.x - _u * 0.25f, _center.y - _u * 0.7f }, fmaxf(1.2f, _r * 0.13f), _theme->on_accent);
    kana_draw_line((rde_vec_2F){ _center.x - _u * 0.25f, _center.y - _u * 0.7f }, (rde_vec_2F){ _center.x + _u, _center.y + _u * 0.75f }, fmaxf(1.2f, _r * 0.13f), _theme->on_accent);
}

// Studying: an amber star (still going); Known: a green seal with a tick. A
// disc of the colour with the icon in white on it, so it reads on any cell.
void kana_selection_draw_mark(KANA_MARK_ _mark, rde_vec_2F _tl, f32 _size, b8 _left) {
    if(_mark == KANA_MARK_NONE) {
        return;
    }
    const kana_theme* _theme = kana_theme_active();
    const f32         _r     = fminf(17.0f, fmaxf(8.0f, _size * 0.075f));
    const f32         _edge  = fmaxf(5.0f, _size * 0.035f);
    const rde_vec_2F  _c     = { _left ? _tl.x + _edge + _r : _tl.x + _size - _edge - _r, _tl.y - _edge - _r };
    const b8          _known = _mark == KANA_MARK_KNOWN;
    rde_rendering_2d_draw_circle(_c, _r, 28, _known ? _theme->score_good : _theme->score_fair, NULL);
    kana_draw_icon_fill(_known ? KANA_ICON_KNOWN : KANA_ICON_STAR, _c, _r * 1.3f, _theme->on_accent);
}
