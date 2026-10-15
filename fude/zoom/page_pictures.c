// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, pictures and Fill: pictures brought in (the pickers, Insert's choices) and the Fill tool (fill.h).
//
// The file is in two parts: pictures (what the pickers bring, Insert's choices, and the pen given back to or taken
// from the app's own tools), then the Fill tool, which fills the closed thing under a tap or, when there is none, the
// area the ink on screen closes round it (the paint bucket, bucket.h).
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// A line's ends this near (screen units), or a fifth of its size: closed, for Fill.
#define FUDE_ZOOM_PAGE_FILL_GAP   24.0

// --- pictures ---------------------------------------------------------------------------------

// Writes a JPEG's or PNG's size in pixels, read from its bytes: a PNG's from its IHDR chunk, a JPEG's from its first
// frame marker. Returns false when it is neither, or its size is 0.
RDE_INTERNAL b8 fude_zoom_picture_size(const u8* _b, u32 _n, u32* _w, u32* _h) {
    if(_n >= 24u && _b[0] == 0x89 && _b[1] == 'P' && _b[2] == 'N' && _b[3] == 'G') {
        *_w = ((u32)_b[16] << 24) | ((u32)_b[17] << 16) | ((u32)_b[18] << 8) | _b[19];
        *_h = ((u32)_b[20] << 24) | ((u32)_b[21] << 16) | ((u32)_b[22] << 8) | _b[23];
        return *_w > 0u && *_h > 0u;
    }
    if(_n < 4u || _b[0] != 0xFF || _b[1] != 0xD8) {
        return false;
    }
    // JPEG: the markers in turn, to the frame's (SOF0..15, but not DHT, JPG, DAC), each skipped by its length.
    u32 _at = 2u;
    while(_at + 9u < _n) {
        if(_b[_at] != 0xFF) {
            return false;
        }
        const u8  _m   = _b[_at + 1u];
        const u32 _len = ((u32)_b[_at + 2u] << 8) | _b[_at + 3u];
        if(_m >= 0xC0 && _m <= 0xCF && _m != 0xC4 && _m != 0xC8 && _m != 0xCC) {
            *_h = ((u32)_b[_at + 5u] << 8) | _b[_at + 6u];
            *_w = ((u32)_b[_at + 7u] << 8) | _b[_at + 8u];
            return *_w > 0u && *_h > 0u;
        }
        _at += 2u + _len;
    }
    return false;
}

