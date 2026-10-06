// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/piece.h"
#include "zoom/codec.h"
#include "zoom/shape.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See piece.h.
// ===========================================================================

#define FUDE_ZOOM_PIECE_KIND     0x43454950u   // 'PIEC'
#define FUDE_ZOOM_PIECE_INDEX    0x58444950u   // 'PIDX'
#define FUDE_ZOOM_PIECE_VERSION  1u
#define FUDE_ZOOM_PIECE_TAG_NAME 0x454D414Eu   // 'NAME'
#define FUDE_ZOOM_PIECE_TAG_BOX  0x20584F42u   // 'BOX '
#define FUDE_ZOOM_PIECE_TAG_ITEM 0x4D455449u   // 'ITEM'
#define FUDE_ZOOM_PIECE_TAG_IDS  0x20534449u   // 'IDS '

// --- numbers as bytes (kfile.h has no 64-bit ones: two words, low first) ---

RDE_INTERNAL void fude_zoom_piece_put_f64(fude_bytes* _b, f64 _v) {
    u64 _bits;
    memcpy(&_bits, &_v, sizeof(_bits));
    fude_put_u32(_b, (u32)(_bits & 0xFFFFFFFFull));
    fude_put_u32(_b, (u32)(_bits >> 32));
}

RDE_INTERNAL f64 fude_zoom_piece_get_f64(fude_reader* _r) {
    const u64 _lo = fude_get_u32(_r), _hi = fude_get_u32(_r);
    const u64 _bits = _lo | (_hi << 32);
    f64 _v;
    memcpy(&_v, &_bits, sizeof(_v));
    return _v;
}

void fude_zoom_piece_clear(fude_zoom_piece* _piece) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(rde_arr_is_inited(&_piece->items)) {
        fude_zoom_clip* _c = (fude_zoom_clip*)_piece->items.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_piece->items); _i++) {
            _heap->free(_heap->allocator, _c[_i].bytes);
        }
        rde_arr_free(&_piece->items);
    }
    memset(_piece, 0, sizeof(*_piece));
}

void fude_zoom_piece_write(const fude_zoom_piece* _piece, fude_bytes* _out) {
    fude_put_header(_out, FUDE_ZOOM_PIECE_VERSION, FUDE_ZOOM_PIECE_KIND);
    u32 _at = fude_chunk_begin(_out, FUDE_ZOOM_PIECE_TAG_NAME);
    fude_put_data(_out, _piece->name, (u32)strlen(_piece->name));
    fude_chunk_end(_out, _at);
    _at = fude_chunk_begin(_out, FUDE_ZOOM_PIECE_TAG_BOX);
    fude_zoom_piece_put_f64(_out, _piece->box.min_x);
    fude_zoom_piece_put_f64(_out, _piece->box.min_y);
    fude_zoom_piece_put_f64(_out, _piece->box.max_x);
    fude_zoom_piece_put_f64(_out, _piece->box.max_y);
    fude_chunk_end(_out, _at);
    const fude_zoom_clip* _c = (const fude_zoom_clip*)_piece->items.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_piece->items); _i++) {
        // Its look (what a paste needs of it), its place in the piece, its bytes.
        _at = fude_chunk_begin(_out, FUDE_ZOOM_PIECE_TAG_ITEM);
        fude_put_u8(_out, _c[_i].look.kind);
        fude_put_u8(_out, _c[_i].look.channels);
        fude_put_u8(_out, (u8)_c[_i].look.q);
        fude_put_u8(_out, _c[_i].look.flags);
        fude_put_u32(_out, _c[_i].look.count);
        fude_put_color(_out, _c[_i].look.color);
        fude_put_color(_out, _c[_i].look.fill);
        fude_put_f32(_out, _c[_i].look.radius);
        fude_zoom_piece_put_f64(_out, _c[_i].on_screen.a);
        fude_zoom_piece_put_f64(_out, _c[_i].on_screen.b);
        fude_zoom_piece_put_f64(_out, _c[_i].on_screen.tx);
        fude_zoom_piece_put_f64(_out, _c[_i].on_screen.ty);
        fude_put_u32(_out, _c[_i].size);
        fude_put_data(_out, _c[_i].bytes, _c[_i].size);
        fude_chunk_end(_out, _at);
    }
}

