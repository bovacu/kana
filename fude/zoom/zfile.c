// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/zfile.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See zfile.h, which has the format.
//
// Laid out as: the CRC, writing (the chunks, a checkpoint, a compaction, a flush), then reading (the chunks found by
// their headers, the last good footer and its index, the frames, buckets and history, and the journal replayed: each a
// step of fude_zoom_file_read's, fude_zoom_file_read_*, sharing a context, fude_zoom_file_reading). A checkpoint is
// built whole in memory and appended in one write; the scene learns where its buckets went only once that write
// succeeded (fude_zoom_file_written_apply). Every file write is synced before it counts.
// ===========================================================================

// The file's kind, its chunks' tags, and the footer's closing word.
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

// Returns frame _slot of the scene (fude_zoom_scene_frame, under this file's name).
RDE_INTERNAL fude_zoom_frame* fude_zoom_file_frame(const fude_zoom_scene* _s, u32 _slot) {
    return fude_zoom_scene_frame(_s, _slot);
}

// --- CRC-32 (the zip one) -------------------------------------------------------------------

// See the header. Eight bytes a step ("slicing by 8"): opening a canvas checks every byte of it, and this was its
// largest cost, a byte at a time. _table[0] is the usual byte-at-a-time table; _table[k] carries a byte through k more
// zero bytes, so the eight lookups of a step, XORed together, are the eight one-byte steps. The tables (8 KB) are
// made on the first call; the bytes are read one by one, so the order of a machine's bytes does not matter. The few
// bytes after the last whole step go a byte at a time.
u32 fude_zoom_crc32(const u8* _data, u32 _size) {
    static u32 _table[8][256];
    static b8  _made = false;
    if(!_made) {
        for(u32 _i = 0; _i < 256u; _i++) {
            u32 _c = _i;
            for(u32 _k = 0; _k < 8u; _k++) {
                _c = (_c & 1u) ? 0xEDB88320u ^ (_c >> 1) : _c >> 1;
            }
            _table[0][_i] = _c;
        }
        for(u32 _i = 0; _i < 256u; _i++) {
            for(u32 _k = 1; _k < 8u; _k++) {
                _table[_k][_i] = _table[0][_table[_k - 1u][_i] & 0xFFu] ^ (_table[_k - 1u][_i] >> 8);
            }
        }
        _made = true;
    }
    u32       _crc = 0xFFFFFFFFu;
    const u8* _p   = _data;
    u32       _n   = _size;
    for(; _n >= 8u; _p += 8, _n -= 8u) {
        const u32 _lo = _crc ^ ((u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24));
        const u32 _hi = (u32)_p[4] | ((u32)_p[5] << 8) | ((u32)_p[6] << 16) | ((u32)_p[7] << 24);
        _crc = _table[7][_lo & 0xFFu] ^ _table[6][(_lo >> 8) & 0xFFu] ^ _table[5][(_lo >> 16) & 0xFFu] ^ _table[4][_lo >> 24] ^
               _table[3][_hi & 0xFFu] ^ _table[2][(_hi >> 8) & 0xFFu] ^ _table[1][(_hi >> 16) & 0xFFu] ^ _table[0][_hi >> 24];
    }
    for(; _n > 0u; _p++, _n--) {
        _crc = _table[0][(_crc ^ *_p) & 0xFFu] ^ (_crc >> 8);
    }
    return _crc ^ 0xFFFFFFFFu;
}

// Ends the chunk begun at _at: the CRC of its payload so far is appended, then its size filled in (counting the CRC).
RDE_INTERNAL void fude_zoom_chunk_end(fude_bytes* _b, u32 _at) {
    const u32 _from = _at + 4u;
    fude_put_u32(_b, fude_zoom_crc32((const u8*)_b->memory + _from, fude_bytes_size(_b) - _from));
    fude_chunk_end(_b, _at);
}

// --- writing --------------------------------------------------------------------------------

// Appends _size bytes to _path and syncs them to disk. Returns false (and logs it) when the file could not be opened,
// written or synced.
RDE_INTERNAL b8 fude_zoom_file_write(const c8* _path, const u8* _data, u32 _size) {
    b8 _ok = false;
    RDE_TRY({
        rde_file* _file = rde_file_open(_path, RDE_FILE_MODE_APPEND_BYTES);
        if(_file != NULL && !rde_failed()) {
            rde_file_write_bytes(_file, _data, _size);
            _ok = !rde_failed();
            _ok = rde_file_sync(_file) && _ok;
            rde_file_close(_file);
        }
    });
    if(!_ok) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "canvas file: could not write %s", _path);
    }
    return _ok;
}

