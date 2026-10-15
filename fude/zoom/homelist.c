// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/homelist.h"
#include "zoom/words.h"
#include "zoom/pages.h"

// ===========================================================================
// See homelist.h.
//
// One folder's cards are gathered in one pass over the notes' list for its folders and one for its canvases, each
// canvas put into its place as it is read (an insertion sort).
// ===========================================================================

// --- what it lists -----------------------------------------------------------------------------

// See the header. The canvas's own path from notes.h, with its extension (".kana") swapped for _ext.
void fude_zoom_home_side_path(u32 _canvas, const c8* _ext, c8* _out, usize _size) {
    c8 _kana[RDE_MAX_PATH];
    fude_notes_canvas_path(_canvas, _kana, sizeof(_kana));
    c8* _dot = rde_strrchr(_kana, '.');
    if(_dot != NULL) {
        *_dot = 0;
    }
    rde_snprintf(_out, _size, "%s.%s", _kana, _ext);
}

// See the header. Folders are dated by when they were made; a canvas by the later of when it was made and when its
// words file says it was left. Of each canvas's words file only its start is read, where that date is
// (fude_zoom_words_left), not its words.
u32 fude_zoom_home_gather(const fude_notes* _notes, u32 _folder, rde_arr* _cards) {
    rde_arr_clear(_cards);
    const fude_note* _all = (const fude_note*)_notes->notes.memory;
    const u32        _n   = (u32)rde_arr_length(&_notes->notes);
    // Its folders first, in their order.
    for(u32 _i = 0; _i < _n; _i++) {
        if(_all[_i].parent != _folder || _all[_i].kind != FUDE_NOTE_FOLDER) {
            continue;
        }
        fude_zoom_home_card _c;
        rde_memset(&_c, 0, sizeof(_c));
        _c.note   = _all[_i].id;
        _c.kind   = FUDE_NOTE_FOLDER;
        _c.edited = _all[_i].created;
        _c.inside = fude_notes_count_in(_notes, _all[_i].id);
        rde_arr_add(_cards, (any)&_c);
    }
    const u32 _folders = (u32)rde_arr_length(_cards);
    // Then its canvases, the most recently left first.
    for(u32 _i = 0; _i < _n; _i++) {
        if(_all[_i].parent != _folder || _all[_i].kind != FUDE_NOTE_CANVAS) {
            continue;
        }
        fude_zoom_home_card _c;
        rde_memset(&_c, 0, sizeof(_c));
        _c.note   = _all[_i].id;
        _c.kind   = FUDE_NOTE_CANVAS;
        _c.pdf    = _all[_i].document != 0u;
        _c.edited = _all[_i].created;
        c8 _path[RDE_MAX_PATH];
        fude_zoom_home_side_path(_c.note, "words", _path, sizeof(_path));
        u64 _left = 0u;
        if(fude_zoom_words_left(_path, &_left) && _left > _c.edited) {
            _c.edited = _left;
        }
        fude_zoom_home_side_path(_c.note, "pages", _path, sizeof(_path));
        fude_zoom_pages _pages;
        _c.notebook = fude_zoom_pages_load(&_pages, _path) && _pages.on;
        // Into its place: the newest left first, and of those left at the same time, the higher id (the newer made)
        // first.
        u32 _at = (u32)rde_arr_length(_cards);
        const fude_zoom_home_card* _list = (const fude_zoom_home_card*)_cards->memory;
        while(_at > _folders && (_list[_at - 1u].edited < _c.edited || (_list[_at - 1u].edited == _c.edited && _list[_at - 1u].note < _c.note))) {
            _at--;
        }
        rde_arr_add(_cards, (any)&_c);
        fude_zoom_home_card* _m = (fude_zoom_home_card*)_cards->memory;
        rde_memmove(&_m[_at + 1u], &_m[_at], (rde_arr_length(_cards) - 1u - _at) * sizeof(fude_zoom_home_card));
        _m[_at] = _c;
    }
    return (u32)rde_arr_length(_cards);
}

// See the header. n cards take n·_least + (n - 1)·_gap, so n = floor((_width + _gap) / (_least + _gap)). A width or
// least that is not above 0 (NaN included) gives 1.
u32 fude_zoom_home_columns(f32 _width, f32 _least, f32 _gap) {
    if(!(_width > 0.0f) || !(_least > 0.0f)) {
        return 1u;
    }
    const f32 _k = rde_math_floor_f32((_width + _gap) / (_least + _gap));
    return _k < 1.0f ? 1u : (u32)_k;
}

