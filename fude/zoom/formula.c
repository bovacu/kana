// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/formula.h"
#include "zoom/fill.h"
#include "zoom/texcalc.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See formula.h.
//
// Two caches, each an array of what was made and a map into it, emptied whole when full: the formulas set (tex.h's
// boxes, keyed by their style and words) and the glyphs cut (fude_zoom_formula_mesh, keyed by glyph and level of
// detail). Its own functions and constants are prefixed fzf_ and FZF_.
// ===========================================================================

#define FZF_SETS_MOST   512u    // Formulas kept set; past it, all are let go and set again as they are drawn.
#define FZF_MESHES_MOST 6000u   // Glyphs' cuts kept (one for each glyph and level); past it, all are let go.
#define FZF_ANSWERED    1024u   // Bytes: a formula answered, its words and its answer, at most.
// Levels of detail: at the coarsest a glyph's curves are no further than 64 font units out, each finer a quarter of it.
#define FZF_LEVELS      9u

// See the header: the caches on the std allocator, the sets keyed by owned strings, the meshes by a u64.
void fude_zoom_formulas_init(fude_zoom_formulas* _f) {
    rde_memset(_f, 0, sizeof(*_f));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _f->sets      = rde_hash_map_new_ex(RDE_HASH_MAP_SLOT_STR_OWNED, 0, RDE_HASH_MAP_SLOT_BYTES, sizeof(u32), NULL, NULL, NULL, _heap);
    _f->boxes     = rde_arr_new(sizeof(fude_zoom_tex_box), _heap);
    _f->meshes    = rde_hash_map_new(sizeof(u64), sizeof(u32), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, _heap);
    _f->mesh_list = rde_arr_new(sizeof(fude_zoom_formula_mesh), _heap);
}

// Lets every set formula go: frees each box's own memory and empties the array and its map.
RDE_INTERNAL void fzf_forget_sets(fude_zoom_formulas* _f) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->boxes); _i++) {
        fude_zoom_tex_box_free(&((fude_zoom_tex_box*)_f->boxes.memory)[_i]);
    }
    rde_arr_clear(&_f->boxes);
    rde_hash_map_clear(&_f->sets);
}

// Lets every cut glyph go: frees each mesh's arrays and empties the list and its map.
RDE_INTERNAL void fzf_forget_meshes(fude_zoom_formulas* _f) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->mesh_list); _i++) {
        fude_zoom_formula_mesh* _m = &((fude_zoom_formula_mesh*)_f->mesh_list.memory)[_i];
        rde_arr_free(&_m->tris);
        rde_arr_free(&_m->ring);
        rde_arr_free(&_m->ends);
    }
    rde_arr_clear(&_f->mesh_list);
    rde_hash_map_clear(&_f->meshes);
}

// See the header. Leaves the struct zeroed, as before fude_zoom_formulas_init.
void fude_zoom_formulas_free(fude_zoom_formulas* _f) {
    fzf_forget_sets(_f);
    fzf_forget_meshes(_f);
    rde_arr_free(&_f->boxes);
    rde_arr_free(&_f->mesh_list);
    rde_hash_map_free(&_f->sets);
    rde_hash_map_free(&_f->meshes);
    if(_f->bytes != NULL) {
        fude_file_free(_f->bytes);
    }
    rde_memset(_f, 0, sizeof(*_f));
}

// See the header. The caches are emptied whether or not the font loads, since what they hold was made from the old one.
b8 fude_zoom_formulas_load(fude_zoom_formulas* _f, const c8* _path) {
    if(_f->bytes != NULL) {
        fude_file_free(_f->bytes);
        _f->bytes = NULL;
    }
    u32 _size = 0;
    _f->bytes = fude_file_read(_path, &_size);
    _f->ready = _f->bytes != NULL && fude_zoom_mfont_load(&_f->font, _f->bytes, _size);
    fzf_forget_sets(_f);
    fzf_forget_meshes(_f);
    return _f->ready;
}

// See the header: fude_zoom_formulas_set_as in display style.
const fude_zoom_tex_box* fude_zoom_formulas_set(fude_zoom_formulas* _f, const c8* _text, u32 _len) {
    return fude_zoom_formulas_set_as(_f, _text, _len, true);
}