// Appends _b to the file and counts it; nothing to append is a success. On failure (a full disk) some of the bytes may
// have reached the file all the same, and there is no way to cut them off again: anything appended after them would
// lie behind a fragment that opening cannot read past (fude_zoom_file_read stops at it), and the offsets a checkpoint
// counts from _f->size would be wrong. So _f->size becomes 0, as after a failed compaction: the next flush or
// checkpoint writes the whole file again (fude_zoom_file_compact), which drops the fragment. What failed is still in
// memory (the caller keeps the journal, and the buckets stay dirty), so the rewrite holds it.
RDE_INTERNAL b8 fude_zoom_file_append(fude_zoom_file* _f, const fude_bytes* _b) {
    const u32 _size = fude_bytes_size(_b);
    if(_size == 0) {
        return true;
    }
    if(!fude_zoom_file_write(_f->path, (const u8*)_b->memory, _size)) {
        _f->size = 0;
        return false;
    }
    _f->size += _size;
    _f->appends++;
    return true;
}

// Returns whether the file holds frame _fr: every one drawn in, a deleted one too, which an undo can bring back (its
// record says it is deleted).
RDE_INTERNAL b8 fude_zoom_file_keeps(const fude_zoom_frame* _fr) {
    return _fr->saved;
}

// Appends bucket _bucket of frame _frame as a BKTS chunk: up to FUDE_ZOOM_BUCKET of its objects, in arrival order,
// each record after its size.
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

// Appends the FRAM chunk: every kept frame's record, parents first, so a frame is written once the one it is in has
// been. It goes over the frames again until a pass writes none, then fills in the count.
RDE_INTERNAL void fude_zoom_file_put_frames(const fude_zoom_scene* _s, fude_bytes* _b) {
    const u32 _n = fude_zoom_scene_frame_count(_s);
    rde_arr   _done = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_add_n(&_done, _n > 0 ? _n : 1u);
    rde_memset(_done.memory, 0, _n > 0 ? _n : 1u);
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

// Appends the HIST chunk: the undo position, then every action's record.
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

// Where a bucket went in a checkpoint being written: its frame's slot and number, and its chunk's place and size in
// the file.
typedef struct {
    u32 frame, bucket;
    u64 offset;
    u32 size;
} fude_zoom_file_written;

// Writes a checkpoint's chunks into _b, as if _b were appended at file offset _base: the dirty buckets (all of them
// when _all; a bucket the file has never had counts as dirty), FRAM, HIST, INDX, FOOT. Where each bucket went is
// listed in _written; the bytes the INDX points at, in *_live.
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

// After a checkpoint is safe on disk: the frames learn where their buckets are, nothing is dirty any more, and the
// journal, which the checkpoint holds, is emptied.
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

// See the header.
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

// See the header. The objects dead for good are first taken out of each frame's arrival order, so the new buckets are
// packed; the old bucket places are forgotten and every bucket is written, atomically (rde_file_write_atomic, the old
// file kept as the .bak): should the new file fail to go into place, the old one is still there.
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
        rde_arr_resize(&_fr->arrival, _w);
        rde_arr_clear(&_fr->offsets);
        rde_arr_clear(&_fr->sizes);
    }

    fude_bytes _b = fude_bytes_new(256u * 1024u);
    fude_put_header(&_b, FUDE_ZOOM_FILE_VERSION, FUDE_ZOOM_FILE_KIND);
    rde_arr _written = rde_arr_new(sizeof(fude_zoom_file_written), rde_memory_allocator_get_default_std());
    u64     _live    = 0;
    fude_zoom_file_put_checkpoint(_s, &_b, 0u, true, &_written, &_live);
    // put_checkpoint took the offsets from where _b began: the header is in it.

    c8 _bak[RDE_MAX_PATH];
    rde_snprintf(_bak, sizeof(_bak), "%s.bak", _f->path);
    b8 _ok = false;
    RDE_TRY({ _ok = rde_file_write_atomic(_f->path, (const u8*)_b.memory, fude_bytes_size(&_b), _bak); });
    if(_ok) {
        fude_zoom_file_written_apply(_s, &_written);
        _f->size          = fude_bytes_size(&_b);
        _f->live          = _f->size;
        _f->journal_bytes = 0;
        _f->appends++;
    } else {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "canvas file: could not put %s into place", _f->path);
        // The buckets as they are in memory no longer match the old file: the next save writes everything again.
        _f->size = 0;
    }
    rde_arr_free(&_written);
    rde_arr_free(&_b);
    return _ok;
}