// See page_parts.h. What the pickers brought (extension.h: imported): each picture in the middle of the screen (each
// next 24 points down and across), its longer side 60% of the screen's, one undo step for them all; then selected,
// the Lasso in hand. A PDF among them opens a canvas of its own over it instead (only the first PDF), and then the
// pictures and other PDFs brought with it are not put anywhere: a notice says so. Every picture is taken; one that
// cannot be read is said.
void fude_zoom_kind_imported(fude_app* _app, u8 _kind, const c8* const* _paths, u32 _count) {
    RDE_UNUSED(_kind);
    fude_zoom_page*  _page = fude_zoom_page_the;
    fude_zoom_scene* _s    = &_page->scene;
    if(!_page->open || _count == 0u) {
        return;
    }
    // A PDF: a canvas of its own over it (the first PDF only); the pictures are gathered apart, the paths only (the
    // caller's, valid through this call).
    rde_arr _pictures_arr = rde_arr_new(sizeof(const c8*), rde_memory_allocator_get_default_std());
    b8      _made_canvas  = false;
    u32     _left_out     = 0;   // PDFs after the first
    c8      _pdf_name[FUDE_NOTE_NAME];
    _pdf_name[0] = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(!fude_import_is_pdf(_paths[_i])) {
            rde_arr_add(&_pictures_arr, (any)&_paths[_i]);
            continue;
        }
        if(_made_canvas) {
            _left_out++;
            continue;
        }
        fude_pdf* _check = fude_pdf_open(_paths[_i]);
        c8 _name[FUDE_NOTE_NAME];
        fude_import_name(_paths[_i], _name, sizeof(_name));
        u32 _id = 0u;
        if(_check != NULL) {
            fude_pdf_close(_check);
            // Opened: the page comes over to it.
            _id = fude_documents_new_canvas(_app, FUDE_NOTE_DOCUMENT_OWN, _paths[_i], _name, 0u);
        }
        if(_id == 0u) {
            fude_notice_show(fude_text(FUDE_TEXT_DOC_CANT_OPEN));
            continue;
        }
        c8 _line[512];
        FUDE_TEXTF(_line, FUDE_TEXT_DOC_IMPORTED, FUDE_TS(_name));
        fude_notice_show(_line);
        rde_snprintf(_pdf_name, sizeof(_pdf_name), "%s", _name);
        _made_canvas = true;
    }
    if(_made_canvas || rde_arr_length(&_pictures_arr) == 0u) {
        // The pictures with a PDF, and the PDFs after it: not onto the canvas being left, and said (in place of the
        // PDF's notice).
        if(_made_canvas && (rde_arr_length(&_pictures_arr) > 0u || _left_out > 0u)) {
            c8 _line[512];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_IMPORT_ONLY_PDF, FUDE_TS(_pdf_name));
            fude_notice_show(_line);
        }
        rde_arr_free(&_pictures_arr);
        return;
    }
    _paths = (const c8* const*)_pictures_arr.memory;
    _count = (u32)rde_arr_length(&_pictures_arr);
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const fude_zoom_v2 _mid  = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ 0.0, 0.0 });
    const f64          _near = 4.0 / _s->camera.z;
    const u32          _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _mid.x - _near, _mid.y - _near, _mid.x + _near, _mid.y + _near });
    const fude_zoom_sim _to   = fude_zoom_sim_compose(fude_zoom_camera_sim(&_s->camera), fude_zoom_scene_sim(_s, _frame, _s->camera.frame));
    const fude_zoom_sim _bk   = fude_zoom_sim_inverse(_to);
    rde_arr _born  = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr _bytes = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    b8      _bad   = false;
    for(u32 _i = 0; _i < _count; _i++) {
        u32 _w = 0, _h = 0;
        if(!fude_picture_bytes(_paths[_i], FUDE_ZOOM_PAGE_PICTURE_PX, &_bytes) || !fude_zoom_picture_size((const u8*)_bytes.memory, (u32)rde_arr_length(&_bytes), &_w, &_h)) {
            _bad = true;
            continue;
        }
        // Its size on screen: its longer side 60% of the screen's, as it fits.
        const f64 _per_px = rde_math_min_f64(1.2 * _half.x / (f64)_w, 1.2 * _half.y / (f64)_h);
        const f64 _step   = 24.0 * (f64)rde_arr_length(&_born);
        const fude_zoom_v2  _at = fude_zoom_sim_apply(_bk, (fude_zoom_v2){ _step, -_step });
        const f64           _k  = fude_zoom_sim_scale(_bk);
        const fude_zoom_place _place = { _at, rde_math_atan2_f64(_bk.b, _bk.a), 1.0 };
        const u32 _o = fude_zoom_scene_add_image(_s, _frame, _place, (f64)_w * _per_px * 0.5 * _k, (f64)_h * _per_px * 0.5 * _k, (const u8*)_bytes.memory,
                                                 (u32)rde_arr_length(&_bytes), 0);
        rde_arr_add(&_born, (any)&_o);
    }
    rde_arr_free(&_bytes);
    if(rde_arr_length(&_born) > 0) {
        fude_zoom_box _box = fude_zoom_box_empty();
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
        }
        fude_zoom_history_push(_s, _frame, _box, NULL, 0, (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        // Selected, to be put where it goes.
        fude_toolbar_set_tool(&_app->ui->bar, FUDE_TOOL_LASSO);
        _page->tool_seen  = (u8)FUDE_TOOL_LASSO;
        _page->shape_tool = 0;
        fude_zoom_select_clear(&_page->selection);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_born); _i++) {
            const fude_zoom_pick _p = { ((const u32*)_born.memory)[_i] };
            rde_arr_add(&_page->selection.picks, (any)&_p);
        }
        fude_zoom_select_update(&_page->selection, _s);
        fude_ui_update(_app->ui);
    }
    if(_bad) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PICTURE_UNREADABLE));
    }
    rde_arr_free(&_born);
    rde_arr_free(&_pictures_arr);
}

// See page_parts.h. _index is product.h's FUDE_ZOOM_INSERT_ (FUDE_ZOOM_PICTURE_CHOICES' order), compared here by its
// number. Photos (where there is a library; Files otherwise) and Files bring pictures in; a text, a sticky note or a
// formula puts the pen to that tool (chosen again: it draws again); the rest open their card, panel or numpad. Always
// returns false: the panel closes.
b8 fude_zoom_picture_choose(fude_app* _app, u32 _index) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_index == 10u && _page != NULL) {
        // Its panel (a tap on a tile puts that one in the middle of the view; its × or a tap off it puts it away).
        // Opened, never toggled: a tablet's tap can come as the touch and as the mouse SDL makes of it, choosing twice.
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_DIAGRAMS);
        return false;
    }
    if(_index == 15u && _page != NULL) {
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_CIRCUITS);   // The parts: circuit.h.
        return false;
    }
    if(_index == 16u && _page != NULL) {
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_MECHANISMS);   // mech.h
        return false;
    }
    if(_index == 17u && _page != NULL) {
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_PLANS);   // plan.h
        return false;
    }
    if(_index == 18u && _page != NULL) {
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_MATHS);   // plot.h
        return false;
    }
    if(_index == 20u && _page != NULL) {
        fude_zoom_page_examples_card(_page);   // Which of them: a canvas each.
        return false;
    }
    if(_index == 19u && _page != NULL) {
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_SEWING);
        return false;
    }
    if(_index == 11u && _page != NULL) {
        fude_zoom_page_kanban_card(_page);   // Its columns asked for: it goes in the middle of the view.
        return false;
    }
    if(_index == 12u && _page != NULL) {
        fude_zoom_page_area_card(_page);   // Its name asked for: round what the lasso holds, else the view.
        return false;
    }
    if(_index == 14u && _page != NULL) {
        fude_zoom_page_sheet_start(_page, FUDE_ZOOM_NONE);   // Its card: its paper, its scale, its size.
        return false;
    }
    if(_index == 13u && _page != NULL) {
        // My pieces' panel: opened, never toggled (a tablet's tap can come twice).
        if(!_page->pieces_open) {
            _page->pieces_scroll = 0.0;
            _page->piece_armed   = -1;
        }
        fude_zoom_page_pieces(_page);
        _page->pieces_open    = true;
        _page->pieces_stencil = false;
        return false;
    }
    if((_index == 8u || _index == 9u) && _page != NULL) {
        // Finger joints (1) or dovetails (2): its sizes on the numpad at once; it goes in the middle of the view.
        fude_zoom_page_joint_start(_page, (u8)(_index - 7u));
        return false;
    }
    if(_index == 7u) {
        fude_import_files(_app);   // A PDF picked: a canvas over it (fude_zoom_kind_imported).
        return false;
    }
    if(_index == 6u && _page != NULL) {
        fude_zoom_page_holes_start(_page);   // Its length on the numpad at once.
        return false;
    }
    if(_index == 5u && _page != NULL) {
        // Its sizes on the numpad at once: it goes in the middle of the view.
        fude_zoom_page_board_start(_page, FUDE_ZOOM_NONE);
        return false;
    }
    if(_index == 4u && _page != NULL) {
        fude_zoom_page_mermaid_card(_page);   // The card at once: the diagram goes in the middle of the view.
        return false;
    }
    if((_index >= 2u && _index <= 3u) || _index == 21u) {
        if(_page == NULL) {
            return false;
        }
        const u8 _mode = _index == 21u ? 3u : (u8)(_index - 1u);   // 1 a text, 2 a sticky note, 3 a formula.
        const u8 _next = _page->text_tool == _mode ? 0u : _mode;
        fude_zoom_page_pen_modes_off(_page);
        _page->text_tool = _next;
        if(_next != 0) {
            fude_zoom_page_pen_under(_page);
        }
        fude_toolbar_refresh(&_app->ui->bar);
        return false;
    }
    if(_index == 0u && fude_import_photos_available()) {
        fude_import_photos(_app);
    } else {
        fude_import_files(_app);
    }
    return false;
}