// See the header. Found by its key in the map, or set now and kept; when FZF_SETS_MOST are kept, every one is let go
// first. A sum's answer goes in only when the words and the answer fit in FZF_ANSWERED bytes; otherwise it is set as
// written.
const fude_zoom_tex_box* fude_zoom_formulas_set_as(fude_zoom_formulas* _f, const c8* _text, u32 _len, b8 _display) {
    if(!_f->ready) {
        return NULL;
    }
    // Its key: 'D' (set as a display) or 'T' (set in a line of text), then its words — compared whole by the map.
    c8  _small[256];
    // A long one is on the pool, for this call only.
    c8* _key = _len + 2u <= sizeof(_small) ? _small : (c8*)rde_malloc((usize)_len + 2u);
    _key[0] = _display ? 'D' : 'T';
    rde_memcpy(_key + 1, _text, _len);
    _key[_len + 1u] = 0;
    const u32* _at = (const u32*)rde_hash_map_find(&_f->sets, _key);
    if(_at != NULL && *_at < (u32)rde_arr_length(&_f->boxes)) {
        if(_key != _small) {
            rde_free(_key);
        }
        return &((const fude_zoom_tex_box*)_f->boxes.memory)[*_at];
    }
    if(rde_arr_length(&_f->boxes) >= FZF_SETS_MOST) {
        fzf_forget_sets(_f);
    }
    fude_zoom_tex_box _b;
    fude_zoom_tex_box_init(&_b);
    // A sum ending in "=" gets its answer after it, as Math Notes does: "\frac{3}{4} + 2^3 =" is set as
    // "\frac{3}{4} + 2^3 = 8.75".
    c8 _answer[48];
    if(fude_zoom_texcalc_answer(_text, _len, _answer, sizeof(_answer)) && _len + rde_strlen(_answer) + 2u < FZF_ANSWERED) {
        c8 _with[FZF_ANSWERED];
        const u32 _n = (u32)rde_snprintf(_with, sizeof(_with), "%.*s %s", (int)_len, _text, _answer);
        fude_zoom_tex_set(&_f->font, _with, _n, _display, &_b);
    } else {
        fude_zoom_tex_set(&_f->font, _text, _len, _display, &_b);
    }
    rde_arr_add(&_f->boxes, (any)&_b);
    const u32 _index = (u32)rde_arr_length(&_f->boxes) - 1u;
    if(!rde_hash_map_add(&_f->sets, _key, (any)&_index)) {
        rde_hash_map_update_entry(&_f->sets, _key, (any)&_index);
    }
    if(_key != _small) {
        rde_free(_key);
    }
    return &((const fude_zoom_tex_box*)_f->boxes.memory)[_index];
}

// See the header. Joined, the contours go one after another, each closed back to its own start, and then the path goes
// back over the joins to the first contour's start; filled even-odd, each join is crossed twice and cancels out.
u32 fude_zoom_formulas_outline(const fude_zoom_formulas* _f, u32 _g, f64 _tol, b8 _apart, rde_arr* _out, rde_arr* _rings) {
    if(!_f->ready) {
        return 0u;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(f64), _heap), _ends = rde_arr_new(sizeof(u32), _heap);
    const u32 _contours = fude_zoom_mfont_outline(&_f->font, _g, _tol, &_pts, &_ends);
    const f64* _p = (const f64*)_pts.memory;
    const u32* _e = (const u32*)_ends.memory;
    const u32  _was = (u32)rde_arr_length(_out);
    // Each contour's points; joined, each is closed by its start again and leads on to the next one's first point.
    // Apart, each is a ring of its own, as the font has it.
    u32 _from = 0;
    for(u32 _c = 0; _c < _contours; _c++) {
        for(u32 _i = _from; _i < _e[_c]; _i++) {
            const fude_zoom_v2 _v = { _p[2u * _i], _p[2u * _i + 1u] };
            rde_arr_add(_out, (any)&_v);
            if(_rings != NULL) {
                const u32 _ring = _apart ? _c : 0u;
                rde_arr_add(_rings, (any)&_ring);
            }
        }
        if(_e[_c] > _from && !_apart) {
            const fude_zoom_v2 _start = { _p[2u * _from], _p[2u * _from + 1u] };
            rde_arr_add(_out, (any)&_start);
            if(_rings != NULL) {
                const u32 _ring = 0u;
                rde_arr_add(_rings, (any)&_ring);
            }
        }
        _from = _e[_c];
    }
    // Back along the joins: the starts of the contours from the second last down to the second; the polygon then
    // closes on the first contour's start.
    if(_contours >= 3u && !_apart) {
        for(u32 _c = _contours - 2u; _c >= 1u; _c--) {
            const u32 _s = _e[_c - 1u];   // Contour _c's start: one past the previous contour's end.
            const fude_zoom_v2 _v = { _p[2u * _s], _p[2u * _s + 1u] };
            rde_arr_add(_out, (any)&_v);
            if(_rings != NULL) {
                const u32 _ring = 0u;
                rde_arr_add(_rings, (any)&_ring);
            }
            if(_c == 1u) {
                break;
            }
        }
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_ends);
    return (u32)rde_arr_length(_out) - _was;
}

