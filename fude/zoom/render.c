// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/render_parts.h"

// ===========================================================================
// See render.h.
//
// render.c is the coordinator of the renderer: the renderer made, trimmed and freed with its caches (texts' layouts,
// graphs' lines, the kept geometry of symbols, shapes and cut boards, decoded strokes); pictures (their textures kept
// by their blob) and a picture drawn; frames and the backdrop; the draw itself (fude_zoom_render); then one object
// alone, the glow, and the frames tools may edit. Each kind of object is drawn by its own module: strokes and fills
// (render_stroke.c), shapes (render_shape.c), texts (render_text.c), symbols (render_symbol.c); render_parts.h is what
// they share and says which module holds what.
//
// Most drawing functions take _as and _extra: _as, when not NULL, draws the whole object in that colour (the
// selection's glow, or a faint thing), _extra points wider on each side.
//
// Part of the renderer: render_parts.h holds what the modules share and says which module holds what. Internal names
// start fude_zoom_render_ (fude_zoom_draw_ and fude_zoom_clip_ for the clipping). Main thread only, inside the app's 2D
// drawing block, as render.h says.
// ===========================================================================

#define FUDE_ZOOM_RENDER_F64_EPS 2.220446049250313e-16   // f64's epsilon, 2^-52

// An object to draw and its draw key (scene.h's fude_zoom_scene_draw_key), sorted by it.
typedef struct {
    u64 z;
    u32 object;
} fude_zoom_render_item;

// The texts' layouts let go: each slot's (and the spare's) arrays freed, made again when it is next laid out into.
void fude_zoom_render_rich_forget(fude_zoom_renderer* _r) {
    if(!rde_arr_is_inited(&_r->rich)) {
        return;
    }
    fude_zoom_render_rich_kept* _k = (fude_zoom_render_rich_kept*)_r->rich.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->rich); _i++) {
        if(rde_arr_is_inited(&_k[_i].rich.runs)) {
            fude_zoom_rich_free(&_k[_i].rich);
        }
        if(rde_arr_is_inited(&_k[_i].plain_spans)) {
            rde_arr_free(&_k[_i].plain_spans);
        }
        rde_memset(&_k[_i], 0, sizeof(_k[_i]));
    }
    rde_lru_clear(&_r->rich_lru);
}

#define FUDE_ZOOM_RENDER_PLOTS 24u   // graphs' lines kept (past it: the one drawn longest ago, or the spare, made into)

// See the header. The kept geometry's, graphs' lines' and texts' layouts' slots are made here, all of them and the
// spare after them (zeroed: their arrays are made when first used), so a pointer to one never moves.
void fude_zoom_render_init(fude_zoom_renderer* _r) {
    rde_memset(_r, 0, sizeof(*_r));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _r->visible      = rde_arr_new(sizeof(fude_zoom_visible), _heap);
    _r->cache        = rde_arr_new(sizeof(fude_zoom_decoded), _heap);
    _r->slot_of      = rde_arr_new(sizeof(u32), _heap);
    _r->found        = rde_arr_new(sizeof(u32), _heap);
    _r->screen       = rde_arr_new(sizeof(rde_vec_2F), _heap);
    _r->radii        = rde_arr_new(sizeof(f32), _heap);
    _r->q            = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    _r->shape        = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _r->fill         = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _r->cut          = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _r->shape_screen = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _r->shape_radii  = rde_arr_new(sizeof(f64), _heap);
    _r->numbers      = rde_arr_new(sizeof(f64), _heap);
    _r->ring_ids     = rde_arr_new(sizeof(u32), _heap);
    _r->items        = rde_arr_new(sizeof(fude_zoom_render_item), _heap);
    _r->mm_per_unit  = 1.0;
    _r->units        = (fude_zoom_units_style){ FUDE_ZOOM_UNIT_MM, 1u, 16u, false };
    _r->drawing      = FUDE_ZOOM_NONE;
    _r->skip         = FUDE_ZOOM_NONE;
    _r->pictures     = rde_arr_new(sizeof(fude_zoom_picture), _heap);
    _r->picture_of   = rde_hash_map_new(sizeof(u64), sizeof(u32), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, _heap);
    _r->cache_budget = FUDE_ZOOM_RENDER_CACHE;
    _r->mesh_budget  = FUDE_ZOOM_RENDER_MESHES;
    _r->mesh_scratch = rde_stroke_mesh_new(_heap);
    _r->geos         = rde_arr_new(sizeof(fude_zoom_geo), _heap);
    rde_arr_resize(&_r->geos, FUDE_ZOOM_RENDER_GEOS + 1u);
    _r->geo_lru      = rde_lru_new(FUDE_ZOOM_RENDER_GEOS, _heap);
    _r->plots        = rde_arr_new(sizeof(fude_zoom_plot_made), _heap);
    rde_arr_resize(&_r->plots, FUDE_ZOOM_RENDER_PLOTS + 1u);
    _r->plot_lru     = rde_lru_new(FUDE_ZOOM_RENDER_PLOTS, _heap);
    _r->plot_key     = rde_arr_new(sizeof(u8), _heap);
    _r->rich         = rde_arr_new(sizeof(fude_zoom_render_rich_kept), _heap);
    rde_arr_resize(&_r->rich, FUDE_ZOOM_RENDER_RICH + 1u);
    _r->rich_lru     = rde_lru_new(FUDE_ZOOM_RENDER_RICH, _heap);
    _r->rich_key     = rde_arr_new(sizeof(u8), _heap);
    _r->link_color   = (rde_color){ 70, 130, 230, 255 };
}

// The graphs' lines let go: each slot's (and the spare's) arrays freed, made again when it is next made into.
RDE_INTERNAL void fude_zoom_render_plots_forget(fude_zoom_renderer* _r) {
    if(!rde_arr_is_inited(&_r->plots)) {
        return;
    }
    fude_zoom_plot_made* _m = (fude_zoom_plot_made*)_r->plots.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->plots); _i++) {
        if(rde_arr_is_inited(&_m[_i].points)) {
            rde_arr_free(&_m[_i].points);
            rde_arr_free(&_m[_i].parts);
            rde_arr_free(&_m[_i].labels);
        }
        rde_memset(&_m[_i], 0, sizeof(_m[_i]));
    }
    rde_lru_clear(&_r->plot_lru);
}

