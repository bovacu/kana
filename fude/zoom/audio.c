// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/audio.h"
#include "zoom/adpcm.h"
#include "drawing/base/kfile.h"

#include <string.h>

#define FZU_VERSION 1u
#define FZU_KIND    0x49445541u   // 'AUDI'
#define FZU_MOST    1000000u      // links or recordings read, at most (a damaged count read as none past it)

void fude_zoom_audio_init(fude_zoom_audio* _a) {
    _a->recordings = rde_arr_new(sizeof(fude_zoom_recording), rde_memory_allocator_get_default_std());
    _a->links      = rde_arr_new(sizeof(fude_zoom_audio_link), rde_memory_allocator_get_default_std());
    _a->next       = 1u;
}

void fude_zoom_audio_free(fude_zoom_audio* _a) {
    rde_arr_free(&_a->recordings);
    rde_arr_free(&_a->links);
}

RDE_INTERNAL void fzu_put_u64(fude_bytes* _b, u64 _v) {
    fude_put_u32(_b, (u32)(_v & 0xFFFFFFFFu));
    fude_put_u32(_b, (u32)(_v >> 32));
}

RDE_INTERNAL u64 fzu_get_u64(fude_reader* _r) {
    const u64 _lo = fude_get_u32(_r), _hi = fude_get_u32(_r);
    return _lo | (_hi << 32);
}

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

b8 fude_zoom_audio_load(fude_zoom_audio* _a, const c8* _path) {
    rde_arr_clear(&_a->recordings);
    rde_arr_clear(&_a->links);
    _a->next = 1u;
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
    // (the next number past every one there, whatever the file said)
    _a->next = _next;
    const fude_zoom_recording* _rec = (const fude_zoom_recording*)_a->recordings.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_a->recordings); _i++) {
        _a->next = _rec[_i].number >= _a->next ? _rec[_i].number + 1u : _a->next;
    }
    return true;
}

u32 fude_zoom_audio_begin(fude_zoom_audio* _a, u64 _started, u32 _rate) {
    fude_zoom_recording _r = { _a->next++, _started, _rate, 0u };
    rde_arr_add(&_a->recordings, (any)&_r);
    return _r.number;
}

fude_zoom_recording* fude_zoom_audio_find(fude_zoom_audio* _a, u32 _number) {
    fude_zoom_recording* _r = (fude_zoom_recording*)_a->recordings.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_a->recordings); _i++) {
        if(_r[_i].number == _number) {
            return &_r[_i];
        }
    }
    return NULL;
}

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
}

void fude_zoom_audio_add_link(fude_zoom_audio* _a, fude_zoom_id _object, u32 _number, u32 _at_ms) {
    u32 _n = 0, _ms = 0;
    if(fude_zoom_audio_link_of(_a, _object, &_n, &_ms)) {
        return;
    }
    const fude_zoom_audio_link _l = { _object, _number, _at_ms };
    rde_arr_add(&_a->links, (any)&_l);
}

b8 fude_zoom_audio_link_of(const fude_zoom_audio* _a, fude_zoom_id _object, u32* _number, u32* _at_ms) {
    const fude_zoom_audio_link* _l = (const fude_zoom_audio_link*)_a->links.memory;
    for(u32 _i = (u32)rde_arr_length(&_a->links); _i-- > 0u;) {
        if(_l[_i].object == _object) {
            *_number = _l[_i].recording;
            *_at_ms  = _l[_i].at_ms;
            return true;
        }
    }
    return false;
}

u32 fude_zoom_audio_link_count(const fude_zoom_audio* _a, u32 _number) {
    const fude_zoom_audio_link* _l = (const fude_zoom_audio_link*)_a->links.memory;
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_a->links); _i++) {
        _n += _l[_i].recording == _number ? 1u : 0u;
    }
    return _n;
}

u32 fude_zoom_audio_frames_in(u64 _file_bytes) {
    const u64 _frames = (_file_bytes / FUDE_ZOOM_ADPCM_BLOCK) * FUDE_ZOOM_ADPCM_FRAMES;
    return _frames > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)_frames;
}
