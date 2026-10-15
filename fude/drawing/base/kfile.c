// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/kfile.h"

// ===========================================================================
// See kfile.h.
// ===========================================================================

// --- writing -------------------------------------------------------------------

// Makes an empty byte array on the std allocator, with room for _capacity bytes (at least 1).
fude_bytes fude_bytes_new(u32 _capacity) {
    return rde_arr_new_with_capacity(sizeof(u8), _capacity > 0 ? _capacity : 1u, rde_memory_allocator_get_default_std());
}

// Returns how many bytes have been written so far.
u32 fude_bytes_size(const fude_bytes* _b) {
    return (u32)rde_arr_length(_b);
}

// Appends one byte.
void fude_put_u8(fude_bytes* _b, u8 _v) {
    rde_arr_add(_b, &_v);
}

// Appends a u16, little-endian.
void fude_put_u16(fude_bytes* _b, u16 _v) {
    u8* _p = rde_arr_add_n(_b, 2u);
    _p[0] = (u8)(_v);
    _p[1] = (u8)(_v >> 8);
}

// Appends an i16 as its two's-complement u16.
void fude_put_i16(fude_bytes* _b, i16 _v) {
    fude_put_u16(_b, (u16)_v);
}

// Appends a u32, little-endian.
void fude_put_u32(fude_bytes* _b, u32 _v) {
    u8* _p = rde_arr_add_n(_b, 4u);
    _p[0] = (u8)(_v);
    _p[1] = (u8)(_v >> 8);
    _p[2] = (u8)(_v >> 16);
    _p[3] = (u8)(_v >> 24);
}

// Appends an f32 as its IEEE-754 bits, written as a u32.
void fude_put_f32(fude_bytes* _b, f32 _v) {
    u32 _bits;
    rde_memcpy(&_bits, &_v, sizeof(_bits));
    fude_put_u32(_b, _bits);
}

// Appends a colour as four bytes: r, g, b, a.
void fude_put_color(fude_bytes* _b, rde_color _c) {
    fude_put_u8(_b, _c.r);
    fude_put_u8(_b, _c.g);
    fude_put_u8(_b, _c.b);
    fude_put_u8(_b, _c.a);
}

// Appends _size raw bytes from _data; nothing when _size is 0.
void fude_put_data(fude_bytes* _b, const any _data, u32 _size) {
    if(_size > 0) {
        rde_memcpy(rde_arr_add_n(_b, _size), _data, _size);
    }
}

// Appends the 12-byte header: "KANA", the version and the kind.
void fude_put_header(fude_bytes* _b, u32 _version, u32 _kind) {
    fude_put_data(_b, "KANA", 4u);
    fude_put_u32(_b, _version);
    fude_put_u32(_b, _kind);
}

// Appends a chunk's tag and a size of 0 to fill in later. Returns the offset of the size, for fude_chunk_end.
u32 fude_chunk_begin(fude_bytes* _b, u32 _tag) {
    fude_put_u32(_b, _tag);
    const u32 _at = fude_bytes_size(_b);
    fude_put_u32(_b, 0u);
    return _at;
}

// Fills in the size at offset _at with the number of bytes written after it.
void fude_chunk_end(fude_bytes* _b, u32 _at) {
    const u32 _size = fude_bytes_size(_b) - (_at + 4u);
    u8*       _p    = rde_arr_at(_b, _at);
    _p[0] = (u8)(_size);
    _p[1] = (u8)(_size >> 8);
    _p[2] = (u8)(_size >> 16);
    _p[3] = (u8)(_size >> 24);
}

// Writes _size bytes to <_path>.tmp, renames the current file (if any) to <_path>.bak, then renames the .tmp to _path.
// A kill before the last rename leaves the previous save as the .bak, which fude_save_load reads when the main file is
// missing. Returns false, and logs it, when the .tmp could not be written or moved into place; in the second case the
// .bak is moved back to _path if _path is gone.
RDE_INTERNAL b8 fude_file_write_atomic(const c8* _path, const u8* _data, u32 _size) {
    c8 _tmp[RDE_MAX_PATH];
    c8 _bak[RDE_MAX_PATH];
    rde_snprintf(_tmp, sizeof(_tmp), "%s.tmp", _path);
    rde_snprintf(_bak, sizeof(_bak), "%s.bak", _path);

    b8 _written = false;
    RDE_TRY({
        rde_file* _file = rde_file_open(_tmp, RDE_FILE_MODE_WRITE_BYTES);
        if(_file != NULL && !rde_failed()) {
            rde_file_write_bytes(_file, _data, _size);
            // On the disk before the renames: otherwise a power cut just after them can leave the new name on a file
            // whose bytes never reached the disk (an empty page), with the good copy already renamed to the .bak.
            _written = !rde_failed() && rde_file_sync(_file);
            rde_file_close(_file);
        }
    });

    if(!_written) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "file: could not write %s", _tmp);
        return false;
    }

    // The previous version becomes the backup, then the new one takes its name. Each rename is atomic, but between
    // the two there is no file at _path; the load then reads the .bak (save.c).
    if(rde_file_exists(_path)) {
        rde_file_move(_path, _bak);
    }

    if(!rde_file_move(_tmp, _path)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "file: could not move %s into place", _tmp);
        if(!rde_file_exists(_path) && rde_file_exists(_bak)) {
            rde_file_move(_bak, _path);
        }
        return false;
    }

    return true;
}