// A graph's lines for its kind _id, text _text and half sizes: as kept, or made now (good until the next is made). The
// key is the whole of what they are made from, so a graph is never drawn with another's lines. New lines go into a
// free slot or the one drawn longest ago (its arrays reused); when that one was drawn in this draw too (more graphs on
// the screen than FUDE_ZOOM_RENDER_PLOTS), into the spare instead, so the graphs kept stay kept and only those past the
// bound are made again each draw (all of them were once, each pushing out the next).
const fude_zoom_plot_made* fude_zoom_render_plot_made(fude_zoom_renderer* _r, const c8* _id, const c8* _text, f64 _hw, f64 _hh) {
    const usize _id_len   = rde_strlen(_id);
    const usize _text_len = rde_strlen(_text);
    const usize _key_len  = _id_len + 1u + _text_len + 2u * sizeof(f64);
    rde_arr_clear(&_r->plot_key);
    u8* _key = (u8*)rde_arr_add_n(&_r->plot_key, _key_len);
    rde_memcpy(_key, _id, _id_len);
    _key[_id_len] = 0x1F;
    rde_memcpy(_key + _id_len + 1u, _text, _text_len);
    rde_memcpy(_key + _id_len + 1u + _text_len, &_hw, sizeof(f64));
    rde_memcpy(_key + _id_len + 1u + _text_len + sizeof(f64), &_hh, sizeof(f64));
    fude_zoom_plot_made* _all  = (fude_zoom_plot_made*)_r->plots.memory;
    u32                  _slot = rde_lru_find(&_r->plot_lru, _key, _key_len);
    if(_slot != RDE_LRU_NONE) {
        _all[_slot].used = _r->draws;
        return &_all[_slot];
    }
    const u32 _oldest = rde_lru_oldest(&_r->plot_lru);
    _slot = FUDE_ZOOM_RENDER_PLOTS;   // (the spare)
    if(rde_lru_length(&_r->plot_lru) < FUDE_ZOOM_RENDER_PLOTS || _all[_oldest].used != _r->draws) {
        _slot = rde_lru_add(&_r->plot_lru, _key, _key_len, NULL);
    }
    fude_zoom_plot_made* _to = &_all[_slot];
    fude_zoom_render_fresh(&_to->points, sizeof(fude_zoom_v2));
    fude_zoom_render_fresh(&_to->parts, sizeof(fude_zoom_symbol_part));
    fude_zoom_render_fresh(&_to->labels, sizeof(fude_zoom_plot_label));
    _to->used = _r->draws;
    fude_zoom_plot_lines_play(_id, _text, _hw, _hh, NULL, &_to->points, &_to->parts, &_to->labels);
    return _to;
}

// --- kept geometry (render.h's fude_zoom_geo) ------------------------------------------------------------------------

// Symbols', shapes' and cut boards' geometry share one cache, each kept by the whole of what it is made from
// (render_parts.h's fude_zoom_render_geo_key), up to FUDE_ZOOM_RENDER_GEOS of them holding FUDE_ZOOM_RENDER_GEO_POINTS
// points and triangle corners in all. Past either, the one used longest ago is let go, never one used in the current
// draw: what does not fit then is made into the spare and drawn from there (made again each draw), while the rest stay
// kept. Forgetting everything at once instead (as symbols' geometry once was) made the whole screen again each time.
// A payload's bytes never change (an edit makes a new payload, with a new serial), so a key never finds stale geometry.

// See render_parts.h.
void fude_zoom_render_fresh(rde_arr* _arr, usize _size) {
    if(rde_arr_is_inited(_arr)) {
        rde_arr_clear(_arr);
    } else {
        *_arr = rde_arr_new(_size, rde_memory_allocator_get_default_std());
    }
}

// Frees what geometry _g holds and empties it (its arrays are made again when it is next made into).
RDE_INTERNAL void fude_zoom_render_geo_free(fude_zoom_geo* _g) {
    rde_arr* _arrays[] = { &_g->points, &_g->parts, &_g->rings, &_g->tris, &_g->tri_range };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    rde_memset(_g, 0, sizeof(*_g));
}

// Lets the geometry in slot _slot go: its arrays freed, its points off the budget, its key forgotten.
RDE_INTERNAL void fude_zoom_render_geo_let_go(fude_zoom_renderer* _r, u32 _slot) {
    fude_zoom_geo* _g = &((fude_zoom_geo*)_r->geos.memory)[_slot];
    _r->geo_points -= _g->cost;
    fude_zoom_render_geo_free(_g);
    rde_lru_remove_slot(&_r->geo_lru, _slot);
}

// Lets the geometry used longest ago go while _more points and corners would pass the budget, or, with _slot, while
// no slot is free; never what was used in this draw. Returns whether there is room now.
RDE_INTERNAL b8 fude_zoom_render_geo_room(fude_zoom_renderer* _r, u64 _more, b8 _slot) {
    while((_slot && rde_lru_length(&_r->geo_lru) >= FUDE_ZOOM_RENDER_GEOS) || _r->geo_points + _more > FUDE_ZOOM_RENDER_GEO_POINTS) {
        const u32 _oldest = rde_lru_oldest(&_r->geo_lru);
        if(_oldest == RDE_LRU_NONE || ((const fude_zoom_geo*)_r->geos.memory)[_oldest].used == _r->draws) {
            return false;
        }
        fude_zoom_render_geo_let_go(_r, _oldest);
    }
    return true;
}

// See render_parts.h.
void fude_zoom_render_geo_forget(fude_zoom_renderer* _r) {
    if(!rde_arr_is_inited(&_r->geos)) {
        return;
    }
    fude_zoom_geo* _g = (fude_zoom_geo*)_r->geos.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->geos); _i++) {
        fude_zoom_render_geo_free(&_g[_i]);
    }
    rde_lru_clear(&_r->geo_lru);
    _r->geo_points = 0;
}

// See render_parts.h.
fude_zoom_geo* fude_zoom_render_geo_find(fude_zoom_renderer* _r, const fude_zoom_render_geo_key* _key) {
    const u32 _slot = rde_lru_find(&_r->geo_lru, (const any)_key, sizeof(*_key));
    if(_slot == RDE_LRU_NONE) {
        return NULL;
    }
    fude_zoom_geo* _g = &((fude_zoom_geo*)_r->geos.memory)[_slot];
    _g->used = _r->draws;
    return _g;
}

// See render_parts.h. What the spare held is freed first, so new geometry has only the arrays its maker makes (usually
// it holds none: its last geometry moved into a slot, arrays and all).
fude_zoom_geo* fude_zoom_render_geo_begin(fude_zoom_renderer* _r) {
    fude_zoom_geo* _g = &((fude_zoom_geo*)_r->geos.memory)[FUDE_ZOOM_RENDER_GEOS];
    fude_zoom_render_geo_free(_g);
    _g->used = _r->draws;
    return _g;
}

// See render_parts.h. The slot it moves into is empty (let go, or never used); the spare is left empty in its place.
fude_zoom_geo* fude_zoom_render_geo_keep(fude_zoom_renderer* _r, const fude_zoom_render_geo_key* _key) {
    fude_zoom_geo* _all  = (fude_zoom_geo*)_r->geos.memory;
    fude_zoom_geo* _made = &_all[FUDE_ZOOM_RENDER_GEOS];
    _made->used = _r->draws;
    _made->cost = (rde_arr_is_inited(&_made->points) ? (u64)rde_arr_length(&_made->points) : 0u) +
                  (rde_arr_is_inited(&_made->tris) ? (u64)rde_arr_length(&_made->tris) : 0u);
    if(!fude_zoom_render_geo_room(_r, _made->cost, true)) {
        return _made;   // (no room in this draw: drawn from the spare, and made again the next time)
    }
    const u32 _slot = rde_lru_add(&_r->geo_lru, (const any)_key, sizeof(*_key), NULL);
    _all[_slot] = *_made;
    rde_memset(_made, 0, sizeof(*_made));
    _r->geo_points += _all[_slot].cost;
    return &_all[_slot];
}

