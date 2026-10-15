// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, woodwork: boards (Insert's Board), fitting parts into a board (nest.h), joint stamps (a row of 32 mm
// holes) and joints (Insert's Finger joint, Dovetail).
//
// Sizes are asked one at a time on the numpad: each answer's callback keeps it and asks the next (a board's, a
// joint's), and the last one puts the thing down, as one undo step. Every size is kept in millimetres and made into
// the frame's units only when the thing is made (FUDE_ZOOM_PAGE_MM_PER_UNIT and the frame's scale from home).
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- boards (shape.h's BOARD; Insert's Board) -------------------------------------------------

// A length in the unit shown, to type over ("600", "23.625").
void fude_zoom_page_length_text(const fude_zoom_page* _page, f64 _mm, c8* _out, usize _size) {
    const f64 _per = fude_zoom_unit_mm(_page->units.unit);
    rde_snprintf(_out, _size, "%.4f", _per > 0.0 ? _mm / _per : _mm);
    c8* _dot = rde_strchr(_out, '.');
    if(_dot != NULL) {
        c8* _end = _out + rde_strlen(_out) - 1;
        while(_end > _dot && *_end == '0') { *_end-- = 0; }
        if(_end == _dot) { *_end = 0; }
    }
}

// A board's sizes in millimetres (length, width, thickness), its look (its grain and material:
// shape.h's fude_zoom_board_look) and its name. False: not a board.
b8 fude_zoom_page_board_of(const fude_zoom_page* _page, u32 _object, f64* _mm, u8* _look, c8* _name, usize _size) {
    const fude_zoom_scene*  _s = &_page->scene;
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_BOARD ? fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS) : 0u;
    if(_count < 4u) {
        return false;
    }
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _per  = _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * FUDE_ZOOM_PAGE_MM_PER_UNIT;
    _mm[0] = rde_math_abs_f64(_n[0]) * 2.0 * _per;
    _mm[1] = rde_math_abs_f64(_n[1]) * 2.0 * _per;
    _mm[2] = _n[2];
    if(_look != NULL) {
        *_look = fude_zoom_board_look(_n, _count);
    }
    if(_name != NULL) {
        fude_zoom_board_name(_n, _count, _name, _size);
    }
    return true;
}

// See page_parts.h. Its rings are past the numbers fude_zoom_scene_shape_numbers copies at most, but the count of them
// (what fude_zoom_board_is_cut reads) is not.
b8 fude_zoom_page_board_cut(const fude_zoom_page* _page, u32 _object) {
    const fude_zoom_scene* _s = &_page->scene;
    if(_object >= fude_zoom_scene_object_count(_s)) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_BOARD) {
        return false;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    return fude_zoom_board_is_cut(_n, _count);
}

// Declared ahead: fude_zoom_page_board_typed asks the next size with it.
RDE_INTERNAL void fude_zoom_page_board_ask(fude_zoom_page* _page, u32 _which);

// A board's size _which typed (_mm; 0 length, 1 width, 2 thickness): kept, and the next asked for; after the
// thickness its name, on the text card (its own when it has one, else "Board <n>"), or straight to the commit when
// there is no card. A size not above 0 is ignored. The units typed in become the canvas's.
void fude_zoom_page_board_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm, u32 _which) {
    RDE_UNUSED(_text);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(!(_mm > 0.0)) {
        return;
    }
    _page->units.unit     = _unit;
    _page->board_mm[_which] = _mm;
    if(_which < 2u) {
        fude_zoom_page_board_ask(_page, _which + 1u);
        return;
    }
    // Its name last, on the text card.
    c8 _name[FUDE_ZOOM_BOARD_NAME];
    _name[0] = 0;
    if(_page->board_editing != FUDE_ZOOM_NONE) {
        f64 _was[3];
        fude_zoom_page_board_of(_page, _page->board_editing, _was, NULL, _name, sizeof(_name));
    }
    if(_name[0] == 0) {
        u32 _boards = 0;
        for(u32 _i = 0; _i < fude_zoom_scene_object_count(&_page->scene); _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _i);
            _boards += (_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_BOARD ? 1u : 0u;
        }
        FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_BOARD_N, FUDE_TN(_boards + 1u));
    }
    if(_page->text_field == NULL) {
        fude_zoom_page_board_commit(_page, _name);
        return;
    }
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    rde_ui_text_editor_insert_at(_page->text_field, 0, _name, rde_strlen(_name));
    rde_ui_text_editor_select_all(_page->text_field);
    rde_ui_text_editor_set_placeholder(_page->text_field, fude_text(FUDE_TEXT_ZOOM_BOARD_NAME));
    _page->text_object = FUDE_ZOOM_NONE;
    _page->text_style  = FUDE_ZOOM_PAGE_TEXT_BOARD;
    fude_zoom_page_text_card(_page, true);
}

// The numpad's answers for a board's length, width and thickness: fude_zoom_page_board_typed with which one.
RDE_INTERNAL void fude_zoom_page_board_length_typed(any _self, const c8* _t, FUDE_ZOOM_UNIT_ _u, f64 _mm)    { fude_zoom_page_board_typed(_self, _t, _u, _mm, 0u); }
RDE_INTERNAL void fude_zoom_page_board_width_typed(any _self, const c8* _t, FUDE_ZOOM_UNIT_ _u, f64 _mm)     { fude_zoom_page_board_typed(_self, _t, _u, _mm, 1u); }
RDE_INTERNAL void fude_zoom_page_board_thickness_typed(any _self, const c8* _t, FUDE_ZOOM_UNIT_ _u, f64 _mm) { fude_zoom_page_board_typed(_self, _t, _u, _mm, 2u); }

// One of its sizes (_which: 0 length, 1 width, 2 thickness) on the numpad, what it is now to type over.
RDE_INTERNAL void fude_zoom_page_board_ask(fude_zoom_page* _page, u32 _which) {
    static const fude_zoom_numpad_done _done[3] = { fude_zoom_page_board_length_typed, fude_zoom_page_board_width_typed, fude_zoom_page_board_thickness_typed };
    static const FUDE_TEXT_ _asked[3] = { FUDE_TEXT_ZOOM_BOARD_LENGTH, FUDE_TEXT_ZOOM_BOARD_WIDTH, FUDE_TEXT_ZOOM_BOARD_THICKNESS };
    c8 _text[48];
    fude_zoom_page_length_text(_page, _page->board_mm[_which], _text, sizeof(_text));
    fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, _done[_which], _page);
    fude_zoom_numpad_ask(&_page->numpad, fude_text(_asked[_which]));
}