// Writes the bytes atomically (fude_file_write_atomic) and frees them, whether or not the write worked.
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

// Makes a reader at the start of _data; with a NULL _data it has no bytes and is already not ok.
fude_reader fude_reader_make(const u8* _data, u32 _size) {
    return (fude_reader){ .data = _data, .size = _data != NULL ? _size : 0u, .pos = 0u, .ok = _data != NULL };
}

// Returns whether _n more bytes are left to read; when they are not, the reader turns not ok for good.
b8 fude_reader_has(fude_reader* _r, u32 _n) {
    if(!_r->ok || _r->size - _r->pos < _n) {
        _r->ok = false;
        return false;
    }
    return true;
}

// Reads one byte; 0 past the end.
u8 fude_get_u8(fude_reader* _r) {
    return fude_reader_has(_r, 1u) ? _r->data[_r->pos++] : 0u;
}

// Reads a little-endian u16; 0 past the end.
u16 fude_get_u16(fude_reader* _r) {
    if(!fude_reader_has(_r, 2u)) {
        return 0u;
    }

    const u8* _p = &_r->data[_r->pos];
    _r->pos += 2u;
    return (u16)((u16)_p[0] | ((u16)_p[1] << 8));
}

// Reads an i16 written by fude_put_i16; 0 past the end.
i16 fude_get_i16(fude_reader* _r) {
    return (i16)fude_get_u16(_r);
}

// Reads a little-endian u32; 0 past the end.
u32 fude_get_u32(fude_reader* _r) {
    if(!fude_reader_has(_r, 4u)) {
        return 0u;
    }

    const u8* _p = &_r->data[_r->pos];
    _r->pos += 4u;
    return (u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24);
}

// Reads an f32 from its IEEE-754 bits; 0 past the end.
f32 fude_get_f32(fude_reader* _r) {
    const u32 _bits = fude_get_u32(_r);
    f32 _v;
    rde_memcpy(&_v, &_bits, sizeof(_v));
    return _v;
}

// Reads a colour as r, g, b, a; zeros past the end.
rde_color fude_get_color(fude_reader* _r) {
    rde_color _c;
    _c.r = fude_get_u8(_r);
    _c.g = fude_get_u8(_r);
    _c.b = fude_get_u8(_r);
    _c.a = fude_get_u8(_r);
    return _c;
}

// Checks the header at the very start of the data, whatever the reader's position, and leaves the reader just after
// it. Returns true for "KANA", a version from 1 to _max_version, and _kind.
b8 fude_read_header(fude_reader* _r, u32 _max_version, u32 _kind) {
    if(!fude_reader_has(_r, FUDE_FILE_HEADER_SIZE) || rde_memcmp(_r->data, "KANA", 4) != 0) {
        return false;
    }

    _r->pos = 4u;
    const u32 _version   = fude_get_u32(_r);
    const u32 _file_kind = fude_get_u32(_r);
    return _r->ok && _version >= 1u && _version <= _max_version && _file_kind == _kind;
}

// Reads the next chunk's tag and size, and makes *_chunk a reader over its payload alone. Returns false at the end of
// the data, or when the size runs past it (the file reader then turns not ok).
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
    _chunk->ok  = true;   // An empty chunk is still a valid chunk.
    _file->pos += _len;
    return true;
}

// Reads the whole file into _out (cleared first). Returns false, with _out empty, when it could not be read whole.
b8 fude_file_read_into(const c8* _path, rde_arr* _out) {
    rde_arr_clear(_out);
    b8 _ok = false;
    RDE_TRY({
        rde_file* _file = rde_file_open(_path, RDE_FILE_MODE_READ_BYTES);
        if(_file != NULL && !rde_failed()) {
            const u64 _n = rde_file_get_size(_file);
            _ok = _n == 0u || rde_file_read_at(_file, 0u, (u8*)rde_arr_add_n(_out, (usize)_n), (usize)_n) == (usize)_n;
            rde_file_close(_file);
        }
    });
    if(!_ok) {
        rde_arr_clear(_out);
    }
    return _ok;
}

// Returns the file's size in bytes, or 0 when it is missing or cannot be opened.
u64 fude_file_size(const c8* _path) {
    if(!rde_file_exists(_path)) {
        return 0u;   // Checked first, because RDE reports opening a missing file as an error.
    }
    u64 _n = 0u;
    RDE_TRY({
        rde_file* _file = rde_file_open(_path, RDE_FILE_MODE_READ_BYTES);
        if(_file != NULL && !rde_failed()) {
            _n = rde_file_get_size(_file);
            rde_file_close(_file);
        }
    });
    return _n;
}

