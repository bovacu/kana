// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/zfile.h"
#include "drawing/base/kfile.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See zfile.h.
// ===========================================================================

#define FUDE_ZOOM_FILE_KIND FUDE_TAG('Z', 'O', 'O', 'M')
#define FUDE_ZOOM_TAG_JRNL  FUDE_TAG('J', 'R', 'N', 'L')
#define FUDE_ZOOM_TAG_BKTS  FUDE_TAG('B', 'K', 'T', 'S')
#define FUDE_ZOOM_TAG_FRAM  FUDE_TAG('F', 'R', 'A', 'M')
#define FUDE_ZOOM_TAG_HIST  FUDE_TAG('H', 'I', 'S', 'T')
#define FUDE_ZOOM_TAG_INDX  FUDE_TAG('I', 'N', 'D', 'X')
#define FUDE_ZOOM_TAG_FOOT  FUDE_TAG('F', 'O', 'O', 'T')
#define FUDE_ZOOM_FOOT_MAGIC FUDE_TAG('Z', 'E', 'N', 'D')

// Past this many replaced bytes (and more than the live ones), a compaction.
#define FUDE_ZOOM_FILE_COMPACT_MIN (64u * 1024u)

RDE_INTERNAL fude_zoom_frame* fude_zoom_file_frame(const fude_zoom_scene* _s, u32 _slot) {
    return fude_zoom_scene_frame(_s, _slot);
}

// --- CRC-32 (the zip one) -------------------------------------------------------------------

u32 fude_zoom_crc32(const u8* _data, u32 _size) {
    static u32 _table[256];
    static b8  _made = false;
    if(!_made) {
        for(u32 _i = 0; _i < 256u; _i++) {
            u32 _c = _i;
            for(u32 _k = 0; _k < 8u; _k++) {
                _c = (_c & 1u) ? 0xEDB88320u ^ (_c >> 1) : _c >> 1;
            }
            _table[_i] = _c;
        }
        _made = true;
    }
    u32 _crc = 0xFFFFFFFFu;
    for(u32 _i = 0; _i < _size; _i++) {
        _crc = _table[(_crc ^ _data[_i]) & 0xFFu] ^ (_crc >> 8);
    }
    return _crc ^ 0xFFFFFFFFu;
}

// A chunk's end: its payload's CRC, then its size filled in.
RDE_INTERNAL void fude_zoom_chunk_end(fude_bytes* _b, u32 _at) {
    const u32 _from = _at + 4u;
    fude_put_u32(_b, fude_zoom_crc32((const u8*)_b->memory + _from, fude_bytes_size(_b) - _from));
    fude_chunk_end(_b, _at);
}

// --- writing --------------------------------------------------------------------------------

RDE_INTERNAL b8 fude_zoom_file_write(const c8* _path, const u8* _data, u32 _size, b8 _append) {
    b8 _ok = false;
    RDE_TRY({
        rde_file* _file = rde_file_open(_path, _append ? RDE_FILE_MODE_APPEND_BYTES : RDE_FILE_MODE_WRITE_BYTES);
        if(_file != NULL && !rde_failed()) {
            rde_file_write_bytes(_file, _data, _size);
            _ok = !rde_failed();
            _ok = rde_file_sync(_file) && _ok;
            rde_file_close(_file);
        }
    });
    if(!_ok) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "fude: could not write %s", _path);
    }
    return _ok;
}

RDE_INTERNAL b8 fude_zoom_file_append(fude_zoom_file* _f, const fude_bytes* _b) {
    const u32 _size = fude_bytes_size(_b);
    if(_size == 0) {
        return true;
    }
    if(!fude_zoom_file_write(_f->path, (const u8*)_b->memory, _size, true)) {
        return false;
    }
    _f->size += _size;
    _f->appends++;
    return true;
}

// The frames a file holds: every one drawn in — a deleted one too, which an
// undo can bring back (its record says it is deleted).
RDE_INTERNAL b8 fude_zoom_file_keeps(const fude_zoom_frame* _fr) {
    return _fr->saved;
}