// See page_parts.h: only the text tools (a text, a sticky note, a formula) show chosen.
b8 fude_zoom_picture_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    const u8 _mode = _index == 21u ? 3u : _index >= 2u && _index <= 3u ? (u8)(_index - 1u) : 0u;
    return fude_zoom_page_the != NULL && _mode != 0u && fude_zoom_page_the->text_tool == _mode;
}

// See page_parts.h: one of the text tools is in hand.
b8 fude_zoom_picture_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && fude_zoom_page_the->text_tool != 0;
}

// See page_parts.h: Insert's choices, in product.h's FUDE_ZOOM_INSERT_ order.
const fude_extension_choice FUDE_ZOOM_PICTURE_CHOICES[] = {
    { FUDE_TEXT_ZOOM_PICTURE_PHOTOS, FUDE_ICON_IMAGE,  fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_PICTURE_FILES,  FUDE_ICON_FOLDER, fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_TEXT,           FUDE_ICON_TEXT,   fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_STICKY,         FUDE_ICON_STICKY, fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_MERMAID,        FUDE_ICON_GRAPH,  fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_INSERT_BOARD,   FUDE_ICON_RECTANGLE, fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_INSERT_HOLES,   FUDE_ICON_PAPER_DOTS, fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_INSERT_PDF,     FUDE_ICON_BOOK,   fude_zoom_picture_choose },   // (a canvas of its own: pdfview.h)
    { FUDE_TEXT_ZOOM_INSERT_FINGERS, FUDE_ICON_COLUMNS, fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_INSERT_DOVETAIL, FUDE_ICON_GUIDED, fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_INSERT_DIAGRAM, FUDE_ICON_SHAPES, fude_zoom_picture_choose },   // (the diagram library: symbol.h)
    { FUDE_TEXT_ZOOM_INSERT_KANBAN,  FUDE_ICON_COLUMNS, fude_zoom_picture_choose },
    { FUDE_TEXT_ZOOM_INSERT_AREA,    FUDE_ICON_AREA,    fude_zoom_picture_choose },   // (a named region, on the map: map.h)
    { FUDE_TEXT_ZOOM_INSERT_PIECES,  FUDE_ICON_FOLDER,  fude_zoom_picture_choose },   // (My pieces: piece.h)
    { FUDE_TEXT_ZOOM_INSERT_SHEET,   FUDE_ICON_PAPER_SQUARES, fude_zoom_picture_choose },   // (rulers on its sides, to scale: sheet.h)
    { FUDE_TEXT_ZOOM_INSERT_PARTS,   FUDE_ICON_SIGNAL_HIGH, fude_zoom_picture_choose },   // (electronics: circuit.h)
    { FUDE_TEXT_ZOOM_INSERT_MECH,    FUDE_ICON_ROTATE, fude_zoom_picture_choose },        // (mechanisms: mech.h)
    { FUDE_TEXT_ZOOM_INSERT_PLAN,    FUDE_ICON_AREA,   fude_zoom_picture_choose },        // (a building's plan: plan.h)
    { FUDE_TEXT_ZOOM_INSERT_MATHS,   FUDE_ICON_CHART_LINE, fude_zoom_picture_choose },    // (graphs, axes: plot.h)
    { FUDE_TEXT_ZOOM_INSERT_SEWING,  FUDE_ICON_CUT,    fude_zoom_picture_choose },        // (a pattern's marks)
    { FUDE_TEXT_ZOOM_EXAMPLES,       FUDE_ICON_GRADUATION, fude_zoom_picture_choose },    // (canvases to learn from: examples.h)
    { FUDE_TEXT_ZOOM_INSERT_FORMULA, FUDE_ICON_CALCULATOR, fude_zoom_picture_choose },    // (LaTeX, set: formula.h)
};
// A compile-time check that the table has its FUDE_ZOOM_INSERT_COUNT rows (the size is -1, which does
// not compile, when not).
typedef char fude_zoom_picture_choices_counted[(sizeof(FUDE_ZOOM_PICTURE_CHOICES) / sizeof(FUDE_ZOOM_PICTURE_CHOICES[0]) == FUDE_ZOOM_INSERT_COUNT) ? 1 : -1];