// A new board (FUDE_ZOOM_NONE), or that board changed: its sizes asked one by one, then its name. A cut board is left
// alone: made again from its sizes, it would lose its cuts (fude_zoom_page_board_cut).
void fude_zoom_page_board_start(fude_zoom_page* _page, u32 _board) {
    if(!_page->open || (_board != FUDE_ZOOM_NONE && fude_zoom_page_board_cut(_page, _board))) {
        return;
    }
    _page->board_editing = _board;
    _page->board_grain   = 0u;
    u8 _look = 0u;
    if(_board != FUDE_ZOOM_NONE && fude_zoom_page_board_of(_page, _board, _page->board_mm, &_look, NULL, 0)) {
        _page->board_grain    = _look & 1u;
        _page->board_material = _look >> 1;
    } else {
        _page->board_editing = FUDE_ZOOM_NONE;   // (made of what the last one was)
        _page->board_mm[0] = 600.0;   // a cabinet's side, to type over
        _page->board_mm[1] = 300.0;
        _page->board_mm[2] = 18.0;
    }
    fude_zoom_page_board_ask(_page, 0u);
}

// The board put down (in the middle of the view, at its real size) or changed where it is: one undo step.
void fude_zoom_page_board_commit(fude_zoom_page* _page, const c8* _name) {
    fude_zoom_scene* _s = &_page->scene;
    const b8 _editing = _page->board_editing != FUDE_ZOOM_NONE && _page->board_editing < fude_zoom_scene_object_count(_s) &&
                        (fude_zoom_scene_object(_s, _page->board_editing)->flags & FUDE_ZOOM_FLAG_ALIVE);
    fude_zoom_object _old;
    rde_memset(&_old, 0, sizeof(_old));
    u32 _frame;
    fude_zoom_place _place;
    if(_editing) {
        _old   = *fude_zoom_scene_object(_s, _page->board_editing);
        _frame = _old.frame;
        _place = fude_zoom_scene_place_of(_s, _page->board_editing);
    } else {
        const f64 _near = 4.0 / _s->camera.z;
        _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
        _place = (fude_zoom_place){ fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame)), (fude_zoom_v2){ 0.0, 0.0 }), 0.0, 1.0 };
    }
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _per  = _place.scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _frame, _home)) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit of its own
    if(!(_per > 0.0)) {
        return;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_board_numbers(_n, _page->board_mm[0] * 0.5 / _per, _page->board_mm[1] * 0.5 / _per, _page->board_mm[2],
                                               (u8)(_page->board_grain | (_page->board_material << 1)), _name);
    // Its wood: its material's (a board changed keeps its own fill while its material is the same).
    rde_color _wood = fude_zoom_material_wood(_page->board_material);
    if(_editing) {
        f64 _was[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _wc = fude_zoom_scene_shape_numbers(_s, _page->board_editing, _was, FUDE_ZOOM_SHAPE_NUMBERS);
        _wood = fude_zoom_board_material(_was, _wc) == _page->board_material ? _old.fill : _wood;
    }
    const f64 _k = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame));
    const u16 _layer_was = _s->layer;
    if(_editing) {
        _s->layer = _old.layer;
        _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
    }
    const u32 _new = fude_zoom_scene_add_shape_fill(_s, _frame, _place, FUDE_ZOOM_SHAPE_BOARD, _n, _count,
                                                    _editing ? _old.color : _page->app->ink->color, _editing ? _old.radius : (f32)(0.75 / rde_math_max_f64(_k, 1e-300)),
                                                    FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, _wood, _editing ? _old.z : 0u);
    _s->layer = _layer_was;
    _s->style = 0;
    const u32 _died = _page->board_editing;
    if(_editing) {
        fude_zoom_scene_set_alive(_s, _died, false);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
            fude_zoom_pick* _pick = &((fude_zoom_pick*)_page->selection.picks.memory)[_i];
            _pick->object = _pick->object == _died ? _new : _pick->object;
        }
    }
    fude_zoom_box _box = fude_zoom_scene_object(_s, _new)->box;
    if(_editing) {
        _box = fude_zoom_box_union(_box, _old.box);
    }
    rde_arr _dead = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), _made = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_add(&_made, (any)&_new);
    if(_editing) {
        rde_arr_add(&_dead, (any)&_died);
        fude_zoom_page_rebind(_page, &_died, &_new, 1u, &_dead, &_made);   // (connectors joined to it follow it)
    }
    fude_zoom_history_push(_s, _frame, _box, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_made.memory, (u32)rde_arr_length(&_made));
    rde_arr_free(&_dead);
    rde_arr_free(&_made);
    fude_zoom_select_update(&_page->selection, _s);
    _page->board_editing = FUDE_ZOOM_NONE;
}

// --- fitting parts into a board (nest.h) ------------------------------------------------------------

// A part to fit into a board: its name, its sizes (millimetres), which way its grain runs, and the lassoed board it
// is, if it is one.
typedef struct {
    c8  name[FUDE_ZOOM_BOARD_NAME];
    f64 length, width;   // mm
    u8  grain;           // 0: along its length
    u32 object;          // a board lassoed, moved onto the stock (FUDE_ZOOM_NONE: one made)
} fude_zoom_fit_part;

// Fitting's limit and a part's wood when its board has no material said.
#define FUDE_ZOOM_PAGE_FIT_MOST  200u                                   // parts in one go, at most
#define FUDE_ZOOM_PAGE_PART_WOOD ((rde_color){ 240, 222, 186, 255 })    // a part laid on a board: paler than it

// A part's wood: its board's material, paler (a board of none said: pale timber's own pale).
RDE_INTERNAL rde_color fude_zoom_page_part_wood(u8 _material) {
    if(_material == FUDE_ZOOM_MATERIAL_NONE) {
        return FUDE_ZOOM_PAGE_PART_WOOD;
    }
    const rde_color _w = fude_zoom_material_wood(_material);
    return (rde_color){ (u8)(_w.r + (255u - _w.r) * 2u / 5u), (u8)(_w.g + (255u - _w.g) * 2u / 5u), (u8)(_w.b + (255u - _w.b) * 2u / 5u), 255 };
}

