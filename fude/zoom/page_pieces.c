// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, my pieces (piece.h): kept from the lasso, put back from Insert, laid as a stencil (instrument.h), and
// a diagram symbol's text.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- my pieces (piece.h): kept from the lasso, put back from Insert -------------------------------

#define FUDE_ZOOM_PAGE_PIECE_TILE 120.0   // a tile: its picture and its name under it
#define FUDE_ZOOM_PAGE_PIECE_PAD  12.0   // the panel's margin, and its tiles' grid's
#define FUDE_ZOOM_PAGE_PIECE_HEAD 52.0   // the panel's head over the grid: its name and its ×

// The folder they are kept in (beside the canvases), created if missing.
const c8* fude_zoom_page_pieces_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };
    fude_save_subdir("pieces", _dir, sizeof(_dir));
    return _dir;
}

// My pieces, read from their folder the first time (fude_zoom_pieces_load does nothing once they are loaded).
fude_zoom_pieces* fude_zoom_page_pieces(fude_zoom_page* _page) {
    fude_zoom_pieces_load(&_page->pieces, fude_zoom_page_pieces_dir());
    return &_page->pieces;
}

// The pictures' lines, made again (the list changed).
void fude_zoom_page_pieces_stale(fude_zoom_page* _page) {
    rde_memset(_page->piece_lines_ready, 0, sizeof(_page->piece_lines_ready));
    _page->piece_armed = -1;
}

// What selection _sel holds (the lasso's, or one of the page's own), with what is in a container among it, and that
// once each.
RDE_INTERNAL void fude_zoom_page_held_with_contents(fude_zoom_page* _page, const fude_zoom_selection* _sel, rde_arr* _out) {
    const fude_zoom_scene* _s = &_page->scene;
    rde_arr_clear(_out);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_sel->picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_sel->picks.memory)[_i].object;
        rde_arr_add(_out, (any)&_o);
        if(fude_zoom_page_container(_s, _o)) {
            fude_zoom_page_container_contents(_s, _o, _out);
        }
    }
    // Once each, in the order drawn (a paste keeps what is over what).
    u32* _all = (u32*)_out->memory;
    u32  _n   = (u32)rde_arr_length(_out);
    for(u32 _i = 1; _i < _n; _i++) {
        const u32 _v = _all[_i];
        u32 _j = _i;
        while(_j > 0 && fude_zoom_scene_object(_s, _all[_j - 1u])->z > fude_zoom_scene_object(_s, _v)->z) {
            _all[_j] = _all[_j - 1u];
            _j--;
        }
        _all[_j] = _v;
    }
    u32 _kept = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_kept == 0u || _all[_kept - 1u] != _all[_i]) {
            _all[_kept++] = _all[_i];
        }
    }
    rde_arr_resize(_out, _kept);
}

// The lasso's row: Keep — its name asked for ("Piece 3" to begin with, chosen).
RDE_INTERNAL void fude_zoom_page_piece_card(fude_zoom_page* _page) {
    if(_page->text_field == NULL || !fude_zoom_select_any(&_page->selection)) {
        return;
    }
    c8 _name[FUDE_ZOOM_PIECE_NAME];
    FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_PIECE_N, FUDE_TN(fude_zoom_page_pieces(_page)->count + 1u));
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    rde_ui_text_editor_insert_at(_page->text_field, 0, _name, rde_strlen(_name));
    rde_ui_text_editor_set_placeholder(_page->text_field, fude_text(FUDE_TEXT_ZOOM_PIECE_NAME));
    _page->text_object = FUDE_ZOOM_NONE;
    _page->text_style  = FUDE_ZOOM_PAGE_TEXT_PIECE;
    fude_zoom_page_text_card(_page, true);
    rde_ui_text_editor_select_all(_page->text_field);
}

// The lasso's row: Keep. Asks for the piece's name on the text card (page_parts.h).
void fude_zoom_page_keep_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL && _page->open) {
        fude_zoom_page_piece_card(_page);
    }
}

// What selection _sel holds (an area with what is in it) as a piece's things at their true size: copies into _clips
// (each from its own units to the canvas's own, ×1, round the middle of what is held), what they cover into *_box.
// False: nothing held. (_clips' bytes: fude_zoom_select_clips_free.)
b8 fude_zoom_page_held_clips(fude_zoom_page* _page, const fude_zoom_selection* _sel, rde_arr* _clips, fude_zoom_box* _box) {
    fude_zoom_box _sb;
    if(!fude_zoom_select_any(_sel) || !fude_zoom_select_box(_sel, &_sb)) {
        return false;
    }
    rde_arr _objects = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_page_held_with_contents(_page, _sel, &_objects);
    fude_zoom_select_clips_of(&_page->scene, (const u32*)_objects.memory, (u32)rde_arr_length(&_objects), _clips);
    rde_arr_free(&_objects);
    // From the screen to the piece's own units: the canvas's own (×1), from the middle of what is held.
    const fude_zoom_sim _from = fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _page->scene.home));
    const fude_zoom_v2  _mid  = fude_zoom_sim_apply(_from, (fude_zoom_v2){ (_sb.min_x + _sb.max_x) * 0.5, (_sb.min_y + _sb.max_y) * 0.5 });
    const fude_zoom_sim _to   = fude_zoom_sim_compose((fude_zoom_sim){ 1.0, 0.0, -_mid.x, -_mid.y }, _from);
    fude_zoom_clip* _c = (fude_zoom_clip*)_clips->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_clips); _i++) {
        _c[_i].on_screen = fude_zoom_sim_compose(_to, _c[_i].on_screen);
    }
    *_box = fude_zoom_sim_box(_to, _sb);
    return rde_arr_length(_clips) > 0u;
}

