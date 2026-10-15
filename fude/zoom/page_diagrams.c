// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, diagrams: a diagram from Mermaid text (graph.h), the diagram library's panel (symbol.h), building plans
// (plan.h: walls, rooms, the parts list), and Kanban boards.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// The Plan tool's choice; FUDE_ZOOM_PLAN_CHOICES, defined with it below, is its table.
RDE_INTERNAL b8 fude_zoom_plan_choose(fude_app* _app, u32 _index);

// --- a diagram from text (graph.h) --------------------------------------------------------

#define FUDE_ZOOM_PAGE_DIAGRAM_PX   15.0     // a diagram's words on screen (points)

// The card for a diagram's text (Mermaid), empty but for a hint.
void fude_zoom_page_mermaid_card(fude_zoom_page* _page) {
    if(_page->text_field == NULL || !_page->open) {
        return;
    }
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    // The hint: Mermaid's syntax is the same in every language, its words are the reader's.
    c8 _hint[256];
    rde_snprintf(_hint, sizeof(_hint), "flowchart LR\n    A[%s] --> B{%s}\n    B -->|%s| C[%s]", fude_text(FUDE_TEXT_ZOOM_MERMAID_SAMPLE_IDEA),
                 fude_text(FUDE_TEXT_ZOOM_MERMAID_SAMPLE_READY), fude_text(FUDE_TEXT_ZOOM_MERMAID_SAMPLE_YES), fude_text(FUDE_TEXT_ZOOM_MERMAID_SAMPLE_START));
    rde_ui_text_editor_set_placeholder(_page->text_field, _hint);
    _page->text_object = FUDE_ZOOM_NONE;
    _page->text_style  = FUDE_ZOOM_PAGE_TEXT_MERMAID;
    fude_zoom_page_text_card(_page, true);
}

// A text's box on the screen at _px: as wide as its longest line, as tall as its lines.
void fude_zoom_page_measure_words(const fude_zoom_page* _page, const c8* _words, f32 _px, f32* _w, f32* _h) {
    u32 _lines = 0;
    f32 _widest = 0.0f;
    for(const c8* _p = _words; ; ) {
        const c8*   _end = rde_strchr(_p, '\n');
        const usize _n   = _end != NULL ? (usize)(_end - _p) : rde_strlen(_p);
        c8 _one[512];
        rde_snprintf(_one, sizeof(_one), "%.*s", (int)(_n < sizeof(_one) - 1u ? _n : sizeof(_one) - 1u), _p);
        _widest = rde_math_max_f32(_widest, _page->app->font != NULL ? fude_draw_text_width(_page->app->font, _page->app->font_px, _one, _px) : (f32)_n * _px * 0.55f);
        _lines++;
        if(_end == NULL) {
            break;
        }
        _p = _end + 1;
    }
    *_w = _widest + _px * 0.3f;
    *_h = (f32)_lines * _px * (f32)FUDE_ZOOM_PAGE_TEXT_LINE + _px * 0.2f;
}

// A plain text centred on _centre (frame units), _px on screen at _k screen points a unit.
RDE_INTERNAL u32 fude_zoom_page_add_label(fude_zoom_page* _page, u32 _frame, fude_zoom_v2 _centre, f64 _k, const c8* _words, rde_color _ink) {
    f32 _w, _h;
    fude_zoom_page_measure_words(_page, _words, (f32)FUDE_ZOOM_PAGE_DIAGRAM_PX, &_w, &_h);
    const fude_zoom_v2 _tl = { _centre.x - (f64)_w * 0.5 / _k, _centre.y + (f64)_h * 0.5 / _k };
    return fude_zoom_scene_add_text(&_page->scene, _frame, (fude_zoom_place){ _tl, 0.0, 1.0 }, FUDE_ZOOM_PAGE_DIAGRAM_PX / _k, (f64)_w / _k, (f64)_h / _k,
                                    FUDE_ZOOM_TEXT_PLAIN, _ink, (rde_color){ 0, 0, 0, 0 }, _words, (u32)rde_strlen(_words), 0);
}

// A Mermaid diagram's point (_x, _y) in the camera's frame (fude_zoom_page_import_mermaid's steps): the graph's middle
// (_gm) on the view's, its Y turned up, a point of it a point on the screen (_k: screen points a unit of the frame).
#define FUDE_ZOOM_PAGE_AT(_x, _y) ((fude_zoom_v2){ _s->camera.at.x + ((_x) - _gm.x) / _k, _s->camera.at.y - ((_y) - _gm.y) / _k })

// What fude_zoom_page_import_mermaid's steps put the diagram down with: the page and its scene; the graph, laid
// out, and how many nodes, edges and subgraphs it has; the words' size on the screen (points); its routes' points
// and where each edge's begin (handles, read only); the camera's frame, screen points a unit of it and the graph's
// middle (for FUDE_ZOOM_PAGE_AT); the ink and its lines' width; what was made (the function's array, added to) and
// each node's shape's id.
typedef struct {
    fude_zoom_page*  page;
    fude_zoom_scene* s;
    fude_zoom_graph* g;
    u32              nodes, edges, subs;
    f32              px;
    rde_arr          pts, starts;
    u32              frame;
    f64              k;
    fude_zoom_v2     gm;
    rde_color        ink;
    f32              r;
    rde_arr*         born;
    u64*             ids;
} fude_zoom_page_mermaid;

// The diagram's subgraphs, first, under what they hold (fude_zoom_page_import_mermaid's): each a box, its title at
// its top left.
RDE_INTERNAL void fude_zoom_page_mermaid_subgraphs(const fude_zoom_page_mermaid* _m) {
    fude_zoom_page* _page = _m->page;
    fude_zoom_scene* _s = _m->s;
    fude_zoom_graph* _g = _m->g;
    const u32 _subs = _m->subs;
    const f32 _px = _m->px;
    const u32 _frame = _m->frame;
    const f64 _k = _m->k;
    const fude_zoom_v2 _gm = _m->gm;
    const rde_color _ink = _m->ink;
    const f32 _r = _m->r;
    rde_arr* _born = _m->born;
    // Subgraphs first, under what they hold: a box, its title at its top left.
    for(u32 _i = 0; _i < _subs; _i++) {
        const fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_g, _i);
        if(_sg->w <= 0.0 || _sg->h <= 0.0) {
            continue;
        }
        const f64 _n[3] = { _sg->w * 0.5 / _k, _sg->h * 0.5 / _k, 8.0 / _k };
        u32 _o = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ FUDE_ZOOM_PAGE_AT(_sg->x, _sg->y), 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, _n, 3u, _ink, _r * 0.6f, 0u, 0);
        rde_arr_add(_born, (any)&_o);
        const c8* _title = _sg->label[0] != 0 ? _sg->label : _sg->id;
        f32 _tw, _th;
        fude_zoom_page_measure_words(_page, _title, _px, &_tw, &_th);
        _o = fude_zoom_page_add_label(_page, _frame, FUDE_ZOOM_PAGE_AT(_sg->x - _sg->w * 0.5 + 10.0 + (f64)_tw * 0.5, _sg->y - _sg->h * 0.5 + 6.0 + (f64)_th * 0.5), _k, _title, _ink);
        rde_arr_add(_born, (any)&_o);
    }
}

// The diagram's nodes (fude_zoom_page_import_mermaid's): each its shape (a box, rounded or a stadium, a circle, a
// diamond, a hexagon, a parallelogram, a trapezoid, a flag) with its words on it; each shape's id kept in ids, for
// the edges joined to it.
RDE_INTERNAL void fude_zoom_page_mermaid_nodes(const fude_zoom_page_mermaid* _m) {
    fude_zoom_page* _page = _m->page;
    fude_zoom_scene* _s = _m->s;
    fude_zoom_graph* _g = _m->g;
    const u32 _nodes = _m->nodes;
    const u32 _frame = _m->frame;
    const f64 _k = _m->k;
    const fude_zoom_v2 _gm = _m->gm;
    const rde_color _ink = _m->ink;
    const f32 _r = _m->r;
    rde_arr* _born = _m->born;
    u64* _ids = _m->ids;
    for(u32 _i = 0; _i < _nodes; _i++) {
        const fude_zoom_graph_node* _n = fude_zoom_graph_node_at(_g, _i);
        const fude_zoom_v2 _c  = FUDE_ZOOM_PAGE_AT(_n->x, _n->y);
        const f64          _hw = _n->w * 0.5 / _k, _hh = _n->h * 0.5 / _k;
        u32 _o = FUDE_ZOOM_NONE;
        const fude_zoom_place _at = { _c, 0.0, 1.0 };
        switch(_n->shape) {
            case FUDE_ZOOM_NODE_CIRCLE: case FUDE_ZOOM_NODE_DOUBLE_CIRCLE: {
                const f64 _e[2] = { _hw, _hh };
                _o = fude_zoom_scene_add_shape(_s, _frame, _at, FUDE_ZOOM_SHAPE_ELLIPSE, _e, 2u, _ink, _r, 0u, 0);
            } break;
            case FUDE_ZOOM_NODE_DIAMOND: {
                const f64 _p[8] = { _hw, 0.0, 0.0, _hh, -_hw, 0.0, 0.0, -_hh };
                _o = fude_zoom_scene_add_shape(_s, _frame, _at, FUDE_ZOOM_SHAPE_POLYGON, _p, 8u, _ink, _r, 0u, 0);
            } break;
            case FUDE_ZOOM_NODE_HEXAGON: {
                const f64 _d = rde_math_min_f64(_hw * 0.4, _hh);
                const f64 _p[12] = { _hw, 0.0, _hw - _d, _hh, -_hw + _d, _hh, -_hw, 0.0, -_hw + _d, -_hh, _hw - _d, -_hh };
                _o = fude_zoom_scene_add_shape(_s, _frame, _at, FUDE_ZOOM_SHAPE_POLYGON, _p, 12u, _ink, _r, 0u, 0);
            } break;
            case FUDE_ZOOM_NODE_PARALLELOGRAM: case FUDE_ZOOM_NODE_PARALLELOGRAM_ALT: {
                const f64 _d = _hh * 0.6 * (_n->shape == FUDE_ZOOM_NODE_PARALLELOGRAM ? 1.0 : -1.0);
                const f64 _p[8] = { -_hw + _d, _hh, -_hw - _d, -_hh, _hw - _d, -_hh, _hw + _d, _hh };
                _o = fude_zoom_scene_add_shape(_s, _frame, _at, FUDE_ZOOM_SHAPE_POLYGON, _p, 8u, _ink, _r, 0u, 0);
            } break;
            case FUDE_ZOOM_NODE_TRAPEZOID: case FUDE_ZOOM_NODE_TRAPEZOID_ALT: {
                const f64 _d = _hh * 0.6 * (_n->shape == FUDE_ZOOM_NODE_TRAPEZOID ? 1.0 : -1.0);
                const f64 _p[8] = { -_hw + _d, _hh, -_hw - _d, -_hh, _hw + _d, -_hh, _hw - _d, _hh };
                _o = fude_zoom_scene_add_shape(_s, _frame, _at, FUDE_ZOOM_SHAPE_POLYGON, _p, 8u, _ink, _r, 0u, 0);
            } break;
            case FUDE_ZOOM_NODE_FLAG: {
                const f64 _d = _hh * 0.6;
                const f64 _p[10] = { -_hw, _hh, -_hw + _d, 0.0, -_hw, -_hh, _hw, -_hh, _hw, _hh };
                _o = fude_zoom_scene_add_shape(_s, _frame, _at, FUDE_ZOOM_SHAPE_POLYGON, _p, 10u, _ink, _r, 0u, 0);
            } break;
            default: {
                const b8  _round = _n->shape == FUDE_ZOOM_NODE_ROUND || _n->shape == FUDE_ZOOM_NODE_STADIUM;
                const f64 _q[3]  = { _hw, _hh, _n->shape == FUDE_ZOOM_NODE_STADIUM ? _hh : _round ? 10.0 / _k : 0.0 };
                _o = fude_zoom_scene_add_shape(_s, _frame, _at, FUDE_ZOOM_SHAPE_RECT, _q, 3u, _ink, _r, 0u, 0);
            } break;
        }
        if(_o != FUDE_ZOOM_NONE) {
            rde_arr_add(_born, (any)&_o);
            _ids[_i] = fude_zoom_scene_object(_s, _o)->id;
        }
        const u32 _l = fude_zoom_page_add_label(_page, _frame, _c, _k, _n->label[0] != 0 ? _n->label : _n->id, _ink);
        rde_arr_add(_born, (any)&_l);
    }
}

// The diagram's edges (fude_zoom_page_import_mermaid's): each an arrow along its route, joined to its nodes'
// shapes, its words by its middle.
RDE_INTERNAL void fude_zoom_page_mermaid_edges(const fude_zoom_page_mermaid* _m) {
    fude_zoom_page* _page = _m->page;
    fude_zoom_scene* _s = _m->s;
    fude_zoom_graph* _g = _m->g;
    const u32 _nodes = _m->nodes, _edges = _m->edges;
    const rde_arr _pts = _m->pts, _starts = _m->starts;
    const u32 _frame = _m->frame;
    const f64 _k = _m->k;
    const fude_zoom_v2 _gm = _m->gm;
    const rde_color _ink = _m->ink;
    const f32 _r = _m->r;
    rde_arr* _born = _m->born;
    const u64* _ids = _m->ids;
    // The edges: arrows along their routes (at most six points kept), their words by their middle.
    const fude_zoom_v2* _rp = (const fude_zoom_v2*)_pts.memory;
    const u32*          _rs = (const u32*)_starts.memory;
    for(u32 _i = 0; _i < _edges && _i + 1u < (u32)rde_arr_length(&_starts); _i++) {
        const fude_zoom_graph_edge* _e = fude_zoom_graph_edge_at(_g, _i);
        const u32 _from = _rs[_i], _count = _rs[_i + 1u] - _rs[_i];
        if(_e->line == FUDE_ZOOM_EDGE_LINE_INVISIBLE || _count < 2u) {
            continue;
        }
        const u32 _k6 = _count > FUDE_ZOOM_ARROW_POINTS ? FUDE_ZOOM_ARROW_POINTS : _count;
        f64 _num[FUDE_ZOOM_ARROW_NUMBERS(FUDE_ZOOM_ARROW_POINTS)];
        const fude_zoom_v2 _first = FUDE_ZOOM_PAGE_AT(_rp[_from].x, _rp[_from].y);
        _num[0] = (f64)_k6;
        for(u32 _j = 0; _j < _k6; _j++) {
            const u32 _src = _from + (_k6 == _count ? _j : (u32)rde_math_round_to_i64_f64((f64)_j * (f64)(_count - 1u) / (f64)(_k6 - 1u)));
            const fude_zoom_v2 _q = FUDE_ZOOM_PAGE_AT(_rp[_src].x, _rp[_src].y);
            _num[1u + 2u * _j] = _q.x - _first.x;
            _num[2u + 2u * _j] = _q.y - _first.y;
        }
        u32 _heads = 0u;
        if(_e->head_end != FUDE_ZOOM_EDGE_HEAD_NONE)   { _heads |= FUDE_ZOOM_ARROW_END; }
        if(_e->head_start != FUDE_ZOOM_EDGE_HEAD_NONE) { _heads |= FUDE_ZOOM_ARROW_START; }
        if(_e->head_end == FUDE_ZOOM_EDGE_HEAD_CIRCLE || _e->head_start == FUDE_ZOOM_EDGE_HEAD_CIRCLE) { _heads |= FUDE_ZOOM_ARROW_CIRCLE; }
        if(_e->line == FUDE_ZOOM_EDGE_LINE_DOTTED)     { _heads |= FUDE_ZOOM_ARROW_DOTTED; }
        _num[1u + 2u * _k6] = (f64)_heads;
        // (the shapes it joins, each id in two halves, as connect.h keeps them now)
        fude_zoom_connect_arrow_put(_num, _k6, _e->from < _nodes ? _ids[_e->from] : 0u, _e->to < _nodes ? _ids[_e->to] : 0u);
        const u32 _o = fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _first, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ARROW, _num, FUDE_ZOOM_ARROW_NUMBERS(_k6), _ink,
                                                 _e->line == FUDE_ZOOM_EDGE_LINE_THICK ? _r * 2.0f : _r, 0u, 0);
        rde_arr_add(_born, (any)&_o);
        if(_e->label[0] != 0) {
            const u32 _l = fude_zoom_page_add_label(_page, _frame, FUDE_ZOOM_PAGE_AT(_e->label_x, _e->label_y), _k, _e->label, _ink);
            rde_arr_add(_born, (any)&_l);
        }
    }
}

