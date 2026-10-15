// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/audio.h"
#include "zoom/adpcm.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See audio.h.
//
// The recordings and the links are two arrays on the std allocator. The recordings are few and searched in full; the
// links (one for every thing written while recording: thousands) are found through a map from the object's id to its
// place (link_at), so adding each one and finding one under the pen are not a search through all of them. The .audio
// file is written whole on every save. The file's own names start with FZU_ and fzu_.
// ===========================================================================

// The .audio file's format version and kind (the layout is in audio.h).
#define FZU_VERSION 1u
#define FZU_KIND    0x49445541u   // 'AUDI'
#define FZU_MOST    1000000u      // recordings or links a file may claim, at most: a larger count is taken as damage

// See the header. The arrays and the map are on the std allocator, since they grow with the canvas.
void fude_zoom_audio_init(fude_zoom_audio* _a) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _a->recordings = rde_arr_new(sizeof(fude_zoom_recording), _heap);
    _a->links      = rde_arr_new(sizeof(fude_zoom_audio_link), _heap);
    _a->link_at    = rde_hash_map_new(sizeof(fude_zoom_id), sizeof(u32), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, _heap);
    _a->next       = 1u;
}

// See the header.
void fude_zoom_audio_free(fude_zoom_audio* _a) {
    rde_arr_free(&_a->recordings);
    rde_arr_free(&_a->links);
    rde_hash_map_free(&_a->link_at);
}

// See the header.
void fude_zoom_audio_clear(fude_zoom_audio* _a) {
    rde_arr_clear(&_a->recordings);
    rde_arr_clear(&_a->links);
    rde_hash_map_clear(&_a->link_at);
    _a->next = 1u;
}

// Makes link_at again from the links: each object's place is its last, the newest, as searching from the newest found.
RDE_INTERNAL void fzu_index_links(fude_zoom_audio* _a) {
    const fude_zoom_audio_link* _l = (const fude_zoom_audio_link*)_a->links.memory;
    const u32                   _n = (u32)rde_arr_length(&_a->links);
    rde_hash_map_clear(&_a->link_at);
    rde_hash_map_reserve(&_a->link_at, _n);
    for(u32 _i = 0; _i < _n; _i++) {
        if(!rde_hash_map_add(&_a->link_at, &_l[_i].object, &_i)) {
            rde_hash_map_update_entry(&_a->link_at, &_l[_i].object, &_i);
        }
    }
}

// Writes a u64 as two u32s, the low half first, as audio.h's layout has it.
RDE_INTERNAL void fzu_put_u64(fude_bytes* _b, u64 _v) {
    fude_put_u32(_b, (u32)(_v & 0xFFFFFFFFu));
    fude_put_u32(_b, (u32)(_v >> 32));
}

// Reads a u64 written by fzu_put_u64.
RDE_INTERNAL u64 fzu_get_u64(fude_reader* _r) {
    const u64 _lo = fude_get_u32(_r), _hi = fude_get_u32(_r);
    return _lo | (_hi << 32);
}

// See the header. The buffer is sized for the whole file at once: 20 bytes a recording and 16 a link.
b8 fude_zoom_audio_save(const fude_zoom_audio* _a, const c8* _path) {
    const u32 _nr = (u32)rde_arr_length(&_a->recordings), _nl = (u32)rde_arr_length(&_a->links);
    fude_bytes _b = fude_bytes_new(64u + _nr * 20u + _nl * 16u);
    fude_put_header(&_b, FZU_VERSION, FZU_KIND);
    fude_put_u32(&_b, _a->next);
    fude_put_u32(&_b, _nr);
    const fude_zoom_recording* _r = (const fude_zoom_recording*)_a->recordings.memory;
    for(u32 _i = 0; _i < _nr; _i++) {
        fude_put_u32(&_b, _r[_i].number);
        fzu_put_u64(&_b, _r[_i].started);
        fude_put_u32(&_b, _r[_i].rate);
        fude_put_u32(&_b, _r[_i].frames);
    }
    fude_put_u32(&_b, _nl);
    const fude_zoom_audio_link* _l = (const fude_zoom_audio_link*)_a->links.memory;
    for(u32 _i = 0; _i < _nl; _i++) {
        fzu_put_u64(&_b, _l[_i].object);
        fude_put_u32(&_b, _l[_i].recording);
        fude_put_u32(&_b, _l[_i].at_ms);
    }
    return fude_bytes_write_and_free(&_b, _path, NULL);
}