// Kept: what the lasso holds (an area with what is in it) at its true size — the canvas's own units, round the
// middle of what it holds — under that name, first in My pieces.
void fude_zoom_page_piece_commit(fude_zoom_page* _page, const c8* _text) {
    if(!fude_zoom_select_any(&_page->selection)) {
        return;
    }
    fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
    if(_lib->count >= FUDE_ZOOM_PIECES) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PIECES_FULL));
        return;
    }
    // Its name: the first line typed (none: "Piece 3").
    c8 _name[FUDE_ZOOM_PIECE_NAME];
    const c8* _a = _text;
    while(*_a == ' ' || *_a == '\n') {
        _a++;
    }
    usize _len = rde_strcspn(_a, "\n");
    while(_len > 0 && _a[_len - 1u] == ' ') {
        _len--;
    }
    if(_len == 0) {
        FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_PIECE_N, FUDE_TN(_lib->count + 1u));
    } else {
        if(_len > sizeof(_name) - 1u) {
            _len = sizeof(_name) - 1u;
            while(_len > 0 && ((u8)_a[_len] & 0xC0u) == 0x80u) {
                _len--;
            }
        }
        rde_memcpy(_name, _a, _len);
        _name[_len] = 0;
    }
    rde_arr _clips = rde_arr_new(sizeof(fude_zoom_clip), rde_memory_allocator_get_default_std());
    fude_zoom_box _box;
    const u32 _at = fude_zoom_page_held_clips(_page, &_page->selection, &_clips, &_box) ?
                    fude_zoom_pieces_add(_lib, fude_zoom_page_pieces_dir(), _name, (const fude_zoom_clip*)_clips.memory, (u32)rde_arr_length(&_clips), _box) : FUDE_ZOOM_NONE;
    fude_zoom_select_clips_free(&_clips);
    rde_arr_free(&_clips);
    fude_zoom_page_pieces_stale(_page);
    if(_at == FUDE_ZOOM_NONE) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
        return;
    }
    c8 _say[300];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PIECE_KEPT, FUDE_TS(_name));
    fude_notice_show(_say);
}

// A piece put down: in the middle of the view at its true size, held by the lasso (to move it); one undo step.
void fude_zoom_page_put_piece(fude_zoom_page* _page, const fude_zoom_piece* _piece) {
    const u32 _n = (u32)rde_arr_length(&_piece->items);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _at_arr = rde_arr_new(sizeof(fude_zoom_clip), _heap);
    rde_arr_resize(&_at_arr, _n);
    fude_zoom_clip* _at = (fude_zoom_clip*)_at_arr.memory;
    // Its middle where the middle of the screen is, in the canvas's own units; on to the screen.
    const fude_zoom_sim _home = fude_zoom_page_frame_sim(_page, _page->scene.home);
    const fude_zoom_v2  _mid  = fude_zoom_sim_apply(fude_zoom_sim_inverse(_home), (fude_zoom_v2){ 0.0, 0.0 });
    const fude_zoom_sim _to   = fude_zoom_sim_compose(_home, (fude_zoom_sim){ 1.0, 0.0, _mid.x, _mid.y });
    const fude_zoom_clip* _c = (const fude_zoom_clip*)_piece->items.memory;
    for(u32 _k = 0; _k < _n; _k++) {
        _at[_k]           = _c[_k];
        _at[_k].on_screen = fude_zoom_sim_compose(_to, _c[_k].on_screen);
    }
    fude_zoom_select_paste_clips(&_page->selection, &_page->scene, _at, _n, (fude_zoom_v2){ 0.0, 0.0 }, (rde_vec_2F){ 0.0f, 0.0f });
    rde_arr_free(&_at_arr);
    _page->map_stale = true;
}

// Piece _i put back (put_piece's).
void fude_zoom_page_piece_put(fude_zoom_page* _page, u32 _i) {
    fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
    if(_i < _lib->count) {
        fude_zoom_page_put_piece(_page, &_lib->list[_i]);
    }
}

// --- a piece laid as an instrument: a stencil (instrument.h) ---