#undef FUDE_ZOOM_PAGE_AT

// Mermaid's flowchart turned into the canvas's own things, laid out (graph.h)
// in the middle of the view: each node its shape with its words on it, each
// subgraph a box with its title, each edge an arrow joining its two shapes
// (their ids kept in it), its words by its middle. One undo step. False: the
// text could not be read (said in a notice, with the line).
// Its subgraphs, nodes and edges are put down by its steps, above (fude_zoom_page_mermaid_*).
b8 fude_zoom_page_import_mermaid(fude_zoom_page* _page, const c8* _text) {
    fude_zoom_scene* _s = &_page->scene;
    fude_zoom_graph  _g;
    fude_zoom_graph_init(&_g);
    u32 _line = 0;
    u8  _error = 0;
    if(!fude_zoom_graph_parse_mermaid(&_g, _text, (u32)rde_strlen(_text), &_line, &_error)) {
        c8 _say[200];
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_MERMAID_FAILED, FUDE_TN(_line));
        fude_notice_show(_say);
        fude_zoom_graph_destroy(&_g);
        return false;
    }
    // Each node's size from its words (screen points); edges' words get room too.
    const f32 _px = (f32)FUDE_ZOOM_PAGE_DIAGRAM_PX;
    const u32 _nodes = (u32)rde_arr_length(&_g.nodes), _edges = (u32)rde_arr_length(&_g.edges), _subs = (u32)rde_arr_length(&_g.subgraphs);
    for(u32 _i = 0; _i < _nodes; _i++) {
        fude_zoom_graph_node* _n = fude_zoom_graph_node_at(&_g, _i);
        f32 _w, _h;
        fude_zoom_page_measure_words(_page, _n->label[0] != 0 ? _n->label : _n->id, _px, &_w, &_h);
        _n->w = rde_math_max_f64((f64)_w + 32.0, 80.0);
        _n->h = rde_math_max_f64((f64)_h + 20.0, 40.0);
        if(_n->shape == FUDE_ZOOM_NODE_CIRCLE || _n->shape == FUDE_ZOOM_NODE_DOUBLE_CIRCLE) {
            _n->w = _n->h = rde_math_max_f64(_n->w, _n->h);
        } else if(_n->shape == FUDE_ZOOM_NODE_DIAMOND) {
            _n->w *= 1.45;
            _n->h *= 1.6;
        } else if(_n->shape == FUDE_ZOOM_NODE_HEXAGON) {
            _n->w += 24.0;
        }
    }
    for(u32 _i = 0; _i < _edges; _i++) {
        fude_zoom_graph_edge* _e = fude_zoom_graph_edge_at(&_g, _i);
        if(_e->label[0] != 0) {
            f32 _w, _h;
            fude_zoom_page_measure_words(_page, _e->label, _px, &_w, &_h);
            _e->label_w = (f64)_w + 8.0;
            _e->label_h = (f64)_h;
        }
    }
    fude_zoom_graph_layout(&_g, (fude_zoom_graph_spacing){ 56.0, 36.0, 18.0, 26.0 });
    rde_arr _pts    = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _starts = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_graph_routes(&_g, &_pts, &_starts);
    // Into the camera's frame, the graph's middle on the view's (its Y turned up), a point a point on screen.
    fude_zoom_box _gb = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _nodes; _i++) {
        const fude_zoom_graph_node* _n = fude_zoom_graph_node_at(&_g, _i);
        _gb = fude_zoom_box_union(_gb, (fude_zoom_box){ _n->x - _n->w * 0.5, _n->y - _n->h * 0.5, _n->x + _n->w * 0.5, _n->y + _n->h * 0.5 });
    }
    const u32 _frame = _s->camera.frame;
    const f64 _k     = _s->camera.z;   // screen points a unit of the camera's frame
    const fude_zoom_v2 _gm = { (_gb.min_x + _gb.max_x) * 0.5, (_gb.min_y + _gb.max_y) * 0.5 };
    const rde_color _ink = _page->app->ink->color;
    const f32       _r   = (f32)(1.0 / _k);
    rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), _ids_arr = rde_arr_new(sizeof(u64), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_ids_arr, _nodes);
    u64*    _ids  = (u64*)_ids_arr.memory;   // (sized once: it stays put)
    const fude_zoom_page_mermaid _m = { .page = _page, .s = _s, .g = &_g, .nodes = _nodes, .edges = _edges, .subs = _subs, .px = _px,
                                        .pts = _pts, .starts = _starts, .frame = _frame, .k = _k, .gm = _gm, .ink = _ink, .r = _r,
                                        .born = &_born, .ids = _ids };
    fude_zoom_page_mermaid_subgraphs(&_m);
    fude_zoom_page_mermaid_nodes(&_m);
    fude_zoom_page_mermaid_edges(&_m);
    const u32 _n = (u32)rde_arr_length(&_born);
    if(_n > 0) {
        fude_zoom_box _box = fude_zoom_box_empty();
        for(u32 _i = 0; _i < _n; _i++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_i])->box);
        }
        fude_zoom_history_push(_s, _frame, _box, NULL, 0, (const u32*)_born.memory, _n);
    }
    rde_arr_free(&_ids_arr);
    rde_arr_free(&_born);
    rde_arr_free(&_pts);
    rde_arr_free(&_starts);
    fude_zoom_graph_destroy(&_g);
    return true;
}

// A constraint whose lines were made again (_old ids now _new): made again joined to them — and the line it keeps so,
// if only the other was made again, made again kept so (connect.h), both into _died_out/_born_out.
RDE_INTERNAL void fude_zoom_page_rebind_constraint(fude_zoom_page* _page, u32 _c, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n,
                                                   rde_arr* _died_out, rde_arr* _born_out) {
    fude_zoom_scene* _s = &_page->scene;
    f64 _num[3];
    if(fude_zoom_scene_shape_numbers(_s, _c, _num, 3u) != 3u) {
        return;
    }
    fude_zoom_id _ids[2] = { (fude_zoom_id)_num[1], (fude_zoom_id)_num[2] };
    b8 _moved[2] = { false, false };
    for(u32 _k = 0; _k < 2u; _k++) {
        for(u32 _i = 0; _i < _n; _i++) {
            if(_ids[_k] == _old[_i]) {
                _ids[_k]   = _new[_i];
                _moved[_k] = true;
                break;
            }
        }
    }
    if(!_moved[0] && !_moved[1]) {
        return;
    }
    u32 _line[2] = { fude_zoom_scene_find_object(_s, _ids[0]), fude_zoom_scene_find_object(_s, _ids[1]) };
    if(_line[0] == FUDE_ZOOM_NONE || _line[1] == FUDE_ZOOM_NONE) {
        return;
    }
    const u8 _kind = (u8)_num[0];
    fude_zoom_sim _m;
    if(_moved[0] != _moved[1] && fude_zoom_connect_constrain(_s, _kind, _line[_moved[0] ? 0 : 1], _line[_moved[0] ? 1 : 0], &_m)) {
        // (the other one made again, kept so: turned or stretched round its middle)
        const u32 _other = _line[_moved[0] ? 1 : 0];
        const fude_zoom_object _o = *fude_zoom_scene_object(_s, _other);
        rde_arr _ns = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
        const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _other, &_ns);
        const u16 _layer = _s->layer;
        const i8  _style = _s->style;
        _s->layer = _o.layer;
        _s->style = _o.q;
        const u32 _made = fude_zoom_scene_add_shape_fill(_s, _o.frame, fude_zoom_place_moved(fude_zoom_scene_place_of(_s, _other), _m), _o.channels,
                                                         (const f64*)_ns.memory, _count, _o.color, _o.radius, _o.flags, _o.fill, _o.z);
        _s->layer = _layer;
        _s->style = _style;
        rde_arr_free(&_ns);
        fude_zoom_scene_set_alive(_s, _other, false);
        rde_arr_add(_died_out, (any)&_other);
        rde_arr_add(_born_out, (any)&_made);
        _line[_moved[0] ? 1 : 0] = _made;
    }
    const u32 _made = fude_zoom_connect_constraint_add(_s, _kind, _line[0], _line[1]);
    fude_zoom_scene_set_alive(_s, _c, false);
    rde_arr_add(_died_out, (any)&_c);
    rde_arr_add(_born_out, (any)&_made);
}

// Connectors joined to what an edit replaced (_died[i] by _born[i]) joined to the replacements instead: each new
// arrow and the old one appended to _died_out and _born_out (in the same step). Constraints, a drawing's properties,
// wires and dimensions on what was replaced are made again the same way, and what plays is told (page_parts.h).
void fude_zoom_page_rebind(fude_zoom_page* _page, const u32* _died, const u32* _born, u32 _n, rde_arr* _died_out, rde_arr* _born_out) {
    fude_zoom_scene*      _s    = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(_n == 0) {
        return;
    }
    fude_zoom_page_play_remade(_page, _died, _born, _n);   // (played: what is made again for them, too)
    rde_arr _ids_arr = rde_arr_new(sizeof(fude_zoom_id), _heap);
    rde_arr_resize(&_ids_arr, (usize)_n * 2u);
    fude_zoom_id* _old = (fude_zoom_id*)_ids_arr.memory;   // (sized once: it stays put)
    fude_zoom_id* _new = _old + _n;
    for(u32 _i = 0; _i < _n; _i++) {
        _old[_i] = fude_zoom_scene_object(_s, _died[_i])->id;
        _new[_i] = fude_zoom_scene_object(_s, _born[_i])->id;
    }
    const u32 _count = fude_zoom_scene_object_count(_s);
    for(u32 _a = 0; _a < _count; _a++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _a);
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_CONSTRAINT) {
            fude_zoom_page_rebind_constraint(_page, _a, _old, _new, _n, _died_out, _born_out);
            continue;
        }
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_PROPS) {
            const u32 _made = fude_zoom_props_remap(_s, _a, _old, _new, _n);   // (a drawing's properties on it made again)
            if(_made != FUDE_ZOOM_NONE) {
                fude_zoom_scene_set_alive(_s, _a, false);
                rde_arr_add(_died_out, (any)&_a);
                rde_arr_add(_born_out, (any)&_made);
            }
            continue;
        }
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_WIRE) {
            const u32 _made = fude_zoom_wire_remap(_s, _a, _old, _new, _n);   // (a wire joined again to its part made again)
            if(_made != FUDE_ZOOM_NONE) {
                fude_zoom_scene_set_alive(_s, _a, false);
                rde_arr_add(_died_out, (any)&_a);
                rde_arr_add(_born_out, (any)&_made);
            }
            continue;
        }
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE ||
           (_o->channels != FUDE_ZOOM_SHAPE_ARROW && _o->channels != FUDE_ZOOM_SHAPE_DIMENSION && _o->channels != FUDE_ZOOM_SHAPE_RADIAL)) {
            continue;
        }
        // (a connector joined again; a dimension measuring them again where they are now: connect.h)
        const u32 _made = _o->channels == FUDE_ZOOM_SHAPE_ARROW ? fude_zoom_connect_remap(_s, _a, _old, _new, _n, _o->layer)
                                                                 : fude_zoom_connect_dim_remap(_s, _a, _old, _new, _n);
        if(_made != FUDE_ZOOM_NONE) {
            fude_zoom_scene_set_alive(_s, _a, false);
            rde_arr_add(_died_out, (any)&_a);
            rde_arr_add(_born_out, (any)&_made);
        }
    }
    rde_arr_free(&_ids_arr);
}

// --- the diagram library (symbol.h) and Kanban boards -------------------------------------------------