// See page_parts.h. The pen in the bar's hand under one of the app's own tools (Fill, a shape): what the pen does then
// is the app tool's, never the eraser's or the lasso's left underneath. The bar's taps are counted as seen, so the
// change is not taken for a tap of the bar's own (which would put the app tool down).
void fude_zoom_page_pen_under(fude_zoom_page* _page) {
    fude_toolbar* _bar = &_page->app->ui->bar;
    if(_bar->tool == FUDE_TOOL_ERASE || _bar->tool == FUDE_TOOL_LASSO) {
        fude_toolbar_set_tool(_bar, FUDE_TOOL_DRAW);
        _page->tool_taps_seen = _bar->tool_taps;
        _page->tool_seen      = (u8)FUDE_TOOL_DRAW;
        fude_zoom_select_clear(&_page->selection);
    }
}

// See page_parts.h. The pen drawing again: what one of the app's tools had it do (a shape or a guide, a wall, the
// curve, fill, trim, a measure, text, wires, rooms) put down; a curve being tapped is kept as far as it goes, and a
// tape left showing goes. Each of those tools, chosen, puts the others down with this; so does another topic. (Not
// the Saw: it cuts with whichever of them is in hand.)
void fude_zoom_page_pen_modes_off(fude_zoom_page* _page) {
    if(_page->curve_tool) {
        fude_zoom_page_curve_commit(_page, false);
    }
    _page->shape_tool   = 0;
    _page->curve_tool   = false;
    _page->curve_count  = 0;
    _page->fill_tool    = false;
    _page->trim_tool    = false;
    _page->measure_tool = 0;
    _page->tape_shown   = false;
    _page->text_tool    = 0;
    _page->wire_tool    = false;
    _page->wiring       = false;
    _page->room_tool    = false;
}

// --- the Fill tool (fill.h) ------------------------------------------------------------------------

// Writes a gesture's outline into _out: its pieces round _object's, in order (they sit one after another in the
// table), their points in their frame's units. _q is a scratch array for the points. Returns its first piece, or
// FUDE_ZOOM_NONE when a piece of it is gone (an erase cut it: then it is not one closed line any more).
RDE_INTERNAL u32 fude_zoom_page_gesture_outline(const fude_zoom_scene* _s, u32 _object, rde_arr* _out, rde_arr* _q) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    u32 _first = _object, _last = _object;
    while(_first > 0 && fude_zoom_scene_object(_s, _first)->id != _o->gesture) {
        const fude_zoom_object* _b = fude_zoom_scene_object(_s, _first - 1u);
        if(_b->gesture != _o->gesture || _b->frame != _o->frame) {
            break;
        }
        _first--;
    }
    while(_last + 1u < fude_zoom_scene_object_count(_s)) {
        const fude_zoom_object* _n = fude_zoom_scene_object(_s, _last + 1u);
        if(_n->gesture != _o->gesture || _n->frame != _o->frame || !(_n->flags & FUDE_ZOOM_FLAG_CONTINUES)) {
            break;
        }
        _last++;
    }
    rde_arr_clear(_out);
    for(u32 _i = _first; _i <= _last; _i++) {
        const fude_zoom_object* _p = fude_zoom_scene_object(_s, _i);
        if(_p->kind != FUDE_ZOOM_KIND_STROKE || !(_p->flags & FUDE_ZOOM_FLAG_ALIVE) || _p->count == 0) {
            return FUDE_ZOOM_NONE;
        }
        rde_arr_clear(_q);
        rde_arr_add_n(_q, _p->count);
        if(!fude_zoom_scene_points(_s, _i, (fude_zoom_qpoint*)_q->memory)) {
            return FUDE_ZOOM_NONE;
        }
        const fude_zoom_qpoint* _pts = (const fude_zoom_qpoint*)_q->memory;
        // Each next piece begins where the last ended, so its first point is left out.
        for(u32 _k = _i == _first ? 0u : 1u; _k < _p->count; _k++) {
            const fude_zoom_v2 _at = fude_zoom_scene_point_at(_p, &_pts[_k]);
            rde_arr_add(_out, (any)&_at);
        }
    }
    return _first;
}