// See the header. A compaction is due when the bytes no index points at (old copies and journals) are more than the
// live ones and more than FUDE_ZOOM_FILE_COMPACT_MIN.
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

// A whole chunk found in the file, by its header alone.
typedef struct {
    u32 tag;
    u64 at;      // the chunk's header
    u32 size;    // its payload's
} fude_zoom_chunk_ref;

// Returns a chunk's payload, read into _buf (rde_arr of u8) and its CRC checked; *_size is the payload without the
// CRC. NULL: unreadable or damaged. Good until _buf's next read.
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

// rde_bsearch's order for the chunks found: by where their headers are.
RDE_INTERNAL i32 fude_zoom_file_by_at(const any _a, const any _b) {
    const u64 _x = ((const fude_zoom_chunk_ref*)_a)->at;
    const u64 _y = ((const fude_zoom_chunk_ref*)_b)->at;
    return (_x > _y) - (_x < _y);
}

// Returns the index in _chunks of the chunk whose header is at file offset _at, or -1 when there is none there or it
// is not a _tag. A binary search: the index names a chunk for every bucket of every frame, among thousands, and
// fude_zoom_file_read_chunks finds the chunks walking the file from its start, so they are in the order of where they
// are, each at a place of its own.
RDE_INTERNAL i32 fude_zoom_file_find(const rde_arr* _chunks, u64 _at, u32 _tag) {
    const fude_zoom_chunk_ref  _key   = { 0u, _at, 0u };
    const fude_zoom_chunk_ref* _c     = (const fude_zoom_chunk_ref*)_chunks->memory;
    const fude_zoom_chunk_ref* _found = rde_bsearch(&_key, _c, rde_arr_length(_chunks), sizeof(fude_zoom_chunk_ref), fude_zoom_file_by_at);
    if(_found == NULL) {
        return -1;
    }
    return _found->tag == _tag ? (i32)(_found - _c) : -1;
}

// Reads _count records from _r, each a u32 size then the record, and hands each to _get. Returns false at the first
// that runs past the data or that _get refuses.
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

// What fude_zoom_file_read's steps share, under the names the function had: the file's record (f) and the scene it
// fills (s); the file being read (file); every whole chunk (chunks: c, n of them), the scratch each is read into
// (buf), and whether the end is torn; the last good footer's chunk and its index's (foot, indx), the index's payload
// (ix, ix_size, kept in ix_buf); the index's head (r: read so far; fram and hist: where the frames' and the history's
// chunks are; the camera, cam_frame and cam; root; frames: how many frames' buckets follow) and the frames' chunk
// (fi).
typedef struct {
    fude_zoom_file*            f;
    fude_zoom_scene*           s;
    rde_file*                  file;
    rde_arr*                   chunks;
    const fude_zoom_chunk_ref* c;
    u32                        n;
    rde_arr*                   buf;
    rde_arr*                   ix_buf;
    b8                         torn;
    i32                        foot, indx;
    u8*                        ix;
    u32                        ix_size;
    fude_reader                r;
    u64                        fram, hist;
    fude_zoom_id               cam_frame, root;
    fude_zoom_camera           cam;
    u32                        frames;
    i32                        fi;
} fude_zoom_file_reading;

// Every whole chunk of the file (_file, _file_size bytes) after its header, by its header alone, into _chunks (rde_arr
// of fude_zoom_chunk_ref). Returns whether the end is torn (a chunk's header or its payload running past it).
RDE_INTERNAL b8 fude_zoom_file_read_chunks(rde_file* _file, u64 _file_size, rde_arr* _chunks) {
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
        rde_arr_add(_chunks, (any)&_c);
        _pos += 8u + _size;
    }
    return _torn;
}