// Each symbol's name in the app's language, in symbol.c's order.
const FUDE_TEXT_ FUDE_ZOOM_PAGE_SYMBOL_NAMES[] = {
    FUDE_TEXT_ZOOM_SYM_PROCESS,
    FUDE_TEXT_ZOOM_SYM_ALTERNATE,
    FUDE_TEXT_ZOOM_SYM_DECISION,
    FUDE_TEXT_ZOOM_SYM_TERMINATOR,
    FUDE_TEXT_ZOOM_SYM_DATA,
    FUDE_TEXT_ZOOM_SYM_PREDEFINED,
    FUDE_TEXT_ZOOM_SYM_DOCUMENT,
    FUDE_TEXT_ZOOM_SYM_DOCUMENTS,
    FUDE_TEXT_ZOOM_SYM_MANUAL_INPUT,
    FUDE_TEXT_ZOOM_SYM_MANUAL_OP,
    FUDE_TEXT_ZOOM_SYM_PREPARATION,
    FUDE_TEXT_ZOOM_SYM_DELAY,
    FUDE_TEXT_ZOOM_SYM_STORED_DATA,
    FUDE_TEXT_ZOOM_SYM_INTERNAL,
    FUDE_TEXT_ZOOM_SYM_DISPLAY,
    FUDE_TEXT_ZOOM_SYM_OFFPAGE,
    FUDE_TEXT_ZOOM_SYM_CONNECTOR,
    FUDE_TEXT_ZOOM_SYM_MERGE,
    FUDE_TEXT_ZOOM_SYM_EXTRACT,
    FUDE_TEXT_ZOOM_SYM_OR,
    FUDE_TEXT_ZOOM_SYM_SUMMING,
    FUDE_TEXT_ZOOM_SYM_COLLATE,
    FUDE_TEXT_ZOOM_SYM_SORT,
    FUDE_TEXT_ZOOM_SYM_ANNOTATION,
    FUDE_TEXT_ZOOM_SYM_CARD,
    FUDE_TEXT_ZOOM_SYM_TAPE,
    FUDE_TEXT_ZOOM_SYM_LOOP_LIMIT,
    FUDE_TEXT_ZOOM_SYM_TRIANGLE,
    FUDE_TEXT_ZOOM_SYM_HEXAGON,
    FUDE_TEXT_ZOOM_SYM_ELLIPSE,
    FUDE_TEXT_ZOOM_SYM_CALLOUT,
    FUDE_TEXT_ZOOM_SYM_CLASS,
    FUDE_TEXT_ZOOM_SYM_INTERFACE,
    FUDE_TEXT_ZOOM_SYM_REQUIRED,
    FUDE_TEXT_ZOOM_SYM_PACKAGE,
    FUDE_TEXT_ZOOM_SYM_NOTE,
    FUDE_TEXT_ZOOM_SYM_COMPONENT,
    FUDE_TEXT_ZOOM_SYM_NODE,
    FUDE_TEXT_ZOOM_SYM_ARTIFACT,
    FUDE_TEXT_ZOOM_SYM_ACTOR,
    FUDE_TEXT_ZOOM_SYM_USE_CASE,
    FUDE_TEXT_ZOOM_SYM_BOUNDARY,
    FUDE_TEXT_ZOOM_SYM_OBJECT,
    FUDE_TEXT_ZOOM_SYM_ENUM,
    FUDE_TEXT_ZOOM_SYM_LIFELINE,
    FUDE_TEXT_ZOOM_SYM_ACTIVATION,
    FUDE_TEXT_ZOOM_SYM_DESTROY,
    FUDE_TEXT_ZOOM_SYM_FRAGMENT,
    FUDE_TEXT_ZOOM_SYM_INITIAL,
    FUDE_TEXT_ZOOM_SYM_FINAL,
    FUDE_TEXT_ZOOM_SYM_FLOW_FINAL,
    FUDE_TEXT_ZOOM_SYM_FORK,
    FUDE_TEXT_ZOOM_SYM_ACTION,
    FUDE_TEXT_ZOOM_SYM_STATE,
    FUDE_TEXT_ZOOM_SYM_CHOICE,
    FUDE_TEXT_ZOOM_SYM_SEND,
    FUDE_TEXT_ZOOM_SYM_RECEIVE,
    FUDE_TEXT_ZOOM_SYM_TIME_EVENT,
    FUDE_TEXT_ZOOM_SYM_HISTORY,
    FUDE_TEXT_ZOOM_SYM_SWIMLANE,
    FUDE_TEXT_ZOOM_SYM_DATABASE,
    FUDE_TEXT_ZOOM_SYM_TABLE,
    FUDE_TEXT_ZOOM_SYM_ENTITY,
    FUDE_TEXT_ZOOM_SYM_WEAK_ENTITY,
    FUDE_TEXT_ZOOM_SYM_RELATIONSHIP,
    FUDE_TEXT_ZOOM_SYM_WEAK_RELATION,
    FUDE_TEXT_ZOOM_SYM_ATTRIBUTE,
    FUDE_TEXT_ZOOM_SYM_MULTIVALUED,
    FUDE_TEXT_ZOOM_SYM_DERIVED,
    FUDE_TEXT_ZOOM_SYM_FILE,
    FUDE_TEXT_ZOOM_SYM_STORAGE,
    FUDE_TEXT_ZOOM_SYM_PERSON,
    FUDE_TEXT_ZOOM_SYM_SYSTEM,
    FUDE_TEXT_ZOOM_SYM_EXTERNAL,
    FUDE_TEXT_ZOOM_SYM_C4_BOUNDARY,
    FUDE_TEXT_ZOOM_SYM_SERVER,
    FUDE_TEXT_ZOOM_SYM_DESKTOP,
    FUDE_TEXT_ZOOM_SYM_LAPTOP,
    FUDE_TEXT_ZOOM_SYM_PHONE,
    FUDE_TEXT_ZOOM_SYM_TABLET,
    FUDE_TEXT_ZOOM_SYM_BROWSER,
    FUDE_TEXT_ZOOM_SYM_CLOUD,
    FUDE_TEXT_ZOOM_SYM_QUEUE,
    FUDE_TEXT_ZOOM_SYM_CACHE,
    FUDE_TEXT_ZOOM_SYM_BALANCER,
    FUDE_TEXT_ZOOM_SYM_GATEWAY,
    FUDE_TEXT_ZOOM_SYM_MICROSERVICE,
    FUDE_TEXT_ZOOM_SYM_FUNCTION,
    FUDE_TEXT_ZOOM_SYM_CONTAINER,
    FUDE_TEXT_ZOOM_SYM_POD,
    FUDE_TEXT_ZOOM_SYM_FIREWALL,
    FUDE_TEXT_ZOOM_SYM_BUCKET,
    FUDE_TEXT_ZOOM_SYM_GLOBE,
    FUDE_TEXT_ZOOM_SYM_ROUTER,
    FUDE_TEXT_ZOOM_SYM_SWITCH,
    FUDE_TEXT_ZOOM_SYM_USER,
    FUDE_TEXT_ZOOM_SYM_BUS,
    FUDE_TEXT_ZOOM_SYM_LOCK,
    FUDE_TEXT_ZOOM_SYM_CLOCK,
    FUDE_TEXT_ZOOM_SYM_GEAR,
    FUDE_TEXT_ZOOM_SYM_FOLDER,
    FUDE_TEXT_ZOOM_SYM_CHART,
    FUDE_TEXT_ZOOM_SYM_CONTAINER_DB,
    FUDE_TEXT_ZOOM_SYM_INTERNET,
    FUDE_TEXT_ZOOM_SYM_START_EVENT,
    FUDE_TEXT_ZOOM_SYM_INTERMEDIATE,
    FUDE_TEXT_ZOOM_SYM_END_EVENT,
    FUDE_TEXT_ZOOM_SYM_TIMER_EVENT,
    FUDE_TEXT_ZOOM_SYM_MESSAGE_EVENT,
    FUDE_TEXT_ZOOM_SYM_TASK,
    FUDE_TEXT_ZOOM_SYM_SUBPROCESS,
    FUDE_TEXT_ZOOM_SYM_XOR_GATEWAY,
    FUDE_TEXT_ZOOM_SYM_AND_GATEWAY,
    FUDE_TEXT_ZOOM_SYM_OR_GATEWAY,
    FUDE_TEXT_ZOOM_SYM_DATA_OBJECT,
    FUDE_TEXT_ZOOM_SYM_DATA_STORE,
    FUDE_TEXT_ZOOM_SYM_POOL,
    FUDE_TEXT_ZOOM_SYM_GROUP,
    FUDE_TEXT_ZOOM_SYM_COLUMN,
    FUDE_TEXT_ZOOM_SYM_TASK_CARD,
    FUDE_TEXT_ZOOM_SYM_LANE,
    FUDE_TEXT_ZOOM_SYM_MILESTONE,
    FUDE_TEXT_ZOOM_SYM_MATRIX,
    FUDE_TEXT_ZOOM_SYM_ARROW_BLOCK,
    FUDE_TEXT_ZOOM_SYM_TIMELINE,
    FUDE_TEXT_ZOOM_SYM_SEQ_BOUNDARY, FUDE_TEXT_ZOOM_SYM_SEQ_CONTROL, FUDE_TEXT_ZOOM_SYM_SEQ_ENTITY, FUDE_TEXT_ZOOM_SYM_TERMINAL,
    FUDE_TEXT_ZOOM_SYM_KEY, FUDE_TEXT_ZOOM_SYM_ENVELOPE, FUDE_TEXT_ZOOM_SYM_CODE, FUDE_TEXT_ZOOM_SYM_USERS, FUDE_TEXT_ZOOM_SYM_TOPIC,
    FUDE_TEXT_ZOOM_SYM_BRACE, FUDE_TEXT_ZOOM_SYM_DFD_STORE, FUDE_TEXT_ZOOM_SYM_AREA,
    FUDE_TEXT_ZOOM_SYM_E_RESISTOR, FUDE_TEXT_ZOOM_SYM_E_POT, FUDE_TEXT_ZOOM_SYM_E_CAP, FUDE_TEXT_ZOOM_SYM_E_ECAP,
    FUDE_TEXT_ZOOM_SYM_E_IND, FUDE_TEXT_ZOOM_SYM_E_DIODE, FUDE_TEXT_ZOOM_SYM_E_LED, FUDE_TEXT_ZOOM_SYM_E_ZENER,
    FUDE_TEXT_ZOOM_SYM_E_NPN, FUDE_TEXT_ZOOM_SYM_E_PNP, FUDE_TEXT_ZOOM_SYM_E_NMOS, FUDE_TEXT_ZOOM_SYM_E_PMOS,
    FUDE_TEXT_ZOOM_SYM_E_VSRC, FUDE_TEXT_ZOOM_SYM_E_BATT, FUDE_TEXT_ZOOM_SYM_E_AC, FUDE_TEXT_ZOOM_SYM_E_CLOCK,
    FUDE_TEXT_ZOOM_SYM_E_ISRC, FUDE_TEXT_ZOOM_SYM_E_GND, FUDE_TEXT_ZOOM_SYM_E_RAIL, FUDE_TEXT_ZOOM_SYM_E_SWITCH,
    FUDE_TEXT_ZOOM_SYM_E_BUTTON, FUDE_TEXT_ZOOM_SYM_E_LAMP, FUDE_TEXT_ZOOM_SYM_E_MOTOR, FUDE_TEXT_ZOOM_SYM_E_BUZZER,
    FUDE_TEXT_ZOOM_SYM_E_FUSE, FUDE_TEXT_ZOOM_SYM_E_OPAMP, FUDE_TEXT_ZOOM_SYM_E_VOLT, FUDE_TEXT_ZOOM_SYM_E_AMP,
    FUDE_TEXT_ZOOM_SYM_E_7SEG, FUDE_TEXT_ZOOM_SYM_E_BREADBOARD, FUDE_TEXT_ZOOM_SYM_L_AND, FUDE_TEXT_ZOOM_SYM_L_OR,
    FUDE_TEXT_ZOOM_SYM_L_NOT, FUDE_TEXT_ZOOM_SYM_L_NAND, FUDE_TEXT_ZOOM_SYM_L_NOR, FUDE_TEXT_ZOOM_SYM_L_XOR,
    FUDE_TEXT_ZOOM_SYM_L_XNOR, FUDE_TEXT_ZOOM_SYM_L_BUF, FUDE_TEXT_ZOOM_SYM_L_DFF, FUDE_TEXT_ZOOM_SYM_L_TFF,
    FUDE_TEXT_ZOOM_SYM_L_IN, FUDE_TEXT_ZOOM_SYM_L_OUT, FUDE_TEXT_ZOOM_SYM_C_555, FUDE_TEXT_ZOOM_SYM_C_LM358,
    FUDE_TEXT_ZOOM_SYM_C_328P, FUDE_TEXT_ZOOM_SYM_C_595, FUDE_TEXT_ZOOM_SYM_C_293, FUDE_TEXT_ZOOM_SYM_C_2003,
    FUDE_TEXT_ZOOM_SYM_C_7400, FUDE_TEXT_ZOOM_SYM_C_7408, FUDE_TEXT_ZOOM_SYM_C_7432, FUDE_TEXT_ZOOM_SYM_C_7486,
    FUDE_TEXT_ZOOM_SYM_C_7404, FUDE_TEXT_ZOOM_SYM_C_7219, FUDE_TEXT_ZOOM_SYM_C_8574, FUDE_TEXT_ZOOM_SYM_C_7805,
    FUDE_TEXT_ZOOM_SYM_C_1117, FUDE_TEXT_ZOOM_SYM_M_DS3231, FUDE_TEXT_ZOOM_SYM_M_MPU, FUDE_TEXT_ZOOM_SYM_M_NRF,
    FUDE_TEXT_ZOOM_SYM_M_HC05, FUDE_TEXT_ZOOM_SYM_M_SR04, FUDE_TEXT_ZOOM_SYM_M_DHT, FUDE_TEXT_ZOOM_SYM_M_SERVO,
    FUDE_TEXT_ZOOM_SYM_M_TP4056, FUDE_TEXT_ZOOM_SYM_M_WS2812, FUDE_TEXT_ZOOM_SYM_M_L298N, FUDE_TEXT_ZOOM_SYM_M_RELAY,
    FUDE_TEXT_ZOOM_SYM_M_LCD, FUDE_TEXT_ZOOM_SYM_M_OLED, FUDE_TEXT_ZOOM_SYM_B_UNO, FUDE_TEXT_ZOOM_SYM_B_NANO,
    FUDE_TEXT_ZOOM_SYM_B_PICO, FUDE_TEXT_ZOOM_SYM_B_PI, FUDE_TEXT_ZOOM_SYM_B_ESP32, FUDE_TEXT_ZOOM_SYM_B_8266,
    FUDE_TEXT_ZOOM_SYM_M_LINK, FUDE_TEXT_ZOOM_SYM_M_PLATE, FUDE_TEXT_ZOOM_SYM_M_GEAR10, FUDE_TEXT_ZOOM_SYM_M_GEAR15,
    FUDE_TEXT_ZOOM_SYM_M_GEAR20, FUDE_TEXT_ZOOM_SYM_M_GEAR30, FUDE_TEXT_ZOOM_SYM_M_GEAR40, FUDE_TEXT_ZOOM_SYM_M_GEAR60,
    FUDE_TEXT_ZOOM_SYM_M_PIVOT, FUDE_TEXT_ZOOM_SYM_M_MOTOR, FUDE_TEXT_ZOOM_SYM_M_SPRING, FUDE_TEXT_ZOOM_SYM_M_WEIGHT,
    FUDE_TEXT_ZOOM_SYM_M_WHEEL, FUDE_TEXT_ZOOM_SYM_M_WALL,
    FUDE_TEXT_ZOOM_SYM_P_DOOR70, FUDE_TEXT_ZOOM_SYM_P_DOOR80, FUDE_TEXT_ZOOM_SYM_P_DOOR90, FUDE_TEXT_ZOOM_SYM_P_DOOR2,
    FUDE_TEXT_ZOOM_SYM_P_SLIDING, FUDE_TEXT_ZOOM_SYM_P_WIN60, FUDE_TEXT_ZOOM_SYM_P_WIN100, FUDE_TEXT_ZOOM_SYM_P_WIN120,
    FUDE_TEXT_ZOOM_SYM_P_WIN150, FUDE_TEXT_ZOOM_SYM_P_WIN200, FUDE_TEXT_ZOOM_SYM_P_OPENING, FUDE_TEXT_ZOOM_SYM_P_BED1,
    FUDE_TEXT_ZOOM_SYM_P_BED2, FUDE_TEXT_ZOOM_SYM_P_BED3, FUDE_TEXT_ZOOM_SYM_P_SOFA, FUDE_TEXT_ZOOM_SYM_P_ARMCHAIR,
    FUDE_TEXT_ZOOM_SYM_P_TABLE, FUDE_TEXT_ZOOM_SYM_P_RTABLE, FUDE_TEXT_ZOOM_SYM_P_CHAIR, FUDE_TEXT_ZOOM_SYM_P_DESK,
    FUDE_TEXT_ZOOM_SYM_P_WARDROBE, FUDE_TEXT_ZOOM_SYM_P_COUNTER, FUDE_TEXT_ZOOM_SYM_P_SINK, FUDE_TEXT_ZOOM_SYM_P_HOB,
    FUDE_TEXT_ZOOM_SYM_P_FRIDGE, FUDE_TEXT_ZOOM_SYM_P_WASHER, FUDE_TEXT_ZOOM_SYM_P_TOILET, FUDE_TEXT_ZOOM_SYM_P_BATH,
    FUDE_TEXT_ZOOM_SYM_P_SHOWER, FUDE_TEXT_ZOOM_SYM_P_BASIN, FUDE_TEXT_ZOOM_SYM_P_STAIRS, FUDE_TEXT_ZOOM_SYM_W_SOCKET,
    FUDE_TEXT_ZOOM_SYM_W_SOCKET2, FUDE_TEXT_ZOOM_SYM_W_SWITCH, FUDE_TEXT_ZOOM_SYM_W_SWITCH2, FUDE_TEXT_ZOOM_SYM_W_DIMMER,
    FUDE_TEXT_ZOOM_SYM_W_BELL, FUDE_TEXT_ZOOM_SYM_W_LIGHT, FUDE_TEXT_ZOOM_SYM_W_WLIGHT, FUDE_TEXT_ZOOM_SYM_W_TUBE,
    FUDE_TEXT_ZOOM_SYM_W_BOARD, FUDE_TEXT_ZOOM_SYM_W_JUNCTION, FUDE_TEXT_ZOOM_SYM_W_TV, FUDE_TEXT_ZOOM_SYM_W_DATA,
    FUDE_TEXT_ZOOM_SYM_W_SMOKE, FUDE_TEXT_ZOOM_SYM_W_THERMO, FUDE_TEXT_ZOOM_SYM_W_FAN, FUDE_TEXT_ZOOM_SYM_W_METER,
    FUDE_TEXT_ZOOM_SYM_X_GRAPH, FUDE_TEXT_ZOOM_SYM_X_AXES, FUDE_TEXT_ZOOM_SYM_X_LINE, FUDE_TEXT_ZOOM_SYM_S_GRAIN,
    FUDE_TEXT_ZOOM_SYM_S_FOLD, FUDE_TEXT_ZOOM_SYM_S_NOTCH, FUDE_TEXT_ZOOM_SYM_S_BUTTON, FUDE_TEXT_ZOOM_SYM_S_HOLE,
    FUDE_TEXT_ZOOM_SYM_S_DART, FUDE_TEXT_ZOOM_SYM_S_LABEL,
    FUDE_TEXT_ZOOM_SYM_M_PULLEY, FUDE_TEXT_ZOOM_SYM_M_ROPE, FUDE_TEXT_ZOOM_SYM_M_CRATE,
    FUDE_TEXT_ZOOM_SYM_M_RAIL, FUDE_TEXT_ZOOM_SYM_M_SLIDER, FUDE_TEXT_ZOOM_SYM_M_RACK,
    FUDE_TEXT_ZOOM_SYM_M_PIN,
    FUDE_TEXT_ZOOM_SYM_C_7402, FUDE_TEXT_ZOOM_SYM_C_7474, FUDE_TEXT_ZOOM_SYM_C_74138, FUDE_TEXT_ZOOM_SYM_C_74157, FUDE_TEXT_ZOOM_SYM_C_74161,
    FUDE_TEXT_ZOOM_SYM_C_74173, FUDE_TEXT_ZOOM_SYM_C_74245, FUDE_TEXT_ZOOM_SYM_C_74283, FUDE_TEXT_ZOOM_SYM_C_74189,
    FUDE_TEXT_ZOOM_SYM_L_CUSTOM,
    FUDE_TEXT_ZOOM_SYM_E_SEGPANEL, FUDE_TEXT_ZOOM_SYM_E_MATRIX, FUDE_TEXT_ZOOM_SYM_E_BARGRAPH, FUDE_TEXT_ZOOM_SYM_C_3914, FUDE_TEXT_ZOOM_SYM_C_4511,
    FUDE_TEXT_ZOOM_SYM_E_METER, FUDE_TEXT_ZOOM_SYM_M_CHARLCD, FUDE_TEXT_ZOOM_SYM_E_SCOPE, FUDE_TEXT_ZOOM_SYM_M_TRACER,
    FUDE_TEXT_ZOOM_SYM_M_CRANK, FUDE_TEXT_ZOOM_SYM_E_SPDT, FUDE_TEXT_ZOOM_SYM_E_DPDT, FUDE_TEXT_ZOOM_SYM_E_LDR, FUDE_TEXT_ZOOM_SYM_E_NTC,
    FUDE_TEXT_ZOOM_SYM_E_SOLENOID, FUDE_TEXT_ZOOM_SYM_E_STEPPER, FUDE_TEXT_ZOOM_SYM_E_ENCODER, FUDE_TEXT_ZOOM_SYM_E_SLOT,
    FUDE_TEXT_ZOOM_SYM_M_BELT, FUDE_TEXT_ZOOM_SYM_M_CHAIN, FUDE_TEXT_ZOOM_SYM_M_SPROCKET, FUDE_TEXT_ZOOM_SYM_M_CAM, FUDE_TEXT_ZOOM_SYM_M_FOLLOWER,
    FUDE_TEXT_ZOOM_SYM_M_RATCHET, FUDE_TEXT_ZOOM_SYM_M_PAWL, FUDE_TEXT_ZOOM_SYM_M_DAMPER, FUDE_TEXT_ZOOM_SYM_M_WORM,
    FUDE_TEXT_ZOOM_SYM_L_JK, FUDE_TEXT_ZOOM_SYM_L_SR, FUDE_TEXT_ZOOM_SYM_C_LM393, FUDE_TEXT_ZOOM_SYM_E_SCR, FUDE_TEXT_ZOOM_SYM_E_TRIAC,
    FUDE_TEXT_ZOOM_SYM_E_XFMR, FUDE_TEXT_ZOOM_SYM_E_BRIDGE, FUDE_TEXT_ZOOM_SYM_E_OPTO, FUDE_TEXT_ZOOM_SYM_E_PHOTODIODE, FUDE_TEXT_ZOOM_SYM_E_PHOTOTRANS,
    FUDE_TEXT_ZOOM_SYM_E_SPEAKER, FUDE_TEXT_ZOOM_SYM_C_4017,
    FUDE_TEXT_ZOOM_SYM_B_PAREN_L, FUDE_TEXT_ZOOM_SYM_B_PAREN_R, FUDE_TEXT_ZOOM_SYM_B_SQUARE_L, FUDE_TEXT_ZOOM_SYM_B_SQUARE_R,
    FUDE_TEXT_ZOOM_SYM_B_CURLY_L, FUDE_TEXT_ZOOM_SYM_B_CURLY_R, FUDE_TEXT_ZOOM_SYM_B_ANGLE_L, FUDE_TEXT_ZOOM_SYM_B_ANGLE_R,
    FUDE_TEXT_ZOOM_SYM_B_BAR, FUDE_TEXT_ZOOM_SYM_B_DOUBLE, FUDE_TEXT_ZOOM_SYM_B_FLOOR_L, FUDE_TEXT_ZOOM_SYM_B_FLOOR_R,
    FUDE_TEXT_ZOOM_SYM_B_CEILING_L, FUDE_TEXT_ZOOM_SYM_B_CEILING_R, FUDE_TEXT_ZOOM_SYM_B_UNDER, FUDE_TEXT_ZOOM_SYM_B_OVER,
    FUDE_TEXT_ZOOM_SYM_B_PARENS, FUDE_TEXT_ZOOM_SYM_B_SQUARES, FUDE_TEXT_ZOOM_SYM_B_CURLIES, FUDE_TEXT_ZOOM_SYM_B_ANGLES,
    FUDE_TEXT_ZOOM_SYM_B_BARS, FUDE_TEXT_ZOOM_SYM_B_DOUBLES,
    FUDE_TEXT_ZOOM_SYM_P_RECT, FUDE_TEXT_ZOOM_SYM_P_ROUNDED, FUDE_TEXT_ZOOM_SYM_P_CIRCLE, FUDE_TEXT_ZOOM_SYM_P_DIAMOND, FUDE_TEXT_ZOOM_SYM_P_PARALLEL,
    FUDE_TEXT_ZOOM_SYM_P_TRAPEZOID, FUDE_TEXT_ZOOM_SYM_P_TRIANGLE, FUDE_TEXT_ZOOM_SYM_P_RIGHT_TRI, FUDE_TEXT_ZOOM_SYM_P_PENTAGON, FUDE_TEXT_ZOOM_SYM_P_HEXAGON,
    FUDE_TEXT_ZOOM_SYM_P_OCTAGON, FUDE_TEXT_ZOOM_SYM_P_STAR, FUDE_TEXT_ZOOM_SYM_P_BURST, FUDE_TEXT_ZOOM_SYM_P_CROSS, FUDE_TEXT_ZOOM_SYM_P_HEART,
    FUDE_TEXT_ZOOM_SYM_P_CLOUD, FUDE_TEXT_ZOOM_SYM_P_SPEECH, FUDE_TEXT_ZOOM_SYM_P_THOUGHT, FUDE_TEXT_ZOOM_SYM_P_ARC, FUDE_TEXT_ZOOM_SYM_P_PIE,
    FUDE_TEXT_ZOOM_SYM_P_RING, FUDE_TEXT_ZOOM_SYM_P_CYLINDER, FUDE_TEXT_ZOOM_SYM_P_CUBE, FUDE_TEXT_ZOOM_SYM_P_ARROW, FUDE_TEXT_ZOOM_SYM_P_DOUBLE_ARROW,
    FUDE_TEXT_ZOOM_SYM_P_CURVED_ARROW, FUDE_TEXT_ZOOM_SYM_P_CHEVRON,
    FUDE_TEXT_ZOOM_SYM_P_VENN2, FUDE_TEXT_ZOOM_SYM_P_VENN3, FUDE_TEXT_ZOOM_SYM_K_BARS, FUDE_TEXT_ZOOM_SYM_K_LINES, FUDE_TEXT_ZOOM_SYM_K_PIE,
    FUDE_TEXT_ZOOM_SYM_G_ANGLE, FUDE_TEXT_ZOOM_SYM_G_RIGHT, FUDE_TEXT_ZOOM_SYM_G_TICK1, FUDE_TEXT_ZOOM_SYM_G_TICK2, FUDE_TEXT_ZOOM_SYM_G_TICK3,
    FUDE_TEXT_ZOOM_SYM_G_PAR1, FUDE_TEXT_ZOOM_SYM_G_PAR2, FUDE_TEXT_ZOOM_SYM_G_POINT, FUDE_TEXT_ZOOM_SYM_G_SEGMENT, FUDE_TEXT_ZOOM_SYM_G_RAY,
    FUDE_TEXT_ZOOM_SYM_G_LINE, FUDE_TEXT_ZOOM_SYM_G_VECTOR, FUDE_TEXT_ZOOM_SYM_G_RADIUS, FUDE_TEXT_ZOOM_SYM_G_PROTRACTOR,
    FUDE_TEXT_ZOOM_SYM_S_BEAKER, FUDE_TEXT_ZOOM_SYM_S_FLASK, FUDE_TEXT_ZOOM_SYM_S_ROUND_FLASK, FUDE_TEXT_ZOOM_SYM_S_TUBE, FUDE_TEXT_ZOOM_SYM_S_CYLINDER,
    FUDE_TEXT_ZOOM_SYM_S_FUNNEL, FUDE_TEXT_ZOOM_SYM_S_BURNER, FUDE_TEXT_ZOOM_SYM_S_CONVEX, FUDE_TEXT_ZOOM_SYM_S_CONCAVE, FUDE_TEXT_ZOOM_SYM_S_MIRROR,
    FUDE_TEXT_ZOOM_SYM_S_MIRROR2, FUDE_TEXT_ZOOM_SYM_S_PRISM, FUDE_TEXT_ZOOM_SYM_S_LAMP, FUDE_TEXT_ZOOM_SYM_S_ATOM, FUDE_TEXT_ZOOM_SYM_S_BENZENE,
    FUDE_TEXT_ZOOM_SYM_S_CELL, FUDE_TEXT_ZOOM_SYM_S_DNA, FUDE_TEXT_ZOOM_SYM_S_THERMO,
    FUDE_TEXT_ZOOM_SYM_T_CHECK, FUDE_TEXT_ZOOM_SYM_T_CROSS, FUDE_TEXT_ZOOM_SYM_T_WARNING, FUDE_TEXT_ZOOM_SYM_T_QUESTION, FUDE_TEXT_ZOOM_SYM_T_EXCLAIM,
    FUDE_TEXT_ZOOM_SYM_T_INFO, FUDE_TEXT_ZOOM_SYM_T_STAR, FUDE_TEXT_ZOOM_SYM_T_HEART, FUDE_TEXT_ZOOM_SYM_T_BULB, FUDE_TEXT_ZOOM_SYM_T_FLAG,
    FUDE_TEXT_ZOOM_SYM_T_PIN, FUDE_TEXT_ZOOM_SYM_T_CLOCK,
};
// A compile-time check that every symbol has a name (the size is -1, which does not compile, when one has not).
typedef char fude_zoom_page_symbol_names_as_symbol_h[(sizeof(FUDE_ZOOM_PAGE_SYMBOL_NAMES) / sizeof(FUDE_ZOOM_PAGE_SYMBOL_NAMES[0]) == FUDE_ZOOM_SYMBOL_KINDS) ? 1 : -1];