// A sticky note at _at (its top left, _frame's units) saying _text, sized as the text tool sizes one.
RDE_INTERNAL u32 fude_zoom_page_add_note(fude_zoom_page* _page, u32 _frame, fude_zoom_v2 _at, const c8* _text) {
    rde_font* _font = _page->app->font;
    const f64 _k    = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame));
    if(_font == NULL || !(_k > 0.0)) {
        return FUDE_ZOOM_NONE;
    }
    const f32 _px    = (f32)FUDE_ZOOM_PAGE_STICKY_PX;
    const f32 _inner = _px * (f32)FUDE_ZOOM_PAGE_STICKY_W * 1.4f;
    const u32 _lines = fude_draw_text_wrap_lines(_font, _page->app->font_px, _text, _px, _inner);
    const f64 _w = ((f64)_inner + 1.2 * (f64)_px) / _k;
    const f64 _h = ((f64)_lines * (f64)_px * FUDE_ZOOM_PAGE_TEXT_LINE + 1.2 * (f64)_px) / _k;
    return fude_zoom_scene_add_text(&_page->scene, _frame, (fude_zoom_place){ _at, 0.0, 1.0 }, (f64)_px / _k, _w, _h, FUDE_ZOOM_TEXT_STICKY,
                                    (rde_color){ 46, 46, 54, 255 }, (rde_color){ 255, 233, 140, 255 }, _text, (u32)rde_strlen(_text), 0);
}

// What fude_zoom_page_fit works with from step to step. Given: the page and its scene, the stock board (its object
// as it was, its numbers, the home frame's units at its frame and mm a unit of its own, its size in mm, its grain,
// its material and its name) and the parts to lay. Made by the steps: whether the board has no grain; the allocator and
// the parts' arrays (nest.h's sizes and places, and whether each part's length lies along the board's x), and how
// many fitted; the board's way into its frame; what was let go and made, the wood used, the parts turned and the
// layer to go back to; and whether the board was cut. Each step takes what it reads under the names the function
// had and hands back what it made; the last frees the arrays.
typedef struct {
    fude_zoom_page*           page;
    fude_zoom_scene*          s;
    u32                       stock;
    const fude_zoom_fit_part* parts;
    u32                       n;
    const f64*                sn;
    fude_zoom_object          board;
    f64                       home_k, per, bw, bh;
    u8                        bg, material;
    const c8*                 stock_name;
    b8                        grainless;
    rde_memory_allocator*     heap;
    rde_arr                   np_arr, pl_arr, across_arr;
    fude_zoom_nest_place*     pl;
    b8*                       length_across;
    u32                       fit;
    fude_zoom_sim             to_frame;
    rde_arr                   died, born;
    f64                       used;
    u32                       turned;
    u16                       was;
    b8                        cuttable;
} fude_zoom_page_fitting;

// Where nest.h lays each part on the board, and how many fit (fude_zoom_page_fit's); each part's size across and
// up the board as its first lines say.
RDE_INTERNAL void fude_zoom_page_fit_nest(fude_zoom_page_fitting* _f) {
    fude_zoom_page* _page = _f->page;
    const fude_zoom_fit_part* _parts = _f->parts;
    const u32 _n = _f->n;
    const f64 _bw = _f->bw, _bh = _f->bh;
    const u8  _bg = _f->bg;
    const u8  _material = _f->material;
    // Each part's size across and up the board: its grain along the board's — free to turn a quarter when the board has
    // none (MDF, melamine) or the grain is not to be kept (Settings).
    const b8 _grainless = _material == FUDE_ZOOM_MATERIAL_MDF || _material == FUDE_ZOOM_MATERIAL_MELAMINE;
    const b8 _turns = _grainless || !_page->keep_grain;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _np_arr = rde_arr_new(sizeof(fude_zoom_nest_part), _heap), _pl_arr = rde_arr_new(sizeof(fude_zoom_nest_place), _heap), _across_arr = rde_arr_new(sizeof(b8), _heap);
    rde_arr_resize(&_np_arr, _n);
    rde_arr_resize(&_pl_arr, _n);
    rde_arr_resize(&_across_arr, _n);
    fude_zoom_nest_part*  _np = (fude_zoom_nest_part*)_np_arr.memory;   // (sized once: they stay put)
    fude_zoom_nest_place* _pl = (fude_zoom_nest_place*)_pl_arr.memory;
    b8* _length_across = (b8*)_across_arr.memory;   // its length along the board's x
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _along = _parts[_i].grain == 0u ? _parts[_i].length : _parts[_i].width;   // the way its grain runs
        const f64 _other = _parts[_i].grain == 0u ? _parts[_i].width : _parts[_i].length;
        _np[_i] = (fude_zoom_nest_part){ _bg == 0u ? _along : _other, _bg == 0u ? _other : _along, _turns };
        _length_across[_i] = (_parts[_i].grain == 0u) == (_bg == 0u);
    }
    const u32 _fit = fude_zoom_nest(_bw, _bh, fude_zoom_page_kerf(_page), _np, _n, _pl);
    _f->grainless     = _grainless;
    _f->heap          = _heap;
    _f->np_arr        = _np_arr;
    _f->pl_arr        = _pl_arr;
    _f->across_arr    = _across_arr;
    _f->pl            = _pl;
    _f->length_across = _length_across;
    _f->fit           = _fit;
}