// See render_parts.h. Kept geometry made larger may pass the budget: the oldest not used in this draw then go.
u32 fude_zoom_render_geo_tris(fude_zoom_renderer* _r, fude_zoom_geo* _g) {
    if(!_g->triangulated) {
        fude_zoom_render_fresh(&_g->tris, sizeof(fude_zoom_v2));
        const u32* _rings = rde_arr_is_inited(&_g->rings) ? (const u32*)_g->rings.memory : NULL;
        fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)_g->points.memory, _rings, (u32)rde_arr_length(&_g->points), &_g->tris);
        _g->triangulated = true;
        const u64 _corners = (u64)rde_arr_length(&_g->tris);
        _g->cost += _corners;
        if(_g != &((fude_zoom_geo*)_r->geos.memory)[FUDE_ZOOM_RENDER_GEOS]) {
            _r->geo_points += _corners;
            fude_zoom_render_geo_room(_r, 0u, false);
        }
    }
    return (u32)(rde_arr_length(&_g->tris) / 3u);
}

// A decoded stroke's arrays let go, its mesh's too (the caller takes its bytes off the budgets).
RDE_INTERNAL void fude_zoom_render_let_go(fude_zoom_decoded* _d) {
    rde_arr* _arrays[] = { &_d->xy, &_d->radius, &_d->tris, &_d->rings, &_d->edges, &_d->edge_lines };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    rde_stroke_mesh_free(&_d->mesh);
    rde_rendering_2d_release_kept_stroke_mesh(&_d->mesh_gpu);   // (its GPU copy too)
}

// See the header.
void fude_zoom_render_trim(fude_zoom_renderer* _r) {
    // The pictures off the GPU too: they come back from their bytes when seen.
    fude_zoom_picture* _p = (fude_zoom_picture*)_r->pictures.memory;
    for(u32 _i = 0; rde_arr_is_inited(&_r->pictures) && _i < (u32)rde_arr_length(&_r->pictures); _i++) {
        if(_p[_i].texture != NULL) {
            rde_texture_unload(_p[_i].texture);
        }
    }
    if(rde_arr_is_inited(&_r->pictures)) {
        rde_arr_clear(&_r->pictures);
        rde_hash_map_clear(&_r->picture_of);
    }
    _r->picture_bytes = 0;
    fude_zoom_decoded* _d = (fude_zoom_decoded*)_r->cache.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->cache); _i++) {
        fude_zoom_render_let_go(&_d[_i]);
    }
    rde_arr_clear(&_r->cache);
    rde_arr_clear(&_r->slot_of);
    _r->cache_bytes = 0;
    _r->mesh_bytes  = 0;
    _r->mesh_full   = 0;
    fude_zoom_render_geo_forget(_r);
    fude_zoom_render_plots_forget(_r);
    fude_zoom_render_rich_forget(_r);
}

// See the header.
void fude_zoom_render_destroy(fude_zoom_renderer* _r) {
    if(rde_arr_is_inited(&_r->cache)) {
        fude_zoom_render_trim(_r);
    }
    fude_zoom_render_plots_forget(_r);
    fude_zoom_render_rich_forget(_r);
    fude_zoom_render_geo_forget(_r);
    rde_lru_free(&_r->plot_lru);
    rde_lru_free(&_r->geo_lru);
    rde_lru_free(&_r->rich_lru);
    rde_arr* _arrays[] = { &_r->visible, &_r->cache, &_r->slot_of, &_r->found, &_r->screen, &_r->radii, &_r->q, &_r->shape, &_r->pictures, &_r->fill, &_r->cut, &_r->shape_screen, &_r->shape_radii,
                           &_r->numbers, &_r->ring_ids, &_r->items, &_r->geos, &_r->plots, &_r->plot_key, &_r->rich, &_r->rich_key };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    if(_r->picture_of.capacity > 0u) {
        rde_hash_map_free(&_r->picture_of);
    }
    rde_stroke_mesh_free(&_r->mesh_scratch);
    rde_memset(_r, 0, sizeof(*_r));
}

// How far off, in screen units, an f64 transform with this translation can be.
RDE_INTERNAL f64 fude_zoom_render_error(fude_zoom_sim _s) {
    return (rde_math_abs_f64(_s.tx) + rde_math_abs_f64(_s.ty)) * FUDE_ZOOM_RENDER_F64_EPS;
}

// --- decoded strokes ------------------------------------------------------------------------

// Lets decoded entry _entry go, its bytes taken off the budget (its mesh's off theirs); the last entry is
// moved into its place.
RDE_INTERNAL void fude_zoom_render_evict(fude_zoom_renderer* _r, u32 _entry) {
    fude_zoom_decoded* _d    = (fude_zoom_decoded*)_r->cache.memory;
    u32*               _slot = (u32*)_r->slot_of.memory;
    _r->cache_bytes -= (u64)_d[_entry].count * 12u + (u64)_d[_entry].tri_count * 24u + (u64)_d[_entry].edge_count * 12u;
    _r->mesh_bytes  -= _d[_entry].mesh_bytes;
    fude_zoom_render_let_go(&_d[_entry]);
    _slot[_d[_entry].object] = 0;
    const u32 _last = (u32)rde_arr_length(&_r->cache) - 1u;
    if(_entry != _last) {
        _d[_entry] = _d[_last];
        _slot[_d[_entry].object] = _entry + 1u;
    }
    rde_arr_remove(&_r->cache, _last);
}