// The longest of a piece's lines (its strokes' middles, its shapes' outlines, its fills' rings) — as many as a
// stencil holds, in the order they were — each thinned to the points it holds: into _st, its own units (its
// larger side FUDE_ZOOM_STENCIL_W, round its middle), its plate's half sizes with them. *_side: that larger side in
// the piece's units. False: no line in it.
b8 fude_zoom_page_stencil_fill(const fude_zoom_piece* _piece, fude_zoom_stencil* _st, f64* _side) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _lines = rde_arr_new(sizeof(fude_zoom_piece_line), _heap);
    const u32 _n = fude_zoom_piece_lines(_piece, false, &_pts, &_lines);
    const fude_zoom_v2*         _p = (const fude_zoom_v2*)_pts.memory;
    const fude_zoom_piece_line* _l = (const fude_zoom_piece_line*)_lines.memory;
    // The longest, by their lengths.
    u32 _pick[FUDE_ZOOM_INSTRUMENT_EDGES];
    f64 _len[FUDE_ZOOM_INSTRUMENT_EDGES];
    u32 _picked = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_l[_i].count < 2u) {
            continue;
        }
        f64 _sum = 0.0;
        for(u32 _k = 1; _k < _l[_i].count; _k++) {
            _sum += rde_math_hypot_f64(_p[_l[_i].first + _k].x - _p[_l[_i].first + _k - 1u].x, _p[_l[_i].first + _k].y - _p[_l[_i].first + _k - 1u].y);
        }
        u32 _at = _picked < FUDE_ZOOM_INSTRUMENT_EDGES ? _picked++ : FUDE_ZOOM_INSTRUMENT_EDGES;
        if(_at == FUDE_ZOOM_INSTRUMENT_EDGES) {
            u32 _least = 0;   // (full: in place of the shortest, if longer)
            for(u32 _k = 1; _k < _picked; _k++) {
                _least = _len[_k] < _len[_least] ? _k : _least;
            }
            if(_sum <= _len[_least]) {
                continue;
            }
            _at = _least;
        }
        _pick[_at] = _i;
        _len[_at]  = _sum;
    }
    // In the order they were.
    for(u32 _i = 1; _i < _picked; _i++) {
        const u32 _v = _pick[_i];
        u32 _j = _i;
        while(_j > 0 && _pick[_j - 1u] > _v) {
            _pick[_j] = _pick[_j - 1u];
            _j--;
        }
        _pick[_j] = _v;
    }
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _picked; _i++) {
        for(u32 _k = 0; _k < _l[_pick[_i]].count; _k++) {
            const fude_zoom_v2 _q = _p[_l[_pick[_i]].first + _k];
            _b = fude_zoom_box_union(_b, (fude_zoom_box){ _q.x, _q.y, _q.x, _q.y });
        }
    }
    rde_memset(_st, 0, sizeof(*_st));
    if(_picked == 0u || fude_zoom_box_is_empty(_b)) {
        rde_arr_free(&_pts);
        rde_arr_free(&_lines);
        return false;
    }
    const f64 _bw = _b.max_x - _b.min_x, _bh = _b.max_y - _b.min_y;
    *_side = rde_math_max_f64(rde_math_max_f64(_bw, _bh), 1e-300);
    const f64          _k = FUDE_ZOOM_STENCIL_W / *_side;
    const fude_zoom_v2 _c = { (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 };
    fude_zoom_v2 _thin[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
    u32 _at = 0;
    for(u32 _i = 0; _i < _picked; _i++) {
        const fude_zoom_piece_line* _ln = &_l[_pick[_i]];
        const u32 _m = fude_zoom_shape_fit_path(&_p[_ln->first], _ln->count, *_side * 0.0015, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS, _thin);
        if(_m < 2u) {
            continue;
        }
        _st->first[_st->lines]  = (u16)_at;
        _st->count[_st->lines]  = (u16)_m;
        _st->closed[_st->lines] = _ln->closed;
        for(u32 _k2 = 0; _k2 < _m; _k2++) {
            _st->points[_at++] = (fude_zoom_v2){ (_thin[_k2].x - _c.x) * _k, (_thin[_k2].y - _c.y) * _k };
        }
        _st->lines++;
    }
    _st->hw    = _bw * 0.5 * _k + 16.0;   // (a margin round its lines: its plate to take it by)
    _st->hh    = _bh * 0.5 * _k + 16.0;
    _st->piece = _piece->id;
    rde_arr_free(&_pts);
    rde_arr_free(&_lines);
    return _st->lines > 0u;
}

// Its lines and plate set for the instrument in _slot (the plate's half sizes kept on it too: its size tab's).
void fude_zoom_page_stencil_set(fude_zoom_page* _page, u32 _slot, const fude_zoom_stencil* _st) {
    _page->instruments.stencils[_slot] = *_st;
    _page->instruments.tools[_slot].radius  = _st->hw;
    _page->instruments.tools[_slot].radius2 = _st->hh;
}

// Piece _i laid as a stencil, in the middle of the view at its true size (the piece's units are the canvas's
// own: as big on the screen as the home frame is now) — on the drawing as any instrument, kept with the canvas
// (which piece it is: its stuck_mm2, unused by its kind).
void fude_zoom_page_piece_stencil(fude_zoom_page* _page, u32 _i) {
    fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
    if(_i >= _lib->count) {
        return;
    }
    fude_zoom_stencil _st;
    f64 _side = 1.0;
    if(!fude_zoom_page_stencil_fill(&_lib->list[_i], &_st, &_side)) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_STENCIL_NONE));
        return;
    }
    const i32 _slot = fude_zoom_instrument_add(&_page->instruments, FUDE_ZOOM_INSTRUMENT_STENCIL, fude_zoom_page_half(_page));
    if(_slot < 0) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_INSTRUMENTS_FULL));
        return;
    }
    fude_zoom_page_stencil_set(_page, (u32)_slot, &_st);
    fude_zoom_instrument* _t = &_page->instruments.tools[_slot];
    _t->at   = (fude_zoom_v2){ 0.0, 0.0 };
    _t->size = _side * fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _page->scene.home)) / FUDE_ZOOM_STENCIL_W;
    fude_zoom_page_instrument_stick(_page, (u32)_slot);
    _t->stuck_mm2 = (f64)_lib->list[_i].id;
}