// The last good footer (its CRC good, its closing word right) whose index is a good INDX chunk: the footer's and the
// index's chunks, and the index's payload (kept in ix_buf), into _rd. Returns the footer's chunk (-1: none).
RDE_INTERNAL i32 fude_zoom_file_read_footer(fude_zoom_file_reading* _rd) {
    rde_file*                  _file   = _rd->file;
    rde_arr*                   _chunks = _rd->chunks;
    const fude_zoom_chunk_ref* _c      = _rd->c;
    const u32                  _n      = _rd->n;
    rde_arr*                   _buf    = _rd->buf;
    rde_arr*                   _ix_buf = _rd->ix_buf;
    // The last good footer, and its good index.
    i32 _foot = -1, _indx = -1;
    u8* _ix = NULL;
    u32 _ix_size = 0;
    for(i32 _i = (i32)_n - 1; _i >= 0 && _foot < 0; _i--) {
        if(_c[_i].tag != FUDE_ZOOM_TAG_FOOT) {
            continue;
        }
        u32 _fs;
        u8* _fp = fude_zoom_file_payload(_file, &_c[_i], _buf, &_fs);
        if(_fp == NULL) {
            continue;
        }
        fude_reader _fr = fude_reader_make(_fp, _fs);
        const u64 _at    = fude_get_u64(&_fr);
        const u32 _magic = fude_get_u32(&_fr);
        if(!_fr.ok || _magic != FUDE_ZOOM_FOOT_MAGIC) {
            continue;
        }
        const i32 _ii = fude_zoom_file_find(_chunks, _at, FUDE_ZOOM_TAG_INDX);
        if(_ii < 0) {
            continue;
        }
        _ix = fude_zoom_file_payload(_file, &_c[_ii], _ix_buf, &_ix_size);
        if(_ix != NULL) {
            _foot = _i;
            _indx = _ii;
        }
    }
    _rd->foot    = _foot;
    _rd->indx    = _indx;
    _rd->ix      = _ix;
    _rd->ix_size = _ix_size;
    return _foot;
}

// The scene's own empty root dropped (the file's frames say which is home), and the index's head read into _rd: where
// the frames' and the history's chunks are, the camera, the root and how many frames' buckets follow. False: it runs
// past the index.
RDE_INTERNAL b8 fude_zoom_file_read_index(fude_zoom_file_reading* _rd) {
    fude_zoom_scene* _s       = _rd->s;
    u8*              _ix      = _rd->ix;
    const u32        _ix_size = _rd->ix_size;
    // The scene's own empty root makes way for the file's: dropped, unless the file's first frame has its id (made
    // on this device, the first id): then that record takes the slot over, as any frame's would.
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
        return false;
    }
    _rd->r         = _r;
    _rd->fram      = _fram;
    _rd->hist      = _hist;
    _rd->cam_frame = _cam_frame;
    _rd->cam       = _cam;
    _rd->root      = _root;
    _rd->frames    = _frames;
    return true;
}

// The frames: the FRAM chunk's records read into the scene, and the root found among them (its chunk into _rd->fi).
// False: the chunk or a record is bad, or the root is not there.
RDE_INTERNAL b8 fude_zoom_file_read_frames(fude_zoom_file_reading* _rd) {
    fude_zoom_scene*           _s      = _rd->s;
    rde_file*                  _file   = _rd->file;
    rde_arr*                   _chunks = _rd->chunks;
    const fude_zoom_chunk_ref* _c      = _rd->c;
    rde_arr*                   _buf    = _rd->buf;
    const u64                  _fram   = _rd->fram;
    const fude_zoom_id         _root   = _rd->root;
    // The frames.
    const i32 _fi = fude_zoom_file_find(_chunks, _fram, FUDE_ZOOM_TAG_FRAM);
    u32 _fs = 0;
    u8* _fp = _fi >= 0 ? fude_zoom_file_payload(_file, &_c[_fi], _buf, &_fs) : NULL;
    if(_fp == NULL) {
        return false;
    }
    fude_reader _frr = fude_reader_make(_fp, _fs);
    const u32 _fcount = fude_get_u32(&_frr);
    const b8 _frames_ok = _frr.ok && fude_zoom_file_records(_s, &_frr, _fcount, fude_zoom_get_frame);
    const u32 _root_slot = fude_zoom_scene_find_frame(_s, _root);
    if(!_frames_ok || _root_slot == FUDE_ZOOM_NONE) {
        return false;
    }
    _s->root = _root_slot;
    _rd->fi = _fi;
    return true;
}