// --- building plans (plan.h): walls, rooms -------------------------------------------------------------

// A room labelled where it is tapped: the area the walls round it close (the bucket's: bucket.h), its name and area as
// a text in its middle (the average of its outline's points). One undo step.
void fude_zoom_page_room_at(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _ink = rde_arr_new(sizeof(fude_zoom_bucket_ink), _heap), _out = rde_arr_new(sizeof(fude_zoom_v2), _heap), _rings = rde_arr_new(sizeof(u32), _heap);
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, _s, &_page->editable);
    fude_zoom_page_view_ink(_page, &_ink, true);
    const FUDE_ZOOM_BUCKET_ _got = fude_zoom_bucket_region((const fude_zoom_bucket_ink*)_ink.memory, (u32)rde_arr_length(&_ink),
                                                           (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y }, (fude_zoom_v2){ _screen.x, _screen.y },
                                                           1.25, 8.0, 0.0, &_out, &_rings);
    if(_got != FUDE_ZOOM_BUCKET_FILLED) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_ROOM_OPEN));
    } else {
        // Its area: its outline less its holes (screen points²), in millimetres² on the canvas.
        const fude_zoom_v2* _p = (const fude_zoom_v2*)_out.memory;
        const u32* _r = (const u32*)_rings.memory;
        const u32 _n = (u32)rde_arr_length(&_out);
        f64 _area = 0.0;
        fude_zoom_v2 _mid = { 0.0, 0.0 };
        u32 _outer = 0;
        for(u32 _from = 0; _from < _n;) {
            u32 _to = _from;
            while(_to < _n && _r[_to] == _r[_from]) {
                _to++;
            }
            const f64 _a = fude_zoom_cut_area(&_p[_from], _to - _from);
            _area += _r[_from] == 0u ? rde_math_abs_f64(_a) : -rde_math_abs_f64(_a);
            if(_r[_from] == 0u) {
                for(u32 _i = _from; _i < _to; _i++) {
                    _mid.x += _p[_i].x;
                    _mid.y += _p[_i].y;
                }
                _outer = _to - _from;
            }
            _from = _to;
        }
        const f64 _mpp = fude_zoom_page_mm_per_point(_page);
        c8 _area_text[48], _text[128];
        fude_zoom_size_area_format(_area * _mpp * _mpp, &_page->units, _area_text, sizeof(_area_text));
        rde_snprintf(_text, sizeof(_text), "%s\n%s", fude_text(FUDE_TEXT_ZOOM_ROOM), _area_text);
        // (the label in its middle, its letters 16 points as the screen is now)
        const fude_zoom_v2 _c = _outer > 0u ? (fude_zoom_v2){ _mid.x / (f64)_outer, _mid.y / (f64)_outer } : (fude_zoom_v2){ _screen.x, _screen.y };
        const f64 _near = 4.0 / _s->camera.z;
        const fude_zoom_v2 _at = fude_zoom_camera_to_frame(&_s->camera, _c);
        const u32 _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
        const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _frame);
        const f64 _k = rde_math_max_f64(fude_zoom_sim_scale(_fs), 1e-300);
        const f64 _w = 130.0, _h = 46.0;
        const fude_zoom_v2 _tl = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), (fude_zoom_v2){ _c.x - _w * 0.5, _c.y + _h * 0.5 });
        const u32 _t = fude_zoom_scene_add_text(_s, _frame, (fude_zoom_place){ _tl, -rde_math_atan2_f64(_fs.b, _fs.a), 1.0 }, 16.0 / _k, _w / _k, _h / _k, FUDE_ZOOM_TEXT_PLAIN,
                                                _page->app->ink->color, (rde_color){ 0, 0, 0, 0 }, _text, (u32)rde_strlen(_text), 0);
        fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _t)->box, NULL, 0, &_t, 1);
    }
    rde_arr_free(&_ink);
    rde_arr_free(&_out);
    rde_arr_free(&_rings);
}

// The Plan tool (Floor plans, House wiring): walls 10, 15 and 25 cm thick, Room, the plan's parts.
RDE_INTERNAL const f64 FUDE_ZOOM_PAGE_WALLS[3] = { 100.0, 150.0, 250.0 };

// A Plan tool's choice: 0 to 2 a wall of FUDE_ZOOM_PAGE_WALLS' thickness, 3 Room, 4 the library's plan tabs. Returns
// false, as a choice that is not a toggle does.
RDE_INTERNAL b8 fude_zoom_plan_choose(fude_app* _app, u32 _index) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    if(_index < 3u) {
        fude_zoom_page_pen_modes_off(_page);
        _page->wall_mm    = FUDE_ZOOM_PAGE_WALLS[_index];
        _page->shape_tool = FUDE_ZOOM_SHAPE_WALL;   // (set, never toggled: chosen again it stays)
        fude_zoom_page_pen_under(_page);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_WALL_HOW));
    } else if(_index == 3u) {
        fude_zoom_page_pen_modes_off(_page);
        _page->room_tool = true;
        fude_zoom_page_pen_under(_page);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_ROOM_HOW));
    } else if(_index == 4u) {
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_PLANS);
    }
    fude_toolbar_refresh(&_app->ui->bar);
    return false;
}