// My pieces' panel laid out on the screen: the panel, its ×, the grid of tiles, how many tiles a row, and how tall
// all the rows are (points), for scrolling.
typedef struct {
    fude_zoom_box panel, close, grid;
    u32           cols;
    f32           content;
} fude_zoom_page_pieces_layout;

// Where the panel is (screen, centre origin, Y up): beside the toolbar, high up, as the diagram library's.
RDE_INTERNAL void fude_zoom_page_pieces_lay(fude_zoom_page* _page, fude_zoom_page_pieces_layout* _l) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const f64 _w = rde_math_min_f64(620.0, _half.x * 2.0 - 140.0), _h = rde_math_min_f64(560.0, _half.y * 2.0 - 150.0);
    const f64 _right = _half.x - 96.0, _top = _half.y - 64.0;
    _l->panel = (fude_zoom_box){ _right - _w, _top - _h, _right, _top };
    _l->close = (fude_zoom_box){ _right - FUDE_ZOOM_PAGE_PIECE_PAD - 36.0, _top - FUDE_ZOOM_PAGE_PIECE_PAD - 36.0, _right - FUDE_ZOOM_PAGE_PIECE_PAD, _top - FUDE_ZOOM_PAGE_PIECE_PAD };
    _l->grid  = (fude_zoom_box){ _l->panel.min_x + FUDE_ZOOM_PAGE_PIECE_PAD, _l->panel.min_y + FUDE_ZOOM_PAGE_PIECE_PAD,
                                 _l->panel.max_x - FUDE_ZOOM_PAGE_PIECE_PAD, _top - FUDE_ZOOM_PAGE_PIECE_HEAD };
    _l->cols    = (u32)rde_math_max_f64(1.0, rde_math_floor_f64((_l->grid.max_x - _l->grid.min_x) / FUDE_ZOOM_PAGE_PIECE_TILE));
    _l->content = (f32)((f64)((fude_zoom_page_pieces(_page)->count + _l->cols - 1u) / _l->cols) * FUDE_ZOOM_PAGE_PIECE_TILE);
}

// Tile _i's box (its top left the grid's, as scrolled).
RDE_INTERNAL fude_zoom_box fude_zoom_page_piece_tile(const fude_zoom_page* _page, const fude_zoom_page_pieces_layout* _l, u32 _i) {
    const f64 _x0 = _l->grid.min_x + (f64)(_i % _l->cols) * FUDE_ZOOM_PAGE_PIECE_TILE;
    const f64 _y1 = _l->grid.max_y - (f64)(_i / _l->cols) * FUDE_ZOOM_PAGE_PIECE_TILE + _page->pieces_scroll;
    return (fude_zoom_box){ _x0, _y1 - FUDE_ZOOM_PAGE_PIECE_TILE, _x0 + FUDE_ZOOM_PAGE_PIECE_TILE, _y1 };
}

// A tile's ×: its top right corner.
RDE_INTERNAL fude_zoom_box fude_zoom_page_piece_drop(fude_zoom_box _tile) {
    return (fude_zoom_box){ _tile.max_x - 32.0, _tile.max_y - 32.0, _tile.max_x - 4.0, _tile.max_y - 4.0 };
}

// ...and its stencil's button (laid as an instrument): its top left corner.
RDE_INTERNAL fude_zoom_box fude_zoom_page_piece_ruler(fude_zoom_box _tile) {
    return (fude_zoom_box){ _tile.min_x + 4.0, _tile.max_y - 32.0, _tile.min_x + 32.0, _tile.max_y - 4.0 };
}