// One bucket of a frame, as a chunk.
RDE_INTERNAL void fude_zoom_file_put_bucket(const fude_zoom_scene* _s, fude_bytes* _b, u32 _frame, u32 _bucket) {
    const fude_zoom_frame* _fr      = fude_zoom_file_frame(_s, _frame);
    const u32*             _arrival = (const u32*)_fr->arrival.memory;
    const u32              _from    = _bucket * FUDE_ZOOM_BUCKET;
    const u32              _total   = (u32)rde_arr_length(&_fr->arrival);
    const u32              _count   = _total - _from < FUDE_ZOOM_BUCKET ? _total - _from : FUDE_ZOOM_BUCKET;
    const u32 _at = fude_chunk_begin(_b, FUDE_ZOOM_TAG_BKTS);
    fude_put_u64(_b, _fr->id);
    fude_put_u32(_b, _bucket);
    fude_put_u32(_b, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _rec = fude_bytes_size(_b);
        fude_put_u32(_b, 0u);
        fude_zoom_put_object(_s, _b, _arrival[_from + _i]);
        fude_chunk_end(_b, _rec);   // the record's size, filled in the way a chunk's is
    }
    fude_zoom_chunk_end(_b, _at);
}

// Parents first: a frame is written once the one it is in has been.
RDE_INTERNAL void fude_zoom_file_put_frames(const fude_zoom_scene* _s, fude_bytes* _b) {
    const u32 _n = fude_zoom_scene_frame_count(_s);
    rde_arr   _done = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_add_n(&_done, _n > 0 ? _n : 1u);
    memset(_done.memory, 0, _n > 0 ? _n : 1u);
    u8* _d = (u8*)_done.memory;

    const u32 _at = fude_chunk_begin(_b, FUDE_ZOOM_TAG_FRAM);
    const u32 _count_at = fude_bytes_size(_b);
    fude_put_u32(_b, 0u);
    u32 _count = 0;
    for(b8 _progress = true; _progress; ) {
        _progress = false;
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_zoom_frame* _fr = fude_zoom_file_frame(_s, _i);
            if(_d[_i] || !fude_zoom_file_keeps(_fr)) {
                continue;
            }
            if(_fr->parent != FUDE_ZOOM_NONE && !_d[_fr->parent]) {
                continue;
            }
            const u32 _rec = fude_bytes_size(_b);
            fude_put_u32(_b, 0u);
            fude_zoom_put_frame(_s, _b, _i);
            fude_chunk_end(_b, _rec);
            _d[_i] = 1u;
            _count++;
            _progress = true;
        }
    }
    u8* _p = rde_arr_at(_b, _count_at);
    _p[0] = (u8)_count;
    _p[1] = (u8)(_count >> 8);
    _p[2] = (u8)(_count >> 16);
    _p[3] = (u8)(_count >> 24);
    fude_zoom_chunk_end(_b, _at);
    rde_arr_free(&_done);
}

RDE_INTERNAL void fude_zoom_file_put_history(const fude_zoom_scene* _s, fude_bytes* _b) {
    const u32 _at = fude_chunk_begin(_b, FUDE_ZOOM_TAG_HIST);
    fude_put_u32(_b, _s->action_count);
    fude_put_u32(_b, (u32)rde_arr_length(&_s->actions));
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_s->actions); _i++) {
        const u32 _rec = fude_bytes_size(_b);
        fude_put_u32(_b, 0u);
        fude_zoom_put_action(_s, _b, _i);
        fude_chunk_end(_b, _rec);
    }
    fude_zoom_chunk_end(_b, _at);
}

typedef struct {
    u32 frame, bucket;
    u64 offset;
    u32 size;
} fude_zoom_file_written;