// The parts laid down where nest.h put them, on the board's layer (fude_zoom_page_fit's): those made new as boards
// of its thickness and material, those lassoed moved there (the old ones let go); the wood they take and the parts
// turned counted.
RDE_INTERNAL void fude_zoom_page_fit_lay(fude_zoom_page_fitting* _f) {
    fude_zoom_page* _page = _f->page;
    fude_zoom_scene* _s = _f->s;
    const fude_zoom_fit_part* _parts = _f->parts;
    const u32 _n = _f->n;
    const f64* _sn = _f->sn;
    const fude_zoom_object _board = _f->board;
    const f64 _home_k = _f->home_k;
    const f64 _per = _f->per;
    const u8  _material = _f->material;
    rde_memory_allocator* _heap = _f->heap;
    fude_zoom_nest_place* _pl = _f->pl;
    b8* _length_across = _f->length_across;
    // Laid down: each part where nest.h put it (its middle, in the board's own units, then its frame's).
    const fude_zoom_sim _to_frame = fude_zoom_object_sim(&_board);
    const f64 _frame_mm = _home_k * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit of the board's frame
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    f64 _used = 0.0;
    u32 _turned = 0;   // (parts laid across the board's grain to fit)
    const u16 _was = _s->layer;
    _s->layer = _board.layer;
    for(u32 _i = 0; _i < _n; _i++) {
        if(!_pl[_i].placed) {
            continue;
        }
        _used += _pl[_i].w * _pl[_i].h;
        const fude_zoom_v2 _mid_local = { -rde_math_abs_f64(_sn[0]) + (_pl[_i].x + _pl[_i].w * 0.5) / _per, -rde_math_abs_f64(_sn[1]) + (_pl[_i].y + _pl[_i].h * 0.5) / _per };
        const fude_zoom_v2 _mid = fude_zoom_sim_apply(_to_frame, _mid_local);
        const f64 _turn = _length_across[_i] != _pl[_i].turned ? 0.0 : RDE_PI * 0.5;   // (turned by nest.h: the other way)
        _turned += _pl[_i].turned ? 1u : 0u;
        if(_parts[_i].object == FUDE_ZOOM_NONE) {
            f64 _n2[FUDE_ZOOM_SHAPE_NUMBERS];
            const u32 _c2 = fude_zoom_board_numbers(_n2, _parts[_i].length * 0.5 / _frame_mm, _parts[_i].width * 0.5 / _frame_mm, _sn[2],
                                                    (u8)(_parts[_i].grain | (_material << 1)), _parts[_i].name);
            const u32 _made = fude_zoom_scene_add_shape_fill(_s, _board.frame, (fude_zoom_place){ _mid, _board.rotation + _turn, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, _n2, _c2,
                                                             _board.color, _board.radius * (f32)_board.scale, FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN,
                                                             fude_zoom_page_part_wood(_material), 0);
            rde_arr_add(&_born, (any)&_made);
        } else {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _parts[_i].object);
            const f64 _scale = _o->scale * fude_zoom_page_home_units(_page, _o->frame) / _home_k;
            const u32 _moved = fude_zoom_scene_copy_stroke(_s, _parts[_i].object, _board.frame, (fude_zoom_place){ _mid, _board.rotation + _turn, _scale });
            if(_moved != FUDE_ZOOM_NONE) {
                fude_zoom_scene_set_alive(_s, _parts[_i].object, false);
                rde_arr_add(&_died, (any)&_parts[_i].object);
                rde_arr_add(&_born, (any)&_moved);
            }
        }
    }
    _f->to_frame = _to_frame;
    _f->died     = _died;
    _f->born     = _born;
    _f->used     = _used;
    _f->turned   = _turned;
    _f->was      = _was;
}

// The board cut round the parts laid on it, as its first lines say (fude_zoom_page_fit's): the board let go and
// its offcuts made; cuttable false when it could not be cut.
RDE_INTERNAL void fude_zoom_page_fit_cut(fude_zoom_page_fitting* _f) {
    fude_zoom_page* _page = _f->page;
    fude_zoom_scene* _s = _f->s;
    u32 _stock = _f->stock;
    const fude_zoom_fit_part* _parts = _f->parts;
    const u32 _n = _f->n;
    const f64* _sn = _f->sn;
    const f64 _per = _f->per;
    const f64 _bw = _f->bw, _bh = _f->bh;
    rde_memory_allocator* _heap = _f->heap;
    fude_zoom_nest_place* _pl = _f->pl;
    const u32 _fit = _f->fit;
    const fude_zoom_sim _to_frame = _f->to_frame;
    rde_arr _died = _f->died;
    rde_arr _born = _f->born;
    // The board cut: each part's place and the wood the saw takes beside it (its kerf, to its right
    // and above it, as nest.h counts it) taken out of the board — what is left are its offcuts, the
    // parts lying where they were cut from, a kerf apart. Not when a board moved onto it was cut
    // to a shape of its own (its place is only its box).
    b8 _cuttable = _fit > 0u;
    for(u32 _i = 0; _i < _n && _cuttable; _i++) {
        if(_pl[_i].placed && _parts[_i].object != FUDE_ZOOM_NONE) {
            f64 _pn[FUDE_ZOOM_SHAPE_NUMBERS];
            const u32 _pc = fude_zoom_scene_shape_numbers(_s, _parts[_i].object, _pn, FUDE_ZOOM_SHAPE_NUMBERS);
            _cuttable = !fude_zoom_board_is_cut(_pn, _pc);
        }
    }
    if(_cuttable) {
        const f64 _kerf = fude_zoom_page_kerf(_page);
        rde_arr _cut = rde_arr_new(sizeof(fude_zoom_v2), _heap), _rings = rde_arr_new(sizeof(u32), _heap);
        u32 _ring = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            if(!_pl[_i].placed) {
                continue;
            }
            const f64 _x0 = _pl[_i].x, _y0 = _pl[_i].y;
            const f64 _x1 = rde_math_min_f64(_pl[_i].x + _pl[_i].w + _kerf, _bw), _y1 = rde_math_min_f64(_pl[_i].y + _pl[_i].h + _kerf, _bh);
            const fude_zoom_v2 _c[4] = { { _x0, _y0 }, { _x1, _y0 }, { _x1, _y1 }, { _x0, _y1 } };
            for(u32 _k = 0; _k < 4u; _k++) {
                const fude_zoom_v2 _local = { -rde_math_abs_f64(_sn[0]) + _c[_k].x / _per, -rde_math_abs_f64(_sn[1]) + _c[_k].y / _per };
                const fude_zoom_v2 _at    = fude_zoom_sim_apply(_to_frame, _local);
                rde_arr_add(&_cut, (any)&_at);
                rde_arr_add(&_rings, (any)&_ring);
            }
            _ring++;
        }
        rde_arr _offcuts = rde_arr_new(sizeof(u32), _heap);
        const u32 _pieces = fude_zoom_cut_board(_s, _stock, (const fude_zoom_v2*)_cut.memory, (const u32*)_rings.memory, (u32)rde_arr_length(&_cut), &_offcuts);
        if(_pieces != FUDE_ZOOM_NONE) {
            fude_zoom_scene_set_alive(_s, _stock, false);
            rde_arr_add(&_died, (any)&_stock);
            for(u32 _i = 0; _i < (u32)rde_arr_length(&_offcuts); _i++) {
                rde_arr_add(&_born, (any)&((const u32*)_offcuts.memory)[_i]);
            }
        } else {
            _cuttable = false;
        }
        rde_arr_free(&_offcuts);
        rde_arr_free(&_cut);
        rde_arr_free(&_rings);
    }
    _f->died     = _died;
    _f->born     = _born;
    _f->cuttable = _cuttable;
}