// Returns object _object's points decoded for drawing (a fill's rings too), from the cache or decoded now and kept;
// NULL when its bytes cannot be read or it has no points (nothing to draw, and no room of 0 to ask rde_arr_add_n
// for). The pointer is good until the next call that decodes, which may move the cache.
const fude_zoom_decoded* fude_zoom_render_decoded(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object) {
    while((u32)rde_arr_length(&_r->slot_of) <= _object) {
        const u32 _grow = fude_zoom_scene_object_count(_s) - (u32)rde_arr_length(&_r->slot_of);
        rde_memset(rde_arr_add_n(&_r->slot_of, _grow > 0 ? _grow : 1u), 0, (usize)(_grow > 0 ? _grow : 1u) * sizeof(u32));
    }
    u32* _slot = (u32*)_r->slot_of.memory;
    if(_slot[_object] != 0) {
        fude_zoom_decoded* _d = &((fude_zoom_decoded*)_r->cache.memory)[_slot[_object] - 1u];
        _d->used = _r->draws;
        return _d;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(_o->count == 0u) {
        return NULL;
    }
    rde_arr_clear(&_r->q);
    rde_arr_add_n(&_r->q, _o->count);
    if(!fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_r->q.memory)) {
        return NULL;
    }
    // Over the budget: the least recently drawn go (never what this draw used), found by their draw stamps. Not an
    // rde_lru: its slots are a fixed count made up front, and this cache is bounded by bytes, its entries from a
    // dot's few bytes to a long stroke's megabytes, so a count would either reserve room for the most strokes a screen
    // can show or throw out what still fits. A stroke is found directly (slot_of, by its object); the scan for the
    // oldest is only made when keeping a new one would pass the budget.
    while(_r->cache_bytes + (u64)_o->count * 12u > _r->cache_budget && rde_arr_length(&_r->cache) > 0) {
        const fude_zoom_decoded* _d     = (const fude_zoom_decoded*)_r->cache.memory;
        u32                      _worst = UINT32_MAX;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->cache); _i++) {
            if(_d[_i].used != _r->draws && (_worst == UINT32_MAX || _d[_i].used < _d[_worst].used)) {
                _worst = _i;
            }
        }
        if(_worst == UINT32_MAX) {
            break;
        }
        fude_zoom_render_evict(_r, _worst);
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_decoded _e = {
        .object = _object, .count = _o->count, .used = _r->draws,
        .xy = rde_arr_new(sizeof(f32), _heap),
        .radius = rde_arr_new(sizeof(f32), _heap),
    };
    rde_arr_resize(&_e.xy, (usize)_o->count * 2u);
    rde_arr_resize(&_e.radius, _o->count);
    f32* _xy  = (f32*)_e.xy.memory;
    f32* _rad = (f32*)_e.radius.memory;
    const fude_zoom_qpoint* _q = (const fude_zoom_qpoint*)_r->q.memory;
    for(u32 _i = 0; _i < _o->count; _i++) {
        _xy[_i * 2u]      = (f32)rde_math_ldexp_f64((f64)_q[_i].x, _o->q);
        _xy[_i * 2u + 1u] = (f32)rde_math_ldexp_f64((f64)_q[_i].y, _o->q);
        _rad[_i]          = fude_zoom_scene_local_radius_at(_o, &_q[_i]);   // before the object's scale: that can change
    }
    if(_o->kind == FUDE_ZOOM_KIND_FILL && (_o->channels & FUDE_ZOOM_CHANNEL_TIME)) {
        _e.rings = rde_arr_new(sizeof(u32), _heap);
        rde_arr_resize(&_e.rings, _o->count);
        u32* _rings = (u32*)_e.rings.memory;
        for(u32 _i = 0; _i < _o->count; _i++) {
            _rings[_i] = _q[_i].time;
        }
    }
    rde_arr_add(&_r->cache, &_e);
    _r->cache_bytes += (u64)_o->count * 12u;
    _slot = (u32*)_r->slot_of.memory;
    _slot[_object] = (u32)rde_arr_length(&_r->cache);
    return &((fude_zoom_decoded*)_r->cache.memory)[rde_arr_length(&_r->cache) - 1u];
}

#define FUDE_ZOOM_RENDER_FAINT 22u   // a faint thing's alpha, of its own (per cent)

// --- pictures --------------------------------------------------------------------------------

// See the header. Textures are kept by their payload's blob (its slot and serial), so copies of a picture share one;
// one that could not be read is kept as NULL and not tried again. A picture is found through picture_of (its payload's
// handle and serial, the pair a payload is known by: a freed slot is given out again with a new serial), kept as the
// pictures move. Past FUDE_ZOOM_RENDER_TEXTURES, the least recently drawn go first, found by their draw stamps (bounded
// by bytes, as the decoded strokes are: see fude_zoom_render_decoded for why not an rde_lru; the scan is made only
// when a new picture would pass the budget).
rde_texture* fude_zoom_render_picture(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, rde_vec_2UI* _size) {
    const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
    const u32               _serial = fude_zoom_blob_serial(_s, _o->blob);
    const u64               _key    = ((u64)_o->blob << 32) | (u64)_serial;
    const u32*              _at     = (const u32*)rde_hash_map_find(&_r->picture_of, (const any)&_key);
    if(_at != NULL) {
        fude_zoom_picture* _p = &((fude_zoom_picture*)_r->pictures.memory)[*_at];
        _p->used = _r->draws;
        if(_size != NULL && _p->texture != NULL) {
            *_size = rde_texture_get_size(_p->texture);
        }
        return _p->texture;
    }
    f64 _hw, _hh;
    const u8* _bytes = NULL;
    u32       _n     = 0;
    if(!fude_zoom_scene_image(_s, _object, &_hw, &_hh, &_bytes, &_n)) {
        return NULL;
    }
    rde_texture* _t = NULL;
    RDE_TRY({ _t = rde_texture_load_from_memory(_bytes, _n, "picture", NULL, NULL); });
    const rde_vec_2UI _px = _t != NULL ? rde_texture_get_size(_t) : (rde_vec_2UI){ 0, 0 };
    const u64 _cost = (u64)_px.x * (u64)_px.y * 4u;
    // Over the budget: the least recently drawn off the GPU (never this draw's), the last moved into its place.
    while(_r->picture_bytes + _cost > FUDE_ZOOM_RENDER_TEXTURES && rde_arr_length(&_r->pictures) > 0) {
        fude_zoom_picture* _p = (fude_zoom_picture*)_r->pictures.memory;
        u32 _worst = UINT32_MAX;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->pictures); _i++) {
            if(_p[_i].used != _r->draws && (_worst == UINT32_MAX || _p[_i].used < _p[_worst].used)) {
                _worst = _i;
            }
        }
        if(_worst == UINT32_MAX) {
            break;
        }
        if(_p[_worst].texture != NULL) {
            rde_texture_unload(_p[_worst].texture);
        }
        _r->picture_bytes -= _p[_worst].bytes;
        const u64 _gone = ((u64)_p[_worst].blob << 32) | (u64)_p[_worst].serial;
        rde_hash_map_remove(&_r->picture_of, (const any)&_gone);
        const u32 _last = (u32)rde_arr_length(&_r->pictures) - 1u;
        if(_worst != _last) {
            _p[_worst] = _p[_last];
            const u64 _moved = ((u64)_p[_worst].blob << 32) | (u64)_p[_worst].serial;
            rde_hash_map_update_entry(&_r->picture_of, (const any)&_moved, (const any)&_worst);
        }
        rde_arr_remove(&_r->pictures, _last);
    }
    const fude_zoom_picture _e = { _o->blob, _serial, _t, _cost, _r->draws };
    const u32 _index = (u32)rde_arr_length(&_r->pictures);
    rde_arr_add(&_r->pictures, (any)&_e);
    rde_hash_map_add(&_r->picture_of, (const any)&_key, (const any)&_index);
    _r->picture_bytes += _cost;
    if(_size != NULL) {
        *_size = _px;
    }
    return _t;
}