// A checkpoint's chunks into _b, as if _b were appended at file offset _base:
// the dirty buckets (all of them when _all), FRAM, HIST, INDX, FOOT. Where each
// bucket went is listed in _written; the bytes the INDX points at, in *_live.
RDE_INTERNAL void fude_zoom_file_put_checkpoint(const fude_zoom_scene* _s, fude_bytes* _b, u64 _base, b8 _all, rde_arr* _written, u64* _live) {
    u64 _live_bytes = 0;
    const u32 _frames = fude_zoom_scene_frame_count(_s);
    for(u32 _f = 0; _f < _frames; _f++) {
        const fude_zoom_frame* _fr = fude_zoom_file_frame(_s, _f);
        if(!fude_zoom_file_keeps(_fr)) {
            continue;
        }
        const u32 _buckets = ((u32)rde_arr_length(&_fr->arrival) + FUDE_ZOOM_BUCKET - 1u) / FUDE_ZOOM_BUCKET;
        for(u32 _k = 0; _k < _buckets; _k++) {
            const b8 _dirty = _all || _k >= (u32)rde_arr_length(&_fr->offsets) ||
                              (_k < (u32)rde_arr_length(&_fr->dirty) && ((const u8*)_fr->dirty.memory)[_k]);
            if(!_dirty) {
                _live_bytes += ((const u32*)_fr->sizes.memory)[_k];
                continue;
            }
            const u32 _start = fude_bytes_size(_b);
            fude_zoom_file_put_bucket(_s, _b, _f, _k);
            const fude_zoom_file_written _w = { _f, _k, _base + _start, fude_bytes_size(_b) - _start };
            rde_arr_add(_written, (any)&_w);
            _live_bytes += _w.size;
        }
    }

    const u64 _fram = _base + fude_bytes_size(_b);
    fude_zoom_file_put_frames(_s, _b);
    const u64 _hist = _base + fude_bytes_size(_b);
    fude_zoom_file_put_history(_s, _b);
    const u64 _indx = _base + fude_bytes_size(_b);

    // The index: every kept frame's buckets, the new places where they moved.
    const u32 _at = fude_chunk_begin(_b, FUDE_ZOOM_TAG_INDX);
    fude_put_u64(_b, _fram);
    fude_put_u64(_b, _hist);
    fude_put_u64(_b, fude_zoom_file_frame(_s, _s->camera.frame)->id);
    fude_put_f64(_b, _s->camera.at.x);
    fude_put_f64(_b, _s->camera.at.y);
    fude_put_f64(_b, _s->camera.z);
    fude_put_u64(_b, fude_zoom_file_frame(_s, _s->root)->id);
    u32 _kept = 0;
    for(u32 _f = 0; _f < _frames; _f++) {
        _kept += fude_zoom_file_keeps(fude_zoom_file_frame(_s, _f)) ? 1u : 0u;
    }
    fude_put_u32(_b, _kept);
    const fude_zoom_file_written* _w  = (const fude_zoom_file_written*)_written->memory;
    const u32                     _nw = (u32)rde_arr_length(_written);
    u32                           _wi = 0;
    for(u32 _f = 0; _f < _frames; _f++) {
        const fude_zoom_frame* _fr = fude_zoom_file_frame(_s, _f);
        if(!fude_zoom_file_keeps(_fr)) {
            continue;
        }
        const u32 _buckets = ((u32)rde_arr_length(&_fr->arrival) + FUDE_ZOOM_BUCKET - 1u) / FUDE_ZOOM_BUCKET;
        fude_put_u64(_b, _fr->id);
        fude_put_u32(_b, _buckets);
        for(u32 _k = 0; _k < _buckets; _k++) {
            // _written is in frame then bucket order, as it was made.
            if(_wi < _nw && _w[_wi].frame == _f && _w[_wi].bucket == _k) {
                fude_put_u64(_b, _w[_wi].offset);
                fude_put_u32(_b, _w[_wi].size);
                _wi++;
            } else {
                fude_put_u64(_b, ((const u64*)_fr->offsets.memory)[_k]);
                fude_put_u32(_b, ((const u32*)_fr->sizes.memory)[_k]);
            }
        }
    }
    fude_zoom_chunk_end(_b, _at);

    const u64 _foot = _base + fude_bytes_size(_b);
    const u32 _ft = fude_chunk_begin(_b, FUDE_ZOOM_TAG_FOOT);
    fude_put_u64(_b, _indx);
    fude_put_u32(_b, FUDE_ZOOM_FOOT_MAGIC);
    fude_zoom_chunk_end(_b, _ft);
    *_live = _live_bytes + (_base + fude_bytes_size(_b) - _fram);
    RDE_UNUSED(_foot);
}