// Returns whether a gesture's outline (_out, from fude_zoom_page_gesture_outline) is a closed line. 1: its ends near
// each other (a fifth of its size, or _gap); 2: its last third crossing its first third (fill.h's region then takes
// the loop round a tap, of the loops it draws itself); 0: neither, or under 3 points.
RDE_INTERNAL u8 fude_zoom_page_closed(rde_arr* _out, f64 _gap) {
    fude_zoom_v2* _p = (fude_zoom_v2*)_out->memory;
    const u32     _n = (u32)rde_arr_length(_out);
    if(_n < 3u) {
        return 0;
    }
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _k = 0; _k < _n; _k++) {
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _p[_k].x, _p[_k].y, _p[_k].x, _p[_k].y });
    }
    const f64 _size = rde_math_max_f64(_b.max_x - _b.min_x, _b.max_y - _b.min_y);
    if(rde_math_hypot_f64(_p[_n - 1u].x - _p[0].x, _p[_n - 1u].y - _p[0].y) <= rde_math_max_f64(0.2 * _size, _gap)) {
        return 1u;
    }
    // Its last third crossing its first third: any piece of the one crossing any piece of the other.
    const u32 _head = _n / 3u, _tail = _n - _n / 3u;
    for(u32 _i = 0; _i + 1u < _head; _i++) {
        for(u32 _j = _n - 2u; _j >= _tail && _j > _i + 1u; _j--) {
            const fude_zoom_v2 _a = _p[_i], _c = _p[_i + 1u], _d = _p[_j], _e = _p[_j + 1u];
            const f64 _den = (_c.x - _a.x) * (_e.y - _d.y) - (_c.y - _a.y) * (_e.x - _d.x);
            if(_den == 0.0) {
                continue;
            }
            const f64 _t = ((_d.x - _a.x) * (_e.y - _d.y) - (_d.y - _a.y) * (_e.x - _d.x)) / _den;
            const f64 _u = ((_d.x - _a.x) * (_c.y - _a.y) - (_d.y - _a.y) * (_c.x - _a.x)) / _den;
            if(_t >= 0.0 && _t <= 1.0 && _u >= 0.0 && _u <= 1.0) {
                return 2u;
            }
        }
    }
    return 0;
}

// The paint bucket's settings, handed to fude_zoom_bucket_region, all in screen points.
#define FUDE_ZOOM_PAGE_BUCKET_GAP   8.0    // gaps in the ink under this are closed
#define FUDE_ZOOM_PAGE_BUCKET_CELL  1.25   // the grid's cell, at its finest
#define FUDE_ZOOM_PAGE_BUCKET_UNDER 1.5    // how far the fill reaches on under the ink that bounds it

// See page_parts.h. The ink on screen (strokes, shapes' outlines, fills' edges), as segments on the screen with their
// half-widths (at least half a point), appended to _ink_out (fude_zoom_bucket_ink): what closes an area round a tap.
// Markers and hidden layers' things are left out. With _walls (a room's bounds), only a plan's walls and openings
// count: a symbol only when it is a door, a window or an opening, and no fills, texts or pictures. It reads the
// editable frames as last worked out (_page->editable).
void fude_zoom_page_view_ink(fude_zoom_page* _page, rde_arr* _ink_out, b8 _walls) {
    fude_zoom_scene*   _s    = &_page->scene;
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _found = rde_arr_new(sizeof(u32), _heap);
    rde_arr _q     = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr _pts   = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr* _ink  = _ink_out;
    const fude_zoom_visible* _v = (const fude_zoom_visible*)_page->editable.memory;
    for(u32 _f = 0; _f < (u32)rde_arr_length(&_page->editable); _f++) {
        const fude_zoom_sim _to    = _v[_f].to_screen;
        const f64           _scale = fude_zoom_sim_scale(_to);
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _v[_f].frame, fude_zoom_sim_box(fude_zoom_sim_inverse(_to), (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y }), &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32               _object = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->count == 0 || (_o->flags & FUDE_ZOOM_FLAG_MARKER) || fude_zoom_scene_hides(_s, _o)) {
                continue;
            }
            if(_walls && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_SYMBOL) {
                // A room's bounds: its walls and what is in them (doors, windows), never its furniture.
                // (a symbol with no number, or one no kind can be, from a damaged file: not an opening)
                f64 _kind_n[1] = { -1.0 };
                const b8 _has = fude_zoom_scene_shape_numbers(_s, _object, _kind_n, 1u) >= 1u && _kind_n[0] >= 0.0 && _kind_n[0] < 4294967296.0;
                if(!_has || !fude_zoom_plan_opening(fude_zoom_plan_of_kind((u32)_kind_n[0]))) {
                    continue;
                }
            }
            if(_walls && (_o->kind == FUDE_ZOOM_KIND_FILL || _o->kind == FUDE_ZOOM_KIND_TEXT || _o->kind == FUDE_ZOOM_KIND_IMAGE)) {
                continue;
            }
            if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
                rde_arr_clear(&_q);
                rde_arr_add_n(&_q, _o->count);
                if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
                    continue;
                }
                const fude_zoom_qpoint* _p = (const fude_zoom_qpoint*)_q.memory;
                fude_zoom_v2 _last = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_p[0]));
                f64          _lr   = fude_zoom_scene_radius_at(_o, &_p[0]) * _scale;
                for(u32 _k = (_o->count > 1u ? 1u : 0u); _k < _o->count; _k++) {
                    const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_p[_k]));
                    const f64          _r  = fude_zoom_scene_radius_at(_o, &_p[_k]) * _scale;
                    const fude_zoom_bucket_ink _seg = { _last, _at, rde_math_max_f64(rde_math_max_f64(_r, _lr), 0.5) };
                    rde_arr_add(_ink, (any)&_seg);
                    _last = _at;
                    _lr   = _r;
                }
            } else if(_o->kind == FUDE_ZOOM_KIND_SHAPE || _o->kind == FUDE_ZOOM_KIND_FILL) {
                b8 _closed = true;
                rde_arr_clear(&_pts);
                f64 _r = 0.5;
                if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                    fude_zoom_scene_shape_outline(_s, _object, 64u, &_pts, &_closed);
                    _r = rde_math_max_f64((f64)_o->radius * _o->scale * _scale, 0.5);
                } else {
                    // A fill's outline only (its first ring).
                    rde_arr_clear(&_q);
                    rde_arr_add_n(&_q, _o->count);
                    if(fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
                        const fude_zoom_qpoint* _p = (const fude_zoom_qpoint*)_q.memory;
                        for(u32 _k = 0; _k < _o->count && _p[_k].time == _p[0].time; _k++) {
                            const fude_zoom_v2 _at = fude_zoom_scene_point_at(_o, &_p[_k]);
                            rde_arr_add(&_pts, (any)&_at);
                        }
                    }
                }
                const u32 _m = (u32)rde_arr_length(&_pts);
                const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
                for(u32 _k = 0; _k + 1u < _m || (_closed && _k < _m && _m > 2u); _k++) {
                    const fude_zoom_bucket_ink _seg = { fude_zoom_sim_apply(_to, _p[_k]), fude_zoom_sim_apply(_to, _p[(_k + 1u) % _m]), _r };
                    rde_arr_add(_ink, (any)&_seg);
                }
            }
        }
    }
    rde_arr_free(&_found);
    rde_arr_free(&_q);
    rde_arr_free(&_pts);
}