// Reads the start of a text file into _out, NUL-terminated and cut to _size - 1 bytes. Returns how many bytes it
// copied. It reads the whole file first, so it is meant for short files.
usize fude_file_read_text(const c8* _path, c8* _out, usize _size) {
    if(_size == 0u) {
        return 0u;
    }
    _out[0] = 0;
    rde_arr _bytes = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    usize   _n     = 0u;
    if(fude_file_read_into(_path, &_bytes)) {
        _n = rde_arr_length(&_bytes) < _size - 1u ? rde_arr_length(&_bytes) : _size - 1u;
        rde_memcpy(_out, _bytes.memory, _n);
        _out[_n] = 0;
    }
    rde_arr_free(&_bytes);
    return _n;
}

// Writes _size bytes as the whole file at _path, with no .tmp or .bak. On failure the partial file is deleted.
b8 fude_file_write(const c8* _path, const any _data, usize _size) {
    b8 _ok = false;
    RDE_TRY({
        rde_file* _file = rde_file_open(_path, RDE_FILE_MODE_WRITE_BYTES);
        if(_file != NULL && !rde_failed()) {
            rde_file_write_bytes(_file, (const u8*)_data, _size);
            _ok = !rde_failed();
            rde_file_close(_file);
            _ok = _ok && !rde_failed();
        }
    });
    if(!_ok && rde_file_exists(_path)) {
        rde_file_delete(_path);
    }
    return _ok;
}

// Reads the whole file into new memory on the std allocator, which the caller frees with fude_file_free. Returns
// NULL, *_size 0, when it cannot be read or is larger than 0xFFFFFFF0 bytes (just under 4 GiB).
u8* fude_file_read(const c8* _path, u32* _size) {
    u8*   _data = NULL;
    usize _len  = 0;
    RDE_TRY({
        rde_file* _file = rde_file_open(_path, RDE_FILE_MODE_READ_BYTES);
        if(_file != NULL && !rde_failed()) {
            // On the standard heap, because the files read are as big as the user made them (a page, the notes'
            // index), more than the engine's fixed pool is sized for.
            _data = rde_file_read_full_file_bytes(_file, &_len, rde_memory_allocator_get_default_std());
            rde_file_close(_file);
        }
    });

    if(_data != NULL && _len > 0xFFFFFFF0u) {
        fude_file_free(_data);   // Too big for a u32 size: refused rather than read cut short.
        _data = NULL;
    }

    *_size = _data != NULL ? (u32)_len : 0u;
    return _data;
}

// Frees memory from fude_file_read, through the std allocator. NULL does nothing.
void fude_file_free(u8* _data) {
    if(_data != NULL) {
        rde_memory_allocator* _allocator = rde_memory_allocator_get_default_std();
        _allocator->free(_allocator->allocator, _data);
    }
}

// Renames a file that could not be parsed to <_path>.bad and logs it, so that no save overwrites it. When an earlier
// one is there already (the same file damaged twice), this one is <_path>.2.bad, .3.bad and on: the first damaged copy
// may be the one that still holds the most, so it is never replaced.
void fude_file_set_aside(const c8* _path) {
    c8 _bad[RDE_MAX_PATH];
    rde_snprintf(_bad, sizeof(_bad), "%s.bad", _path);
    for(u32 _n = 2u; rde_file_exists(_bad) && _n < FUDE_FILE_ASIDE_MAX; _n++) {
        rde_snprintf(_bad, sizeof(_bad), "%s.%u.bad", _path, _n);
    }
    rde_file_move(_path, _bad);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "file: %s could not be read; kept as %s", _path, _bad);
}

// --- 64 bits: two 32-bit halves, the low one first (every number is little-endian) ----------

// Appends a u64 as two u32 halves, the low one first.
void fude_put_u64(fude_bytes* _b, u64 _v) {
    fude_put_u32(_b, (u32)_v);
    fude_put_u32(_b, (u32)(_v >> 32));
}

// Appends an f64 as its IEEE-754 bits, written as a u64.
void fude_put_f64(fude_bytes* _b, f64 _v) {
    u64 _bits;
    rde_memcpy(&_bits, &_v, sizeof(_bits));
    fude_put_u64(_b, _bits);
}

// Reads a u64 written as two u32 halves, the low one first; 0 past the end.
u64 fude_get_u64(fude_reader* _r) {
    const u64 _lo = fude_get_u32(_r);
    const u64 _hi = fude_get_u32(_r);
    return _lo | (_hi << 32);
}

// Reads an f64 from its IEEE-754 bits; 0 past the end.
f64 fude_get_f64(fude_reader* _r) {
    const u64 _bits = fude_get_u64(_r);
    f64       _v;
    rde_memcpy(&_v, &_bits, sizeof(_v));
    return _v;
}
