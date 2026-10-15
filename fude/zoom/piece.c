// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/piece.h"
#include "zoom/codec.h"
#include "zoom/shape.h"
#include "zoom/symbol.h"

// ===========================================================================
// See piece.h.
//
// In order: one piece's file (written and read through kfile.h's chunks), the list and its index, and a piece's
// lines. The list is a fixed array, the newest first; adding moves every piece down one, and each change writes only
// what it changed: the piece's own file, the index, or both.
// ===========================================================================

// The tags of the piece file's kind, the index file's kind and their chunks, each four ASCII letters read as a
// little-endian u32. The index and the pieces share FUDE_ZOOM_PIECE_VERSION.
#define FUDE_ZOOM_PIECE_KIND     0x43454950u   // 'PIEC'
#define FUDE_ZOOM_PIECE_INDEX    0x58444950u   // 'PIDX'
#define FUDE_ZOOM_PIECE_VERSION  1u
#define FUDE_ZOOM_PIECE_TAG_NAME 0x454D414Eu   // 'NAME'
#define FUDE_ZOOM_PIECE_TAG_BOX  0x20584F42u   // 'BOX '
#define FUDE_ZOOM_PIECE_TAG_ITEM 0x4D455449u   // 'ITEM'
#define FUDE_ZOOM_PIECE_TAG_IDS  0x20534449u   // 'IDS '

// --- numbers as bytes (kfile.h has no 64-bit ones: two words, low first) ---

// Appends _v's 64 bits exactly, as two u32 words, the low one first.
RDE_INTERNAL void fude_zoom_piece_put_f64(fude_bytes* _b, f64 _v) {
    u64 _bits;
    rde_memcpy(&_bits, &_v, sizeof(_bits));
    fude_put_u32(_b, (u32)(_bits & 0xFFFFFFFFull));
    fude_put_u32(_b, (u32)(_bits >> 32));
}

// Reads an f64 written by fude_zoom_piece_put_f64.
RDE_INTERNAL f64 fude_zoom_piece_get_f64(fude_reader* _r) {
    const u64 _lo = fude_get_u32(_r), _hi = fude_get_u32(_r);
    const u64 _bits = _lo | (_hi << 32);
    f64 _v;
    rde_memcpy(&_v, &_bits, sizeof(_v));
    return _v;
}

// See the header. Safe on a zeroed piece.
void fude_zoom_piece_clear(fude_zoom_piece* _piece) {
    if(rde_arr_is_inited(&_piece->items)) {
        fude_zoom_clip* _c = (fude_zoom_clip*)_piece->items.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_piece->items); _i++) {
            rde_arr_free(&_c[_i].bytes);
        }
        rde_arr_free(&_piece->items);
    }
    rde_memset(_piece, 0, sizeof(*_piece));
}

// See the header. The NAME chunk holds the name's bytes without its NUL, the BOX chunk the box's four f64s (min x,
// min y, max x, max y), and each ITEM chunk one thing: its kind, channels, q and flags as bytes, its point count, its
// colour and fill, its radius, its place (a, b, tx, ty as f64s), and its bytes' size followed by its bytes.
void fude_zoom_piece_write(const fude_zoom_piece* _piece, fude_bytes* _out) {
    fude_put_header(_out, FUDE_ZOOM_PIECE_VERSION, FUDE_ZOOM_PIECE_KIND);
    u32 _at = fude_chunk_begin(_out, FUDE_ZOOM_PIECE_TAG_NAME);
    fude_put_data(_out, _piece->name, (u32)rde_strlen(_piece->name));
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
        fude_put_u32(_out, fude_zoom_clip_size(&_c[_i]));
        fude_put_data(_out, fude_zoom_clip_data(&_c[_i]), fude_zoom_clip_size(&_c[_i]));
        fude_chunk_end(_out, _at);
    }
}

// See the header. A name longer than FUDE_ZOOM_PIECE_NAME is cut; things past FUDE_ZOOM_PIECE_ITEMS, an ITEM whose
// bytes run past its chunk, and chunks of unknown tags are skipped. Every thing read is marked alive.
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
            rde_memcpy(_piece->name, _chunk.data, _n);
            _piece->name[_n] = 0;
        } else if(_tag == FUDE_ZOOM_PIECE_TAG_BOX) {
            _piece->box.min_x = fude_zoom_piece_get_f64(&_chunk);
            _piece->box.min_y = fude_zoom_piece_get_f64(&_chunk);
            _piece->box.max_x = fude_zoom_piece_get_f64(&_chunk);
            _piece->box.max_y = fude_zoom_piece_get_f64(&_chunk);
        } else if(_tag == FUDE_ZOOM_PIECE_TAG_ITEM && rde_arr_length(&_piece->items) < FUDE_ZOOM_PIECE_ITEMS) {
            fude_zoom_clip _c;
            rde_memset(&_c, 0, sizeof(_c));
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
            const u32 _size  = fude_get_u32(&_chunk);
            if(!_chunk.ok || !fude_reader_has(&_chunk, _size)) {
                continue;
            }
            _c.look.flags |= FUDE_ZOOM_FLAG_ALIVE;
            fude_zoom_clip_put(&_c, _chunk.data + _chunk.pos, _size);
            rde_arr_add(&_piece->items, (any)&_c);
        }
    }
    _piece->families = fude_zoom_piece_families(_piece);
    return _file.ok && rde_arr_length(&_piece->items) > 0;
}