// Piece _i's picture: its lines fitted into _box (made once, kept until the list changes).
void fude_zoom_page_piece_picture(fude_zoom_page* _page, u32 _i, fude_zoom_box _box, rde_color _ink) {
    fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
    if(_i >= _lib->count || _i >= FUDE_ZOOM_PIECES) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(!rde_arr_is_inited(&_page->piece_points[_i])) {
        _page->piece_points[_i] = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        _page->piece_lines[_i]  = rde_arr_new(sizeof(fude_zoom_piece_line), _heap);
    }
    if(!_page->piece_lines_ready[_i]) {
        fude_zoom_piece_lines(&_lib->list[_i], true, &_page->piece_points[_i], &_page->piece_lines[_i]);
        _page->piece_lines_ready[_i] = true;
    }
    const fude_zoom_box _pb = _lib->list[_i].box;
    const f64 _pw = rde_math_max_f64(_pb.max_x - _pb.min_x, 1e-300), _ph = rde_math_max_f64(_pb.max_y - _pb.min_y, 1e-300);
    const f64 _k  = rde_math_min_f64((_box.max_x - _box.min_x) / _pw, (_box.max_y - _box.min_y) / _ph);
    const fude_zoom_v2 _c = { (_box.min_x + _box.max_x) * 0.5 - (_pb.min_x + _pb.max_x) * 0.5 * _k, (_box.min_y + _box.max_y) * 0.5 - (_pb.min_y + _pb.max_y) * 0.5 * _k };
    const fude_zoom_v2*         _p     = (const fude_zoom_v2*)_page->piece_points[_i].memory;
    const fude_zoom_piece_line* _lines = (const fude_zoom_piece_line*)_page->piece_lines[_i].memory;
    const u32 _total = (u32)rde_arr_length(&_page->piece_points[_i]);
    const u32 _step  = _total > 1600u ? _total / 1600u + 1u : 1u;   // (a picture: a few thousand lines at most)
    for(u32 _l = 0; _l < (u32)rde_arr_length(&_page->piece_lines[_i]); _l++) {
        const fude_zoom_piece_line* _ln = &_lines[_l];
        rde_vec_2F _prev = { (f32)(_c.x + _p[_ln->first].x * _k), (f32)(_c.y + _p[_ln->first].y * _k) };
        const rde_vec_2F _first = _prev;
        for(u32 _j = _step; _j < _ln->count + _step; _j += _step) {
            const u32 _at = _j < _ln->count ? _j : _ln->count - 1u;
            const rde_vec_2F _now = { (f32)(_c.x + _p[_ln->first + _at].x * _k), (f32)(_c.y + _p[_ln->first + _at].y * _k) };
            rde_rendering_2d_draw_line_1(_prev, _now, _ink, 1.2f);
            _prev = _now;
            if(_at == _ln->count - 1u) {
                break;
            }
        }
        if(_ln->closed && _ln->count > 2u) {
            rde_rendering_2d_draw_line_1(_prev, _first, _ink, 1.2f);
        }
    }
}