b8 fude_zoom_piece_read(fude_zoom_piece* _piece, const u8* _data, u32 _size) {
    fude_zoom_piece_clear(_piece);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _piece->items = rde_arr_new(sizeof(fude_zoom_clip), _heap);
    fude_reader _file = fude_reader_make(_data, _size);
    if(!fude_read_header(&_file, FUDE_ZOOM_PIECE_VERSION, FUDE_ZOOM_PIECE_KIND)) {
        return false;
    }
    u32 _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_file, &_tag, &_chunk)) {
        if(_tag == FUDE_ZOOM_PIECE_TAG_NAME) {
            const u32 _n = _chunk.size < FUDE_ZOOM_PIECE_NAME - 1u ? _chunk.size : FUDE_ZOOM_PIECE_NAME - 1u;
            memcpy(_piece->name, _chunk.data, _n);
            _piece->name[_n] = 0;
        } else if(_tag == FUDE_ZOOM_PIECE_TAG_BOX) {
            _piece->box.min_x = fude_zoom_piece_get_f64(&_chunk);
            _piece->box.min_y = fude_zoom_piece_get_f64(&_chunk);
            _piece->box.max_x = fude_zoom_piece_get_f64(&_chunk);
            _piece->box.max_y = fude_zoom_piece_get_f64(&_chunk);
        } else if(_tag == FUDE_ZOOM_PIECE_TAG_ITEM && rde_arr_length(&_piece->items) < FUDE_ZOOM_PIECE_ITEMS) {
            fude_zoom_clip _c;
            memset(&_c, 0, sizeof(_c));
            _c.look.kind     = fude_get_u8(&_chunk);
            _c.look.channels = fude_get_u8(&_chunk);
            _c.look.q        = (i8)fude_get_u8(&_chunk);
            _c.look.flags    = fude_get_u8(&_chunk);
            _c.look.count    = fude_get_u32(&_chunk);
            _c.look.color    = fude_get_color(&_chunk);
            _c.look.fill     = fude_get_color(&_chunk);
            _c.look.radius   = fude_get_f32(&_chunk);
            _c.on_screen.a   = fude_zoom_piece_get_f64(&_chunk);
            _c.on_screen.b   = fude_zoom_piece_get_f64(&_chunk);
            _c.on_screen.tx  = fude_zoom_piece_get_f64(&_chunk);
            _c.on_screen.ty  = fude_zoom_piece_get_f64(&_chunk);
            _c.size          = fude_get_u32(&_chunk);
            if(!_chunk.ok || !fude_reader_has(&_chunk, _c.size)) {
                continue;
            }
            _c.look.flags |= FUDE_ZOOM_FLAG_ALIVE;
            _c.bytes = _heap->malloc(_heap->allocator, _c.size > 0 ? _c.size : 1u);
            memcpy(_c.bytes, _chunk.data + _chunk.pos, _c.size);
            rde_arr_add(&_piece->items, (any)&_c);
        }
    }
    return _file.ok && rde_arr_length(&_piece->items) > 0;
}

// --- the list ---

RDE_INTERNAL void fude_zoom_pieces_path(const c8* _dir, u32 _id, c8* _out, usize _size) {
    snprintf(_out, _size, "%s%u.piece", _dir, _id);
}

// Their order, written.
RDE_INTERNAL void fude_zoom_pieces_write_index(const fude_zoom_pieces* _p, const c8* _dir) {
    fude_bytes _b = fude_bytes_new(256u);
    fude_put_header(&_b, FUDE_ZOOM_PIECE_VERSION, FUDE_ZOOM_PIECE_INDEX);
    const u32 _at = fude_chunk_begin(&_b, FUDE_ZOOM_PIECE_TAG_IDS);
    fude_put_u32(&_b, _p->next_id);
    fude_put_u32(&_b, _p->count);
    for(u32 _i = 0; _i < _p->count; _i++) {
        fude_put_u32(&_b, _p->list[_i].id);
    }
    fude_chunk_end(&_b, _at);
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%sindex.kana", _dir);
    fude_bytes_write_and_free(&_b, _path, NULL);
}