// See page_parts.h.
b8 fude_zoom_plan_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    const fude_zoom_page* _page = fude_zoom_page_the;
    return _page != NULL && ((_index < 3u && _page->shape_tool == FUDE_ZOOM_SHAPE_WALL && _page->wall_mm == FUDE_ZOOM_PAGE_WALLS[_index]) || (_index == 3u && _page->room_tool));
}

// See page_parts.h.
b8 fude_zoom_plan_selected(const fude_app* _app) {
    const fude_zoom_page* _page = fude_zoom_page_the;
    return _page != NULL && _app->ui->bar.tool == FUDE_TOOL_DRAW && (_page->shape_tool == FUDE_ZOOM_SHAPE_WALL || _page->room_tool);
}

// The Plan tool's choices, in fude_zoom_plan_choose's order.
const fude_extension_choice FUDE_ZOOM_PLAN_CHOICES[] = {
    { FUDE_TEXT_ZOOM_WALL_10, FUDE_ICON_LINE,      fude_zoom_plan_choose },
    { FUDE_TEXT_ZOOM_WALL_15, FUDE_ICON_LINE,      fude_zoom_plan_choose },
    { FUDE_TEXT_ZOOM_WALL_25, FUDE_ICON_RECTANGLE, fude_zoom_plan_choose },
    { FUDE_TEXT_ZOOM_ROOM,    FUDE_ICON_AREA,      fude_zoom_plan_choose },
    { FUDE_TEXT_ZOOM_INSERT_PLAN, FUDE_ICON_PLUS,  fude_zoom_plan_choose },
};
// A compile-time check that the table has its FUDE_ZOOM_PLAN_CHOICES_N rows (the size is -1, which does
// not compile, when not).
typedef char fude_zoom_plan_choices_counted[(sizeof(FUDE_ZOOM_PLAN_CHOICES) / sizeof(FUDE_ZOOM_PLAN_CHOICES[0]) == FUDE_ZOOM_PLAN_CHOICES_N) ? 1 : -1];

// The parts list (Export): every part's symbol on the canvas (electronics', mechanisms', a plan's) counted by its kind
// and its value (its text's first line), as CSV with a header row; hidden layers' are left out. False: no part.
b8 fude_zoom_page_parts_list(const fude_zoom_page* _page, fude_bytes* _out) {
    const fude_zoom_scene* _s = &_page->scene;
    typedef struct { u32 kind; c8 value[64]; u32 count; } fude_zoom_page_part_count;
    rde_arr _rows = rde_arr_new(sizeof(fude_zoom_page_part_count), rde_memory_allocator_get_default_std());
    rde_arr _n = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SYMBOL || fude_zoom_scene_hides(_s, _o)) {
            continue;
        }
        const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _i, &_n);
        if(_count < 4u) {
            continue;
        }
        const u32 _kind = (u32)((const f64*)_n.memory)[0];
        const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
        if(_info == NULL || fude_zoom_symbol_topic(_kind) == FUDE_ZOOM_SYMBOL_TOPIC_NONE) {
            continue;   // (a diagram's shapes, maths, sewing, shapes, charts and the rest are not parts: only a topic's are)
        }
        fude_zoom_symbol_text((const f64*)_n.memory, _count, _text, sizeof(_text));
        _text[rde_strcspn(_text, "\n")] = 0;   // (its value: its first line)
        fude_zoom_page_part_count* _r = (fude_zoom_page_part_count*)_rows.memory;
        u32 _k = 0;
        for(; _k < (u32)rde_arr_length(&_rows); _k++) {
            if(_r[_k].kind == _kind && rde_strncmp(_r[_k].value, _text, sizeof(_r[_k].value) - 1u) == 0) {
                _r[_k].count++;
                break;
            }
        }
        if(_k == (u32)rde_arr_length(&_rows)) {
            fude_zoom_page_part_count _c = { _kind, { 0 }, 1u };
            rde_snprintf(_c.value, sizeof(_c.value), "%s", _text);
            rde_arr_add(&_rows, (any)&_c);
        }
    }
    const u32 _nr = (u32)rde_arr_length(&_rows);
    if(_nr > 0u) {
        c8 _line[400];
        rde_snprintf(_line, sizeof(_line), "%s,%s,%s\n", fude_text(FUDE_TEXT_ZOOM_PARTS_PART), fude_text(FUDE_TEXT_ZOOM_PARTS_VALUE), fude_text(FUDE_TEXT_ZOOM_PARTS_COUNT));
        fude_put_data(_out, _line, (u32)rde_strlen(_line));
        const fude_zoom_page_part_count* _r = (const fude_zoom_page_part_count*)_rows.memory;
        for(u32 _k = 0; _k < _nr; _k++) {
            // (a value with a comma or a quote: quoted, its quotes doubled)
            c8 _v[160];
            u32 _w = 0;
            _v[_w++] = '"';
            for(const c8* _c = _r[_k].value; *_c != 0 && _w + 3u < sizeof(_v); _c++) {
                if(*_c == '"') {
                    _v[_w++] = '"';
                }
                _v[_w++] = *_c;
            }
            _v[_w++] = '"';
            _v[_w] = 0;
            rde_snprintf(_line, sizeof(_line), "\"%s\",%s,%u\n", fude_text(FUDE_ZOOM_PAGE_SYMBOL_NAMES[_r[_k].kind]), _v, _r[_k].count);
            fude_put_data(_out, _line, (u32)rde_strlen(_line));
        }
    }
    rde_arr_free(&_rows);
    rde_arr_free(&_n);
    return _nr > 0u;
}

// The library's tabs' names, by tab: the symbols' families, then the connectors' and Custom.
RDE_INTERNAL const FUDE_TEXT_ FUDE_ZOOM_PAGE_LIBRARY_TABS[] = {
    FUDE_TEXT_ZOOM_LIB_FLOW, FUDE_TEXT_ZOOM_LIB_UML, FUDE_TEXT_ZOOM_LIB_BEHAVIOUR, FUDE_TEXT_ZOOM_LIB_DATA,
    FUDE_TEXT_ZOOM_LIB_ARCH, FUDE_TEXT_ZOOM_LIB_BPMN, FUDE_TEXT_ZOOM_LIB_PLANNING, FUDE_TEXT_ZOOM_LIB_ELECTRONICS,
    FUDE_TEXT_ZOOM_LIB_LOGIC, FUDE_TEXT_ZOOM_LIB_CHIPS, FUDE_TEXT_ZOOM_LIB_BOARDS, FUDE_TEXT_ZOOM_LIB_MECHANISMS,
    FUDE_TEXT_ZOOM_LIB_FLOORPLAN, FUDE_TEXT_ZOOM_LIB_WIRING, FUDE_TEXT_ZOOM_LIB_MATHS, FUDE_TEXT_ZOOM_LIB_SEWING,
    FUDE_TEXT_ZOOM_LIB_SHAPES, FUDE_TEXT_ZOOM_LIB_BRACKETS, FUDE_TEXT_ZOOM_LIB_CHARTS, FUDE_TEXT_ZOOM_LIB_GEOMETRY, FUDE_TEXT_ZOOM_LIB_SCIENCE,
    FUDE_TEXT_ZOOM_LIB_STAMPS, FUDE_TEXT_ZOOM_LIB_CONNECTORS,
    FUDE_TEXT_ZOOM_LIB_CUSTOM,
};
// A compile-time check that every tab has a name (the size is -1, which does not compile, when one has not).
typedef char fude_zoom_page_library_tabs_named[(sizeof(FUDE_ZOOM_PAGE_LIBRARY_TABS) / sizeof(FUDE_ZOOM_PAGE_LIBRARY_TABS[0]) == FUDE_ZOOM_PAGE_LIBRARY_TABS_N) ? 1 : -1];
// ...in the order the panel shows them: plain shapes and brackets first.
RDE_INTERNAL const u8 FUDE_ZOOM_PAGE_LIBRARY_ORDER[] = {
    FUDE_ZOOM_SYMBOL_FAMILY_SHAPES, FUDE_ZOOM_SYMBOL_FAMILY_STAMPS, FUDE_ZOOM_SYMBOL_FAMILY_BRACKETS, FUDE_ZOOM_SYMBOL_FAMILY_CHARTS,
    FUDE_ZOOM_SYMBOL_FAMILY_GEOMETRY, FUDE_ZOOM_SYMBOL_FAMILY_SCIENCE,
    FUDE_ZOOM_SYMBOL_FAMILY_FLOW, FUDE_ZOOM_SYMBOL_FAMILY_UML, FUDE_ZOOM_SYMBOL_FAMILY_BEHAVIOUR, FUDE_ZOOM_SYMBOL_FAMILY_DATA,
    FUDE_ZOOM_SYMBOL_FAMILY_ARCHITECTURE, FUDE_ZOOM_SYMBOL_FAMILY_BPMN, FUDE_ZOOM_SYMBOL_FAMILY_PLANNING, FUDE_ZOOM_SYMBOL_FAMILY_ELECTRONICS,
    FUDE_ZOOM_SYMBOL_FAMILY_LOGIC, FUDE_ZOOM_SYMBOL_FAMILY_CHIPS, FUDE_ZOOM_SYMBOL_FAMILY_BOARDS, FUDE_ZOOM_SYMBOL_FAMILY_MECHANISMS,
    FUDE_ZOOM_SYMBOL_FAMILY_FLOORPLAN, FUDE_ZOOM_SYMBOL_FAMILY_WIRING, FUDE_ZOOM_SYMBOL_FAMILY_MATHS, FUDE_ZOOM_SYMBOL_FAMILY_SEWING,
    FUDE_ZOOM_PAGE_LIBRARY_CONNECTORS, FUDE_ZOOM_PAGE_LIBRARY_CUSTOM,
};
// A compile-time check that every tab has its place (the size is -1, which does not compile, when one has not).
typedef char fude_zoom_page_library_order_counted[(sizeof(FUDE_ZOOM_PAGE_LIBRARY_ORDER) / sizeof(FUDE_ZOOM_PAGE_LIBRARY_ORDER[0]) == FUDE_ZOOM_PAGE_LIBRARY_TABS_N) ? 1 : -1];

// Is tab _f shown (the panel opened for diagrams or for circuits)?
RDE_INTERNAL b8 fude_zoom_page_library_shows(const fude_zoom_page* _page, u32 _f) {
    const u32 _tabs = _page->library_tabs != 0u ? _page->library_tabs : FUDE_ZOOM_PAGE_LIBRARY_DIAGRAMS;
    return ((_tabs >> _f) & 1u) != 0u;
}

// The panel opened with the tabs _tabs (its tab the first of them, unless it is on one already).
void fude_zoom_page_library_open_tabs(fude_zoom_page* _page, u32 _tabs) {
    if(!_page->library_open || _page->library_tabs != _tabs) {
        _page->library_scroll = 0.0;
    }
    _page->library_tabs = _tabs;
    if(!fude_zoom_page_library_shows(_page, _page->library_family)) {
        for(u32 _o = 0; _o < FUDE_ZOOM_PAGE_LIBRARY_TABS_N; _o++) {
            const u32 _f = FUDE_ZOOM_PAGE_LIBRARY_ORDER[_o];
            if((_tabs >> _f) & 1u) {
                _page->library_family = (u8)_f;
                break;
            }
        }
    }
    _page->library_open = true;
}

// A connector style (symbol.h's heads): what a new connector is, chosen from the library's Connectors tab. Its name and
// its heads number (the FUDE_ZOOM_ARROW_ bits and both ends' styles).
typedef struct {
    FUDE_TEXT_ name;
    u32        heads;
} fude_zoom_page_connector_style;

// Short names for the table: the end's head, the start's head, a dotted line, and the two ends' styles.
#define FZPG_E  FUDE_ZOOM_ARROW_END
#define FZPG_S  FUDE_ZOOM_ARROW_START
#define FZPG_D  FUDE_ZOOM_ARROW_DOTTED
#define FZPG(_end, _start) FUDE_ZOOM_ARROW_STYLES(FUDE_ZOOM_HEAD_##_end, FUDE_ZOOM_HEAD_##_start)
// The connector styles, in the tab's order.
RDE_INTERNAL const fude_zoom_page_connector_style FUDE_ZOOM_PAGE_CONNECTOR_STYLES[] = {
    { FUDE_TEXT_ZOOM_CONN_ARROW,        FZPG_E },
    { FUDE_TEXT_ZOOM_CONN_LINE,         0u },
    { FUDE_TEXT_ZOOM_CONN_BOTH,         FZPG_E | FZPG_S },
    { FUDE_TEXT_ZOOM_CONN_NAVIGABLE,    FZPG_E | FZPG(OPEN, CLASSIC) },
    { FUDE_TEXT_ZOOM_CONN_DEPENDENCY,   FZPG_E | FZPG_D | FZPG(OPEN, CLASSIC) },
    { FUDE_TEXT_ZOOM_CONN_INHERITANCE,  FZPG_E | FZPG(HOLLOW, CLASSIC) },
    { FUDE_TEXT_ZOOM_CONN_REALIZATION,  FZPG_E | FZPG_D | FZPG(HOLLOW, CLASSIC) },
    { FUDE_TEXT_ZOOM_CONN_AGGREGATION,  FZPG_E | FZPG(HOLLOW_DIAMOND, CLASSIC) },
    { FUDE_TEXT_ZOOM_CONN_COMPOSITION,  FZPG_E | FZPG(DIAMOND, CLASSIC) },
    { FUDE_TEXT_ZOOM_CONN_REPLY,        FZPG_E | FZPG_D | FZPG(OPEN, CLASSIC) },
    { FUDE_TEXT_ZOOM_CONN_ONE_MANY,     FZPG_E | FZPG_S | FZPG(ONE_MANY, ONLY_ONE) },
    { FUDE_TEXT_ZOOM_CONN_ZERO_MANY,    FZPG_E | FZPG_S | FZPG(ZERO_MANY, ZERO_ONE) },
    { FUDE_TEXT_ZOOM_CONN_ONE_ONE,      FZPG_E | FZPG_S | FZPG(ONLY_ONE, ONLY_ONE) },
    { FUDE_TEXT_ZOOM_CONN_MANY_MANY,    FZPG_E | FZPG_S | FZPG(MANY, MANY) },
    { FUDE_TEXT_ZOOM_CONN_MESSAGE_FLOW, FZPG_E | FZPG_S | FZPG_D | FZPG(HOLLOW, RING) },
    { FUDE_TEXT_ZOOM_CONN_DOT,          FZPG_E | FZPG(DOT, CLASSIC) },
    // (0.1.85: more ends)
    { FUDE_TEXT_ZOOM_CONN_DOTTED,       FZPG_D },
    { FUDE_TEXT_ZOOM_CONN_DOTTED_ARROW, FZPG_E | FZPG_D },
    { FUDE_TEXT_ZOOM_CONN_BOTH_OPEN,    FZPG_E | FZPG_S | FZPG(OPEN, OPEN) },
    { FUDE_TEXT_ZOOM_CONN_DOTS,         FZPG_E | FZPG_S | FZPG(DOT, DOT) },
    { FUDE_TEXT_ZOOM_CONN_BARS,         FZPG_E | FZPG_S | FZPG(BAR, BAR) },
    { FUDE_TEXT_ZOOM_CONN_DOT_ARROW,    FZPG_E | FZPG_S | FZPG(CLASSIC, DOT) },
};
// How many connector styles there are.
#define FUDE_ZOOM_PAGE_CONNECTOR_STYLES_N ((u32)(sizeof(FUDE_ZOOM_PAGE_CONNECTOR_STYLES) / sizeof(FUDE_ZOOM_PAGE_CONNECTOR_STYLES[0])))