// The panel, over the page: its name, its ×, a tile a piece — its picture, its name, its × (tapped twice).
void fude_zoom_page_render_pieces(fude_zoom_page* _page) {
    if(!_page->pieces_open || !_page->open || _page->presenting) {
        return;
    }
    fude_zoom_page_pieces_layout _l;
    fude_zoom_page_pieces_lay(_page, &_l);
    const fude_theme* _t = fude_theme_active();
    const fude_zoom_box _p = _l.panel;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_p.min_x + _p.max_x) * 0.5), (f32)((_p.min_y + _p.max_y) * 0.5) },
                                                        (rde_vec_2F){ (f32)(_p.max_x - _p.min_x), (f32)(_p.max_y - _p.min_y) }, 0.04f, 8u, _t->surface, 1.0f, _t->outline, NULL);
    const c8* _title = fude_text(_page->pieces_stencil ? FUDE_TEXT_ZOOM_STENCIL_PICK : FUDE_TEXT_ZOOM_INSERT_PIECES);
    fude_zoom_render_text_at(&_page->renderer, _title, (u32)rde_strlen(_title), (fude_zoom_box){ _p.min_x + FUDE_ZOOM_PAGE_PIECE_PAD + 4.0, _p.max_y - FUDE_ZOOM_PAGE_PIECE_HEAD, _l.close.min_x - 8.0, _p.max_y },
                             17.0, false, true, _t->text);
    {
        const fude_zoom_box _b = _l.close;
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        rde_rendering_2d_draw_circle_with_border(_c, 16.0f, 24u, _t->surface_2, 1.0f, _t->outline, NULL);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y - 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y + 5.0f }, _t->text, 1.6f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y + 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y - 5.0f }, _t->text, 1.6f);
    }
    fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
    if(_lib->count == 0u) {
        const c8* _none = fude_text(FUDE_TEXT_ZOOM_PIECES_EMPTY);
        fude_zoom_render_text_in(&_page->renderer, _none, (u32)rde_strlen(_none), _l.grid, 15.0, false, _t->text_soft);
        return;
    }
    for(u32 _i = 0; _i < _lib->count; _i++) {
        const fude_zoom_box _tile = fude_zoom_page_piece_tile(_page, &_l, _i);
        if(_tile.max_y > _l.grid.max_y + 0.5 || _tile.min_y < _l.grid.min_y - 0.5) {
            continue;   // (only those wholly in the grid as it is scrolled)
        }
        const rde_vec_2F _tc = { (f32)((_tile.min_x + _tile.max_x) * 0.5), (f32)((_tile.min_y + _tile.max_y) * 0.5) };
        rde_rendering_2d_draw_rounded_rectangle_with_border(_tc, (rde_vec_2F){ (f32)FUDE_ZOOM_PAGE_PIECE_TILE - 8.0f, (f32)FUDE_ZOOM_PAGE_PIECE_TILE - 8.0f }, 0.1f, 8u,
                                                            _t->page, 1.0f, _t->outline, NULL);
        fude_zoom_page_piece_picture(_page, _i, (fude_zoom_box){ _tile.min_x + 14.0, _tile.min_y + 30.0, _tile.max_x - 14.0, _tile.max_y - 14.0 }, _t->text);
        const c8* _name = _lib->list[_i].name;
        fude_zoom_render_text_in(&_page->renderer, _name, (u32)rde_strlen(_name), (fude_zoom_box){ _tile.min_x + 6.0, _tile.min_y + 6.0, _tile.max_x - 6.0, _tile.min_y + 28.0 }, 12.0, false, _t->text);
        // Its stencil's button.
        {
            const fude_zoom_box _r = fude_zoom_page_piece_ruler(_tile);
            const rde_vec_2F _rc = { (f32)((_r.min_x + _r.max_x) * 0.5), (f32)((_r.min_y + _r.max_y) * 0.5) };
            rde_rendering_2d_draw_circle_with_border(_rc, 11.0f, 20u, _t->surface_2, 1.0f, _t->outline, NULL);
            fude_zoom_render_text_in(&_page->renderer, FUDE_ICON_RULER, (u32)rde_strlen(FUDE_ICON_RULER), _r, 13.0, false, _t->text);
        }
        // Its ×: red while it waits for the second tap.
        const fude_zoom_box _d = fude_zoom_page_piece_drop(_tile);
        const rde_vec_2F _dc = { (f32)((_d.min_x + _d.max_x) * 0.5), (f32)((_d.min_y + _d.max_y) * 0.5) };
        const b8 _armed = _page->piece_armed == (i32)_i && rde_engine_get_time_now() - _page->piece_armed_at < 3.0;
        rde_rendering_2d_draw_circle_with_border(_dc, 11.0f, 20u, _armed ? _t->score_poor : _t->surface_2, 1.0f, _armed ? _t->score_poor : _t->outline, NULL);
        const rde_color _x = _armed ? fude_kit_text_on(_t->score_poor) : _t->text_soft;
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _dc.x - 4.0f, _dc.y - 4.0f }, (rde_vec_2F){ _dc.x + 4.0f, _dc.y + 4.0f }, _x, 1.4f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _dc.x - 4.0f, _dc.y + 4.0f }, (rde_vec_2F){ _dc.x + 4.0f, _dc.y - 4.0f }, _x, 1.4f);
    }
}

// A hand down: on the panel, its own (a tap, or a drag through the grid); off it, the panel goes. True: taken.
b8 fude_zoom_page_pieces_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(!_page->pieces_open) {
        return false;
    }
    fude_zoom_page_pieces_layout _l;
    fude_zoom_page_pieces_lay(_page, &_l);
    if(!fude_zoom_page_box_holds(_l.panel, _at)) {
        _page->pieces_open = false;   // (a press off it puts it away, and does nothing else)
        _page->pieces_hand = 0u;
        _page->pieces_gone = _hand;
        return true;
    }
    _page->pieces_hand    = _hand;
    _page->pieces_down    = _at;
    _page->pieces_from    = _page->pieces_scroll;
    _page->pieces_dragged = false;
    return true;
}

// The hand on My pieces' panel moved: once it has gone 8 points up or down, the grid scrolls with it, no further than
// its rows go. A hand whose press put the panel away is still taken, so the page does not see it. True: taken.
b8 fude_zoom_page_pieces_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(_page->pieces_gone == _hand && _hand != 0u) {
        return true;
    }
    if(!_page->pieces_open || _page->pieces_hand != _hand || _hand == 0u) {
        return false;
    }
    const f64 _dy = (f64)_at.y - (f64)_page->pieces_down.y;
    if(rde_math_abs_f64(_dy) > 8.0 || _page->pieces_dragged) {
        _page->pieces_dragged = true;
        fude_zoom_page_pieces_layout _l;
        fude_zoom_page_pieces_lay(_page, &_l);
        const f64 _most = rde_math_max_f64(0.0, (f64)_l.content - (_l.grid.max_y - _l.grid.min_y));
        _page->pieces_scroll = rde_math_min_f64(rde_math_max_f64(_page->pieces_from + _dy, 0.0), _most);
    }
    return true;
}