void fude_zoom_pieces_load(fude_zoom_pieces* _p, const c8* _dir) {
    if(_p->loaded) {
        return;
    }
    memset(_p, 0, sizeof(*_p));
    _p->loaded  = true;
    _p->next_id = 1u;
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%sindex.kana", _dir);
    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        return;
    }
    fude_reader _file = fude_reader_make(_data, _size);
    u32 _ids[FUDE_ZOOM_PIECES];
    u32 _n = 0;
    if(fude_read_header(&_file, FUDE_ZOOM_PIECE_VERSION, FUDE_ZOOM_PIECE_INDEX)) {
        u32 _tag;
        fude_reader _chunk;
        while(fude_next_chunk(&_file, &_tag, &_chunk)) {
            if(_tag != FUDE_ZOOM_PIECE_TAG_IDS) {
                continue;
            }
            _p->next_id = fude_get_u32(&_chunk);
            const u32 _count = fude_get_u32(&_chunk);
            for(u32 _i = 0; _i < _count && _n < FUDE_ZOOM_PIECES && _chunk.ok; _i++) {
                _ids[_n++] = fude_get_u32(&_chunk);
            }
        }
    }
    fude_file_free(_data);
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_pieces_path(_dir, _ids[_i], _path, sizeof(_path));
        u8* _piece = fude_file_read(_path, &_size);
        if(_piece == NULL) {
            continue;
        }
        fude_zoom_piece* _into = &_p->list[_p->count];
        if(fude_zoom_piece_read(_into, _piece, _size)) {
            _into->id = _ids[_i];
            _p->next_id = _p->next_id > _ids[_i] ? _p->next_id : _ids[_i] + 1u;
            _p->count++;
        } else {
            fude_zoom_piece_clear(_into);
        }
        fude_file_free(_piece);
    }
}

void fude_zoom_pieces_free(fude_zoom_pieces* _p) {
    for(u32 _i = 0; _i < _p->count; _i++) {
        fude_zoom_piece_clear(&_p->list[_i]);
    }
    memset(_p, 0, sizeof(*_p));
}

RDE_INTERNAL b8 fude_zoom_pieces_write_one(const fude_zoom_piece* _piece, const c8* _dir) {
    fude_bytes _b = fude_bytes_new(4096u);
    fude_zoom_piece_write(_piece, &_b);
    c8 _path[RDE_MAX_PATH];
    fude_zoom_pieces_path(_dir, _piece->id, _path, sizeof(_path));
    return fude_bytes_write_and_free(&_b, _path, NULL);
}