// See page_parts.h. The symbols counted out by family, then each family's kinds written in the table's order: two
// passes over the table, once.
void fude_zoom_page_library_make(fude_zoom_page* _page) {
    _page->library_kinds = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_memset(_page->library_first, 0, sizeof(_page->library_first));
    const u32 _area = fude_zoom_symbol_find("area");   // (an area: Insert's own)
    const u32 _n    = fude_zoom_symbol_count();
    for(u32 _k = 0; _k < _n; _k++) {
        const u8 _f = fude_zoom_symbol_info_of(_k)->family;
        if(_k != _area && _f < FUDE_ZOOM_SYMBOL_FAMILIES) {
            _page->library_first[_f + 1u]++;
        }
    }
    for(u32 _f = 0; _f < FUDE_ZOOM_SYMBOL_FAMILIES; _f++) {
        _page->library_first[_f + 1u] += _page->library_first[_f];
    }
    rde_arr_resize(&_page->library_kinds, _page->library_first[FUDE_ZOOM_SYMBOL_FAMILIES]);
    u32* _kinds = (u32*)_page->library_kinds.memory;
    u32  _at[FUDE_ZOOM_SYMBOL_FAMILIES];
    rde_memcpy(_at, _page->library_first, sizeof(_at));
    for(u32 _k = 0; _k < _n; _k++) {
        const u8 _f = fude_zoom_symbol_info_of(_k)->family;
        if(_k != _area && _f < FUDE_ZOOM_SYMBOL_FAMILIES) {
            _kinds[_at[_f]++] = _k;
        }
    }
}

// How many tiles a tab has, and tile _i's (a symbol's kind, a connector style's place, or Custom's item: custom_items'
// — made again by the count, which comes first). A family's are library_kinds' (fude_zoom_page_library_make).
RDE_INTERNAL u32 fude_zoom_page_library_count(fude_zoom_page* _page, u8 _tab) {
    if(_tab == FUDE_ZOOM_PAGE_LIBRARY_CONNECTORS) {
        return FUDE_ZOOM_PAGE_CONNECTOR_STYLES_N;
    }
    if(_tab == FUDE_ZOOM_PAGE_LIBRARY_CUSTOM) {
        return fude_zoom_page_custom_items(_page);
    }
    return _tab < FUDE_ZOOM_SYMBOL_FAMILIES ? _page->library_first[_tab + 1u] - _page->library_first[_tab] : 0u;
}

// Tile _i of tab _tab: a symbol's kind (the family's symbols in the table's order, an area left out), a connector
// style's place, or Custom's item. FUDE_ZOOM_NONE past the last.
RDE_INTERNAL u32 fude_zoom_page_library_item(const fude_zoom_page* _page, u8 _tab, u32 _i) {
    if(_tab == FUDE_ZOOM_PAGE_LIBRARY_CONNECTORS) {
        return _i;
    }
    if(_tab == FUDE_ZOOM_PAGE_LIBRARY_CUSTOM) {
        return _i < (u32)rde_arr_length(&_page->custom_items) ? ((const u32*)_page->custom_items.memory)[_i] : FUDE_ZOOM_NONE;
    }
    if(_tab >= FUDE_ZOOM_SYMBOL_FAMILIES || _i >= _page->library_first[_tab + 1u] - _page->library_first[_tab]) {
        return FUDE_ZOOM_NONE;
    }
    return ((const u32*)_page->library_kinds.memory)[_page->library_first[_tab] + _i];
}

#define FUDE_ZOOM_PAGE_LIB_TILE   96.0f    // a tile: its shape and its name under it
#define FUDE_ZOOM_PAGE_LIB_PAD    12.0f    // the panel's margin
#define FUDE_ZOOM_PAGE_LIB_TAB_H  32.0f    // a tab's height

// A panel across _x0 to _x1 (screen points from the middle, Y up), from _y0 up to _y1, moved clear of the toolbar
// wherever it was put: to the bar's side with more room, as wide as it was if that fits (never under _least: else
// where it was).
void fude_zoom_page_clear_of_bar(const fude_zoom_page* _page, f64* _x0, f64* _x1, f64 _y0, f64 _y1, f64 _least) {
    if(_page->app == NULL || _page->app->ui == NULL || _page->app->ui->bar.hidden) {
        return;
    }
    // (the bar's panel: UI canvas units, bottom-left origin — screen points)
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const fude_toolbar* _bar = &_page->app->ui->bar;
    const f64 _b0 = _bar->center.x - _bar->panel_size.x * 0.5 - _half.x, _b1 = _bar->center.x + _bar->panel_size.x * 0.5 - _half.x;
    const f64 _bb = _bar->center.y - _bar->panel_size.y * 0.5 - _half.y, _bt = _bar->center.y + _bar->panel_size.y * 0.5 - _half.y;
    if(!(_b1 > *_x0 && _b0 < *_x1 && _bt > _y0 && _bb < _y1)) {
        return;
    }
    const f64 _w = *_x1 - *_x0;
    const f64 _room_r = _half.x - 12.0 - (_b1 + 12.0), _room_l = (_b0 - 12.0) - (-_half.x + 12.0);
    if(_room_r >= _room_l && _room_r >= _least) {
        const f64 _ww = rde_math_min_f64(_w, _room_r);
        *_x0 = rde_math_max_f64(*_x0, _b1 + 12.0);
        *_x1 = rde_math_min_f64(*_x0 + _ww, _half.x - 12.0);
        *_x0 = *_x1 - _ww;
    } else if(_room_l > _room_r && _room_l >= _least) {
        const f64 _ww = rde_math_min_f64(_w, _room_l);
        *_x1 = rde_math_min_f64(*_x1, _b0 - 12.0);
        *_x0 = rde_math_max_f64(*_x1 - _ww, -_half.x + 12.0);
        *_x1 = *_x0 + _ww;
    }
}

// Where the panel and its parts are (screen, centre origin, Y up): beside the toolbar, high up — clear of the bar
// wherever it was put (on its side with more room), when there is room for it.
void fude_zoom_page_library_lay(fude_zoom_page* _page, fude_zoom_page_library_layout* _l) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    f64 _w = rde_math_min_f64(620.0, _half.x * 2.0 - 140.0);
    const f64 _h = rde_math_min_f64(660.0, _half.y * 2.0 - 150.0);
    f64 _right = _half.x - 96.0;
    const f64 _top = _half.y - 64.0;
    {
        f64 _x0 = _right - _w, _x1 = _right;
        fude_zoom_page_clear_of_bar(_page, &_x0, &_x1, _top - _h, _top, 320.0);
        _w     = _x1 - _x0;
        _right = _x1;
    }
    _l->panel = (fude_zoom_box){ _right - _w, _top - _h, _right, _top };
    _l->close = (fude_zoom_box){ _right - FUDE_ZOOM_PAGE_LIB_PAD - 36.0, _top - FUDE_ZOOM_PAGE_LIB_PAD - 36.0, _right - FUDE_ZOOM_PAGE_LIB_PAD, _top - FUDE_ZOOM_PAGE_LIB_PAD };
    // Its tabs, as wide as their names, in rows (the first row leaves the × its room).
    const f32 _px = 14.0f;
    f64 _x = _l->panel.min_x + FUDE_ZOOM_PAGE_LIB_PAD, _y = _top - FUDE_ZOOM_PAGE_LIB_PAD;
    f64 _limit = _l->close.min_x - 8.0;
    for(u32 _o = 0; _o < FUDE_ZOOM_PAGE_LIBRARY_TABS_N; _o++) {
        const u32 _f = FUDE_ZOOM_PAGE_LIBRARY_ORDER[_o];
        if(!fude_zoom_page_library_shows(_page, _f)) {
            _l->tabs[_f] = (fude_zoom_box){ 1e9, 1e9, 1e9, 1e9 };   // (not this panel's)
            continue;
        }
        const c8* _name = fude_text(FUDE_ZOOM_PAGE_LIBRARY_TABS[_f]);
        const f64 _tw = (_page->app->font != NULL ? fude_draw_text_width(_page->app->font, _page->app->font_px, _name, _px) : 8.0f * (f32)rde_strlen(_name)) + 26.0;
        if(_x + _tw > _limit && _x > _l->panel.min_x + FUDE_ZOOM_PAGE_LIB_PAD + 1.0) {
            _x     = _l->panel.min_x + FUDE_ZOOM_PAGE_LIB_PAD;
            _y    -= FUDE_ZOOM_PAGE_LIB_TAB_H + 6.0;
            _limit = _l->panel.max_x - FUDE_ZOOM_PAGE_LIB_PAD;
        }
        _l->tabs[_f] = (fude_zoom_box){ _x, _y - FUDE_ZOOM_PAGE_LIB_TAB_H, _x + _tw, _y };
        _x += _tw + 6.0;
    }
    _l->grid = (fude_zoom_box){ _l->panel.min_x + FUDE_ZOOM_PAGE_LIB_PAD, _l->panel.min_y + FUDE_ZOOM_PAGE_LIB_PAD,
                                _l->panel.max_x - FUDE_ZOOM_PAGE_LIB_PAD, _y - FUDE_ZOOM_PAGE_LIB_TAB_H - 10.0 };
    _l->cols = (u32)rde_math_max_f64(1.0, rde_math_floor_f64((_l->grid.max_x - _l->grid.min_x) / FUDE_ZOOM_PAGE_LIB_TILE));
    const u32 _n = fude_zoom_page_library_count(_page, _page->library_family);
    _l->content = (f32)((f64)((_n + _l->cols - 1u) / _l->cols) * FUDE_ZOOM_PAGE_LIB_TILE);
}

// See page_parts.h. Its edges count as inside.
b8 fude_zoom_page_box_holds(fude_zoom_box _b, rde_vec_2F _p) {
    return _p.x >= _b.min_x && _p.x <= _b.max_x && _p.y >= _b.min_y && _p.y <= _b.max_y;
}

// Tile _i's box (its top left the grid's, as scrolled).
RDE_INTERNAL fude_zoom_box fude_zoom_page_library_box(const fude_zoom_page* _page, const fude_zoom_page_library_layout* _l, u32 _i) {
    const f64 _x0 = _l->grid.min_x + (f64)(_i % _l->cols) * FUDE_ZOOM_PAGE_LIB_TILE;
    const f64 _y1 = _l->grid.max_y - (f64)(_i / _l->cols) * FUDE_ZOOM_PAGE_LIB_TILE + _page->library_scroll;
    return (fude_zoom_box){ _x0, _y1 - FUDE_ZOOM_PAGE_LIB_TILE, _x0 + FUDE_ZOOM_PAGE_LIB_TILE, _y1 };
}

// The tile under _p (its symbol's kind; FUDE_ZOOM_NONE: none), its box into *_box (may be NULL).
u32 fude_zoom_page_library_tile(fude_zoom_page* _page, const fude_zoom_page_library_layout* _l, rde_vec_2F _p, fude_zoom_box* _box) {
    if(!fude_zoom_page_box_holds(_l->grid, _p)) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _n = fude_zoom_page_library_count(_page, _page->library_family);
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_box _b = fude_zoom_page_library_box(_page, _l, _i);
        if(_p.x >= _b.min_x && _p.x < _b.max_x && _p.y <= _b.max_y && _p.y > _b.min_y) {
            if(_box != NULL) {
                *_box = _b;
            }
            return fude_zoom_page_library_item(_page, _page->library_family, _i);
        }
    }
    return FUDE_ZOOM_NONE;
}

// A symbol drawn into a box on the screen (its preview): its parts, filled as the page, solid in _ink. Its geometry is
// the renderer's _r, kept with its triangles (fude_zoom_render_symbol_geo: a tile is always the same size, so it is
// made once, not each frame), drawn round the box's middle.
RDE_INTERNAL void fude_zoom_page_draw_symbol_in(fude_zoom_renderer* _r, u32 _kind, fude_zoom_box _box, rde_color _ink, rde_color _paper) {
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL) {
        return;
    }
    const f64 _bw = _box.max_x - _box.min_x, _bh = _box.max_y - _box.min_y;
    const f64 _k  = rde_math_min_f64(_bw / (f64)_info->w, _bh / (f64)_info->h);
    const f64 _hw = (f64)_info->w * 0.5 * _k, _hh = (f64)_info->h * 0.5 * _k;
    // (very long or flat ones — a bus, a bar, a timeline — are drawn at their own shape, a lane shorter)
    const fude_zoom_v2 _c = { (_box.min_x + _box.max_x) * 0.5, (_box.min_y + _box.max_y) * 0.5 };
    const fude_zoom_geo* _geo = fude_zoom_render_symbol_geo(_r, _kind, rde_math_max_f64(_hw, 1.0), rde_math_max_f64(_hh, 1.0), 48u);
    const u32 _np = _geo->parts_n;
    const fude_zoom_v2*          _p     = (const fude_zoom_v2*)_geo->points.memory;   // (its own units: round _c)
    const fude_zoom_symbol_part* _pa    = (const fude_zoom_symbol_part*)_geo->parts.memory;
    const u32*                   _range = (const u32*)_geo->tri_range.memory;
    const fude_zoom_v2*          _tris  = (const fude_zoom_v2*)_geo->tris.memory;
    for(u32 _i = 0; _i < _np; _i++) {
        const fude_zoom_v2* _q = &_p[_pa[_i].first];
        const u32 _m = _pa[_i].count;
        const u8  _fl = _pa[_i].flags;
        if(_range[2u * _i + 1u] > 0u) {
            // (its triangles, its holes — the rings joined to it — left out)
            const fude_zoom_v2* _tr = &_tris[3u * _range[2u * _i]];
            const rde_color _col = (_fl & FUDE_ZOOM_SYMBOL_SOLID) ? _ink : ((_fl & FUDE_ZOOM_SYMBOL_TINTED) ? _pa[_i].fill : _paper);
            for(u32 _j = 0; _j < _range[2u * _i + 1u]; _j++) {
                const rde_vec_2F _v[3] = { { (f32)(_tr[3u * _j].x + _c.x), (f32)(_tr[3u * _j].y + _c.y) }, { (f32)(_tr[3u * _j + 1u].x + _c.x), (f32)(_tr[3u * _j + 1u].y + _c.y) },
                                           { (f32)(_tr[3u * _j + 2u].x + _c.x), (f32)(_tr[3u * _j + 2u].y + _c.y) } };
                rde_rendering_2d_draw_polygon(_v, 3u, _col, NULL);
            }
        }
        if(_fl & FUDE_ZOOM_SYMBOL_NO_LINE) {
            continue;
        }
        const rde_color _lc = _pa[_i].line.a > 0u ? _pa[_i].line : _ink;
        const b8  _closed = (_fl & FUDE_ZOOM_SYMBOL_CLOSED) != 0u;
        const u32 _e = _closed ? _m : _m - 1u;
        if(_pa[_i].width > 1.2f && _m >= 2u && _m < 256u) {
            // (a wide line of its own — a stamp's tick —: one stroke, its turns joined)
            rde_vec_2F _sp[257];
            f32        _sr[257];
            for(u32 _j = 0; _j <= _e; _j++) {
                _sp[_j] = (rde_vec_2F){ (f32)(_q[_j % _m].x + _c.x), (f32)(_q[_j % _m].y + _c.y) };
                _sr[_j] = _pa[_i].width * 0.5f;
            }
            rde_rendering_2d_draw_stroke(_sp, _sr, _e + 1u, _lc);
            continue;
        }
        for(u32 _j = 0; _j < _e; _j++) {
            const fude_zoom_v2 _a = { _q[_j].x + _c.x, _q[_j].y + _c.y }, _b = { _q[(_j + 1u) % _m].x + _c.x, _q[(_j + 1u) % _m].y + _c.y };
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _lc, 1.2f);
        }
    }
    // A graph's, axes', a number line's: a sample of what it draws (plot.h), its grid faint, its curve coloured.
    if(fude_zoom_plot_is(_info->id)) {
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        rde_arr _pp = rde_arr_new(sizeof(fude_zoom_v2), _heap), _pq = rde_arr_new(sizeof(fude_zoom_symbol_part), _heap), _fill = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        fude_zoom_plot_lines(_info->id, rde_strcmp(_info->id, "number line") == 0 ? "-5 5" : (rde_strcmp(_info->id, "graph") == 0 ? "sin(x)\nx -6 6" :
                             fude_zoom_chart_is(_info->id) ? fude_zoom_chart_example(_info->id) : ""),
                             rde_math_max_f64(_hw, 1.0), rde_math_max_f64(_hh, 1.0), &_pp, &_pq, NULL);
        const fude_zoom_v2* _q = (const fude_zoom_v2*)_pp.memory;
        const fude_zoom_symbol_part* _qa = (const fude_zoom_symbol_part*)_pq.memory;
        rde_color _faint = _ink;
        _faint.a = (u8)((u32)_ink.a * 30u / 100u);
        static const rde_color _series[6] = { { 70, 140, 255, 255 }, { 240, 80, 70, 255 }, { 60, 190, 110, 255 }, { 245, 160, 40, 255 }, { 170, 100, 240, 255 }, { 40, 190, 200, 255 } };
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_pq); _i++) {
            if(_qa[_i].flags >= 64u && _qa[_i].count >= 3u) {
                // (a region: a chart's bar or slice solid in its series' colour; a graph's shading faint)
                rde_color _rc = _series[(_qa[_i].flags >= 128u ? _qa[_i].flags - 128u : _qa[_i].flags - 64u) % 6u];
                _rc.a = _qa[_i].flags >= 128u ? 255u : 56u;
                rde_arr_clear(&_fill);
                for(u32 _j = 0; _j < _qa[_i].count; _j++) {
                    const fude_zoom_v2 _a = { _q[_qa[_i].first + _j].x + _c.x, _q[_qa[_i].first + _j].y + _c.y };
                    rde_arr_add(&_fill, (any)&_a);
                }
                rde_arr _tri = rde_arr_new(sizeof(fude_zoom_v2), _heap);
                const u32 _nt = fude_zoom_fill_triangulate((const fude_zoom_v2*)_fill.memory, (u32)rde_arr_length(&_fill), &_tri);
                const fude_zoom_v2* _t3 = (const fude_zoom_v2*)_tri.memory;
                for(u32 _j = 0; _j < _nt; _j++) {
                    rde_rendering_2d_draw_triangle((rde_vec_2F){ (f32)_t3[3u * _j].x, (f32)_t3[3u * _j].y }, (rde_vec_2F){ (f32)_t3[3u * _j + 1u].x, (f32)_t3[3u * _j + 1u].y },
                                                   (rde_vec_2F){ (f32)_t3[3u * _j + 2u].x, (f32)_t3[3u * _j + 2u].y }, _rc, NULL);
                }
                rde_arr_free(&_tri);
                continue;
            }
            const rde_color _col = _qa[_i].flags >= 32u ? _series[((_qa[_i].flags - 32u) & 7u) % 6u] : ((_qa[_i].flags & FUDE_ZOOM_SYMBOL_DASHED) ? _faint : _ink);
            for(u32 _j = 0; _j + 1u < _qa[_i].count; _j++) {
                const fude_zoom_v2 _a = _q[_qa[_i].first + _j], _b = _q[_qa[_i].first + _j + 1u];
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_a.x + _c.x), (f32)(_a.y + _c.y) }, (rde_vec_2F){ (f32)(_b.x + _c.x), (f32)(_b.y + _c.y) }, _col,
                                             _qa[_i].flags >= 32u ? 1.6f : 1.0f);
            }
        }
        rde_arr_free(&_pp);
        rde_arr_free(&_pq);
        rde_arr_free(&_fill);
    }
    // A class's compartments (their lines, as its text would have them).
    if(_info->place == FUDE_ZOOM_SYMBOL_TEXT_PARTS && _info->boxes != 4u) {
        fude_zoom_box _boxes[8];
        const u32 _nb = fude_zoom_symbol_text_boxes(_kind, rde_math_max_f64(_hw, 1.0), rde_math_max_f64(_hh, 1.0), _info->boxes, _boxes, 8u);
        for(u32 _i = 1; _i < _nb; _i++) {
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_c.x - _hw), (f32)(_c.y + _boxes[_i].max_y) }, (rde_vec_2F){ (f32)(_c.x + _hw), (f32)(_c.y + _boxes[_i].max_y) }, _ink, 1.2f);
        }
    }
}