// The hand on My pieces' panel lifted: unless it dragged, what it pressed acts. The × closes the panel; a tile's
// stencil button lays it as an instrument; its × arms it, and a second tap within three seconds deletes it; anywhere
// else on a tile puts the piece down (or lays it as a stencil, when the panel was opened from the Instruments). True:
// taken.
b8 fude_zoom_page_pieces_up(fude_zoom_page* _page, u64 _hand) {
    if(_page->pieces_gone == _hand && _hand != 0u) {
        _page->pieces_gone = 0u;
        return true;
    }
    if(!_page->pieces_open || _page->pieces_hand != _hand || _hand == 0u) {
        return false;
    }
    _page->pieces_hand = 0u;
    if(_page->pieces_dragged) {
        return true;
    }
    fude_zoom_page_pieces_layout _l;
    fude_zoom_page_pieces_lay(_page, &_l);
    const rde_vec_2F _at = _page->pieces_down;
    if(fude_zoom_page_box_holds(_l.close, _at)) {
        _page->pieces_open = false;
        return true;
    }
    fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
    for(u32 _i = 0; _i < _lib->count; _i++) {
        const fude_zoom_box _tile = fude_zoom_page_piece_tile(_page, &_l, _i);
        if(!fude_zoom_page_box_holds(_tile, _at) || !fude_zoom_page_box_holds(_l.grid, _at)) {
            continue;
        }
        if(fude_zoom_page_box_holds(fude_zoom_page_piece_ruler(_tile), _at)) {
            _page->pieces_open = false;
            fude_zoom_page_piece_stencil(_page, _i);   // (laid as an instrument)
            return true;
        }
        if(fude_zoom_page_box_holds(fude_zoom_page_piece_drop(_tile), _at)) {
            // Its ×: once arms it (red), again — within three seconds — lets it go.
            if(_page->piece_armed == (i32)_i && rde_engine_get_time_now() - _page->piece_armed_at < 3.0) {
                c8 _key[FUDE_ZOOM_CUSTOM_KEY];
                rde_snprintf(_key, sizeof(_key), "%u", _lib->list[_i].id);
                fude_zoom_page_custom_delete(_page, FUDE_ZOOM_CUSTOM_PIECE, _key);   // (its template canvas with it)
            } else {
                _page->piece_armed    = (i32)_i;
                _page->piece_armed_at = rde_engine_get_time_now();
                fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PIECE_DELETE_SURE));
            }
            return true;
        }
        _page->pieces_open = false;
        if(_page->pieces_stencil) {
            fude_zoom_page_piece_stencil(_page, _i);   // (opened from the Instruments: laid as one)
        } else {
            fude_zoom_page_piece_put(_page, _i);
        }
        return true;
    }
    return true;
}

// Its text on the text card (the symbol _object's: what it says now).
void fude_zoom_page_symbol_edit(fude_zoom_page* _page, u32 _object) {
    if(_page->text_field == NULL) {
        return;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(&_page->scene, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
    _page->symbol_editing = _object;
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    rde_ui_text_editor_insert_at(_page->text_field, 0, _text, rde_strlen(_text));
    rde_ui_text_editor_select_all(_page->text_field);
    rde_ui_text_editor_set_placeholder(_page->text_field, fude_text(FUDE_TEXT_ZOOM_SYMBOL_HINT));
    _page->text_object = FUDE_ZOOM_NONE;
    _page->text_style  = FUDE_ZOOM_PAGE_TEXT_SYMBOL;
    fude_zoom_page_text_card(_page, true);
}

// A symbol in the middle of the view, as big on the screen as it comes, filled as the page, in the
// brush's colour; one undo step, the lasso on it, its text asked for.
void fude_zoom_page_symbol_put(fude_zoom_page* _page, u32 _kind) {
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL || !_page->open) {
        return;
    }
    if(_page->play_on) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLAY_LOCKED));   // (playing: nothing added)
        return;
    }
    fude_zoom_scene* _s    = &_page->scene;
    const f64        _near = 4.0 / _s->camera.z;
    const u32 _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _frame);
    const f64 _k = rde_math_max_f64(fude_zoom_sim_scale(_fs), 1e-300);
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), (fude_zoom_v2){ 0.0, 0.0 });
    // (a circuit's part: its value to begin with, as it comes — "1k", "red" —, its letters smaller; not asked for)
    const fude_zoom_part* _part = fude_zoom_part_of_kind(_kind);
    const fude_zoom_mech_part* _mech = fude_zoom_mech_of_kind(_kind);
    const fude_zoom_plan_part* _plan = fude_zoom_plan_of_kind(_kind);
    const b8 _own = _part != NULL || _mech != NULL || _plan != NULL;
    // (a plan's door, window, furniture: its true size — centimetres — on the canvas)
    f64 _hw = (f64)_info->w * 0.5 / _k, _hh = (f64)_info->h * 0.5 / _k;
    if(_plan != NULL && _plan->true_size) {
        const f64 _per = fude_zoom_page_home_units(_page, _frame) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit of the frame
        _hw = (f64)_plan->w * 5.0 / _per;
        _hh = (f64)_plan->h * 5.0 / _per;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
    const u32 _count = fude_zoom_symbol_numbers(_n, _kind, _hw, _hh, (_own ? 12.0 : FUDE_ZOOM_PAGE_SYMBOL_PX) / _k,
                                                _part != NULL ? _part->value : (_mech != NULL ? _mech->value : (_plan != NULL ? _plan->value :
                                                (rde_strcmp(_info->id, "graph") == 0 ? "y = sin(x)" : (rde_strcmp(_info->id, "number line") == 0 ? "-5 5" :
                                                (fude_zoom_chart_is(_info->id) ? fude_zoom_chart_example(_info->id) : ""))))));
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _frame, (fude_zoom_place){ _mid, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, _n, _count, _page->app->ink->color,
                                                     (f32)(1.0 / _k), FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, FUDE_THEME_PAGE_FILL, 0);
    fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _made)->box, NULL, 0, &_made, 1);
    fude_zoom_select_clear(&_page->selection);
    rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made });
    fude_zoom_select_update(&_page->selection, _s);
    if(!_own) {
        fude_zoom_page_symbol_edit(_page, _made);
    }
}