// Glyph _g at the detail _px asks for: its triangles and its contours, as kept, or made now and kept (when
// FZF_MESHES_MOST are kept, every one is let go first). Returns NULL when no font is loaded or _px is not above 0.
RDE_INTERNAL const fude_zoom_formula_mesh* fzf_mesh(fude_zoom_formulas* _f, u32 _g, f64 _px) {
    if(!_f->ready || !(_px > 0.0)) {
        return NULL;
    }
    // The detail: curves no further out than a third of a screen point, at the coarsest level that is enough.
    const f64 _want = 0.33 * (f64)_f->font.units_per_em / _px;
    u32 _level = 0;
    f64 _tol = 64.0;
    while(_level + 1u < FZF_LEVELS && _tol > _want) {
        _tol *= 0.25;
        _level++;
    }
    const u64 _key = ((u64)_g << 8) | _level;
    const u32* _at = (const u32*)rde_hash_map_find(&_f->meshes, &_key);
    if(_at != NULL && *_at < (u32)rde_arr_length(&_f->mesh_list)) {
        return &((const fude_zoom_formula_mesh*)_f->mesh_list.memory)[*_at];
    }
    if(rde_arr_length(&_f->mesh_list) >= FZF_MESHES_MOST) {
        fzf_forget_meshes(_f);
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_formula_mesh _m = { rde_arr_new(sizeof(fude_zoom_v2), _heap), rde_arr_new(sizeof(fude_zoom_v2), _heap), rde_arr_new(sizeof(u32), _heap) };
    rde_arr _poly = rde_arr_new(sizeof(fude_zoom_v2), _heap), _ids = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_formulas_outline(_f, _g, _tol, false, &_poly, NULL);
    if(rde_arr_length(&_poly) >= 3u) {
        fude_zoom_fill_triangulate((const fude_zoom_v2*)_poly.memory, (u32)rde_arr_length(&_poly), &_m.tris);
    }
    // Its contours apart, and where each ends: render_text.c draws the glyph's soft edge along them.
    fude_zoom_formulas_outline(_f, _g, _tol, true, &_m.ring, &_ids);
    const u32* _id = (const u32*)_ids.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_ids); _i++) {
        if(_i + 1u == (u32)rde_arr_length(&_ids) || _id[_i + 1u] != _id[_i]) {
            const u32 _end = _i + 1u;
            rde_arr_add(&_m.ends, (any)&_end);
        }
    }
    rde_arr_free(&_poly);
    rde_arr_free(&_ids);
    rde_arr_add(&_f->mesh_list, (any)&_m);
    const u32 _index = (u32)rde_arr_length(&_f->mesh_list) - 1u;
    if(!rde_hash_map_add(&_f->meshes, &_key, &_index)) {
        rde_hash_map_update_entry(&_f->meshes, &_key, &_index);
    }
    return &((const fude_zoom_formula_mesh*)_f->mesh_list.memory)[_index];
}

// See the header.
u32 fude_zoom_formulas_mesh(fude_zoom_formulas* _f, u32 _g, f64 _px, const fude_zoom_v2** _out) {
    const fude_zoom_formula_mesh* _m = fzf_mesh(_f, _g, _px);
    *_out = _m != NULL ? (const fude_zoom_v2*)_m->tris.memory : NULL;
    return _m != NULL ? (u32)(rde_arr_length(&_m->tris) / 3u) : 0u;
}

// See the header. The pointers are NULL, and 0 is returned, when no font is loaded or _px is not above 0.
u32 fude_zoom_formulas_rings(fude_zoom_formulas* _f, u32 _g, f64 _px, const fude_zoom_v2** _points, const u32** _ends) {
    const fude_zoom_formula_mesh* _m = fzf_mesh(_f, _g, _px);
    *_points = _m != NULL ? (const fude_zoom_v2*)_m->ring.memory : NULL;
    *_ends   = _m != NULL ? (const u32*)_m->ends.memory : NULL;
    return _m != NULL ? (u32)rde_arr_length(&_m->ends) : 0u;
}

// See the header: the box's width, and its height plus depth, in ems, padded on each side and times _size.
void fude_zoom_formulas_size(const fude_zoom_tex_box* _box, f64 _size, f64* _w, f64* _h) {
    *_w = (_box != NULL ? _box->width : 1.0) * _size + 2.0 * FUDE_ZOOM_FORMULA_PAD * _size;
    *_h = (_box != NULL ? _box->height + _box->depth : 1.0) * _size + 2.0 * FUDE_ZOOM_FORMULA_PAD * _size;
}