// Each frame's buckets, as the index lists them after its head: each BKTS chunk's records read into the scene, and
// where it is and how big kept on its frame (its bytes live). False: the index, a chunk or a record is bad.
RDE_INTERNAL b8 fude_zoom_file_read_buckets(fude_zoom_file_reading* _rd) {
    fude_zoom_file*            _f      = _rd->f;
    fude_zoom_scene*           _s      = _rd->s;
    rde_file*                  _file   = _rd->file;
    rde_arr*                   _chunks = _rd->chunks;
    const fude_zoom_chunk_ref* _c      = _rd->c;
    rde_arr*                   _buf    = _rd->buf;
    fude_reader                _r      = _rd->r;
    const u32                  _frames = _rd->frames;
    // Each frame's buckets.
    for(u32 _k = 0; _k < _frames; _k++) {
        const fude_zoom_id _id      = fude_get_u64(&_r);
        const u32          _buckets = fude_get_u32(&_r);
        const u32          _slot    = fude_zoom_scene_find_frame(_s, _id);
        if(!_r.ok || _slot == FUDE_ZOOM_NONE) {
            return false;
        }
        for(u32 _b = 0; _b < _buckets; _b++) {
            const u64 _at   = fude_get_u64(&_r);
            const u32 _size = fude_get_u32(&_r);
            const i32 _bi   = fude_zoom_file_find(_chunks, _at, FUDE_ZOOM_TAG_BKTS);
            u32 _bs = 0;
            u8* _bp = _bi >= 0 ? fude_zoom_file_payload(_file, &_c[_bi], _buf, &_bs) : NULL;
            if(!_r.ok || _bp == NULL) {
                return false;
            }
            fude_reader _br = fude_reader_make(_bp, _bs);
            fude_get_u64(&_br);   // the frame's id (the index's is the one used)
            fude_get_u32(&_br);   // the bucket's number
            const u32 _count = fude_get_u32(&_br);
            const b8  _ok    = _br.ok && fude_zoom_file_records(_s, &_br, _count, fude_zoom_get_object);
            if(!_ok) {
                return false;
            }
            fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _slot);
            rde_arr_add(&_fr->offsets, (any)&_at);
            rde_arr_add(&_fr->sizes, (any)&_size);
            _f->live += _size;
        }
    }
    return true;
}

// The history (its actions read until one is over something missing, and how many are done), the chunks' bytes
// that are live, nothing dirty yet, and the camera (the root's, where its frame is not there). False: the HIST
// chunk is bad.
RDE_INTERNAL b8 fude_zoom_file_read_history(fude_zoom_file_reading* _rd) {
    fude_zoom_file*            _f         = _rd->f;
    fude_zoom_scene*           _s         = _rd->s;
    rde_file*                  _file      = _rd->file;
    rde_arr*                   _chunks    = _rd->chunks;
    const fude_zoom_chunk_ref* _c         = _rd->c;
    rde_arr*                   _buf       = _rd->buf;
    const u64                  _hist      = _rd->hist;
    const i32                  _fi        = _rd->fi;
    const i32                  _indx      = _rd->indx;
    const i32                  _foot      = _rd->foot;
    const fude_zoom_id         _cam_frame = _rd->cam_frame;
    const fude_zoom_camera     _cam       = _rd->cam;
    // The history.
    const i32 _hi = fude_zoom_file_find(_chunks, _hist, FUDE_ZOOM_TAG_HIST);
    u32 _hs = 0;
    u8* _hp = _hi >= 0 ? fude_zoom_file_payload(_file, &_c[_hi], _buf, &_hs) : NULL;
    if(_hp == NULL) {
        return false;
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
    return true;
}

// The journal since the footer replayed, and the file's size (where the replay stopped) and whether it was repaired
// (its end torn, or passed over) into the file's record.
RDE_INTERNAL void fude_zoom_file_read_journal(fude_zoom_file_reading* _rd) {
    fude_zoom_file*            _f    = _rd->f;
    fude_zoom_scene*           _s    = _rd->s;
    rde_file*                  _file = _rd->file;
    const fude_zoom_chunk_ref* _c    = _rd->c;
    const u32                  _n    = _rd->n;
    rde_arr*                   _buf  = _rd->buf;
    const i32                  _foot = _rd->foot;
    b8                         _torn = _rd->torn;
    // The journal since: every good JRNL after the footer, in order. Anything else after it (a checkpoint a kill
    // cut short) is passed over, a bad JRNL ends the replay, and either way the end is cut off by compacting once
    // open.
    _s->replaying = true;
    u64 _end = _c[_foot].at + 8u + _c[_foot].size;
    for(u32 _i = (u32)_foot + 1u; _i < _n; _i++) {
        if(_c[_i].tag != FUDE_ZOOM_TAG_JRNL) {
            _torn = true;
            continue;
        }
        u32 _js = 0;
        u8* _jp = fude_zoom_file_payload(_file, &_c[_i], _buf, &_js);
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
}

// Reads the file at _path into the scene (zfile.h's OPENING), setting _f's size, live bytes, journal bytes and
// repaired. Returns OK, MISSING or CORRUPT; CORRUPT leaves the scene half-filled. It never writes: the caller
// compacts a torn end away, or sets a damaged file aside.
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
    const b8 _torn = fude_zoom_file_read_chunks(_file, _file_size, &_chunks);
    const fude_zoom_chunk_ref* _c = (const fude_zoom_chunk_ref*)_chunks.memory;
    const u32                  _n = (u32)rde_arr_length(&_chunks);
    rde_arr _buf    = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());   // each chunk read in turn
    rde_arr _ix_buf = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());   // the index's, kept to the end
    fude_zoom_file_reading _rd;
    rde_memset(&_rd, 0, sizeof(_rd));
    _rd.f      = _f;
    _rd.s      = _s;
    _rd.file   = _file;
    _rd.chunks = &_chunks;
    _rd.c      = _c;
    _rd.n      = _n;
    _rd.buf    = &_buf;
    _rd.ix_buf = &_ix_buf;
    _rd.torn   = _torn;

    const i32 _foot = fude_zoom_file_read_footer(&_rd);
    FUDE_LOAD_ _result = FUDE_LOAD_CORRUPT;
    if(_foot < 0) {
        goto done;
    }

    {
        // The objects go into their frames' orders as they come and into no index; fude_zoom_scene_loaded sorts each
        // order and builds each index once (scene.h, loading). Nothing read on the way looks them up: a journal's ops
        // find objects and frames by id.
        _s->loading = true;
        if(!fude_zoom_file_read_index(&_rd)) {
            goto done;
        }
        if(!fude_zoom_file_read_frames(&_rd)) {
            goto done;
        }
        if(!fude_zoom_file_read_buckets(&_rd)) {
            goto done;
        }
        if(!fude_zoom_file_read_history(&_rd)) {
            goto done;
        }
        fude_zoom_file_read_journal(&_rd);
        _result       = FUDE_LOAD_OK;
    }