// --- the list ---

// Writes piece _id's file path, <_dir><_id>.piece, into _out.
RDE_INTERNAL void fude_zoom_pieces_path(const c8* _dir, u32 _id, c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%s%u.piece", _dir, _id);
}

// Writes the index, <_dir>index.kana: one IDS chunk with the next id, the count, and each piece's id in list order. A
// failed write is not reported.
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
    rde_snprintf(_path, sizeof(_path), "%sindex.kana", _dir);
    fude_bytes_write_and_free(&_b, _path, NULL);
}

// See the header. Without an index the list starts empty. The next id is kept above every id read, even when the
// index says less.
void fude_zoom_pieces_load(fude_zoom_pieces* _p, const c8* _dir) {
    if(_p->loaded) {
        return;
    }
    rde_memset(_p, 0, sizeof(*_p));
    _p->loaded  = true;
    _p->next_id = 1u;
    c8 _path[RDE_MAX_PATH];
    rde_snprintf(_path, sizeof(_path), "%sindex.kana", _dir);
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

// See the header. Zeroing clears loaded as well.
void fude_zoom_pieces_free(fude_zoom_pieces* _p) {
    for(u32 _i = 0; _i < _p->count; _i++) {
        fude_zoom_piece_clear(&_p->list[_i]);
    }
    rde_memset(_p, 0, sizeof(*_p));
}

// Writes one piece to its file in _dir. Returns false when it could not be written.
RDE_INTERNAL b8 fude_zoom_pieces_write_one(const fude_zoom_piece* _piece, const c8* _dir) {
    fude_bytes _b = fude_bytes_new(4096u);
    fude_zoom_piece_write(_piece, &_b);
    c8 _path[RDE_MAX_PATH];
    fude_zoom_pieces_path(_dir, _piece->id, _path, sizeof(_path));
    return fude_bytes_write_and_free(&_b, _path, NULL);
}

// See the header. The piece's file is written before it joins the list, so a piece that could not be written is never
// listed; at most FUDE_ZOOM_PIECE_ITEMS things are kept. The id is used up even when the write fails.
u32 fude_zoom_pieces_add(fude_zoom_pieces* _p, const c8* _dir, const c8* _name, const fude_zoom_clip* _items, u32 _n, fude_zoom_box _box) {
    if(_p->count >= FUDE_ZOOM_PIECES || _n == 0u) {
        return FUDE_ZOOM_NONE;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_piece _new;
    rde_memset(&_new, 0, sizeof(_new));
    _new.id    = _p->next_id++;
    _new.box   = _box;
    _new.items = rde_arr_new(sizeof(fude_zoom_clip), _heap);
    rde_snprintf(_new.name, sizeof(_new.name), "%s", _name);
    for(u32 _i = 0; _i < _n && _i < FUDE_ZOOM_PIECE_ITEMS; _i++) {
        fude_zoom_clip _c = _items[_i];
        fude_zoom_clip_put(&_c, fude_zoom_clip_data(&_items[_i]), fude_zoom_clip_size(&_items[_i]));
        rde_arr_add(&_new.items, (any)&_c);
    }
    _new.families = fude_zoom_piece_families(&_new);
    if(!fude_zoom_pieces_write_one(&_new, _dir)) {
        fude_zoom_piece_clear(&_new);
        return FUDE_ZOOM_NONE;
    }
    // First in the list (the newest first).
    rde_memmove(&_p->list[1], &_p->list[0], sizeof(_p->list[0]) * _p->count);
    _p->list[0] = _new;
    _p->count++;
    fude_zoom_pieces_write_index(_p, _dir);
    return 0u;
}

// See the header. The index is written before the file is deleted, so it never names a piece whose file is gone.
void fude_zoom_pieces_remove(fude_zoom_pieces* _p, const c8* _dir, u32 _i) {
    if(_i >= _p->count) {
        return;
    }
    c8 _path[RDE_MAX_PATH];
    fude_zoom_pieces_path(_dir, _p->list[_i].id, _path, sizeof(_path));
    fude_zoom_piece_clear(&_p->list[_i]);
    rde_memmove(&_p->list[_i], &_p->list[_i + 1u], sizeof(_p->list[0]) * (_p->count - _i - 1u));
    _p->count--;
    rde_memset(&_p->list[_p->count], 0, sizeof(_p->list[0]));
    fude_zoom_pieces_write_index(_p, _dir);
    rde_file_delete(_path);
}

// See the header. The name is cut to FUDE_ZOOM_PIECE_NAME bytes. A failed write is logged by fude_bytes_write_and_free.
b8 fude_zoom_pieces_rename(fude_zoom_pieces* _p, const c8* _dir, u32 _i, const c8* _name) {
    if(_i >= _p->count) {
        return false;
    }
    rde_snprintf(_p->list[_i].name, sizeof(_p->list[_i].name), "%s", _name);
    return fude_zoom_pieces_write_one(&_p->list[_i], _dir);
}

// See the header. The old things are freed before the new are copied in, so a failed write leaves the new ones in
// memory but the old file on disk.
b8 fude_zoom_pieces_replace(fude_zoom_pieces* _p, const c8* _dir, u32 _i, const fude_zoom_clip* _items, u32 _n, fude_zoom_box _box) {
    if(_i >= _p->count || _n == 0u) {
        return false;
    }
    fude_zoom_piece* _piece = &_p->list[_i];
    fude_zoom_select_clips_free(&_piece->items);
    for(u32 _k = 0; _k < _n && _k < FUDE_ZOOM_PIECE_ITEMS; _k++) {
        fude_zoom_clip _c = _items[_k];
        fude_zoom_clip_put(&_c, fude_zoom_clip_data(&_items[_k]), fude_zoom_clip_size(&_items[_k]));
        rde_arr_add(&_piece->items, (any)&_c);
    }
    _piece->box      = _box;
    _piece->families = fude_zoom_piece_families(_piece);
    return fude_zoom_pieces_write_one(_piece, _dir);
}

// See the header.
b8 fude_zoom_piece_save(const fude_zoom_piece* _piece, const c8* _path) {
    fude_bytes _b = fude_bytes_new(4096u);
    fude_zoom_piece_write(_piece, &_b);
    return fude_bytes_write_and_free(&_b, _path, NULL);
}

// See the header.
b8 fude_zoom_piece_load(fude_zoom_piece* _piece, const c8* _path) {
    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        return false;
    }
    const b8 _ok = fude_zoom_piece_read(_piece, _data, _size);
    fude_file_free(_data);
    return _ok;
}

// --- its lines ---

// Adds one line of _n points _p, carried by _to into the piece's units, to _points and its fude_zoom_piece_line to
// _lines. An empty line adds nothing.
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

// See the header. A symbol's kind is its shape's first number; one the library does not know adds nothing.
u32 fude_zoom_piece_families(const fude_zoom_piece* _piece) {
    u32 _bits = 0;
    const fude_zoom_clip* _c = (const fude_zoom_clip*)_piece->items.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_piece->items); _i++) {
        if(_c[_i].look.kind != FUDE_ZOOM_KIND_SHAPE || _c[_i].look.channels != FUDE_ZOOM_SHAPE_SYMBOL || fude_zoom_clip_size(&_c[_i]) < sizeof(f64)) {
            continue;
        }
        f64 _kind;
        rde_memcpy(&_kind, fude_zoom_clip_data(&_c[_i]), sizeof(_kind));
        const fude_zoom_symbol_info* _info = _kind >= 0.0 ? fude_zoom_symbol_info_of((u32)_kind) : NULL;
        _bits |= _info != NULL ? 1u << _info->family : 0u;
    }
    return _bits;
}

