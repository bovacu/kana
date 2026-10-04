// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/kfile.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See kfile.h.
// ===========================================================================

// --- writing -------------------------------------------------------------------

fude_bytes fude_bytes_new(u32 _capacity) {
    return rde_arr_new_with_capacity(sizeof(u8), _capacity > 0 ? _capacity : 1u, rde_memory_allocator_get_default_std());
}

u32 fude_bytes_size(const fude_bytes* _b) {
    return (u32)rde_arr_length(_b);
}

void fude_put_u8(fude_bytes* _b, u8 _v) {
    rde_arr_add(_b, &_v);
}

void fude_put_u16(fude_bytes* _b, u16 _v) {
    u8* _p = rde_arr_add_n(_b, 2u);
    _p[0] = (u8)(_v);
    _p[1] = (u8)(_v >> 8);
}

void fude_put_i16(fude_bytes* _b, i16 _v) {
    fude_put_u16(_b, (u16)_v);
}

void fude_put_u32(fude_bytes* _b, u32 _v) {
    u8* _p = rde_arr_add_n(_b, 4u);
    _p[0] = (u8)(_v);
    _p[1] = (u8)(_v >> 8);
    _p[2] = (u8)(_v >> 16);
    _p[3] = (u8)(_v >> 24);
}

void fude_put_f32(fude_bytes* _b, f32 _v) {
    u32 _bits;
    memcpy(&_bits, &_v, sizeof(_bits));
    fude_put_u32(_b, _bits);
}

void fude_put_color(fude_bytes* _b, rde_color _c) {
    fude_put_u8(_b, _c.r);
    fude_put_u8(_b, _c.g);
    fude_put_u8(_b, _c.b);
    fude_put_u8(_b, _c.a);
}

void fude_put_data(fude_bytes* _b, const void* _data, u32 _size) {
    if(_size > 0) {
        memcpy(rde_arr_add_n(_b, _size), _data, _size);
    }
}

void fude_put_header(fude_bytes* _b, u32 _version, u32 _kind) {
    fude_put_data(_b, "KANA", 4u);
    fude_put_u32(_b, _version);
    fude_put_u32(_b, _kind);
}

u32 fude_chunk_begin(fude_bytes* _b, u32 _tag) {
    fude_put_u32(_b, _tag);
    const u32 _at = fude_bytes_size(_b);
    fude_put_u32(_b, 0u);
    return _at;
}

void fude_chunk_end(fude_bytes* _b, u32 _at) {
    const u32 _size = fude_bytes_size(_b) - (_at + 4u);
    u8*       _p    = rde_arr_at(_b, _at);
    _p[0] = (u8)(_size);
    _p[1] = (u8)(_size >> 8);
    _p[2] = (u8)(_size >> 16);
    _p[3] = (u8)(_size >> 24);
}

RDE_INTERNAL b8 fude_file_write_atomic(const c8* _path, const u8* _data, u32 _size) {
    c8 _tmp[RDE_MAX_PATH];
    c8 _bak[RDE_MAX_PATH];
    snprintf(_tmp, sizeof(_tmp), "%s.tmp", _path);
    snprintf(_bak, sizeof(_bak), "%s.bak", _path);

    b8 _written = false;
    RDE_TRY({
        rde_file* _file = rde_file_open(_tmp, RDE_FILE_MODE_WRITE_BYTES);
        if(_file != NULL && !rde_failed()) {
            rde_file_write_bytes(_file, _data, _size);
            _written = !rde_failed();
            rde_file_close(_file);
        }
    });

    if(!_written) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "fude: could not write %s", _tmp);
        return false;
    }

    // The previous version becomes the backup, then the new one takes its name.
    // Rename replaces the destination atomically.
    if(rde_file_exists(_path)) {
        rde_file_move(_path, _bak);
    }

    if(!rde_file_move(_tmp, _path)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "fude: could not move %s into place", _tmp);
        if(!rde_file_exists(_path) && rde_file_exists(_bak)) {
            rde_file_move(_bak, _path);
        }
        return false;
    }

    return true;
}