// See the header. Anything wrong in the file (a short read, a count past FZU_MOST, a bad rate) drops it all, so a
// damaged file never gives half a list.
b8 fude_zoom_audio_load(fude_zoom_audio* _a, const c8* _path) {
    fude_zoom_audio_clear(_a);
    if(!rde_file_exists(_path)) {
        return false;
    }
    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        return false;
    }
    fude_reader _r = fude_reader_make(_data, _size);
    b8 _ok = fude_read_header(&_r, FZU_VERSION, FZU_KIND);
    const u32 _next = _ok ? fude_get_u32(&_r) : 1u;
    const u32 _nr   = _ok ? fude_get_u32(&_r) : 0u;
    _ok = _ok && _r.ok && _nr <= FZU_MOST;
    for(u32 _i = 0; _ok && _i < _nr; _i++) {
        fude_zoom_recording _rec;
        _rec.number  = fude_get_u32(&_r);
        _rec.started = fzu_get_u64(&_r);
        _rec.rate    = fude_get_u32(&_r);
        _rec.frames  = fude_get_u32(&_r);
        _ok = _r.ok && _rec.rate > 0u && _rec.rate <= 192000u;
        if(_ok) {
            rde_arr_add(&_a->recordings, (any)&_rec);
        }
    }
    const u32 _nl = _ok ? fude_get_u32(&_r) : 0u;
    _ok = _ok && _r.ok && _nl <= FZU_MOST;
    for(u32 _i = 0; _ok && _i < _nl; _i++) {
        fude_zoom_audio_link _l;
        _l.object    = fzu_get_u64(&_r);
        _l.recording = fude_get_u32(&_r);
        _l.at_ms     = fude_get_u32(&_r);
        _ok = _r.ok;
        if(_ok) {
            rde_arr_add(&_a->links, (any)&_l);
        }
    }
    fude_file_free(_data);
    if(!_ok) {
        rde_arr_clear(&_a->recordings);
        rde_arr_clear(&_a->links);
        return false;
    }
    fzu_index_links(_a);
    // (the next number past every one there, whatever the file said)
    _a->next = _next;
    const fude_zoom_recording* _rec = (const fude_zoom_recording*)_a->recordings.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_a->recordings); _i++) {
        _a->next = _rec[_i].number >= _a->next ? _rec[_i].number + 1u : _a->next;
    }
    return true;
}

// See the header. The recording takes the next number; its length is 0 until the page sets it when it ends.
u32 fude_zoom_audio_begin(fude_zoom_audio* _a, u64 _started, u32 _rate) {
    fude_zoom_recording _r = { _a->next++, _started, _rate, 0u };
    rde_arr_add(&_a->recordings, (any)&_r);
    return _r.number;
}

// See the header.
fude_zoom_recording* fude_zoom_audio_find(fude_zoom_audio* _a, u32 _number) {
    fude_zoom_recording* _r = (fude_zoom_recording*)_a->recordings.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_a->recordings); _i++) {
        if(_r[_i].number == _number) {
            return &_r[_i];
        }
    }
    return NULL;
}

// See the header. Both arrays are compacted in place, keeping their order, and the links' places found again.
void fude_zoom_audio_remove(fude_zoom_audio* _a, u32 _number) {
    fude_zoom_recording* _r = (fude_zoom_recording*)_a->recordings.memory;
    u32 _w = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_a->recordings); _i++) {
        if(_r[_i].number != _number) {
            _r[_w++] = _r[_i];
        }
    }
    rde_arr_resize(&_a->recordings, _w);
    fude_zoom_audio_link* _l = (fude_zoom_audio_link*)_a->links.memory;
    _w = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_a->links); _i++) {
        if(_l[_i].recording != _number) {
            _l[_w++] = _l[_i];
        }
    }
    rde_arr_resize(&_a->links, _w);
    fzu_index_links(_a);
}

// See the header. An object in link_at is linked already; a new link's place is the end of links.
void fude_zoom_audio_add_link(fude_zoom_audio* _a, fude_zoom_id _object, u32 _number, u32 _at_ms) {
    const u32 _at = (u32)rde_arr_length(&_a->links);
    if(!rde_hash_map_add(&_a->link_at, &_object, &_at)) {
        return;
    }
    const fude_zoom_audio_link _l = { _object, _number, _at_ms };
    rde_arr_add(&_a->links, (any)&_l);
}

// See the header. The place is copied out of the map's entry, which is packed (rde_hash_map).
b8 fude_zoom_audio_link_of(const fude_zoom_audio* _a, fude_zoom_id _object, u32* _number, u32* _at_ms) {
    const any _found = rde_hash_map_find(&_a->link_at, &_object);
    if(_found == NULL) {
        return false;
    }
    u32 _at;
    rde_memcpy(&_at, _found, sizeof(_at));
    const fude_zoom_audio_link* _l = &((const fude_zoom_audio_link*)_a->links.memory)[_at];
    *_number = _l->recording;
    *_at_ms  = _l->at_ms;
    return true;
}

// See the header.
u32 fude_zoom_audio_link_count(const fude_zoom_audio* _a, u32 _number) {
    const fude_zoom_audio_link* _l = (const fude_zoom_audio_link*)_a->links.memory;
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_a->links); _i++) {
        _n += _l[_i].recording == _number ? 1u : 0u;
    }
    return _n;
}

// See the header.
f64 fude_zoom_audio_begun(f64 _down, f64 _down_linked, f64 _started, f64 _now) {
    return _down >= _started && _down > _down_linked && _now - _down < FUDE_ZOOM_AUDIO_DOWN_MOST ? _down : _now;
}

// See the header. A length past a u32's range is held at its largest.
u32 fude_zoom_audio_frames_in(u64 _file_bytes) {
    const u64 _frames = (_file_bytes / FUDE_ZOOM_ADPCM_BLOCK) * FUDE_ZOOM_ADPCM_FRAMES;
    return _frames > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)_frames;
}