// A picture: its quad through the transforms. Many screens across (deep inside
// it), only the part on the screen is drawn — a sub-rectangle of its pixels,
// whose corners are then near the screen (no far-out vertex).
RDE_INTERNAL void fude_zoom_render_image(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra) {
    const fude_zoom_object* _o     = fude_zoom_scene_object(_s, _object);
    const f64               _size  = rde_math_max_f64(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * fude_zoom_sim_scale(_to_screen);
    if(_size < 0.5) {
        return;
    }
    f64 _hw, _hh;
    if(!fude_zoom_scene_image(_s, _object, &_hw, &_hh, NULL, NULL)) {
        return;
    }
    const fude_zoom_sim _all   = fude_zoom_sim_compose(_to_screen, fude_zoom_object_sim(_o));
    const f64           _angle = rde_math_atan2_f64(_all.b, _all.a);
    const f64           _k     = fude_zoom_sim_scale(_all);
    if(_as != NULL) {
        // A glow: its outline.
        const fude_zoom_v2 _c[4] = { { -_hw, -_hh }, { _hw, -_hh }, { _hw, _hh }, { -_hw, _hh } };
        rde_vec_2F _pts[4];
        for(u32 _i = 0; _i < 4u; _i++) {
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_all, _c[_i]);
            _pts[_i] = (rde_vec_2F){ (f32)_p.x, (f32)_p.y };
        }
        rde_rendering_2d_draw_polygon_border(_pts, 4u, 1.5f + _extra, *_as, NULL);
        return;
    }
    rde_vec_2UI  _px;
    rde_texture* _t = fude_zoom_render_picture(_r, _s, _object, &_px);
    if(_t == NULL || _px.x == 0u || _px.y == 0u) {
        return;
    }
    // The part of it on the screen (a box round the screen, in its own units),
    // in its pixels: all of it, unless it reaches far past the screen.
    f64 _u0 = -_hw, _v0 = -_hh, _u1 = _hw, _v1 = _hh;
    if(_k * rde_math_max_f64(_hw, _hh) > FUDE_ZOOM_RENDER_FAR * 0.1) {
        const fude_zoom_box _seen = fude_zoom_sim_box(fude_zoom_sim_inverse(_all), (fude_zoom_box){ -_half.x * 1.5, -_half.y * 1.5, _half.x * 1.5, _half.y * 1.5 });
        _u0 = rde_math_max_f64(_u0, _seen.min_x); _v0 = rde_math_max_f64(_v0, _seen.min_y);
        _u1 = rde_math_min_f64(_u1, _seen.max_x); _v1 = rde_math_min_f64(_v1, _seen.max_y);
        if(_u0 >= _u1 || _v0 >= _v1) {
            return;
        }
    }
    // Its own units (Y up) → its pixels, from its top-left, y down (the
    // engine's way for a texture's rectangles).
    const f64 _sx = (f64)_px.x / (2.0 * _hw), _sy = (f64)_px.y / (2.0 * _hh);
    const rde_vec_2F _tl = { (f32)((_u0 + _hw) * _sx), (f32)((_hh - _v1) * _sy) };
    const rde_vec_2F _br = { (f32)((_u1 + _hw) * _sx), (f32)((_hh - _v0) * _sy) };
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(_all, (fude_zoom_v2){ (_u0 + _u1) * 0.5, (_v0 + _v1) * 0.5 });
    // The whole picture drawn at its pixels' size times this is its size on screen.
    const rde_vec_2F _scale = { (f32)(_k / _sx), (f32)(_k / _sy) };
    rde_rendering_2d_draw_texture_partial_2(_t, (rde_vec_3F){ (f32)_mid.x, (f32)_mid.y, 0.0f }, _scale, (f32)(-_angle * (180.0 / RDE_PI)), (rde_color){ 255, 255, 255, 255 }, _tl, _br);
    _r->strokes_drawn++;
}

// --- frames ----------------------------------------------------------------------------------

// rde_sort's order for render items: by draw key.
RDE_INTERNAL int fude_zoom_render_by_z(const any _a, const any _b) {
    const u64 _za = ((const fude_zoom_render_item*)_a)->z, _zb = ((const fude_zoom_render_item*)_b)->z;
    return _za < _zb ? -1 : (_za > _zb ? 1 : 0);
}