// The paint bucket (bucket.h): the area round _screen that the ink on screen closes (gaps under
// FUDE_ZOOM_PAGE_BUCKET_GAP points closed), made a fill in the brush's colour, in the camera's frame, just under the
// lowest line its box reaches (so every line round it is drawn over it). One undo step. An area that is open, or
// nothing to fill, is said; a tap on the ink itself does nothing.
RDE_INTERNAL void fude_zoom_page_bucket(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_scene*   _s    = &_page->scene;
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _ink   = rde_arr_new(sizeof(fude_zoom_bucket_ink), _heap);
    rde_arr _found = rde_arr_new(sizeof(u32), _heap);
    rde_arr _q     = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr _pts   = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    fude_zoom_page_view_ink(_page, &_ink, false);
    rde_arr _out   = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _rings = rde_arr_new(sizeof(u32), _heap);
    const FUDE_ZOOM_BUCKET_ _got = fude_zoom_bucket_region((const fude_zoom_bucket_ink*)_ink.memory, (u32)rde_arr_length(&_ink),
                                                           (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y }, (fude_zoom_v2){ _screen.x, _screen.y },
                                                           FUDE_ZOOM_PAGE_BUCKET_CELL, FUDE_ZOOM_PAGE_BUCKET_GAP, FUDE_ZOOM_PAGE_BUCKET_UNDER, &_out, &_rings);
    if(_got == FUDE_ZOOM_BUCKET_FILLED) {
        // Into the camera's frame, under the lowest line it reaches; its points kept in steps of about
        // a 64th of a screen point from its first.
        const u32           _frame = _s->camera.frame;
        const fude_zoom_sim _back  = fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera));
        const u32           _k     = (u32)rde_arr_length(&_out);
        fude_zoom_v2*       _p     = (fude_zoom_v2*)_out.memory;
        fude_zoom_box       _box   = fude_zoom_box_empty();
        for(u32 _i = 0; _i < _k; _i++) {
            _p[_i] = fude_zoom_sim_apply(_back, _p[_i]);
            _box   = fude_zoom_box_union(_box, (fude_zoom_box){ _p[_i].x, _p[_i].y, _p[_i].x, _p[_i].y });
        }
        u64 _z = 0;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _frame, _box, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_found.memory)[_i]);
            if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && (_o->kind == FUDE_ZOOM_KIND_STROKE || _o->kind == FUDE_ZOOM_KIND_SHAPE) && (_z == 0 || _o->z < _z)) {
                _z = _o->z;
            }
        }
        const i8  _qe   = fude_zoom_quantum_for(_s->camera.z * 4.0);
        const f64 _grid = rde_math_ldexp_f64(1.0, _qe);
        rde_arr _qp_arr = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
        rde_arr_resize(&_qp_arr, _k);
        fude_zoom_qpoint* _qp = (fude_zoom_qpoint*)_qp_arr.memory;
        for(u32 _i = 0; _i < _k; _i++) {
            _qp[_i] = (fude_zoom_qpoint){ (i32)rde_math_round_to_i64_f64((_p[_i].x - _p[0].x) / _grid), (i32)rde_math_round_to_i64_f64((_p[_i].y - _p[0].y) / _grid), 0u, ((const u32*)_rings.memory)[_i] };
        }
        // Just under that line (as a closed line's fill goes, fude_zoom_page_fill_at), not over it; on top when no line
        // was found (0). A line's order key counts from FUDE_ZOOM_Z_STEP up, so one less is never 0
        // (which means on top).
        const u32 _fill = fude_zoom_scene_add_fill(_s, _frame, (fude_zoom_place){ _p[0], 0.0, 1.0 }, _qe, _qp, _k, _page->app->ink->color, _z > 0u ? _z - 1u : 0u);
        rde_arr_free(&_qp_arr);
        fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _fill)->box, NULL, 0, &_fill, 1);
    } else if(_got == FUDE_ZOOM_BUCKET_OPEN) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FILL_OPEN));
    } else if(_got != FUDE_ZOOM_BUCKET_ON_INK) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FILL_NOTHING));
    }
    rde_arr_free(&_out);
    rde_arr_free(&_rings);
    rde_arr_free(&_ink);
    rde_arr_free(&_found);
    rde_arr_free(&_q);
    rde_arr_free(&_pts);
}