b8 fude_bytes_write_and_free(fude_bytes* _b, const c8* _path, u32* _out_bytes) {
    const u32 _size = fude_bytes_size(_b);
    const b8  _ok   = fude_file_write_atomic(_path, (const u8*)_b->memory, _size);
    if(_out_bytes != NULL) {
        *_out_bytes = _size;
    }
    rde_arr_free(_b);
    return _ok;
}

// --- reading -------------------------------------------------------------------

fude_reader fude_reader_make(const u8* _data, u32 _size) {
    return (fude_reader){ .data = _data, .size = _data != NULL ? _size : 0u, .pos = 0u, .ok = _data != NULL };
}

b8 fude_reader_has(fude_reader* _r, u32 _n) {
    if(!_r->ok || _r->size - _r->pos < _n) {
        _r->ok = false;
        return false;
    }
    return true;
}

u8 fude_get_u8(fude_reader* _r) {
    return fude_reader_has(_r, 1u) ? _r->data[_r->pos++] : 0u;
}

u16 fude_get_u16(fude_reader* _r) {
    if(!fude_reader_has(_r, 2u)) {
        return 0u;
    }

    const u8* _p = &_r->data[_r->pos];
    _r->pos += 2u;
    return (u16)((u16)_p[0] | ((u16)_p[1] << 8));
}

i16 fude_get_i16(fude_reader* _r) {
    return (i16)fude_get_u16(_r);
}

u32 fude_get_u32(fude_reader* _r) {
    if(!fude_reader_has(_r, 4u)) {
        return 0u;
    }

    const u8* _p = &_r->data[_r->pos];
    _r->pos += 4u;
    return (u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24);
}

f32 fude_get_f32(fude_reader* _r) {
    const u32 _bits = fude_get_u32(_r);
    f32 _v;
    memcpy(&_v, &_bits, sizeof(_v));
    return _v;
}

rde_color fude_get_color(fude_reader* _r) {
    rde_color _c;
    _c.r = fude_get_u8(_r);
    _c.g = fude_get_u8(_r);
    _c.b = fude_get_u8(_r);
    _c.a = fude_get_u8(_r);
    return _c;
}

b8 fude_read_header(fude_reader* _r, u32 _max_version, u32 _kind) {
    if(!fude_reader_has(_r, FUDE_FILE_HEADER_SIZE) || memcmp(_r->data, "KANA", 4) != 0) {
        return false;
    }

    _r->pos = 4u;
    const u32 _version   = fude_get_u32(_r);
    const u32 _file_kind = fude_get_u32(_r);
    return _r->ok && _version >= 1u && _version <= _max_version && _file_kind == _kind;
}

b8 fude_next_chunk(fude_reader* _file, u32* _tag, fude_reader* _chunk) {
    if(!_file->ok || _file->pos >= _file->size) {
        return false;
    }

    *_tag           = fude_get_u32(_file);
    const u32 _len  = fude_get_u32(_file);
    if(!_file->ok || _len > _file->size - _file->pos) {
        _file->ok = false;
        return false;
    }

    *_chunk     = fude_reader_make(&_file->data[_file->pos], _len);
    _chunk->ok  = true;   // an empty chunk is still a chunk
    _file->pos += _len;
    return true;
}

u8* fude_file_read(const c8* _path, u32* _size) {
    u8*   _data = NULL;
    usize _len  = 0;
    RDE_TRY({
        rde_file* _file = rde_file_open(_path, RDE_FILE_MODE_READ_BYTES);
        if(_file != NULL && !rde_failed()) {
            _data = rde_file_read_full_file_bytes(_file, &_len, rde_memory_allocator_get_default());
            rde_file_close(_file);
        }
    });

    if(_data != NULL && _len > 0xFFFFFFF0u) {
        fude_file_free(_data);
        _data = NULL;
    }

    *_size = (u32)_len;
    return _data;
}

void fude_file_free(u8* _data) {
    if(_data != NULL) {
        rde_memory_allocator* _allocator = rde_memory_allocator_get_default();
        _allocator->free(_allocator->allocator, _data);
    }
}

void fude_file_set_aside(const c8* _path) {
    c8 _bad[RDE_MAX_PATH];
    snprintf(_bad, sizeof(_bad), "%s.bad", _path);
    rde_file_move(_path, _bad);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "fude: %s could not be read; kept as %s", _path, _bad);
}