done:
    if(_result != FUDE_LOAD_OK) {
        _s->loading = false;   // (its callers throw it away; were one kept, it would not go on skipping its index)
    }
    rde_arr_free(&_ix_buf);
    rde_arr_free(&_buf);
    rde_arr_free(&_chunks);
    rde_file_close(_file);
    return _result;
}

// See the header. It reads through a fude_zoom_file of its own, which is then dropped; nothing is compacted or set
// aside.
FUDE_LOAD_ fude_zoom_file_peek(const c8* _path, fude_zoom_scene* _s) {
    fude_zoom_file _f;
    rde_memset(&_f, 0, sizeof(_f));
    rde_snprintf(_f.path, sizeof(_f.path), "%s", _path);
    const FUDE_LOAD_ _r = fude_zoom_file_read(&_f, _path, _s);
    if(_r != FUDE_LOAD_OK) {
        return _r;
    }
    fude_zoom_scene_loaded(_s);
    return _f.repaired ? FUDE_LOAD_RECOVERED : FUDE_LOAD_OK;
}

// See the header.
FUDE_LOAD_ fude_zoom_file_open(fude_zoom_file* _f, const c8* _path, fude_zoom_scene* _s) {
    rde_memset(_f, 0, sizeof(*_f));
    rde_snprintf(_f->path, sizeof(_f->path), "%s", _path);
    FUDE_LOAD_ _r = fude_zoom_file_read(_f, _path, _s);
    if(_r == FUDE_LOAD_MISSING || _r == FUDE_LOAD_CORRUPT) {
        // The main file damaged (kept as .bad, never overwritten) or missing: a kill between a compaction's two
        // renames leaves only the .bak, and without reading it the canvas would open empty and the next compaction
        // would write over the backup. The backup is read instead, as every save is (save.h).
        c8 _bak[RDE_MAX_PATH];
        rde_snprintf(_bak, sizeof(_bak), "%s.bak", _path);
        if(_r == FUDE_LOAD_MISSING && !rde_file_exists(_bak)) {
            return _r;   // nothing saved yet
        }
        const u32 _device = _s->device;
        fude_zoom_scene_destroy(_s);
        fude_zoom_scene_init(_s, _device);
        if(_r == FUDE_LOAD_CORRUPT) {
            fude_file_set_aside(_path);
        }
        fude_zoom_file _from_bak;
        rde_memset(&_from_bak, 0, sizeof(_from_bak));
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