// What did not fit, said in a sticky note beside the board (fude_zoom_page_fit's).
RDE_INTERNAL void fude_zoom_page_fit_left(fude_zoom_page_fitting* _f) {
    fude_zoom_page* _page = _f->page;
    const fude_zoom_fit_part* _parts = _f->parts;
    const u32 _n = _f->n;
    const fude_zoom_object _board = _f->board;
    const c8* _stock_name = _f->stock_name;
    rde_memory_allocator* _heap = _f->heap;
    fude_zoom_nest_place* _pl = _f->pl;
    const u32 _fit = _f->fit;
    rde_arr _born = _f->born;
    // What did not fit: a note beside the board, a line a part (the same ones counted).
    if(_fit < _n) {
        rde_arr _say = rde_arr_new(sizeof(c8), _heap);
        c8 _line[FUDE_ZOOM_BOARD_NAME + 3u * FUDE_ZOOM_UNITS_TEXT + 32u];
        FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_FIT_LEFT, FUDE_TS(_stock_name));
        rde_memcpy(rde_arr_add_n(&_say, rde_strlen(_line)), _line, rde_strlen(_line));
        rde_arr _said_arr = rde_arr_new(sizeof(b8), _heap);
        rde_arr_resize(&_said_arr, _n);
        b8* _said = (b8*)_said_arr.memory;
        for(u32 _i = 0; _i < _n; _i++) {
            if(_pl[_i].placed || _said[_i]) {
                continue;
            }
            u32 _same = 0;
            for(u32 _j = _i; _j < _n; _j++) {
                if(!_pl[_j].placed && !_said[_j] && rde_strcmp(_parts[_j].name, _parts[_i].name) == 0 && _parts[_j].length == _parts[_i].length && _parts[_j].width == _parts[_i].width) {
                    _said[_j] = true;
                    _same++;
                }
            }
            c8 _l[FUDE_ZOOM_UNITS_TEXT], _w[FUDE_ZOOM_UNITS_TEXT];
            fude_zoom_units_format(_parts[_i].length, &_page->units, _l, sizeof(_l));
            fude_zoom_units_format(_parts[_i].width, &_page->units, _w, sizeof(_w));
            if(_same > 1u) {
                rde_snprintf(_line, sizeof(_line), "\n\xE2\x80\xA2 %s %s \xC3\x97 %s (\xC3\x97%u)", _parts[_i].name, _l, _w, _same);
            } else {
                rde_snprintf(_line, sizeof(_line), "\n\xE2\x80\xA2 %s %s \xC3\x97 %s", _parts[_i].name, _l, _w);
            }
            rde_memcpy(rde_arr_add_n(&_say, rde_strlen(_line)), _line, rde_strlen(_line));
        }
        rde_arr_add(&_say, &(c8){ 0 });
        rde_arr_free(&_said_arr);
        const fude_zoom_v2 _at = { _board.box.max_x + 12.0 / rde_math_max_f64(fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _board.frame)), 1e-300), _board.box.max_y };
        const u32 _note = fude_zoom_page_add_note(_page, _board.frame, _at, (const c8*)_say.memory);
        if(_note != FUDE_ZOOM_NONE) {
            rde_arr_add(&_born, (any)&_note);
        }
        rde_arr_free(&_say);
    }
    _f->born = _born;
}

// The fit finished (fude_zoom_page_fit's): the layer as it was, one undo step for all it let go and made, the
// lasso emptied, a notice of how it went; the arrays freed.
RDE_INTERNAL void fude_zoom_page_fit_finish(fude_zoom_page_fitting* _f) {
    fude_zoom_page* _page = _f->page;
    fude_zoom_scene* _s = _f->s;
    const u32 _n = _f->n;
    const fude_zoom_object _board = _f->board;
    const f64 _bw = _f->bw, _bh = _f->bh;
    const b8  _grainless = _f->grainless;
    rde_arr _np_arr = _f->np_arr, _pl_arr = _f->pl_arr, _across_arr = _f->across_arr;
    const u32 _fit = _f->fit;
    rde_arr _died = _f->died;
    rde_arr _born = _f->born;
    const f64 _used = _f->used;
    const u32 _turned = _f->turned;
    const u16 _was = _f->was;
    const b8  _cuttable = _f->cuttable;
    _s->layer = _was;
    if(rde_arr_length(&_born) > 0) {
        fude_zoom_box _box = _board.box;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
        }
        fude_zoom_history_push(_s, _board.frame, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    }
    fude_zoom_select_clear(&_page->selection);
    // Said with the kerf it was cut with (thinnest: as good as none).
    c8 _done[200], _kerf_said[FUDE_ZOOM_UNITS_TEXT];
    const u32 _percent = (u32)rde_math_round_to_i64_f64(_bw * _bh > 0.0 ? 100.0 * _used / (_bw * _bh) : 0.0);
    if(!_cuttable) {
        FUDE_TEXTF(_done, FUDE_TEXT_ZOOM_FIT_DONE, FUDE_TN(_fit), FUDE_TN(_n), FUDE_TN(_percent));
    } else if(fude_zoom_page_kerf(_page) < 0.01) {
        FUDE_TEXTF(_done, FUDE_TEXT_ZOOM_FIT_CUT_THIN, FUDE_TN(_fit), FUDE_TN(_n), FUDE_TN(_percent));
    } else {
        fude_zoom_units_format(fude_zoom_page_kerf(_page), &_page->units, _kerf_said, sizeof(_kerf_said));
        FUDE_TEXTF(_done, FUDE_TEXT_ZOOM_FIT_CUT, FUDE_TN(_fit), FUDE_TN(_n), FUDE_TN(_percent), FUDE_TS(_kerf_said));
    }
    if(_turned > 0u && !_grainless) {
        c8 _also[120];
        FUDE_TEXTF(_also, FUDE_TEXT_ZOOM_FIT_TURNED, FUDE_TN(_turned));
        const usize _l = rde_strlen(_done);
        rde_snprintf(_done + _l, sizeof(_done) - _l, ". %s", _also);
    }
    fude_notice_show(_done);
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    rde_arr_free(&_np_arr);
    rde_arr_free(&_pl_arr);
    rde_arr_free(&_across_arr);
}