// Its text saved: the symbol made again with it, where it is (connectors joined to it following it); one undo step.
void fude_zoom_page_symbol_text_commit(fude_zoom_page* _page, const c8* _text) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _was = _page->symbol_editing;
    _page->symbol_editing = FUDE_ZOOM_NONE;
    if(_was >= fude_zoom_scene_object_count(_s)) {
        return;
    }
    const fude_zoom_object _old = *fude_zoom_scene_object(_s, _was);
    if(!(_old.flags & FUDE_ZOOM_FLAG_ALIVE) || _old.kind != FUDE_ZOOM_KIND_SHAPE || _old.channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _c0 = fude_zoom_scene_shape_numbers(_s, _was, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    c8 _before[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _c0, _before, sizeof(_before));
    const u32 _kind = fude_zoom_symbol_kind_of(_n, _c0);
    if(rde_strcmp(_before, _text) == 0 || _c0 < 4u || _kind == FUDE_ZOOM_NONE) {
        return;   // (as it was; a damaged file's kind too)
    }
    // A sized display (display.h) as big as its text says now: its pins as far apart as they were, its top left where
    // it was (what is wired to its first pins left as it is).
    f64 _hw = _n[1], _hh = _n[2], _w0, _h0, _w1, _h1;
    fude_zoom_place _at = fude_zoom_scene_place_of(_s, _was);
    const fude_zoom_part* _p0 = fude_zoom_part_of_text(_kind, _before);
    const fude_zoom_part* _p1 = fude_zoom_part_of_text(_kind, _text);
    if(_p0 != _p1 && fude_zoom_display_room(_p0, &_w0, &_h0) && fude_zoom_display_room(_p1, &_w1, &_h1)) {
        _hw = _w1 * 0.5 * _n[1] / (_w0 * 0.5);
        _hh = _h1 * 0.5 * _n[2] / (_h0 * 0.5);
        const f64 _dx = _hw - _n[1], _dy = _n[2] - _hh;   // (its middle moved, its own units)
        const f64 _ca = rde_math_cos_f64(_at.rotation) * _at.scale, _sa = rde_math_sin_f64(_at.rotation) * _at.scale;
        _at.t.x += _ca * _dx - _sa * _dy;
        _at.t.y += _sa * _dx + _ca * _dy;
    }
    f64 _m[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_symbol_numbers(_m, _kind, _hw, _hh, _n[3], _text);
    const u16 _layer = _s->layer;
    _s->layer = _old.layer;
    _s->style = 0;   // (a symbol is made again in no line style, as every remake of one is)
    const u32 _made = fude_zoom_scene_add_shape_fill(_s, _old.frame, _at, FUDE_ZOOM_SHAPE_SYMBOL, _m, _count, _old.color, _old.radius,
                                                     (u8)(_old.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _old.fill, _old.z);
    _s->layer = _layer;
    _s->style = 0;
    fude_zoom_scene_set_alive(_s, _was, false);
    rde_arr _dead = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_add(&_dead, (any)&_was);
    rde_arr_add(&_born, (any)&_made);
    fude_zoom_page_rebind(_page, &_was, &_made, 1u, &_dead, &_born);
    fude_zoom_history_push(_s, _old.frame, fude_zoom_box_union(_old.box, fude_zoom_scene_object(_s, _made)->box), (const u32*)_dead.memory, (u32)rde_arr_length(&_dead),
                           (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    rde_arr_free(&_dead);
    rde_arr_free(&_born);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        fude_zoom_pick* _p = &((fude_zoom_pick*)_page->selection.picks.memory)[_i];
        _p->object = _p->object == _was ? _made : _p->object;
    }
    fude_zoom_select_update(&_page->selection, _s);
}

// The symbol the lasso holds alone (FUDE_ZOOM_NONE: none, or more).
u32 fude_zoom_page_lone_symbol(const fude_zoom_page* _page) {
    if(rde_arr_length(&_page->selection.picks) != 1u) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object;
    const fude_zoom_object* _b = _o < fude_zoom_scene_object_count(&_page->scene) ? fude_zoom_scene_object(&_page->scene, _o) : NULL;
    return _b != NULL && (_b->flags & FUDE_ZOOM_FLAG_ALIVE) && _b->kind == FUDE_ZOOM_KIND_SHAPE && _b->channels == FUDE_ZOOM_SHAPE_SYMBOL ? _o : FUDE_ZOOM_NONE;
}