u32 fude_zoom_pieces_add(fude_zoom_pieces* _p, const c8* _dir, const c8* _name, const fude_zoom_clip* _items, u32 _n, fude_zoom_box _box) {
    if(_p->count >= FUDE_ZOOM_PIECES || _n == 0u) {
        return FUDE_ZOOM_NONE;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_piece _new;
    memset(&_new, 0, sizeof(_new));
    _new.id    = _p->next_id++;
    _new.box   = _box;
    _new.items = rde_arr_new(sizeof(fude_zoom_clip), _heap);
    snprintf(_new.name, sizeof(_new.name), "%s", _name);
    for(u32 _i = 0; _i < _n && _i < FUDE_ZOOM_PIECE_ITEMS; _i++) {
        fude_zoom_clip _c = _items[_i];
        _c.bytes = _heap->malloc(_heap->allocator, _c.size > 0 ? _c.size : 1u);
        memcpy(_c.bytes, _items[_i].bytes, _c.size);
        rde_arr_add(&_new.items, (any)&_c);
    }
    if(!fude_zoom_pieces_write_one(&_new, _dir)) {
        fude_zoom_piece_clear(&_new);
        return FUDE_ZOOM_NONE;
    }
    // First in the list (the newest first).
    memmove(&_p->list[1], &_p->list[0], sizeof(_p->list[0]) * _p->count);
    _p->list[0] = _new;
    _p->count++;
    fude_zoom_pieces_write_index(_p, _dir);
    return 0u;
}

void fude_zoom_pieces_remove(fude_zoom_pieces* _p, const c8* _dir, u32 _i) {
    if(_i >= _p->count) {
        return;
    }
    c8 _path[RDE_MAX_PATH];
    fude_zoom_pieces_path(_dir, _p->list[_i].id, _path, sizeof(_path));
    fude_zoom_piece_clear(&_p->list[_i]);
    memmove(&_p->list[_i], &_p->list[_i + 1u], sizeof(_p->list[0]) * (_p->count - _i - 1u));
    _p->count--;
    memset(&_p->list[_p->count], 0, sizeof(_p->list[0]));
    fude_zoom_pieces_write_index(_p, _dir);
    rde_file_delete(_path);
}

void fude_zoom_pieces_rename(fude_zoom_pieces* _p, const c8* _dir, u32 _i, const c8* _name) {
    if(_i >= _p->count) {
        return;
    }
    snprintf(_p->list[_i].name, sizeof(_p->list[_i].name), "%s", _name);
    fude_zoom_pieces_write_one(&_p->list[_i], _dir);
}

// --- its lines ---

RDE_INTERNAL void fude_zoom_piece_line_add(rde_arr* _points, rde_arr* _lines, const fude_zoom_v2* _p, u32 _n, b8 _closed, fude_zoom_sim _to) {
    if(_n == 0u) {
        return;
    }
    const fude_zoom_piece_line _line = { (u32)rde_arr_length(_points), _n, _closed };
    fude_zoom_v2* _out = rde_arr_add_n(_points, _n);
    for(u32 _i = 0; _i < _n; _i++) {
        _out[_i] = fude_zoom_sim_apply(_to, _p[_i]);
    }
    rde_arr_add(_lines, (any)&_line);
}

u32 fude_zoom_piece_lines(const fude_zoom_piece* _piece, b8 _boxes, rde_arr* _points, rde_arr* _lines) {
    rde_arr_clear(_points);
    rde_arr_clear(_lines);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _q   = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    const fude_zoom_clip* _c = (const fude_zoom_clip*)_piece->items.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_piece->items); _i++) {
        const fude_zoom_clip* _it = &_c[_i];
        const fude_zoom_sim   _to = _it->on_screen;
        if(_it->look.kind == FUDE_ZOOM_KIND_SHAPE) {
            b8 _closed = false;
            fude_zoom_shape_outline(_it->look.channels, (const f64*)_it->bytes, _it->size / (u32)sizeof(f64), 48u, &_pts, &_closed);
            fude_zoom_piece_line_add(_points, _lines, (const fude_zoom_v2*)_pts.memory, (u32)rde_arr_length(&_pts), _closed, _to);
            continue;
        }
        if(_it->look.kind == FUDE_ZOOM_KIND_STROKE || _it->look.kind == FUDE_ZOOM_KIND_FILL) {
            rde_arr_clear(&_q);
            fude_zoom_qpoint* _qp = rde_arr_add_n(&_q, _it->look.count);
            if(_it->look.count == 0u || !fude_zoom_codec_decode(_it->bytes, _it->size, _qp, _it->look.count, _it->look.channels)) {
                continue;
            }
            const f64 _g = ldexp(1.0, _it->look.q);
            rde_arr_clear(&_pts);
            fude_zoom_v2* _v = rde_arr_add_n(&_pts, _it->look.count);
            for(u32 _k = 0; _k < _it->look.count; _k++) {
                _v[_k] = (fude_zoom_v2){ (f64)_qp[_k].x * _g, (f64)_qp[_k].y * _g };
            }
            if(_it->look.kind == FUDE_ZOOM_KIND_STROKE) {
                fude_zoom_piece_line_add(_points, _lines, _v, _it->look.count, false, _to);
                continue;
            }
            // A fill: a ring each (its points' times say which).
            for(u32 _from = 0; _from < _it->look.count;) {
                u32 _to_k = _from;
                while(_to_k < _it->look.count && _qp[_to_k].time == _qp[_from].time) {
                    _to_k++;
                }
                fude_zoom_piece_line_add(_points, _lines, &_v[_from], _to_k - _from, true, _to);
                _from = _to_k;
            }
            continue;
        }
        if(_boxes && (_it->look.kind == FUDE_ZOOM_KIND_IMAGE || _it->look.kind == FUDE_ZOOM_KIND_TEXT) && _it->size >= 2u * sizeof(f64)) {
            // A picture: its half sizes first; a text: its letters' size, then its width and height (its box from its top left).
            f64 _a, _b, _cc = 0.0;
            memcpy(&_a, _it->bytes, sizeof(f64));
            memcpy(&_b, _it->bytes + sizeof(f64), sizeof(f64));
            if(_it->look.kind == FUDE_ZOOM_KIND_TEXT && _it->size >= 3u * sizeof(f64)) {
                memcpy(&_cc, _it->bytes + 2u * sizeof(f64), sizeof(f64));
            }
            fude_zoom_v2 _box[4];
            if(_it->look.kind == FUDE_ZOOM_KIND_IMAGE) {
                _box[0] = (fude_zoom_v2){ -_a, -_b }; _box[1] = (fude_zoom_v2){ _a, -_b }; _box[2] = (fude_zoom_v2){ _a, _b }; _box[3] = (fude_zoom_v2){ -_a, _b };
            } else {
                _box[0] = (fude_zoom_v2){ 0.0, 0.0 }; _box[1] = (fude_zoom_v2){ _b, 0.0 }; _box[2] = (fude_zoom_v2){ _b, -_cc }; _box[3] = (fude_zoom_v2){ 0.0, -_cc };
            }
            fude_zoom_piece_line_add(_points, _lines, _box, 4u, true, _to);
        }
    }
    rde_arr_free(&_q);
    rde_arr_free(&_pts);
    return (u32)rde_arr_length(_lines);
}