// The parts laid out on _stock (a plain board) as tightly as they go (nest.h), the kerf between them, each one's grain
// along the board's (or turned a quarter when the board has no grain, or Settings does not keep it): those made new
// (FUDE_ZOOM_NONE) made there as boards of its thickness and material, those lassoed moved there; the board cut round
// them into offcuts; what did not fit in a note beside it. One undo step; a notice says how it went.
// Its steps are above (fude_zoom_page_fit_*, in the order it takes them), sharing a fude_zoom_page_fitting.
RDE_INTERNAL void fude_zoom_page_fit(fude_zoom_page* _page, u32 _stock, const fude_zoom_fit_part* _parts, u32 _n) {
    fude_zoom_scene* _s = &_page->scene;
    f64 _sn[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _sc = fude_zoom_scene_shape_numbers(_s, _stock, _sn, FUDE_ZOOM_SHAPE_NUMBERS);
    if(_sc < 4u || fude_zoom_board_is_cut(_sn, _sc) || _n == 0) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FIT_PLAIN));
        return;
    }
    const fude_zoom_object _board = *fude_zoom_scene_object(_s, _stock);
    const f64 _home_k = fude_zoom_page_home_units(_page, _board.frame);
    const f64 _per    = _board.scale * _home_k * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit of its own
    const f64 _bw = 2.0 * rde_math_abs_f64(_sn[0]) * _per, _bh = 2.0 * rde_math_abs_f64(_sn[1]) * _per;
    const u8  _bg = fude_zoom_board_grain(_sn, _sc);   // its grain: along x (0) or y (1)
    const u8  _material = fude_zoom_board_material(_sn, _sc);   // (the parts made of it too)
    c8 _stock_name[FUDE_ZOOM_BOARD_NAME];
    fude_zoom_board_name(_sn, _sc, _stock_name, sizeof(_stock_name));
    fude_zoom_page_fitting _f = { .page = _page, .s = _s, .stock = _stock, .parts = _parts, .n = _n, .sn = _sn, .board = _board,
                                  .home_k = _home_k, .per = _per, .bw = _bw, .bh = _bh, .bg = _bg, .material = _material,
                                  .stock_name = _stock_name };
    fude_zoom_page_fit_nest(&_f);
    fude_zoom_page_fit_lay(&_f);
    fude_zoom_page_fit_cut(&_f);
    fude_zoom_page_fit_left(&_f);
    fude_zoom_page_fit_finish(&_f);
}

// Reads a list of parts into _parts (fude_zoom_fit_part, at most FUDE_ZOOM_PAGE_FIT_MOST), one a line (or after each
// ";"), each line as nest.h's fude_zoom_nest_read_line reads it: a name, length x width, and how many either after them
// ("Side 600 x 300 x 2") or first ("2x Side 600 x 300"); a number in the units shown unless it says its own (600mm,
// 23 5/8", 60cm). A part's length is the larger size, its grain along it. Returns how many lines gave a part.
RDE_INTERNAL u32 fude_zoom_page_parse_parts(const fude_zoom_page* _page, const c8* _text, rde_arr* _parts) {
    u32 _read = 0;
    for(const c8* _p = _text; *_p != 0;) {
        const c8* _end = rde_strpbrk(_p, "\n;");   // a part a line (or each after a ";")
        const usize _len = _end != NULL ? (usize)(_end - _p) : rde_strlen(_p);
        fude_zoom_nest_line _line;
        if(fude_zoom_nest_read_line(_p, _len, _page->units.unit, FUDE_ZOOM_PAGE_FIT_MOST, &_line)) {
            for(u32 _i = 0; _i < _line.count && rde_arr_length(_parts) < FUDE_ZOOM_PAGE_FIT_MOST; _i++) {
                fude_zoom_fit_part _part;
                rde_memset(&_part, 0, sizeof(_part));
                rde_snprintf(_part.name, sizeof(_part.name), "%s", _line.name);
                _part.length = _line.length;
                _part.width  = _line.width;
                _part.grain  = 0u;
                _part.object = FUDE_ZOOM_NONE;
                rde_arr_add(_parts, (any)&_part);
            }
            _read++;
        }
        if(_end == NULL) {
            break;
        }
        _p = _end + 1;
    }
    return _read;
}

// The parts typed (_text, as fude_zoom_page_parse_parts reads it) fitted into board _stock; a notice when no line
// gave a part.
void fude_zoom_page_fit_list(fude_zoom_page* _page, u32 _stock, const c8* _text) {
    if(_stock >= fude_zoom_scene_object_count(&_page->scene) || !(fude_zoom_scene_object(&_page->scene, _stock)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    rde_arr _parts = rde_arr_new(sizeof(fude_zoom_fit_part), rde_memory_allocator_get_default_std());
    if(fude_zoom_page_parse_parts(_page, _text, &_parts) == 0) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FIT_NONE));
    } else {
        fude_zoom_page_fit(_page, _stock, (const fude_zoom_fit_part*)_parts.memory, (u32)rde_arr_length(&_parts));
    }
    rde_arr_free(&_parts);
}

// The lassoed boards: how many, and the largest by area (the stock) into *_largest (FUDE_ZOOM_NONE: none).
u32 fude_zoom_page_lassoed_boards(const fude_zoom_page* _page, u32* _largest) {
    const fude_zoom_scene* _s = &_page->scene;
    u32 _count = 0;
    f64 _best  = -1.0;
    *_largest  = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        f64 _mm[3];
        if(!(fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_page_board_of(_page, _o, _mm, NULL, NULL, 0)) {
            continue;
        }
        _count++;
        if(_mm[0] * _mm[1] > _best) {
            _best     = _mm[0] * _mm[1];
            *_largest = _o;
        }
    }
    return _count;
}