// See page_parts.h. A tap with the Fill tool: the closed thing under it (a shape, a fill, a line that comes back near
// where it began) filled with the brush's colour, or let go when it is that colour already; with none, the paint
// bucket. Of several, the one in the deepest frame, then the latest drawn. One undo step.
void fude_zoom_page_fill_at(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_scene* _s     = &_page->scene;
    const rde_color  _brush = _page->app->ink->color;
    rde_arr _outline = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _q       = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    rde_arr _found   = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr _region  = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _rings   = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    // The best so far: 1 a shape, 2 a fill, 3 a closed line (its first piece).
    u8  _what = 0;
    u32 _pick = FUDE_ZOOM_NONE, _pick_depth = 0;
    u64 _pick_z = 0;
    fude_zoom_v2 _pick_tap = { 0.0, 0.0 };   // A closed line's: the tap, its frame's units,
    f64          _pick_gap = 0.0;            // and its ends' gap counted as closed.
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, _s, &_page->editable);
    const fude_zoom_visible* _v = (const fude_zoom_visible*)_page->editable.memory;
    for(u32 _f = 0; _f < (u32)rde_arr_length(&_page->editable); _f++) {
        const f64          _scale = fude_zoom_sim_scale(_v[_f].to_screen);
        const fude_zoom_v2 _p     = fude_zoom_sim_apply(fude_zoom_sim_inverse(_v[_f].to_screen), (fude_zoom_v2){ _screen.x, _screen.y });
        const f64          _near  = 2.0 / _scale;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _v[_f].frame, (fude_zoom_box){ _p.x - _near, _p.y - _near, _p.x + _near, _p.y + _near }, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32               _object = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_touchable(_s, _o)) {
                continue;
            }
            u8  _kind = 0;
            u32 _at   = _object;
            u64 _z    = _o->z;
            if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                b8 _closed;
                fude_zoom_scene_shape_outline(_s, _object, 96u, &_outline, &_closed);
                if(_closed && fude_zoom_fill_inside((const fude_zoom_v2*)_outline.memory, (u32)rde_arr_length(&_outline), _p)) {
                    _kind = 1u;
                }
            } else if(_o->kind == FUDE_ZOOM_KIND_FILL && _o->count >= 3u) {
                rde_arr_clear(&_q);
                rde_arr_add_n(&_q, _o->count);
                if(fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
                    // What the eraser left of it (its points' rings: fill.h).
                    rde_arr_clear(&_outline);
                    rde_arr_clear(&_rings);
                    for(u32 _k = 0; _k < _o->count; _k++) {
                        const fude_zoom_qpoint* _qk = &((const fude_zoom_qpoint*)_q.memory)[_k];
                        const fude_zoom_v2      _pt = fude_zoom_scene_point_at(_o, _qk);
                        rde_arr_add(&_outline, (any)&_pt);
                        rde_arr_add(&_rings, (any)&_qk->time);
                    }
                    _kind = fude_zoom_fill_inside_rings((const fude_zoom_v2*)_outline.memory, (const u32*)_rings.memory, _o->count, _p) ? 2u : 0u;
                }
            } else if(_o->kind == FUDE_ZOOM_KIND_STROKE && !(_o->flags & FUDE_ZOOM_FLAG_MARKER)) {
                const u32 _first  = fude_zoom_page_gesture_outline(_s, _object, &_outline, &_q);
                const u8  _closed = _first != FUDE_ZOOM_NONE ? fude_zoom_page_closed(&_outline, FUDE_ZOOM_PAGE_FILL_GAP / _scale) : 0u;
                if(_closed != 0u) {
                    if(fude_zoom_fill_region((const fude_zoom_v2*)_outline.memory, (u32)rde_arr_length(&_outline), _p, _closed == 1u, &_region) >= 3u) {
                        _kind = 3u;
                        _at   = _first;
                        _z    = fude_zoom_scene_object(_s, _first)->z - 1u;   // Its fill goes just under it.
                    }
                }
            }
            // The deepest frame's first, then the latest drawn; a fill before the line it fills.
            const b8 _better = _kind != 0 && (_what == 0 || _v[_f].depth > _pick_depth ||
                               (_v[_f].depth == _pick_depth && (_z > _pick_z || (_z == _pick_z && _kind == 2u))));
            if(_better) {
                _what = _kind; _pick = _at; _pick_depth = _v[_f].depth; _pick_z = _z; _pick_tap = _p; _pick_gap = FUDE_ZOOM_PAGE_FILL_GAP / _scale;
            }
        }
    }
    // What is made of what was tapped stays on its layer (the bucket's goes on the current one).
    const u16 _layer_was = _s->layer;
    if(_pick != FUDE_ZOOM_NONE && _what != 0) {
        _s->layer = fude_zoom_scene_object(_s, _pick)->layer;
    }
    if(_what == 0) {
        fude_zoom_page_bucket(_page, _screen);   // Not one closed thing: the area the lines round it close.
    } else if(_what == 1u) {
        // A shape: made again filled in the brush's colour (or not filled, when it already was), just over where it
        // was, its line as it was (dashed, hatched); the old one dies.
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _pick);
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _count = fude_zoom_scene_shape_numbers(_s, _pick, _n, FUDE_ZOOM_SHAPE_NUMBERS);
        const rde_color _now  = (_o.flags & FUDE_ZOOM_FLAG_FILL_OWN) ? _o.fill : _o.color;
        const b8        _drop = (_o.flags & FUDE_ZOOM_FLAG_FILLED) && rde_memcmp(&_now, &_brush, sizeof(rde_color)) == 0;
        const u8        _flags = _drop ? 0u : (u8)(FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN);
        _s->style = _o.channels == FUDE_ZOOM_SHAPE_SYMBOL || _o.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _o.q;   // (its own line and hatch: put back to 0 below)
        const u32 _new = fude_zoom_scene_add_shape_fill(_s, _o.frame, fude_zoom_scene_place_of(_s, _pick), _o.channels, _n, _count, _o.color, _o.radius, _flags, _brush, _o.z + 1u);
        fude_zoom_scene_set_alive(_s, _pick, false);
        fude_zoom_history_push(_s, _o.frame, fude_zoom_scene_object(_s, _new)->box, &_pick, 1, &_new, 1);
    } else if(_what == 2u) {
        // A fill: in the brush's colour now, or gone when it already was.
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _pick);
        if(rde_memcmp(&_o.color, &_brush, sizeof(rde_color)) == 0) {
            fude_zoom_scene_set_alive(_s, _pick, false);
            fude_zoom_history_push(_s, _o.frame, _o.box, &_pick, 1, NULL, 0);
        } else {
            rde_arr_clear(&_q);
            rde_arr_add_n(&_q, _o.count);
            if(fude_zoom_scene_points(_s, _pick, (fude_zoom_qpoint*)_q.memory)) {
                const u32 _new = fude_zoom_scene_add_fill(_s, _o.frame, fude_zoom_scene_place_of(_s, _pick), _o.q, (const fude_zoom_qpoint*)_q.memory, _o.count, _brush, _o.z);
                fude_zoom_scene_set_alive(_s, _pick, false);
                fude_zoom_history_push(_s, _o.frame, _o.box, &_pick, 1, &_new, 1);
            }
        }
    } else {
        // A closed line: a fill under it (the loop of it round the tap), its points in the line's own quantum from
        // its first corner.
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _pick);
        fude_zoom_page_gesture_outline(_s, _pick, &_outline, &_q);
        const b8 _ends = fude_zoom_page_closed(&_outline, _pick_gap) == 1u;
        fude_zoom_fill_region((const fude_zoom_v2*)_outline.memory, (u32)rde_arr_length(&_outline), _pick_tap, _ends, &_region);
        const u32           _n    = (u32)rde_arr_length(&_region);
        const fude_zoom_v2* _pts  = (const fude_zoom_v2*)_region.memory;
        const f64           _grid = rde_math_ldexp_f64(1.0, _o.q);
        rde_arr_clear(&_q);
        fude_zoom_qpoint* _qp = rde_arr_add_n(&_q, _n);
        for(u32 _k = 0; _k < _n; _k++) {
            _qp[_k] = (fude_zoom_qpoint){ (i32)rde_math_round_to_i64_f64((_pts[_k].x - _pts[0].x) / _grid), (i32)rde_math_round_to_i64_f64((_pts[_k].y - _pts[0].y) / _grid), 0u, 0u };
        }
        const u32 _new = fude_zoom_scene_add_fill(_s, _o.frame, (fude_zoom_place){ _pts[0], 0.0, 1.0 }, _o.q, _qp, _n, _brush, _pick_z);
        fude_zoom_history_push(_s, _o.frame, fude_zoom_scene_object(_s, _new)->box, NULL, 0, &_new, 1);
    }
    _s->layer = _layer_was;
    _s->style = 0;
    rde_arr_free(&_outline);
    rde_arr_free(&_q);
    rde_arr_free(&_found);
    rde_arr_free(&_region);
    rde_arr_free(&_rings);
}

// See page_parts.h. Taken in hand, it lets go of what the lasso holds and puts the pen under it.
void fude_zoom_fill_press(fude_app* _app) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    const b8 _on = !_page->fill_tool;
    fude_zoom_page_pen_modes_off(_page);
    _page->fill_tool = _on;
    if(_on) {
        fude_zoom_select_clear(&_page->selection);
        fude_zoom_page_pen_under(_page);
    }
    fude_toolbar_refresh(&_app->ui->bar);
}

// See page_parts.h.
b8 fude_zoom_fill_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && fude_zoom_page_the->fill_tool;
}