// After a checkpoint is safe on disk: the frames know where their buckets are,
// and nothing is dirty any more.
RDE_INTERNAL void fude_zoom_file_written_apply(fude_zoom_scene* _s, const rde_arr* _written) {
    const fude_zoom_file_written* _w = (const fude_zoom_file_written*)_written->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_written); _i++) {
        fude_zoom_frame* _fr = fude_zoom_file_frame(_s, _w[_i].frame);
        while((u32)rde_arr_length(&_fr->offsets) <= _w[_i].bucket) {
            const u64 _zero64 = 0;
            const u32 _zero32 = 0;
            rde_arr_add(&_fr->offsets, (any)&_zero64);
            rde_arr_add(&_fr->sizes, (any)&_zero32);
        }
        ((u64*)_fr->offsets.memory)[_w[_i].bucket] = _w[_i].offset;
        ((u32*)_fr->sizes.memory)[_w[_i].bucket]   = _w[_i].size;
    }
    for(u32 _f = 0; _f < fude_zoom_scene_frame_count(_s); _f++) {
        rde_arr_clear(&fude_zoom_file_frame(_s, _f)->dirty);
    }
    rde_arr_clear(&_s->journal);
}

b8 fude_zoom_file_checkpoint(fude_zoom_file* _f, fude_zoom_scene* _s) {
    if(_f->size == 0) {
        return fude_zoom_file_compact(_f, _s);
    }
    fude_bytes _b       = fude_bytes_new(64u * 1024u);
    rde_arr    _written = rde_arr_new(sizeof(fude_zoom_file_written), rde_memory_allocator_get_default_std());
    u64        _live    = 0;
    fude_zoom_file_put_checkpoint(_s, &_b, _f->size, false, &_written, &_live);
    const b8 _ok = fude_zoom_file_append(_f, &_b);
    if(_ok) {
        fude_zoom_file_written_apply(_s, &_written);
        _f->live          = _live;
        _f->journal_bytes = 0;
    }
    rde_arr_free(&_written);
    rde_arr_free(&_b);
    return _ok;
}

b8 fude_zoom_file_compact(fude_zoom_file* _f, fude_zoom_scene* _s) {
    // Each kept frame's arrival order without the garbage: the new buckets.
    for(u32 _fi = 0; _fi < fude_zoom_scene_frame_count(_s); _fi++) {
        fude_zoom_frame* _fr = fude_zoom_file_frame(_s, _fi);
        u32*             _a  = (u32*)_fr->arrival.memory;
        u32              _w  = 0;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_fr->arrival); _i++) {
            fude_zoom_object* _o = fude_zoom_scene_object(_s, _a[_i]);
            if(_o->flags & FUDE_ZOOM_FLAG_GARBAGE) {
                _o->file_pos = FUDE_ZOOM_NONE;
                continue;
            }
            _o->file_pos = _w;
            _a[_w++]     = _a[_i];
        }
        _fr->arrival.count = _w;
        rde_arr_clear(&_fr->offsets);
        rde_arr_clear(&_fr->sizes);
    }

    fude_bytes _b = fude_bytes_new(256u * 1024u);
    fude_put_header(&_b, FUDE_ZOOM_FILE_VERSION, FUDE_ZOOM_FILE_KIND);
    rde_arr _written = rde_arr_new(sizeof(fude_zoom_file_written), rde_memory_allocator_get_default_std());
    u64     _live    = 0;
    fude_zoom_file_put_checkpoint(_s, &_b, 0u, true, &_written, &_live);
    // put_checkpoint took the offsets from where _b began: the header is in it.

    c8 _tmp[RDE_MAX_PATH];
    c8 _bak[RDE_MAX_PATH];
    snprintf(_tmp, sizeof(_tmp), "%s.tmp", _f->path);
    snprintf(_bak, sizeof(_bak), "%s.bak", _f->path);
    b8 _ok = fude_zoom_file_write(_tmp, (const u8*)_b.memory, fude_bytes_size(&_b), false);
    if(_ok) {
        if(rde_file_exists(_f->path)) {
            rde_file_move(_f->path, _bak);
        }
        _ok = rde_file_move(_tmp, _f->path);
        if(!_ok && !rde_file_exists(_f->path) && rde_file_exists(_bak)) {
            rde_file_move(_bak, _f->path);
        }
    }
    if(_ok) {
        fude_zoom_file_written_apply(_s, &_written);
        _f->size          = fude_bytes_size(&_b);
        _f->live          = _f->size;
        _f->journal_bytes = 0;
        _f->appends++;
    } else {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "fude: could not put %s into place", _f->path);
        // The buckets as they are in memory no longer match the old file: the
        // next save writes everything again.
        _f->size = 0;
    }
    rde_arr_free(&_written);
    rde_arr_free(&_b);
    return _ok;
}