// The parts card's answer: its parts, cut from the board it was opened for (fit_stock), each its two sizes (its grain
// along the longer), named by its row ("Part 3"). Nothing when that board has gone meanwhile.
RDE_INTERNAL void fude_zoom_page_parts_done(any _self, const fude_zoom_parts_size* _sizes, u32 _n) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    const u32 _stock = _page->fit_stock;
    if(_n == 0 || _stock >= fude_zoom_scene_object_count(&_page->scene) || !(fude_zoom_scene_object(&_page->scene, _stock)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    fude_zoom_fit_part _parts[FUDE_ZOOM_PARTS_ROWS];
    for(u32 _i = 0; _i < _n && _i < FUDE_ZOOM_PARTS_ROWS; _i++) {
        rde_memset(&_parts[_i], 0, sizeof(_parts[_i]));
        FUDE_TEXTF(_parts[_i].name, FUDE_TEXT_ZOOM_PART_N, FUDE_TN(_i + 1u));
        _parts[_i].length = rde_math_max_f64(_sizes[_i].width, _sizes[_i].height);
        _parts[_i].width  = rde_math_min_f64(_sizes[_i].width, _sizes[_i].height);
        _parts[_i].grain  = 0u;
        _parts[_i].object = FUDE_ZOOM_NONE;
    }
    fude_zoom_page_fit(_page, _stock, _parts, _n < FUDE_ZOOM_PARTS_ROWS ? _n : FUDE_ZOOM_PARTS_ROWS);
}

// The selection's Fit parts: one board, its list of parts to type; several, the others fitted into the largest.
void fude_zoom_page_fit_selection(fude_zoom_page* _page) {
    u32 _stock = FUDE_ZOOM_NONE;
    const u32 _boards = fude_zoom_page_lassoed_boards(_page, &_stock);
    if(_stock == FUDE_ZOOM_NONE) {
        return;
    }
    if(_boards == 1u) {
        // Its parts' sizes, typed on the card (partsform.h).
        _page->fit_stock = _stock;
        f64 _mm[3];
        c8 _name[FUDE_ZOOM_BOARD_NAME], _title[200], _kerf[96];
        fude_zoom_page_board_of(_page, _stock, _mm, NULL, _name, sizeof(_name));
        FUDE_TEXTF(_title, FUDE_TEXT_ZOOM_FIT_TITLE, FUDE_TS(_name));
        fude_zoom_page_kerf_said(_page, _kerf, sizeof(_kerf));
        fude_zoom_parts_open(&_page->parts_form, _title, _kerf, _page->units_choice, fude_zoom_page_parts_done, _page);
        return;
    }
    // The others lassoed, onto the largest.
    rde_arr _parts = rde_arr_new(sizeof(fude_zoom_fit_part), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        fude_zoom_fit_part _part;
        rde_memset(&_part, 0, sizeof(_part));
        f64 _mm[3];
        if(_o == _stock || !fude_zoom_page_board_of(_page, _o, _mm, &_part.grain, _part.name, sizeof(_part.name))) {
            continue;
        }
        _part.grain &= 1u;       // (its look: its material over its grain)
        _part.length = _mm[0];   // (its own x: what its grain says "along" of)
        _part.width  = _mm[1];
        _part.object = _o;
        rde_arr_add(&_parts, (any)&_part);
    }
    fude_zoom_page_fit(_page, _stock, (const fude_zoom_fit_part*)_parts.memory, (u32)rde_arr_length(&_parts));
    rde_arr_free(&_parts);
}

// --- joint stamps: a row of 32 mm holes (research §2.10, item 9) --------------------------------

#define FUDE_ZOOM_PAGE_HOLE_PITCH 32.0   // mm between holes (the cabinet maker's "System 32")
#define FUDE_ZOOM_PAGE_HOLE_D      5.0   // mm across a hole (shelf pins, hinge plates)

// The row typed: holes 32 mm apart over that length (from end to end), across the middle of the view
// at their real size, each a circle (its middle snapped to). One undo step.
void fude_zoom_page_holes_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text);
    fude_zoom_page*  _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s    = &_page->scene;
    _page->units.unit = _unit;
    if(!(_mm >= 0.0) || !_page->open) {
        return;
    }
    const u32 _count = (u32)rde_math_min_f64(rde_math_floor_f64(_mm / FUDE_ZOOM_PAGE_HOLE_PITCH + 1e-9) + 1.0, 200.0);
    const f64 _near  = 4.0 / _s->camera.z;
    const u32 _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame)), (fude_zoom_v2){ 0.0, 0.0 });
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _per  = fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _frame, _home)) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit of the frame
    const f64 _k    = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame));
    if(!(_per > 0.0) || !(_k > 0.0)) {
        return;
    }
    const f64 _r     = FUDE_ZOOM_PAGE_HOLE_D * 0.5 / _per;
    const f64 _pitch = FUDE_ZOOM_PAGE_HOLE_PITCH / _per;
    const f64 _from  = _mid.x - _pitch * (f64)(_count - 1u) * 0.5;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _born_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_born_arr, _count);
    u32* _born = (u32*)_born_arr.memory;
    fude_zoom_box _box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _count; _i++) {
        const f64 _n[2] = { _r, _r };
        _born[_i] = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ { _from + _pitch * (f64)_i, _mid.y }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, _n, 2u,
                                              _page->app->ink->color, (f32)(0.6 / _k), 0u, 0);
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _born[_i])->box);
    }
    fude_zoom_history_push(_s, _frame, _box, NULL, 0, _born, _count);
    rde_arr_free(&_born_arr);
}

// --- joints (Insert's Finger joint, Dovetail) ----------------------------------------------------

#define FUDE_ZOOM_PAGE_DOVETAIL_SLOPE 0.125   // a dovetail's sides: one across in eight (hardwood's)
// A joint's outline's points at most: a finger joint's fingers are held to fit.
#define FUDE_ZOOM_PAGE_JOINT_POINTS   512u

// Its outline (millimetres, x along the board's edge from its left end, y 0 the end and -depth
// the shoulder), into _p: how many points. A finger joint's square teeth (an odd number, so
// both ends are fingers); a dovetail's tails, wider at the end, a half pin at each side.
RDE_INTERNAL u32 fude_zoom_page_joint_outline(u8 _kind, const f64 _mm[3], fude_zoom_v2* _p) {
    const f64 _len = rde_math_max_f64(_mm[0], 1.0), _d = rde_math_max_f64(_mm[2], 0.5);
    u32 _n = 0;
    if(_kind == 1u) {
        u32 _count = (u32)rde_math_max_f64(1.0, rde_math_round_f64(_len / rde_math_max_f64(_mm[1], 0.5)));
        if(_count % 2u == 0u) {
            _count = _len / (f64)(_count - 1u) - rde_math_max_f64(_mm[1], 0.5) < rde_math_max_f64(_mm[1], 0.5) - _len / (f64)(_count + 1u) ? _count - 1u : _count + 1u;
        }
        _count = _count > (FUDE_ZOOM_PAGE_JOINT_POINTS - 2u) / 2u ? ((FUDE_ZOOM_PAGE_JOINT_POINTS - 2u) / 2u) | 1u : _count;
        const f64 _w = _len / (f64)_count;
        _p[_n++] = (fude_zoom_v2){ 0.0, -_d };
        for(u32 _i = 0; _i < _count; _i++) {
            const f64 _y = _i % 2u == 0u ? 0.0 : -_d;
            _p[_n++] = (fude_zoom_v2){ (f64)_i * _w, _y };
            _p[_n++] = (fude_zoom_v2){ (f64)(_i + 1u) * _w, _y };
        }
        _p[_n++] = (fude_zoom_v2){ _len, -_d };
        return _n;
    }
    const u32 _tails = (u32)rde_math_min_f64(rde_math_max_f64(rde_math_round_f64(_mm[1]), 1.0), 60.0);
    const f64 _s     = _d * FUDE_ZOOM_PAGE_DOVETAIL_SLOPE;
    f64 _pin = rde_math_min_f64(rde_math_max_f64(_d * 0.5, 6.0), _len / (f64)(_tails * 2u));   // a pin's narrow side, at the end
    f64 _tail = (_len - (f64)_tails * _pin) / (f64)_tails;               // a tail's wide side, at the end
    if(_tail < 2.0 * _s + 1.0) {
        _tail = 2.0 * _s + 1.0;
        _pin  = rde_math_max_f64((_len - (f64)_tails * _tail) / (f64)_tails, 0.5);
    }
    _p[_n++] = (fude_zoom_v2){ 0.0, -_d };
    for(u32 _k = 0; _k < _tails; _k++) {
        const f64 _a = _pin * 0.5 + (f64)_k * (_tail + _pin), _b = _a + _tail;
        _p[_n++] = (fude_zoom_v2){ _a + _s, -_d };
        _p[_n++] = (fude_zoom_v2){ _a, 0.0 };
        _p[_n++] = (fude_zoom_v2){ _b, 0.0 };
        _p[_n++] = (fude_zoom_v2){ _b - _s, -_d };
    }
    _p[_n++] = (fude_zoom_v2){ _len, -_d };
    return _n;
}