// Draws frame _frame through _to_screen and adds it to visible: its objects on the screen in their draw key's order
// (what the selection lifts, the text being written and what a hidden layer holds left out), and each frame in it at
// least FUDE_ZOOM_RENDER_MIN_PX across on the screen, recursively (4096 deep at most). Its objects are found into the
// renderer's found and sorted in its items, above those of the frames it is inside (a stack: a run per frame being
// drawn, cut back when it is done), so nothing is allocated once those have grown to the deepest draw's.
RDE_INTERNAL void fude_zoom_render_frame(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_screen, fude_zoom_v2 _half, u32 _depth) {
    if(_depth > 4096u) {
        return;
    }
    const fude_zoom_visible _v = { _frame, _to_screen, fude_zoom_scene_depth(_s, _frame) };
    rde_arr_add(&_r->visible, (any)&_v);
    _r->frames_drawn++;

    // The screen (a little more) in this frame's units.
    const fude_zoom_box _screen = { -_half.x - 2.0, -_half.y - 2.0, _half.x + 2.0, _half.y + 2.0 };
    const fude_zoom_box _view   = fude_zoom_sim_box(fude_zoom_sim_inverse(_to_screen), _screen);

    const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _frame);
    rde_arr_clear(&_r->found);
    fude_zoom_scene_query(_s, _frame, _view, &_r->found);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->kids); _i++) {
        const u32               _k = ((const u32*)_f->kids.memory)[_i];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _k);
        if(_o->flags & FUDE_ZOOM_FLAG_ALIVE && fude_zoom_box_overlaps(_o->box, _view)) {
            rde_arr_add(&_r->found, (any)&_k);
        }
    }
    const u32 _n    = (u32)rde_arr_length(&_r->found);
    const u32 _base = (u32)rde_arr_length(&_r->items);   // (this frame's run starts here)
    if(_n == 0u) {
        return;
    }
    fude_zoom_render_item* _run = (fude_zoom_render_item*)rde_arr_add_n(&_r->items, _n);
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = ((const u32*)_r->found.memory)[_i];
        _run[_i] = (fude_zoom_render_item){ fude_zoom_scene_draw_key(_s, fude_zoom_scene_object(_s, _o)), _o };
    }
    rde_sort(_run, _n, sizeof(_run[0]), fude_zoom_render_by_z);

    // In order, markers too: a highlight over what was there before it, under what came after (found on the
    // tablet: a frame drew its markers first, under everything — a board, writing — drawn long before them).
    const u8* _lifted   = _r->lifted != NULL ? (const u8*)_r->lifted->memory : NULL;
    const u32 _lifted_n = _r->lifted != NULL ? (u32)rde_arr_length(_r->lifted) : 0u;
    const u8* _faint    = _r->faint != NULL ? (const u8*)_r->faint->memory : NULL;
    const u32 _faint_n  = _r->faint != NULL ? (u32)rde_arr_length(_r->faint) : 0u;
    for(u32 _i = 0; _i < _n; _i++) {
        // (read from the stack each time: a frame drawn in this one grows it, which may move it)
        const u32 _object = ((const fude_zoom_render_item*)_r->items.memory)[_base + _i].object;
        if((_object < _lifted_n && _lifted[_object]) || _object == _r->skip) {
            continue;   // the selection draws it, where the drag has it (a text being written: itself, as written)
        }
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
        if(_o->kind != FUDE_ZOOM_KIND_FRAME && fude_zoom_scene_hides(_s, _o)) {
            continue;   // on a hidden layer
        }
        // (faint: what a recording played has not reached yet — its own colour, mostly see-through)
        rde_color        _faint_color;
        const rde_color* _as = NULL;
        if(_object < _faint_n && _faint[_object] && _o->kind != FUDE_ZOOM_KIND_FRAME) {
            _faint_color   = fude_theme_resolve(_o->color);
            _faint_color.a = (u8)((u32)_faint_color.a * FUDE_ZOOM_RENDER_FAINT / 100u);
            _as            = &_faint_color;
        }
        if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
            if(_as != NULL) {
                fude_zoom_render_stroke_as(_r, _s, _object, _to_screen, _half, _as, 0.0f);
            } else {
                fude_zoom_render_stroke(_r, _s, _object, _to_screen, _half);
            }
        } else if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
            fude_zoom_render_shape(_r, _s, _object, _to_screen, _half, _as, 0.0f);
        } else if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
            fude_zoom_render_image(_r, _s, _object, _to_screen, _half, _as, 0.0f);
        } else if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
            fude_zoom_render_text(_r, _s, _object, _to_screen, _as);
        } else if(_o->kind == FUDE_ZOOM_KIND_FILL) {
            fude_zoom_render_fill(_r, _s, _object, _to_screen, _half, _as, 0.0f);
        } else if(_o->kind == FUDE_ZOOM_KIND_FRAME) {
            const fude_zoom_frame* _c       = fude_zoom_scene_frame(_s, _o->child);
            const fude_zoom_sim    _c_sim   = fude_zoom_sim_compose(_to_screen, fude_zoom_sim_from_xform(_c->xf));
            const f64              _on_screen = rde_math_max_f64(_c->anchor.max_x - _c->anchor.min_x, _c->anchor.max_y - _c->anchor.min_y) * fude_zoom_sim_scale(_to_screen);
            if(_on_screen >= FUDE_ZOOM_RENDER_MIN_PX) {
                fude_zoom_render_frame(_r, _s, _o->child, _c_sim, _half, _depth + 1u);
            }
        }
    }
    rde_arr_resize(&_r->items, _base);
}

// The colour something covering a point shows: a shape's own fill, if it has one.
RDE_INTERNAL rde_color fude_zoom_render_cover_color(const fude_zoom_object* _o) {
    return fude_theme_resolve(_o->kind == FUDE_ZOOM_KIND_SHAPE && (_o->flags & FUDE_ZOOM_FLAG_FILL_OWN) ? _o->fill : _o->color);
}

// Point _i of decoded stroke _d where it is in its frame, as fude_zoom_scene_point_at places a decoded quantum: the
// same numbers, as a stroke's decoded points are its quanta's own values (exact in an f32 below 2^24 quanta from its
// first point, a million points at the zoom it was drawn at).
RDE_INTERNAL fude_zoom_v2 fude_zoom_render_decoded_at(const fude_zoom_object* _o, const fude_zoom_decoded* _d, u32 _i) {
    const f32*         _xy = (const f32*)_d->xy.memory;
    const fude_zoom_v2 _l  = { (f64)_xy[_i * 2u], (f64)_xy[_i * 2u + 1u] };
    if(_o->rotation == 0.0 && _o->scale == 1.0) {
        return (fude_zoom_v2){ _o->t.x + _l.x, _o->t.y + _l.y };
    }
    return fude_zoom_sim_apply(fude_zoom_object_sim(_o), _l);
}