b8 fude_zoom_file_flush(fude_zoom_file* _f, fude_zoom_scene* _s) {
    if(_f->size == 0) {
        // No file yet: the first one is written whole, once there is something in it.
        b8 _any = fude_bytes_size(&_s->journal) > 0;
        for(u32 _i = 0; _i < fude_zoom_scene_frame_count(_s) && !_any; _i++) {
            _any = fude_zoom_file_frame(_s, _i)->saved;
        }
        return _any ? fude_zoom_file_compact(_f, _s) : true;
    }
    if(fude_bytes_size(&_s->journal) > 0) {
        fude_bytes _b  = fude_bytes_new(fude_bytes_size(&_s->journal) + 16u);
        const u32  _at = fude_chunk_begin(&_b, FUDE_ZOOM_TAG_JRNL);
        fude_put_data(&_b, _s->journal.memory, fude_bytes_size(&_s->journal));
        fude_zoom_chunk_end(&_b, _at);
        const b8 _ok = fude_zoom_file_append(_f, &_b);
        if(_ok) {
            _f->journal_bytes += fude_bytes_size(&_b);
            rde_arr_clear(&_s->journal);
        }
        rde_arr_free(&_b);
        if(!_ok) {
            return false;
        }
    }
    if(_f->journal_bytes >= FUDE_ZOOM_FILE_CHECKPOINT_BYTES && !fude_zoom_file_checkpoint(_f, _s)) {
        return false;
    }
    if(_f->size > _f->live && _f->size - _f->live > _f->live && _f->size - _f->live > FUDE_ZOOM_FILE_COMPACT_MIN) {
        return fude_zoom_file_compact(_f, _s);
    }
    return true;
}

// --- reading --------------------------------------------------------------------------------

typedef struct {
    u32 tag;
    u64 at;      // the chunk's header
    u32 size;    // its payload's
} fude_zoom_chunk_ref;

// A chunk's payload, read into _buf (rde_arr of u8) and its CRC checked
// (without the CRC: *_size is the rest). NULL: unreadable or damaged. Good
// until _buf's next read.
RDE_INTERNAL u8* fude_zoom_file_payload(rde_file* _file, const fude_zoom_chunk_ref* _c, rde_arr* _buf, u32* _size) {
    if(_c->size < 4u) {
        return NULL;
    }
    rde_arr_resize(_buf, _c->size);
    u8* _data = (u8*)_buf->memory;
    if(rde_file_read_at(_file, _c->at + 8u, _data, _c->size) != _c->size) {
        return NULL;
    }
    const u32 _body = _c->size - 4u;
    const u32 _crc  = (u32)_data[_body] | ((u32)_data[_body + 1u] << 8) | ((u32)_data[_body + 2u] << 16) | ((u32)_data[_body + 3u] << 24);
    if(fude_zoom_crc32(_data, _body) != _crc) {
        return NULL;
    }
    *_size = _body;
    return _data;
}

RDE_INTERNAL i32 fude_zoom_file_find(const rde_arr* _chunks, u64 _at, u32 _tag) {
    const fude_zoom_chunk_ref* _c = (const fude_zoom_chunk_ref*)_chunks->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_chunks); _i++) {
        if(_c[_i].at == _at) {
            return _c[_i].tag == _tag ? (i32)_i : -1;
        }
    }
    return -1;
}

// Records of _count, each u32 size then the record, given to _get.
RDE_INTERNAL b8 fude_zoom_file_records(fude_zoom_scene* _s, fude_reader* _r, u32 _count, b8 (*_get)(fude_zoom_scene*, fude_reader*)) {
    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _size = fude_get_u32(_r);
        if(!_r->ok || !fude_reader_has(_r, _size)) {
            return false;
        }
        fude_reader _rec = fude_reader_make(_r->data + _r->pos, _size);
        _r->pos += _size;
        if(!_get(_s, &_rec)) {
            return false;
        }
    }
    return true;
}