// A connector of _heads drawn across a box on the screen (its tile): its line (dashed or not) and its ends.
RDE_INTERNAL void fude_zoom_page_draw_connector_in(u32 _heads, fude_zoom_box _box, rde_color _ink, rde_color _paper) {
    const f64 _y = (_box.min_y + _box.max_y) * 0.5;
    fude_zoom_v2 _a = { _box.min_x + 4.0, _y }, _b = { _box.max_x - 4.0, _y };
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _hp = rde_arr_new(sizeof(fude_zoom_v2), _heap), _hs = rde_arr_new(sizeof(fude_zoom_symbol_part), _heap);
    const f64 _size = 9.0;
    for(u32 _end = 0; _end < 2u; _end++) {
        if(!(_heads & (_end == 0u ? FUDE_ZOOM_ARROW_END : FUDE_ZOOM_ARROW_START))) {
            continue;
        }
        const fude_zoom_v2 _tip = _end == 0u ? _b : _a, _from = _end == 0u ? _a : _b;
        const f64 _trim = fude_zoom_symbol_head(_end == 0u ? FUDE_ZOOM_ARROW_END_STYLE(_heads) : FUDE_ZOOM_ARROW_START_STYLE(_heads), (_heads & FUDE_ZOOM_ARROW_CIRCLE) != 0u,
                                                _tip, _from, _size, &_hp, &_hs);
        const fude_zoom_v2* _q = (const fude_zoom_v2*)_hp.memory;
        const fude_zoom_symbol_part* _h = (const fude_zoom_symbol_part*)_hs.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_hs); _i++) {
            rde_vec_2F _poly[64];
            const u32 _m = _h[_i].count < 64u ? _h[_i].count : 64u;
            for(u32 _j = 0; _j < _m; _j++) {
                _poly[_j] = (rde_vec_2F){ (f32)_q[_h[_i].first + _j].x, (f32)_q[_h[_i].first + _j].y };
            }
            if((_h[_i].flags & (FUDE_ZOOM_SYMBOL_SOLID | FUDE_ZOOM_SYMBOL_FILLED)) && _m >= 3u) {
                rde_rendering_2d_draw_polygon(_poly, _m, (_h[_i].flags & FUDE_ZOOM_SYMBOL_SOLID) ? _ink : _paper, NULL);
            }
            const b8 _closed = (_h[_i].flags & FUDE_ZOOM_SYMBOL_CLOSED) != 0u;
            for(u32 _j = 0; _j + (_closed ? 0u : 1u) < _m; _j++) {
                rde_rendering_2d_draw_line_1(_poly[_j], _poly[(_j + 1u) % _m], _ink, 1.2f);
            }
        }
        // (its line short of the head)
        if(_end == 0u) { _b.x -= _trim; } else { _a.x += _trim; }
    }
    if(_heads & FUDE_ZOOM_ARROW_DOTTED) {
        // Its dashes 4 long, 8 apart, counted by index (an f64 stepped by 8 stops moving far from 0; a tile is a
        // screen's width at most, so a few hundred of them).
        const f64 _span = _b.x - _a.x;
        const u32 _dashes = _span > 0.0 && _span < 8.0 * 4096.0 ? (u32)rde_math_ceil_f64(_span / 8.0) : 0u;
        for(u32 _i = 0; _i < _dashes; _i++) {
            const f64 _x = _a.x + 8.0 * (f64)_i;
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x, (f32)_y }, (rde_vec_2F){ (f32)rde_math_min_f64(_x + 4.0, _b.x), (f32)_y }, _ink, 1.2f);
        }
    } else {
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_y }, (rde_vec_2F){ (f32)_b.x, (f32)_y }, _ink, 1.2f);
    }
    rde_arr_free(&_hp);
    rde_arr_free(&_hs);
}

// The panel, over the page (under the toolbar): its tabs, a tile a symbol of the family shown, its ×.
void fude_zoom_page_render_library(fude_zoom_page* _page) {
    if(!_page->library_open) {
        return;
    }
    fude_zoom_page_library_layout _l;
    fude_zoom_page_library_lay(_page, &_l);
    const fude_theme* _t = fude_theme_active();
    const rde_vec_2F _pc = { (f32)((_l.panel.min_x + _l.panel.max_x) * 0.5), (f32)((_l.panel.min_y + _l.panel.max_y) * 0.5) };
    rde_rendering_2d_draw_rounded_rectangle_with_border(_pc, (rde_vec_2F){ (f32)(_l.panel.max_x - _l.panel.min_x), (f32)(_l.panel.max_y - _l.panel.min_y) }, 0.04f, 8u,
                                                        _t->surface, 1.0f, _t->outline, NULL);
    // Its tabs.
    for(u32 _f = 0; _f < FUDE_ZOOM_PAGE_LIBRARY_TABS_N; _f++) {
        if(!fude_zoom_page_library_shows(_page, _f)) {
            continue;
        }
        const fude_zoom_box _b = _l.tabs[_f];
        const b8 _on = _f == _page->library_family;
        rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) },
                                                            (rde_vec_2F){ (f32)(_b.max_x - _b.min_x), (f32)(_b.max_y - _b.min_y) }, 0.5f, 8u,
                                                            _on ? _t->accent : _t->surface_2, 1.0f, _on ? _t->accent : _t->outline, NULL);
        const c8* _name = fude_text(FUDE_ZOOM_PAGE_LIBRARY_TABS[_f]);
        fude_zoom_render_text_in(&_page->renderer, _name, (u32)rde_strlen(_name), _b, 14.0, false, _on ? fude_kit_text_on(_t->accent) : _t->text);
    }
    // Its ×.
    {
        const fude_zoom_box _b = _l.close;
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        rde_rendering_2d_draw_circle_with_border(_c, 16.0f, 24u, _t->surface_2, 1.0f, _t->outline, NULL);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y - 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y + 5.0f }, _t->text, 1.6f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y + 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y - 5.0f }, _t->text, 1.6f);
    }
    // Its tiles (those wholly in the grid as it is scrolled). Custom's none yet: what to do to have some.
    const u32 _count = fude_zoom_page_library_count(_page, _page->library_family);
    if(_count == 0u && _page->library_family == FUDE_ZOOM_PAGE_LIBRARY_CUSTOM) {
        const c8* _none = fude_text(FUDE_TEXT_ZOOM_CUSTOM_EMPTY);
        fude_zoom_render_text_in(&_page->renderer, _none, (u32)rde_strlen(_none), (fude_zoom_box){ _l.grid.min_x + 12.0, _l.grid.min_y, _l.grid.max_x - 12.0, _l.grid.max_y },
                                 15.0, true, _t->text_soft);
    }
    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _k = fude_zoom_page_library_item(_page, _page->library_family, _i);
        const fude_zoom_box _tb = fude_zoom_page_library_box(_page, &_l, _i);
        const f64 _x0 = _tb.min_x, _y1 = _tb.max_y;
        if(_y1 > _l.grid.max_y + 0.5 || _y1 - FUDE_ZOOM_PAGE_LIB_TILE < _l.grid.min_y - 0.5) {
            continue;
        }
        if(_page->library_family == FUDE_ZOOM_PAGE_LIBRARY_CUSTOM) {
            fude_zoom_page_custom_tile(_page, _k, _tb);
            continue;
        }
        const fude_zoom_box _shape = { _x0 + 14.0, _y1 - 62.0, _x0 + FUDE_ZOOM_PAGE_LIB_TILE - 14.0, _y1 - 10.0 };
        const b8 _connectors = _page->library_family == FUDE_ZOOM_PAGE_LIBRARY_CONNECTORS;
        if(_connectors) {
            const u32 _heads = FUDE_ZOOM_PAGE_CONNECTOR_STYLES[_k].heads;
            if(_heads == _page->connector_heads) {
                rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)(_x0 + FUDE_ZOOM_PAGE_LIB_TILE * 0.5), (f32)(_y1 - FUDE_ZOOM_PAGE_LIB_TILE * 0.5) },
                                                                    (rde_vec_2F){ FUDE_ZOOM_PAGE_LIB_TILE - 6.0f, FUDE_ZOOM_PAGE_LIB_TILE - 6.0f }, 0.12f, 8u,
                                                                    (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);   // (the one new connectors are)
            }
            fude_zoom_page_draw_connector_in(_heads, _shape, _t->text, _t->page);
        } else {
            fude_zoom_page_draw_symbol_in(&_page->renderer, _k, _shape, _t->text, _t->page);
        }
        const c8* _name = fude_text(_connectors ? FUDE_ZOOM_PAGE_CONNECTOR_STYLES[_k].name : FUDE_ZOOM_PAGE_SYMBOL_NAMES[_k]);
        fude_zoom_render_text_in(&_page->renderer, _name, (u32)rde_strlen(_name), (fude_zoom_box){ _x0 + 3.0, _y1 - FUDE_ZOOM_PAGE_LIB_TILE + 2.0, _x0 + FUDE_ZOOM_PAGE_LIB_TILE - 3.0, _y1 - 66.0 },
                                 11.0, true, _t->text);
    }
    // The grid goes on: a hint of it at its foot.
    if(_page->library_scroll + (f64)(_l.grid.max_y - _l.grid.min_y) < (f64)_l.content - 1.0) {
        const rde_vec_2F _a = { (f32)((_l.grid.min_x + _l.grid.max_x) * 0.5 - 8.0), (f32)(_l.grid.min_y + 4.0) };
        rde_rendering_2d_draw_line_1(_a, (rde_vec_2F){ _a.x + 8.0f, _a.y - 6.0f }, _t->outline, 1.6f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _a.x + 8.0f, _a.y - 6.0f }, (rde_vec_2F){ _a.x + 16.0f, _a.y }, _t->outline, 1.6f);
    }
}

// A connector style chosen: new connectors are of it, and those the lasso holds become so (one undo step).
RDE_INTERNAL void fude_zoom_page_connector_style_choose(fude_zoom_page* _page, u32 _style) {
    if(_style >= FUDE_ZOOM_PAGE_CONNECTOR_STYLES_N) {
        return;
    }
    const u32 _heads = FUDE_ZOOM_PAGE_CONNECTOR_STYLES[_style].heads;
    _page->connector_heads = _heads;
    fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _dead = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap), _num = rde_arr_new(sizeof(f64), _heap);
    fude_zoom_box _box = fude_zoom_box_empty();
    u32 _frame = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        fude_zoom_pick* _p = &((fude_zoom_pick*)_page->selection.picks.memory)[_i];
        const fude_zoom_object _old = *fude_zoom_scene_object(_s, _p->object);
        if(!(_old.flags & FUDE_ZOOM_FLAG_ALIVE) || _old.kind != FUDE_ZOOM_KIND_SHAPE || _old.channels != FUDE_ZOOM_SHAPE_ARROW) {
            continue;
        }
        const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _p->object, &_num);
        f64* _n = (f64*)_num.memory;
        const u32 _k = fude_zoom_arrow_points(_n, _count);   // (0 for a damaged file's count: left as it is)
        if(_k < 2u || _count < 2u * _k + 2u) {
            continue;
        }
        _n[1u + 2u * _k] = (f64)_heads;
        const u16 _layer = _s->layer;
        _s->layer = _old.layer;
        _s->style = _old.channels == FUDE_ZOOM_SHAPE_SYMBOL || _old.kind != FUDE_ZOOM_KIND_SHAPE ? 0 : _old.q;   // (its own line, as its layer)
        const u32 _made = fude_zoom_scene_add_shape_fill(_s, _old.frame, fude_zoom_scene_place_of(_s, _p->object), FUDE_ZOOM_SHAPE_ARROW, _n, _count, _old.color, _old.radius,
                                                         (u8)(_old.flags & (FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN)), _old.fill, _old.z);
        _s->layer = _layer;
        _s->style = 0;
        fude_zoom_scene_set_alive(_s, _p->object, false);
        rde_arr_add(&_dead, (any)&_p->object);
        rde_arr_add(&_born, (any)&_made);
        _box   = fude_zoom_box_union(_box, _old.box);
        _frame = _old.frame;
        _p->object = _made;
    }
    if(rde_arr_length(&_born) > 0 && _frame != FUDE_ZOOM_NONE) {
        fude_zoom_history_push(_s, _frame, _box, (const u32*)_dead.memory, (u32)rde_arr_length(&_dead), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        fude_zoom_select_update(&_page->selection, _s);
    }
    rde_arr_free(&_dead);
    rde_arr_free(&_born);
    rde_arr_free(&_num);
    c8 _say[200];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_CONN_CHOSEN, FUDE_TS(fude_text(FUDE_ZOOM_PAGE_CONNECTOR_STYLES[_style].name)));
    fude_notice_show(_say);
}