// What is drawn round point _p (frame _frame's units, a unit _scale points on the screen) there: the topmost alive
// stroke, fill or shape covering it drawn under the child frame whose draw key is _slot (*_under) and over it
// (*_over). Under and over are by draw key (scene.h's fude_zoom_scene_draw_key: the layer's rank, then the order key),
// as fude_zoom_render_frame sorts what it draws, so the backdrop says what drawing the frame would show; what a hidden
// layer holds is drawn nowhere, so it covers nothing. A stroke's and a fill's points come from the decoded strokes'
// cache, a shape's outline from the kept geometry, at the detail its size there asks for, as it would be drawn:
// nothing is decoded or made again while the camera stays inside.
RDE_INTERNAL void fude_zoom_render_cover(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _p, f64 _scale, u64 _slot, u32* _under,
                                         u32* _over) {
    rde_arr_clear(&_r->found);
    fude_zoom_scene_query(_s, _frame, (fude_zoom_box){ _p.x, _p.y, _p.x, _p.y }, &_r->found);
    u64 _best_under = 0, _best_over = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->found); _i++) {
        const u32               _object = ((const u32*)_r->found.memory)[_i];
        const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
        if(fude_zoom_scene_hides(_s, _o)) {
            continue;
        }
        const u64 _key = fude_zoom_scene_draw_key(_s, _o);
        if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
            // Inside a filled shape, or on its line: its outline (all its numbers) where it is in its frame.
            const f64            _size   = rde_math_max_f64(_o->box.max_x - _o->box.min_x, _o->box.max_y - _o->box.min_y) * _scale;
            const fude_zoom_geo* _g      = fude_zoom_render_shape_geo(_r, _s, _object, fude_zoom_shape_segments(_size), UINT32_MAX);
            const u32            _k      = (u32)rde_arr_length(&_g->points);
            const b8             _closed = _g->closed;
            const fude_zoom_sim  _place  = fude_zoom_object_sim(_o);
            rde_arr_clear(&_r->shape);
            for(u32 _a = 0; _a < _k; _a++) {
                const fude_zoom_v2 _at = fude_zoom_sim_apply(_place, ((const fude_zoom_v2*)_g->points.memory)[_a]);
                rde_arr_add(&_r->shape, (any)&_at);
            }
            const fude_zoom_v2* _pp = (const fude_zoom_v2*)_r->shape.memory;
            b8 _in = _closed && (_o->flags & FUDE_ZOOM_FLAG_FILLED) && rde_math_point_in_polygon_2d_f64(_p, _pp, _k);
            const f64 _rr = (f64)_o->radius * _o->scale;
            for(u32 _a = 0; _a + 1u < _k + (_closed ? 1u : 0u) && !_in; _a++) {
                _in = rde_math_point_segment_distance_2d_f64(_p, _pp[_a], _pp[(_a + 1u) % _k]) <= _rr;
            }
            if(_in && _key < _slot && (*_under == FUDE_ZOOM_NONE || _key > _best_under)) {
                *_under = _object; _best_under = _key;
            }
            if(_in && _key > _slot && (*_over == FUDE_ZOOM_NONE || _key > _best_over)) {
                *_over = _object; _best_over = _key;
            }
            continue;
        }
        if(_o->kind == FUDE_ZOOM_KIND_FILL && _o->count >= 3u) {
            // Inside it.
            const fude_zoom_decoded* _d = fude_zoom_render_decoded(_r, _s, _object);
            if(_d == NULL) {
                continue;
            }
            const fude_zoom_v2 _in_o = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_object_sim(_o)), _p);
            rde_arr_clear(&_r->fill);
            fude_zoom_v2* _pts = rde_arr_add_n(&_r->fill, _d->count);
            const f32* _xy = (const f32*)_d->xy.memory;
            for(u32 _a = 0; _a < _d->count; _a++) {
                _pts[_a] = (fude_zoom_v2){ _xy[_a * 2u], _xy[_a * 2u + 1u] };
            }
            const b8 _in = fude_zoom_fill_inside_rings((const fude_zoom_v2*)_r->fill.memory, (const u32*)_d->rings.memory, _d->count, _in_o);
            if(_in && _key < _slot && (*_under == FUDE_ZOOM_NONE || _key > _best_under)) {
                *_under = _object; _best_under = _key;
            }
            if(_in && _key > _slot && (*_over == FUDE_ZOOM_NONE || _key > _best_over)) {
                *_over = _object; _best_over = _key;
            }
            continue;
        }
        if(_o->kind != FUDE_ZOOM_KIND_STROKE || _o->count == 0) {
            continue;
        }
        // A stroke: its decoded points where they are, each segment's width its wider end's (fude_zoom_scene_radius_at:
        // the decoded radius times its scale, as an f32).
        const fude_zoom_decoded* _d = fude_zoom_render_decoded(_r, _s, _object);
        if(_d == NULL) {
            continue;
        }
        const f32* _rad    = (const f32*)_d->radius.memory;
        b8         _covers = false;
        for(u32 _k = 0; _k < _d->count && !_covers; _k++) {
            const fude_zoom_v2 _a  = fude_zoom_render_decoded_at(_o, _d, _k);
            const fude_zoom_v2 _b  = _k + 1u < _d->count ? fude_zoom_render_decoded_at(_o, _d, _k + 1u) : _a;
            const f64          _ra = (f64)(f32)((f64)_rad[_k] * _o->scale);
            const f64          _rr = rde_math_max_f64(_ra, _k + 1u < _d->count ? (f64)(f32)((f64)_rad[_k + 1u] * _o->scale) : 0.0);
            _covers = rde_math_point_segment_distance_sq_2d_f64(_p, _a, _b) <= _rr * _rr;
        }
        if(!_covers) {
            continue;
        }
        if(_key < _slot && (*_under == FUDE_ZOOM_NONE || _key > _best_under)) {
            *_under     = _object;
            _best_under = _key;
        }
        if(_key > _slot && (*_over == FUDE_ZOOM_NONE || _key > _best_over)) {
            *_over     = _object;
            _best_over = _key;
        }
    }
}

// See the header. Three steps: the highest frame drawn exactly (three up at most, while the error of its transform to
// the screen stays under FUDE_ZOOM_RENDER_EXACT_PT); the backdrop, from the root down; then that frame and the frames
// beside it in its parent that reach the screen, in the parent's order. Last, the colour of anything drawn later above
// that covers the camera's point goes over everything.
void fude_zoom_render(fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper) {
    _r->draws++;
    _r->strokes_drawn = _r->points_drawn = _r->frames_drawn = _r->meshes_made = 0;
    rde_arr_clear(&_r->visible);
    const fude_zoom_camera* _cam     = &_s->camera;
    const fude_zoom_sim     _cam_sim = fude_zoom_camera_sim(_cam);

    // The highest frame drawn exactly (three up at most).
    u32           _top     = _cam->frame;
    fude_zoom_sim _top_sim = _cam_sim;
    for(u32 _l = 0; _l < 3u; _l++) {
        const u32 _up = fude_zoom_scene_frame(_s, _top)->parent;
        if(_up == FUDE_ZOOM_NONE) {
            break;
        }
        const fude_zoom_sim _up_sim = fude_zoom_sim_compose(_cam_sim, fude_zoom_scene_sim(_s, _up, _cam->frame));
        if(fude_zoom_render_error(_up_sim) >= FUDE_ZOOM_RENDER_EXACT_PT) {
            break;
        }
        _top     = _up;
        _top_sim = _up_sim;
    }

    // Above it, the backdrop: from the root down, what covers the camera's point.
    rde_color _back = _paper;
    b8        _has_over = false;
    rde_color _over_color = _paper;
    {
        u32 _chain[256];
        u32 _levels = 0;
        for(u32 _f = fude_zoom_scene_frame(_s, _top)->parent; _f != FUDE_ZOOM_NONE && _levels < 256u; _f = fude_zoom_scene_frame(_s, _f)->parent) {
            _chain[_levels++] = _f;
        }
        u32 _below = _top;   // the frame of the path one level down
        for(i32 _l = (i32)_levels - 1; _l >= 0; _l--) {
            const u32 _f    = _chain[_l];
            // The child on the path: from the top, the next one down.
            _below = _l > 0 ? _chain[_l - 1] : _top;
            const fude_zoom_frame* _child = fude_zoom_scene_frame(_s, _below);
            const u64 _slot = _child->object != FUDE_ZOOM_NONE ? fude_zoom_scene_draw_key(_s, fude_zoom_scene_object(_s, _child->object)) : 0u;
            const fude_zoom_v2 _p = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _cam->frame, _f), _cam->at);
            const f64 _scale = fude_zoom_sim_scale(fude_zoom_sim_compose(_cam_sim, fude_zoom_scene_sim(_s, _f, _cam->frame)));
            u32 _under = FUDE_ZOOM_NONE, _over = FUDE_ZOOM_NONE;
            fude_zoom_render_cover(_r, _s, _f, _p, _scale, _slot, &_under, &_over);
            if(_over != FUDE_ZOOM_NONE && !_has_over) {
                _has_over   = true;
                _over_color = fude_zoom_render_cover_color(fude_zoom_scene_object(_s, _over));
            }
            if(_under != FUDE_ZOOM_NONE) {
                _back = fude_zoom_render_cover_color(fude_zoom_scene_object(_s, _under));
            }
        }
    }
    if(rde_memcmp(&_back, &_paper, sizeof(_back)) != 0) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)(_half.x * 2.0 + 4.0), (f32)(_half.y * 2.0 + 4.0) }, _back);
    }

    // The top frame, with the frames beside it in its parent (a cousin's
    // drawing can reach the screen too), in that parent's order.
    const u32 _parent = fude_zoom_scene_frame(_s, _top)->parent;
    if(_parent == FUDE_ZOOM_NONE) {
        fude_zoom_render_frame(_r, _s, _top, _top_sim, _half, 0u);
    } else {
        // (sorted on the renderer's stack of items, under the frames drawn from here, which go on above them)
        const fude_zoom_frame* _p = fude_zoom_scene_frame(_s, _parent);
        const u32 _n    = (u32)rde_arr_length(&_p->kids);
        const u32 _base = (u32)rde_arr_length(&_r->items);
        if(_n > 0u) {
            fude_zoom_render_item* _run = (fude_zoom_render_item*)rde_arr_add_n(&_r->items, _n);
            for(u32 _i = 0; _i < _n; _i++) {
                const u32 _k = ((const u32*)_p->kids.memory)[_i];
                _run[_i] = (fude_zoom_render_item){ fude_zoom_scene_draw_key(_s, fude_zoom_scene_object(_s, _k)), _k };
            }
            rde_sort(_run, _n, sizeof(_run[0]), fude_zoom_render_by_z);
        }
        const fude_zoom_box _screen = { -_half.x, -_half.y, _half.x, _half.y };
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const fude_zoom_render_item*)_r->items.memory)[_base + _i].object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                continue;
            }
            const u32           _kid     = _o->child;
            const fude_zoom_sim _kid_sim = _kid == _top ? _top_sim : fude_zoom_sim_compose(_top_sim, fude_zoom_scene_sim(_s, _kid, _top));
            const fude_zoom_frame* _kf   = fude_zoom_scene_frame(_s, _kid);
            const fude_zoom_box _in_kid  = fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_sim_from_xform(_kf->xf)), _kf->anchor);
            if(_kid == _top || fude_zoom_box_overlaps(fude_zoom_sim_box(_kid_sim, _in_kid), _screen)) {
                fude_zoom_render_frame(_r, _s, _kid, _kid_sim, _half, 0u);
            }
        }
        rde_arr_resize(&_r->items, _base);
    }

    if(_has_over) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)(_half.x * 2.0 + 4.0), (f32)(_half.y * 2.0 + 4.0) }, _over_color);
    }
}