// The file at _path into the scene. CORRUPT leaves the scene half-filled.
RDE_INTERNAL FUDE_LOAD_ fude_zoom_file_read(fude_zoom_file* _f, const c8* _path, fude_zoom_scene* _s) {
    if(!rde_file_exists(_path)) {
        return FUDE_LOAD_MISSING;
    }
    rde_file* _file = NULL;
    RDE_TRY({ _file = rde_file_open(_path, RDE_FILE_MODE_READ_BYTES); });
    if(_file == NULL) {
        return FUDE_LOAD_CORRUPT;
    }
    const u64 _file_size = rde_file_get_size(_file);
    u8        _head[FUDE_FILE_HEADER_SIZE];
    if(_file_size < FUDE_FILE_HEADER_SIZE || rde_file_read_at(_file, 0, _head, FUDE_FILE_HEADER_SIZE) != FUDE_FILE_HEADER_SIZE) {
        rde_file_close(_file);
        return FUDE_LOAD_CORRUPT;
    }
    fude_reader _hr = fude_reader_make(_head, FUDE_FILE_HEADER_SIZE);
    if(!fude_read_header(&_hr, FUDE_ZOOM_FILE_VERSION, FUDE_ZOOM_FILE_KIND)) {
        rde_file_close(_file);
        return FUDE_LOAD_CORRUPT;
    }

    // Every whole chunk, by its header alone.
    rde_arr _chunks = rde_arr_new(sizeof(fude_zoom_chunk_ref), rde_memory_allocator_get_default_std());
    u64     _pos    = FUDE_FILE_HEADER_SIZE;
    b8      _torn   = false;
    while(_pos < _file_size) {
        u8 _ch[8];
        if(_pos + 8u > _file_size || rde_file_read_at(_file, _pos, _ch, 8u) != 8u) {
            _torn = true;
            break;
        }
        const u32 _tag  = (u32)_ch[0] | ((u32)_ch[1] << 8) | ((u32)_ch[2] << 16) | ((u32)_ch[3] << 24);
        const u32 _size = (u32)_ch[4] | ((u32)_ch[5] << 8) | ((u32)_ch[6] << 16) | ((u32)_ch[7] << 24);
        if(_pos + 8u + _size > _file_size) {
            _torn = true;
            break;
        }
        const fude_zoom_chunk_ref _c = { _tag, _pos, _size };
        rde_arr_add(&_chunks, (any)&_c);
        _pos += 8u + _size;
    }
    const fude_zoom_chunk_ref* _c = (const fude_zoom_chunk_ref*)_chunks.memory;
    const u32                  _n = (u32)rde_arr_length(&_chunks);
    rde_arr _buf    = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());   // each chunk read in turn
    rde_arr _ix_buf = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());   // the index's, kept to the end

    // The last good footer, and its good index.
    i32 _foot = -1, _indx = -1;
    u8* _ix = NULL;
    u32 _ix_size = 0;
    for(i32 _i = (i32)_n - 1; _i >= 0 && _foot < 0; _i--) {
        if(_c[_i].tag != FUDE_ZOOM_TAG_FOOT) {
            continue;
        }
        u32 _fs;
        u8* _fp = fude_zoom_file_payload(_file, &_c[_i], &_buf, &_fs);
        if(_fp == NULL) {
            continue;
        }
        fude_reader _fr = fude_reader_make(_fp, _fs);
        const u64 _at    = fude_get_u64(&_fr);
        const u32 _magic = fude_get_u32(&_fr);
        if(!_fr.ok || _magic != FUDE_ZOOM_FOOT_MAGIC) {
            continue;
        }
        const i32 _ii = fude_zoom_file_find(&_chunks, _at, FUDE_ZOOM_TAG_INDX);
        if(_ii < 0) {
            continue;
        }
        _ix = fude_zoom_file_payload(_file, &_c[_ii], &_ix_buf, &_ix_size);
        if(_ix != NULL) {
            _foot = _i;
            _indx = _ii;
        }
    }
    FUDE_LOAD_ _result = FUDE_LOAD_CORRUPT;
    if(_foot < 0) {
        goto done;
    }

    {
        // The scene's own empty root makes way for the file's: dropped, unless
        // the file's first frame has its id (made on this device, the first
        // id): then that record takes the slot over, as any frame's would.
        fude_zoom_frame* _first = fude_zoom_scene_frame(_s, 0);
        _first->removed = true;
        _first->saved   = false;
        _s->home        = FUDE_ZOOM_NONE;   // the file's frames say which is home (scene_loaded: else the root)

        fude_reader _r = fude_reader_make(_ix, _ix_size);
        const u64 _fram = fude_get_u64(&_r);
        const u64 _hist = fude_get_u64(&_r);
        const fude_zoom_id _cam_frame = fude_get_u64(&_r);
        fude_zoom_camera _cam;
        _cam.at.x = fude_get_f64(&_r);
        _cam.at.y = fude_get_f64(&_r);
        _cam.z    = fude_get_f64(&_r);
        const fude_zoom_id _root = fude_get_u64(&_r);
        const u32 _frames = fude_get_u32(&_r);
        if(!_r.ok) {
            goto done;
        }

        // The frames.
        const i32 _fi = fude_zoom_file_find(&_chunks, _fram, FUDE_ZOOM_TAG_FRAM);
        u32 _fs = 0;
        u8* _fp = _fi >= 0 ? fude_zoom_file_payload(_file, &_c[_fi], &_buf, &_fs) : NULL;
        if(_fp == NULL) {
            goto done;
        }
        fude_reader _frr = fude_reader_make(_fp, _fs);
        const u32 _fcount = fude_get_u32(&_frr);
        const b8 _frames_ok = _frr.ok && fude_zoom_file_records(_s, &_frr, _fcount, fude_zoom_get_frame);
        const u32 _root_slot = fude_zoom_scene_find_frame(_s, _root);
        if(!_frames_ok || _root_slot == FUDE_ZOOM_NONE) {
            goto done;
        }
        _s->root = _root_slot;

        // Each frame's buckets.
        for(u32 _k = 0; _k < _frames; _k++) {
            const fude_zoom_id _id      = fude_get_u64(&_r);
            const u32          _buckets = fude_get_u32(&_r);
            const u32          _slot    = fude_zoom_scene_find_frame(_s, _id);
            if(!_r.ok || _slot == FUDE_ZOOM_NONE) {
                goto done;
            }
            for(u32 _b = 0; _b < _buckets; _b++) {
                const u64 _at   = fude_get_u64(&_r);
                const u32 _size = fude_get_u32(&_r);
                const i32 _bi   = fude_zoom_file_find(&_chunks, _at, FUDE_ZOOM_TAG_BKTS);
                u32 _bs = 0;
                u8* _bp = _bi >= 0 ? fude_zoom_file_payload(_file, &_c[_bi], &_buf, &_bs) : NULL;
                if(!_r.ok || _bp == NULL) {
                    goto done;
                }
                fude_reader _br = fude_reader_make(_bp, _bs);
                fude_get_u64(&_br);   // the frame's id (the index's is the one used)
                fude_get_u32(&_br);   // the bucket's number
                const u32 _count = fude_get_u32(&_br);
                const b8  _ok    = _br.ok && fude_zoom_file_records(_s, &_br, _count, fude_zoom_get_object);
                if(!_ok) {
                    goto done;
                }
                fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _slot);
                rde_arr_add(&_fr->offsets, (any)&_at);
                rde_arr_add(&_fr->sizes, (any)&_size);
                _f->live += _size;
            }
        }

        // The history.
        const i32 _hi = fude_zoom_file_find(&_chunks, _hist, FUDE_ZOOM_TAG_HIST);
        u32 _hs = 0;
        u8* _hp = _hi >= 0 ? fude_zoom_file_payload(_file, &_c[_hi], &_buf, &_hs) : NULL;
        if(_hp == NULL) {
            goto done;
        }
        fude_reader _hr2 = fude_reader_make(_hp, _hs);
        const u32 _action_count = fude_get_u32(&_hr2);
        const u32 _actions      = fude_get_u32(&_hr2);
        // An action over something missing ends the history there; the canvas still opens.
        fude_zoom_file_records(_s, &_hr2, _actions, fude_zoom_get_action);
        _s->action_count = _action_count <= (u32)rde_arr_length(&_s->actions) ? _action_count : (u32)rde_arr_length(&_s->actions);
        _f->live += _c[_fi].size + _c[_hi].size + _c[_indx].size + _c[_foot].size + 4u * 8u;

        // What the buckets hold is what the file has: nothing is dirty yet.
        for(u32 _fi2 = 0; _fi2 < fude_zoom_scene_frame_count(_s); _fi2++) {
            rde_arr_clear(&fude_zoom_scene_frame(_s, _fi2)->dirty);
        }

        const u32 _cam_slot = fude_zoom_scene_find_frame(_s, _cam_frame);
        if(_cam_slot != FUDE_ZOOM_NONE && _cam.z > 0.0) {
            _s->camera = (fude_zoom_camera){ _cam_slot, _cam.at, _cam.z };
        } else {
            _s->camera = (fude_zoom_camera){ _s->root, { 0.0, 0.0 }, 1.0 };
        }

        // The journal since: every good JRNL after the footer, in order. Anything
        // else after it (a checkpoint a kill cut short) or a bad one: the end is
        // cut off by compacting once open.
        _s->replaying = true;
        u64 _end = _c[_foot].at + 8u + _c[_foot].size;
        for(u32 _i = (u32)_foot + 1u; _i < _n; _i++) {
            if(_c[_i].tag != FUDE_ZOOM_TAG_JRNL) {
                _torn = true;
                continue;
            }
            u32 _js = 0;
            u8* _jp = fude_zoom_file_payload(_file, &_c[_i], &_buf, &_js);
            if(_jp == NULL) {
                _torn = true;
                break;
            }
            fude_reader _jr = fude_reader_make(_jp, _js);
            const b8 _ok = fude_zoom_apply_ops(_s, &_jr);
            if(!_ok) {
                _torn = true;
                break;
            }
            _f->journal_bytes += 8u + _c[_i].size;
            _end = _c[_i].at + 8u + _c[_i].size;
        }
        _s->replaying = false;
        _f->size      = _end;
        _f->repaired  = _torn;
        _result       = FUDE_LOAD_OK;
    }