// A hand down: on the panel, its own (a tap or a drag through the grid); off it, the panel goes. True: taken.
b8 fude_zoom_page_library_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(!_page->library_open) {
        return false;
    }
    fude_zoom_page_library_layout _l;
    fude_zoom_page_library_lay(_page, &_l);
    if(!fude_zoom_page_box_holds(_l.panel, _at)) {
        _page->library_open = false;   // (a press off it puts it away, and does nothing else)
        _page->library_hand = 0u;
        _page->library_gone = _hand;
        return true;
    }
    _page->library_hand    = _hand;
    _page->library_down    = _at;
    _page->library_from    = _page->library_scroll;
    _page->library_dragged = false;
    _page->library_down_at = rde_engine_get_time_now();
    return true;
}

// The hand on the library's panel moved: once it has gone 8 points up or down, the grid scrolls with it, no further
// than its rows go. A hand whose press put the panel away is still taken. True: taken.
b8 fude_zoom_page_library_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(_page->library_gone == _hand && _hand != 0u) {
        return true;   // (the press that put it away: its moves too)
    }
    if(!_page->library_open || _page->library_hand != _hand || _hand == 0u) {
        return false;
    }
    const f64 _dy = (f64)_at.y - (f64)_page->library_down.y;
    if(rde_math_abs_f64(_dy) > 8.0 || _page->library_dragged) {
        _page->library_dragged = true;
        fude_zoom_page_library_layout _l;
        fude_zoom_page_library_lay(_page, &_l);
        const f64 _most = rde_math_max_f64(0.0, (f64)_l.content - (_l.grid.max_y - _l.grid.min_y));
        _page->library_scroll = rde_math_min_f64(rde_math_max_f64(_page->library_from + _dy, 0.0), _most);   // (up: what is below comes up)
    }
    return true;
}

// The hand on the library's panel lifted: unless it dragged, what it pressed acts. The × closes the panel, a tab is
// chosen, a Custom tile's ⋯ opens its card, and any other tile closes the panel and is put down (a symbol), chosen (a
// connector style) or tapped (Custom's). True: taken.
b8 fude_zoom_page_library_up(fude_zoom_page* _page, u64 _hand) {
    if(_page->library_gone == _hand && _hand != 0u) {
        _page->library_gone = 0u;
        return true;
    }
    if(!_page->library_open || _page->library_hand != _hand || _hand == 0u) {
        return false;
    }
    _page->library_hand = 0u;
    if(_page->library_dragged) {
        return true;
    }
    fude_zoom_page_library_layout _l;
    fude_zoom_page_library_lay(_page, &_l);
    const rde_vec_2F _at = _page->library_down;
    if(fude_zoom_page_box_holds(_l.close, _at)) {
        _page->library_open = false;
        return true;
    }
    for(u32 _f = 0; _f < FUDE_ZOOM_PAGE_LIBRARY_TABS_N; _f++) {
        if(fude_zoom_page_library_shows(_page, _f) && fude_zoom_page_box_holds(_l.tabs[_f], _at)) {
            _page->library_family = (u8)_f;
            _page->library_scroll = 0.0;
            return true;
        }
    }
    fude_zoom_box _tb;
    const u32 _kind = fude_zoom_page_library_tile(_page, &_l, _at, &_tb);
    if(_kind != FUDE_ZOOM_NONE && _page->library_family == FUDE_ZOOM_PAGE_LIBRARY_CUSTOM) {
        // Its ⋯: its card (Edit or Inside, Rename, Delete); elsewhere: put down.
        if(fude_zoom_page_box_holds(fude_zoom_page_custom_more(_tb), _at)) {
            fude_zoom_page_custom_card(_page, _kind);
        } else {
            _page->library_open = false;
            fude_zoom_page_custom_tap(_page, _kind);
        }
    } else if(_kind != FUDE_ZOOM_NONE && _page->library_family == FUDE_ZOOM_PAGE_LIBRARY_CONNECTORS) {
        _page->library_open = false;
        fude_zoom_page_connector_style_choose(_page, _kind);   // new connectors so (and those the lasso holds)
    } else if(_kind != FUDE_ZOOM_NONE) {
        _page->library_open = false;
        fude_zoom_page_symbol_put(_page, _kind);
    }
    return true;
}

// --- Kanban boards: a column a line, sticky notes kept in them ---

#define FUDE_ZOOM_PAGE_KANBAN_MOST    10u   // columns at most

// A template's columns (kanbanform.h), a line each.
void fude_zoom_page_kanban_preset(u32 _which, c8* _out, usize _size) {
    const c8* _names[FUDE_ZOOM_KANBAN_ROWS];
    const u32 _n = fude_zoom_kanban_template(_which, _names);
    _out[0] = 0;
    usize _at = 0;
    for(u32 _i = 0; _i < _n && _at < _size; _i++) {
        _at += (usize)rde_snprintf(_out + _at, _size - _at, "%s%s", _i > 0u ? "\n" : "", _names[_i]);
    }
}

// The card's columns, a line each, made into the board.
RDE_INTERNAL void fude_zoom_page_kanban_done(any _self, const c8* const* _names, u32 _n) {
    c8    _text[FUDE_ZOOM_KANBAN_ROWS * FUDE_ZOOM_KANBAN_NAME];
    usize _at = 0;
    _text[0] = 0;
    for(u32 _i = 0; _i < _n && _at < sizeof(_text); _i++) {
        _at += (usize)rde_snprintf(_text + _at, sizeof(_text) - _at, "%s%s", _i > 0u ? "\n" : "", _names[_i]);
    }
    fude_zoom_page_kanban_commit((fude_zoom_page*)_self, _text);
}

// Insert's Kanban board: its columns asked for on their card (kanbanform.h: a row each, the templates over them).
void fude_zoom_page_kanban_card(fude_zoom_page* _page) {
    fude_zoom_kanban_open(&_page->kanban_form, fude_zoom_page_kanban_done, _page);
}

// The board: a column a line (a COLUMN symbol each, its name in its header), side by side in the middle
// of the view; one undo step, the lasso on them.
void fude_zoom_page_kanban_commit(fude_zoom_page* _page, const c8* _text) {
    c8  _names[FUDE_ZOOM_PAGE_KANBAN_MOST][128];
    u32 _n = 0;
    for(const c8* _c = _text; *_c != 0 && _n < FUDE_ZOOM_PAGE_KANBAN_MOST;) {
        const c8* _end = _c;
        while(*_end != 0 && *_end != '\n' && *_end != '\r') {
            _end++;
        }
        const c8* _a = _c;
        while(_a < _end && *_a == ' ') { _a++; }
        const c8* _b = _end;
        while(_b > _a && _b[-1] == ' ') { _b--; }
        if(_b > _a) {
            usize _len = (usize)(_b - _a) < sizeof(_names[0]) - 1u ? (usize)(_b - _a) : sizeof(_names[0]) - 1u;
            while(_len > 0 && ((u8)_a[_len] & 0xC0u) == 0x80u && (usize)(_b - _a) > _len) {
                _len--;   // (cut at a whole character)
            }
            rde_memcpy(_names[_n], _a, _len);
            _names[_n][_len] = 0;
            _n++;
        }
        _c = *_end != 0 ? _end + 1 : _end;
    }
    if(_n == 0u) {
        return;
    }
    fude_zoom_scene* _s    = &_page->scene;
    const f64        _near = 4.0 / _s->camera.z;
    const u32 _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
    const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _frame);
    const f64 _k = rde_math_max_f64(fude_zoom_sim_scale(_fs), 1e-300);
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(_fs), (fude_zoom_v2){ 0.0, 0.0 });
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(fude_zoom_symbol_find("column"));
    const f64 _cw = (f64)_info->w / _k, _ch = (f64)_info->h / _k, _gap = 18.0 / _k;
    const f64 _left = _mid.x - ((f64)_n * _cw + (f64)(_n - 1u) * _gap) * 0.5 + _cw * 0.5;
    u32 _made[FUDE_ZOOM_PAGE_KANBAN_MOST];
    fude_zoom_box _box = fude_zoom_box_empty();
    fude_zoom_select_clear(&_page->selection);
    for(u32 _i = 0; _i < _n; _i++) {
        f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _count = fude_zoom_symbol_numbers(_num, fude_zoom_symbol_find("column"), _cw * 0.5, _ch * 0.5, 17.0 / _k, _names[_i]);
        _made[_i] = fude_zoom_scene_add_shape_fill(_s, _frame, (fude_zoom_place){ { _left + (f64)_i * (_cw + _gap), _mid.y }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, _num, _count,
                                                   _page->app->ink->color, (f32)(1.0 / _k), FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, FUDE_THEME_PAGE_FILL, 0);
        _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, _made[_i])->box);
        rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made[_i] });
    }
    fude_zoom_history_push(_s, _frame, _box, NULL, 0, _made, _n);
    fude_zoom_select_update(&_page->selection, _s);
}

// Sticky notes let go in a column (a lasso's move): those in it lined up under its header in their order
// down it, a little apart, in its middle. One undo step after the move's.
void fude_zoom_page_kanban_tidy(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _cols = rde_arr_new(sizeof(u32), _heap);
    // The columns the moved notes are in.
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        const fude_zoom_object* _note = fude_zoom_scene_object(_s, _o);
        u8 _style = 0;
        if(!(_note->flags & FUDE_ZOOM_FLAG_ALIVE) || _note->kind != FUDE_ZOOM_KIND_TEXT ||
           !fude_zoom_scene_text(_s, _o, NULL, NULL, NULL, &_style, NULL, NULL) || _style != FUDE_ZOOM_TEXT_STICKY) {
            continue;
        }
        const fude_zoom_v2 _c = { (_note->box.min_x + _note->box.max_x) * 0.5, (_note->box.min_y + _note->box.max_y) * 0.5 };
        rde_arr _found = rde_arr_new(sizeof(u32), _heap);
        fude_zoom_scene_query(_s, _note->frame, (fude_zoom_box){ _c.x, _c.y, _c.x, _c.y }, &_found);
        for(u32 _f = 0; _f < (u32)rde_arr_length(&_found); _f++) {
            const u32 _col = ((const u32*)_found.memory)[_f];
            const fude_zoom_object* _co = fude_zoom_scene_object(_s, _col);
            f64 _cn[1];
            if((_co->flags & FUDE_ZOOM_FLAG_ALIVE) && _co->kind == FUDE_ZOOM_KIND_SHAPE && _co->channels == FUDE_ZOOM_SHAPE_SYMBOL &&
               fude_zoom_scene_shape_numbers(_s, _col, _cn, 1u) == 1u && fude_zoom_symbol_kind_of(_cn, 1u) == fude_zoom_symbol_find("column")) {
                b8 _known = false;
                for(u32 _k = 0; _k < (u32)rde_arr_length(&_cols); _k++) {
                    _known = _known || ((const u32*)_cols.memory)[_k] == _col;
                }
                if(!_known) {
                    rde_arr_add(&_cols, (any)&_col);
                }
            }
        }
        rde_arr_free(&_found);
    }
    rde_arr _ids = rde_arr_new(sizeof(u32), _heap), _before = rde_arr_new(sizeof(fude_zoom_place), _heap), _after = rde_arr_new(sizeof(fude_zoom_place), _heap);
    fude_zoom_box _all = fude_zoom_box_empty();
    u32 _frame = FUDE_ZOOM_NONE;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_cols); _k++) {
        const u32 _col = ((const u32*)_cols.memory)[_k];
        const fude_zoom_object* _co = fude_zoom_scene_object(_s, _col);
        const fude_zoom_box _cb = _co->box;
        f64 _cn[3];
        fude_zoom_scene_shape_numbers(_s, _col, _cn, 3u);
        const f64 _band = rde_math_min_f64(_cn[2] * 0.32, 44.0) * _co->scale;   // (its header: symbol.c's)
        const f64 _gap  = rde_math_max_f64((_cb.max_x - _cb.min_x) * 0.05, 1e-9);
        // Its notes, top down.
        rde_arr _found = rde_arr_new(sizeof(u32), _heap), _notes = rde_arr_new(sizeof(u32), _heap);
        fude_zoom_scene_query(_s, _co->frame, _cb, &_found);
        for(u32 _f = 0; _f < (u32)rde_arr_length(&_found); _f++) {
            const u32 _o = ((const u32*)_found.memory)[_f];
            const fude_zoom_object* _note = fude_zoom_scene_object(_s, _o);
            u8 _style = 0;
            const fude_zoom_v2 _c = { (_note->box.min_x + _note->box.max_x) * 0.5, (_note->box.min_y + _note->box.max_y) * 0.5 };
            if((_note->flags & FUDE_ZOOM_FLAG_ALIVE) && _note->kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_scene_text(_s, _o, NULL, NULL, NULL, &_style, NULL, NULL) &&
               _style == FUDE_ZOOM_TEXT_STICKY && _c.x >= _cb.min_x && _c.x <= _cb.max_x && _c.y >= _cb.min_y && _c.y <= _cb.max_y) {
                rde_arr_add(&_notes, (any)&_o);
            }
        }
        u32* _ns = (u32*)_notes.memory;
        const u32 _nn = (u32)rde_arr_length(&_notes);
        for(u32 _a = 1; _a < _nn; _a++) {   // (by their tops, highest first: few)
            for(u32 _b = _a; _b > 0 && fude_zoom_scene_object(_s, _ns[_b])->box.max_y > fude_zoom_scene_object(_s, _ns[_b - 1u])->box.max_y; _b--) {
                const u32 _t = _ns[_b]; _ns[_b] = _ns[_b - 1u]; _ns[_b - 1u] = _t;
            }
        }
        f64 _top = _cb.max_y - _band - _gap;
        const f64 _cx = (_cb.min_x + _cb.max_x) * 0.5;
        for(u32 _i = 0; _i < _nn; _i++) {
            const fude_zoom_box _nb = fude_zoom_scene_object(_s, _ns[_i])->box;
            const fude_zoom_v2 _d = { _cx - (_nb.min_x + _nb.max_x) * 0.5, _top - _nb.max_y };
            _top -= (_nb.max_y - _nb.min_y) + _gap;
            if(rde_math_abs_f64(_d.x) + rde_math_abs_f64(_d.y) < 1e-9) {
                continue;
            }
            const fude_zoom_place _was = fude_zoom_scene_place_of(_s, _ns[_i]);
            const fude_zoom_place _now = fude_zoom_place_moved(_was, (fude_zoom_sim){ 1.0, 0.0, _d.x, _d.y });
            fude_zoom_scene_set_place(_s, _ns[_i], _now);
            rde_arr_add(&_ids, (any)&_ns[_i]);
            rde_arr_add(&_before, (any)&_was);
            rde_arr_add(&_after, (any)&_now);
            _all = fude_zoom_box_union(_all, _nb);
            _frame = _co->frame;
        }
        rde_arr_free(&_found);
        rde_arr_free(&_notes);
    }
    if(rde_arr_length(&_ids) > 0 && _frame != FUDE_ZOOM_NONE) {
        fude_zoom_history_push_moved(_s, _frame, _all, (const u32*)_ids.memory, (const fude_zoom_place*)_before.memory, (const fude_zoom_place*)_after.memory,
                                     (u32)rde_arr_length(&_ids));
        fude_zoom_select_update(&_page->selection, _s);
    }
    rde_arr_free(&_ids);
    rde_arr_free(&_before);
    rde_arr_free(&_after);
    rde_arr_free(&_cols);
}