// See the header.
void fude_zoom_render_object(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
        fude_zoom_render_stroke(_r, _s, _object, _to_screen, _half);
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
        fude_zoom_render_shape(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_IMAGE) {
        fude_zoom_render_image(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_FILL) {
        fude_zoom_render_fill(_r, _s, _object, _to_screen, _half, NULL, 0.0f);
        return;
    }
    if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
        fude_zoom_render_text(_r, _s, _object, _to_screen, NULL);
        return;
    }
    if(_o->kind != FUDE_ZOOM_KIND_FRAME) {
        return;   // (a bookmark: never drawn)
    }
    // A frame: everything in it, through its own place — not one of the frames
    // the pen can reach this draw (the list is as the canvas was drawn).
    const u32           _kept = (u32)rde_arr_length(&_r->visible);
    const fude_zoom_sim _c    = fude_zoom_sim_compose(_to_screen, fude_zoom_sim_from_xform(fude_zoom_scene_frame(_s, _o->child)->xf));
    const rde_arr*      _was  = _r->lifted;
    _r->lifted = NULL;
    fude_zoom_render_frame(_r, _s, _o->child, _c, _half, 1u);
    _r->lifted = _was;
    rde_arr_resize(&_r->visible, _kept);
}

// See the header. A text's glow is its box grown by _extra. Only a stroke is drawn as a stroke: any other kind (a
// frame, a bookmark, a layer) has no outline to glow and draws nothing, rather than being decoded as a stroke it is
// not (its points, if any, are not a stroke's, and none at all ends in rde_arr_add_n with a count of 0, which RDE
// stops at).
void fude_zoom_render_glow(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, f32 _extra, rde_color _color) {
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_TEXT) {
        // A text: its box, a little larger.
        fude_zoom_v2 _c[4];
        fude_zoom_scene_text_corners(_s, _object, _c);
        rde_vec_2F _p[4];
        const fude_zoom_v2 _mid = fude_zoom_sim_apply(_to_screen, (fude_zoom_v2){ (_c[0].x + _c[2].x) * 0.5, (_c[0].y + _c[2].y) * 0.5 });
        for(u32 _i = 0; _i < 4u; _i++) {
            const fude_zoom_v2 _q = fude_zoom_sim_apply(_to_screen, _c[_i]);
            const f64 _d = rde_math_hypot_f64(_q.x - _mid.x, _q.y - _mid.y);
            const f64 _k = _d > 0.0 ? (_d + (f64)_extra) / _d : 1.0;
            _p[_i] = (rde_vec_2F){ (f32)(_mid.x + (_q.x - _mid.x) * _k), (f32)(_mid.y + (_q.y - _mid.y) * _k) };
        }
        rde_rendering_2d_draw_polygon(_p, 4u, _color, NULL);
        return;
    }
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_SHAPE) {
        fude_zoom_render_shape(_r, _s, _object, _to_screen, _half, &_color, _extra);
        return;
    }
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_IMAGE) {
        fude_zoom_render_image(_r, _s, _object, _to_screen, _half, &_color, _extra);
        return;
    }
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_FILL) {
        fude_zoom_render_fill(_r, _s, _object, _to_screen, _half, &_color, _extra);
        return;
    }
    if(fude_zoom_scene_object(_s, _object)->kind == FUDE_ZOOM_KIND_STROKE) {
        fude_zoom_render_stroke_as(_r, _s, _object, _to_screen, _half, &_color, _extra);
    }
}

// See the header.
u32 fude_zoom_render_editable(const fude_zoom_renderer* _r, const fude_zoom_scene* _s, rde_arr* _out) {
    const u32 _depth = fude_zoom_scene_depth(_s, _s->camera.frame);
    u32       _n     = 0;
    const fude_zoom_visible* _v = (const fude_zoom_visible*)_r->visible.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_r->visible); _i++) {
        // The camera's depth and below; above it while its units are still a
        // few points on screen (what is seen there can be erased, lassoed,
        // filled — further up a stroke's width runs to thousands of points).
        if(_v[_i].depth >= _depth || fude_zoom_sim_scale(_v[_i].to_screen) <= FUDE_ZOOM_RENDER_EDIT_UP) {
            rde_arr_add(_out, (any)&_v[_i]);
            _n++;
        }
    }
    return _n;
}