done:
    rde_arr_free(&_ix_buf);
    rde_arr_free(&_buf);
    rde_arr_free(&_chunks);
    rde_file_close(_file);
    return _result;
}

FUDE_LOAD_ fude_zoom_file_open(fude_zoom_file* _f, const c8* _path, fude_zoom_scene* _s) {
    memset(_f, 0, sizeof(*_f));
    snprintf(_f->path, sizeof(_f->path), "%s", _path);
    FUDE_LOAD_ _r = fude_zoom_file_read(_f, _path, _s);
    if(_r == FUDE_LOAD_MISSING) {
        return _r;
    }
    if(_r == FUDE_LOAD_CORRUPT) {
        // Kept as .bad, never overwritten; the backup, if there is one.
        const u32 _device = _s->device;
        fude_zoom_scene_destroy(_s);
        fude_zoom_scene_init(_s, _device);
        fude_file_set_aside(_path);
        c8 _bak[RDE_MAX_PATH];
        snprintf(_bak, sizeof(_bak), "%s.bak", _path);
        fude_zoom_file _from_bak;
        memset(&_from_bak, 0, sizeof(_from_bak));
        if(fude_zoom_file_read(&_from_bak, _bak, _s) != FUDE_LOAD_OK) {
            fude_zoom_scene_destroy(_s);
            fude_zoom_scene_init(_s, _device);
            return FUDE_LOAD_CORRUPT;
        }
        fude_zoom_scene_loaded(_s);
        _f->size = 0;   // the next save writes the main file afresh
        fude_zoom_file_compact(_f, _s);
        return FUDE_LOAD_RECOVERED;
    }
    fude_zoom_scene_loaded(_s);
    if(_f->repaired) {
        fude_zoom_file_compact(_f, _s);
    }
    return FUDE_LOAD_OK;
}