// It put down: its outline at its real size in the middle of the view, a straight line a
// stretch (their ends snap), one undo step.
void fude_zoom_page_joint_put(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    fude_zoom_v2 _p[FUDE_ZOOM_PAGE_JOINT_POINTS];
    const u32 _n = fude_zoom_page_joint_outline(_page->joint_kind, _page->joint_mm, _p);
    const f64 _near  = 4.0 / _s->camera.z;
    const u32 _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _frame)), (fude_zoom_v2){ 0.0, 0.0 });
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const f64 _per  = fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _frame, _home)) * FUDE_ZOOM_PAGE_MM_PER_UNIT;   // mm a unit of the frame
    const f64 _k    = fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _frame));
    if(_n < 2u || !(_per > 0.0) || !(_k > 0.0)) {
        return;
    }
    const fude_zoom_v2 _o = { _mid.x - _page->joint_mm[0] * 0.5 / _per, _mid.y + _page->joint_mm[2] * 0.5 / _per };   // (its middle in the middle)
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _born = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_box _box = fude_zoom_box_empty();
    for(u32 _i = 0; _i + 1u < _n; _i++) {
        const fude_zoom_v2 _a = { _o.x + _p[_i].x / _per, _o.y + _p[_i].y / _per };
        const f64 _num[2] = { (_p[_i + 1u].x - _p[_i].x) / _per, (_p[_i + 1u].y - _p[_i].y) / _per };
        if(rde_math_abs_f64(_num[0]) + rde_math_abs_f64(_num[1]) <= 0.0) {
            continue;
        }
        const u32 _line = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _a, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, _num, 2u,
                                                    _page->app->ink->color, (f32)(0.75 / _k), 0u, 0);
        rde_arr_add(&_born, (any)&_line);
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _line)->box);
    }
    if(rde_arr_length(&_born) > 0u) {
        fude_zoom_history_push(_s, _frame, _box, NULL, 0, (const u32*)_born.memory, (u32)rde_arr_length(&_born));
    }
    rde_arr_free(&_born);
}

// Declared ahead: fude_zoom_page_joint_typed asks the next size with it.
RDE_INTERNAL void fude_zoom_page_joint_ask(fude_zoom_page* _page);

// The numpad's answer for the joint's size being asked (joint_step: its length, a finger's width or a dovetail's tails,
// its depth): kept, the next asked, and after the last the joint put down. A dovetail's tails are a count, read from
// the text as typed. A value not above 0 is ignored.
RDE_INTERNAL void fude_zoom_page_joint_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    _page->units.unit = _unit;
    if(!_page->open || _page->joint_step >= 3u) {
        return;
    }
    // (a dovetail's tails: how many, as typed)
    const f64 _v = _page->joint_kind == 2u && _page->joint_step == 1u ? rde_strtod(_text != NULL ? _text : "0", NULL) : _mm;
    if(!(_v > 0.0)) {
        return;
    }
    _page->joint_mm[_page->joint_step++] = _v;
    if(_page->joint_step < 3u) {
        fude_zoom_page_joint_ask(_page);
    } else {
        fude_zoom_page_joint_put(_page);
    }
}

// The joint's size being asked on the numpad, what it is now to type over.
RDE_INTERNAL void fude_zoom_page_joint_ask(fude_zoom_page* _page) {
    c8 _text[48];
    const b8 _count = _page->joint_kind == 2u && _page->joint_step == 1u;
    if(_count) {
        rde_snprintf(_text, sizeof(_text), "%u", (u32)_page->joint_mm[1]);
    } else {
        fude_zoom_page_length_text(_page, _page->joint_mm[_page->joint_step], _text, sizeof(_text));
    }
    fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_joint_typed, _page);
    static const FUDE_TEXT_ _asked[3] = { FUDE_TEXT_ZOOM_JOINT_LENGTH, FUDE_TEXT_ZOOM_JOINT_FINGER, FUDE_TEXT_ZOOM_JOINT_DEPTH };
    fude_zoom_numpad_ask(&_page->numpad, fude_text(_count ? FUDE_TEXT_ZOOM_JOINT_TAILS : _asked[_page->joint_step]));
}

// Insert's Finger joint (_kind 1) or Dovetail (2): its sizes asked one by one, starting from what is offered (a
// drawer's side, an 18 mm board).
void fude_zoom_page_joint_start(fude_zoom_page* _page, u8 _kind) {
    if(!_page->open) {
        return;
    }
    _page->joint_kind  = _kind;
    _page->joint_step  = 0u;
    _page->joint_mm[0] = _kind == 1u ? 300.0 : 200.0;
    _page->joint_mm[1] = _kind == 1u ? 15.0 : 3.0;
    _page->joint_mm[2] = 18.0;
    fude_zoom_page_joint_ask(_page);
}

// Insert's row of holes: its length asked for on the numpad, ten pitches offered.
void fude_zoom_page_holes_start(fude_zoom_page* _page) {
    if(!_page->open) {
        return;
    }
    c8 _text[48];
    fude_zoom_page_length_text(_page, 320.0, _text, sizeof(_text));   // ten pitches, to type over
    fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_holes_typed, _page);
    fude_zoom_numpad_ask(&_page->numpad, fude_text(FUDE_TEXT_ZOOM_HOLES_LENGTH));
}