// See the header. A shape's outline is cut with 48 segments (fude_zoom_shape_outline's _segments); a stroke's and a
// fill's points are decoded from their bytes (codec.h) and scaled by 2^q. A thing that cannot be decoded is left out.
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
            fude_zoom_shape_outline(_it->look.channels, (const f64*)fude_zoom_clip_data(_it), fude_zoom_clip_size(_it) / (u32)sizeof(f64), 48u, &_pts, &_closed);
            fude_zoom_piece_line_add(_points, _lines, (const fude_zoom_v2*)_pts.memory, (u32)rde_arr_length(&_pts), _closed, _to);
            continue;
        }
        if(_it->look.kind == FUDE_ZOOM_KIND_STROKE || _it->look.kind == FUDE_ZOOM_KIND_FILL) {
            rde_arr_clear(&_q);
            fude_zoom_qpoint* _qp = rde_arr_add_n(&_q, _it->look.count);
            if(_it->look.count == 0u || !fude_zoom_codec_decode(fude_zoom_clip_data(_it), fude_zoom_clip_size(_it), _qp, _it->look.count, _it->look.channels)) {
                continue;
            }
            const f64 _g = rde_math_ldexp_f64(1.0, _it->look.q);
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
        if(_boxes && (_it->look.kind == FUDE_ZOOM_KIND_IMAGE || _it->look.kind == FUDE_ZOOM_KIND_TEXT) && fude_zoom_clip_size(_it) >= 2u * sizeof(f64)) {
            // A picture: its half sizes first; a text: its letters' size, then its width and height (its box from its
            // top left).
            f64 _a, _b, _cc = 0.0;
            rde_memcpy(&_a, fude_zoom_clip_data(_it), sizeof(f64));
            rde_memcpy(&_b, fude_zoom_clip_data(_it) + sizeof(f64), sizeof(f64));
            if(_it->look.kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_clip_size(_it) >= 3u * sizeof(f64)) {
                rde_memcpy(&_cc, fude_zoom_clip_data(_it) + 2u * sizeof(f64), sizeof(f64));
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
