#include "save.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See save.h.
// ===========================================================================

#define KANA_TAG(_a, _b, _c, _d) ((u32)(u8)(_a) | ((u32)(u8)(_b) << 8) | ((u32)(u8)(_c) << 16) | ((u32)(u8)(_d) << 24))

#define KANA_KIND_DOCUMENT  KANA_TAG('D', 'O', 'C', ' ')
#define KANA_KIND_SETTINGS  KANA_TAG('S', 'E', 'T', 'T')

#define KANA_CHUNK_VIEW     KANA_TAG('V', 'I', 'E', 'W')
#define KANA_CHUNK_STROKES  KANA_TAG('S', 'T', 'R', 'K')
#define KANA_CHUNK_POINTS   KANA_TAG('P', 'N', 'T', 'S')
#define KANA_CHUNK_PREFS    KANA_TAG('P', 'R', 'E', 'F')

#define KANA_HEADER_SIZE        12u
#define KANA_STROKE_RECORD_SIZE 12u
#define KANA_POINT_RECORD_SIZE  20u

#define KANA_STROKE_FLAG_FROM_PEN 0x01u

// --- writing -------------------------------------------------------------------
//
// The file is built in an rde_arr of bytes, then written in one go.

typedef rde_arr TYPE(u8) kana_bytes;

RDE_INTERNAL kana_bytes kana_bytes_new(u32 _capacity) {
    // The standard heap, like the ink: a page's size is the user's to decide.
    return rde_arr_new_with_capacity(sizeof(u8), _capacity, rde_memory_allocator_get_default_std());
}

RDE_INTERNAL u32 kana_bytes_size(const kana_bytes* _b) {
    return (u32)rde_arr_length(_b);
}

RDE_INTERNAL void kana_put_u8(kana_bytes* _b, u8 _v) {
    rde_arr_add(_b, &_v);
}

RDE_INTERNAL void kana_put_u32(kana_bytes* _b, u32 _v) {
    u8* _p = rde_arr_add_n(_b, 4u);
    _p[0] = (u8)(_v);
    _p[1] = (u8)(_v >> 8);
    _p[2] = (u8)(_v >> 16);
    _p[3] = (u8)(_v >> 24);
}

RDE_INTERNAL void kana_put_f32(kana_bytes* _b, f32 _v) {
    u32 _bits;
    memcpy(&_bits, &_v, sizeof(_bits));
    kana_put_u32(_b, _bits);
}

RDE_INTERNAL void kana_put_color(kana_bytes* _b, rde_color _c) {
    kana_put_u8(_b, _c.r);
    kana_put_u8(_b, _c.g);
    kana_put_u8(_b, _c.b);
    kana_put_u8(_b, _c.a);
}

RDE_INTERNAL void kana_put_header(kana_bytes* _b, u32 _kind) {
    kana_put_u8(_b, 'K');
    kana_put_u8(_b, 'A');
    kana_put_u8(_b, 'N');
    kana_put_u8(_b, 'A');
    kana_put_u32(_b, KANA_SAVE_VERSION);
    kana_put_u32(_b, _kind);
}

// Returns where the size goes; kana_chunk_end fills it in.
RDE_INTERNAL u32 kana_chunk_begin(kana_bytes* _b, u32 _tag) {
    kana_put_u32(_b, _tag);
    const u32 _at = kana_bytes_size(_b);
    kana_put_u32(_b, 0u);
    return _at;
}

RDE_INTERNAL void kana_chunk_end(kana_bytes* _b, u32 _at) {
    const u32 _size = kana_bytes_size(_b) - (_at + 4u);
    u8*       _p    = rde_arr_at(_b, _at);
    _p[0] = (u8)(_size);
    _p[1] = (u8)(_size >> 8);
    _p[2] = (u8)(_size >> 16);
    _p[3] = (u8)(_size >> 24);
}

// Writes the finished bytes (see kana_save_write_atomic) and frees them.
RDE_INTERNAL b8 kana_bytes_write_and_free(kana_bytes* _b, const c8* _path, u32* _out_bytes);

// --- reading -------------------------------------------------------------------

typedef struct {
    const u8* data;
    u32       size;
    u32       pos;
    b8        ok;     // false once anything read past the end
} kana_reader;

RDE_INTERNAL b8 kana_reader_has(kana_reader* _r, u32 _n) {
    if(!_r->ok || _r->size - _r->pos < _n) {
        _r->ok = false;
        return false;
    }
    return true;
}

RDE_INTERNAL u8 kana_get_u8(kana_reader* _r) {
    return kana_reader_has(_r, 1u) ? _r->data[_r->pos++] : 0u;
}

RDE_INTERNAL u32 kana_get_u32(kana_reader* _r) {
    if(!kana_reader_has(_r, 4u)) {
        return 0u;
    }

    const u8* _p = &_r->data[_r->pos];
    _r->pos += 4u;
    return (u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24);
}

RDE_INTERNAL f32 kana_get_f32(kana_reader* _r) {
    const u32 _bits = kana_get_u32(_r);
    f32 _v;
    memcpy(&_v, &_bits, sizeof(_v));
    return _v;
}

RDE_INTERNAL rde_color kana_get_color(kana_reader* _r) {
    rde_color _c;
    _c.r = kana_get_u8(_r);
    _c.g = kana_get_u8(_r);
    _c.b = kana_get_u8(_r);
    _c.a = kana_get_u8(_r);
    return _c;
}

// "KANA", a version this build reads, and the expected kind.
RDE_INTERNAL b8 kana_read_header(kana_reader* _r, u32 _kind) {
    if(!kana_reader_has(_r, KANA_HEADER_SIZE) || memcmp(_r->data, "KANA", 4) != 0) {
        return false;
    }

    _r->pos = 4u;
    const u32 _version = kana_get_u32(_r);
    const u32 _file_kind = kana_get_u32(_r);
    return _r->ok && _version >= 1u && _version <= KANA_SAVE_VERSION && _file_kind == _kind;
}

// --- files ---------------------------------------------------------------------

const c8* kana_save_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };

    if(_dir[0] != 0) {
        return _dir;
    }

#if defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_ANDROID)
    rde_memory_allocator* _allocator = rde_memory_allocator_get_default();
#if defined(RDE_PLATFORM_IOS)
    const c8* _base = rde_ios_get_internal_storage_data_path(_allocator);
#else
    const c8* _base = rde_android_get_internal_storage_data_path(_allocator);
#endif
    if(_base != NULL) {
        const usize _len = strlen(_base);
        snprintf(_dir, sizeof(_dir), "%s%skana/", _base, (_len > 0 && _base[_len - 1] == '/') ? "" : "/");
        _allocator->free(_allocator->allocator, (any)_base);
    }
#endif

    if(_dir[0] == 0) {
        snprintf(_dir, sizeof(_dir), "./saves/");
    }

    // Parents of a file path: create_missing_dirs strips the file name and leaves
    // existing folders alone. (rde_file_create_dir REPLACES an existing folder.)
    c8 _probe[RDE_MAX_PATH];
    snprintf(_probe, sizeof(_probe), "%s%s", _dir, KANA_SAVE_DOCUMENT_FILE);
    if(!rde_file_create_missing_dirs(_probe)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not create the save folder %s", _dir);
    }

    return _dir;
}

RDE_INTERNAL b8 kana_save_write_atomic(const c8* _path, const u8* _data, u32 _size) {
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
        rde_log_level(RDE_LOG_LEVEL_ERROR, "kana: could not write %s", _tmp);
        return false;
    }

    // The previous version becomes the backup, then the new one takes its name.
    // Rename replaces the destination atomically.
    if(rde_file_exists(_path)) {
        rde_file_move(_path, _bak);
    }

    if(!rde_file_move(_tmp, _path)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "kana: could not move %s into place", _tmp);
        if(!rde_file_exists(_path) && rde_file_exists(_bak)) {
            rde_file_move(_bak, _path);
        }
        return false;
    }

    return true;
}

RDE_INTERNAL b8 kana_bytes_write_and_free(kana_bytes* _b, const c8* _path, u32* _out_bytes) {
    const u32 _size = kana_bytes_size(_b);
    const b8  _ok   = kana_save_write_atomic(_path, (const u8*)_b->memory, _size);
    if(_out_bytes != NULL) {
        *_out_bytes = _size;
    }
    rde_arr_free(_b);
    return _ok;
}

// The whole file, or NULL when it cannot be read. Free with kana_save_free.
RDE_INTERNAL u8* kana_save_read(const c8* _path, u32* _size) {
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
        rde_memory_allocator* _allocator = rde_memory_allocator_get_default();
        _allocator->free(_allocator->allocator, _data);
        _data = NULL;
    }

    *_size = (u32)_len;
    return _data;
}

RDE_INTERNAL void kana_save_free(u8* _data) {
    if(_data != NULL) {
        rde_memory_allocator* _allocator = rde_memory_allocator_get_default();
        _allocator->free(_allocator->allocator, _data);
    }
}

// Keeps a file that did not parse, so nothing can overwrite it.
RDE_INTERNAL void kana_save_set_aside(const c8* _path) {
    c8 _bad[RDE_MAX_PATH];
    snprintf(_bad, sizeof(_bad), "%s.bad", _path);
    rde_file_move(_path, _bad);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "kana: %s could not be read; kept as %s", _path, _bad);
}

// --- document ------------------------------------------------------------------

b8 kana_save_document(const c8* _path, const kana_ink* _ink, kana_view _view, u32* _out_bytes) {
    const u32 _total   = kana_ink_stroke_count(_ink);
    u32       _strokes = 0;
    u32       _points  = 0;
    for(u32 _s = 0; _s < _total; _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);
        if(_stroke->alive && _stroke->point_count > 0) {
            _strokes++;
            _points += _stroke->point_count;
        }
    }

    // Sized up front (+1: rde_arr grows when an add reaches its capacity), so the
    // whole file is built without a single reallocation.
    kana_bytes _b = kana_bytes_new(KANA_HEADER_SIZE + 20u + (16u + _strokes * KANA_STROKE_RECORD_SIZE) + (16u + _points * KANA_POINT_RECORD_SIZE) + 1u);
    kana_put_header(&_b, KANA_KIND_DOCUMENT);

    u32 _chunk = kana_chunk_begin(&_b, KANA_CHUNK_VIEW);
    kana_put_f32(&_b, _view.offset.x);
    kana_put_f32(&_b, _view.offset.y);
    kana_put_f32(&_b, _view.zoom);
    kana_chunk_end(&_b, _chunk);

    _chunk = kana_chunk_begin(&_b, KANA_CHUNK_STROKES);
    kana_put_u32(&_b, _strokes);
    kana_put_u32(&_b, KANA_STROKE_RECORD_SIZE);
    for(u32 _s = 0; _s < _total; _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        kana_put_u32(&_b, _stroke->point_count);
        kana_put_color(&_b, _stroke->color);
        kana_put_u8(&_b, _stroke->from_pen ? KANA_STROKE_FLAG_FROM_PEN : 0u);
        kana_put_u8(&_b, 0u);
        kana_put_u8(&_b, 0u);
        kana_put_u8(&_b, 0u);
    }
    kana_chunk_end(&_b, _chunk);

    _chunk = kana_chunk_begin(&_b, KANA_CHUNK_POINTS);
    kana_put_u32(&_b, _points);
    kana_put_u32(&_b, KANA_POINT_RECORD_SIZE);
    for(u32 _s = 0; _s < _total; _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        const kana_ink_point* _p = kana_ink_stroke_points(_ink, _stroke);
        for(u32 _i = 0; _i < _stroke->point_count; _i++) {
            kana_put_f32(&_b, _p[_i].position.x);
            kana_put_f32(&_b, _p[_i].position.y);
            kana_put_f32(&_b, _p[_i].pressure);
            kana_put_f32(&_b, _p[_i].radius);
            kana_put_f32(&_b, _p[_i].time);
        }
    }
    kana_chunk_end(&_b, _chunk);

    return kana_bytes_write_and_free(&_b, _path, _out_bytes);
}

RDE_INTERNAL b8 kana_finite(f32 _v) {
    return isfinite(_v) != 0;
}

typedef struct {
    u32       point_count;
    rde_color color;
    b8        from_pen;
} kana_loaded_stroke;

// Parses the whole document before touching _ink, so a damaged file changes nothing.
RDE_INTERNAL b8 kana_parse_document(const u8* _data, u32 _size, kana_ink* _ink, kana_view* _view) {
    kana_reader _r = { .data = _data, .size = _size, .pos = 0, .ok = true };
    if(!kana_read_header(&_r, KANA_KIND_DOCUMENT)) {
        return false;
    }

    kana_view           _v            = *_view;
    kana_loaded_stroke* _strokes      = NULL;
    u32                 _stroke_count = 0;
    kana_ink_point*     _points       = NULL;
    u32                 _point_count  = 0;
    b8                  _have_strokes = false;
    b8                  _have_points  = false;
    b8                  _ok           = true;

    while(_ok && _r.pos < _r.size) {
        const u32 _tag  = kana_get_u32(&_r);
        const u32 _len  = kana_get_u32(&_r);
        if(!_r.ok || _len > _r.size - _r.pos) {
            _ok = false;
            break;
        }

        // Each chunk is read through its own window, so a short or long payload
        // can never run into the next chunk.
        kana_reader _c   = { .data = &_r.data[_r.pos], .size = _len, .pos = 0, .ok = true };
        _r.pos          += _len;

        if(_tag == KANA_CHUNK_VIEW) {
            const f32 _x = kana_get_f32(&_c);
            const f32 _y = kana_get_f32(&_c);
            const f32 _z = kana_get_f32(&_c);
            if(_c.ok && kana_finite(_x) && kana_finite(_y) && kana_finite(_z) && _z > 0.0f) {
                _v.offset = (rde_vec_2F){ _x, _y };
                _v.zoom   = rde_math_clamp_f32(_z, KANA_CANVAS_ZOOM_MIN, KANA_CANVAS_ZOOM_MAX);
            }
        } else if(_tag == KANA_CHUNK_STROKES && !_have_strokes) {
            const u32 _count  = kana_get_u32(&_c);
            const u32 _record = kana_get_u32(&_c);
            if(!_c.ok || _record < KANA_STROKE_RECORD_SIZE || (u64)_count * _record > (u64)(_c.size - _c.pos)) {
                _ok = false;
                break;
            }

            _strokes = _count > 0 ? rde_malloc((usize)_count * sizeof(kana_loaded_stroke)) : NULL;
            for(u32 _i = 0; _i < _count; _i++) {
                const u32 _start = _c.pos;
                _strokes[_i].point_count = kana_get_u32(&_c);
                _strokes[_i].color       = kana_get_color(&_c);
                _strokes[_i].from_pen    = (kana_get_u8(&_c) & KANA_STROKE_FLAG_FROM_PEN) != 0;
                _c.pos = _start + _record;   // skip fields a newer build added
                if(_strokes[_i].point_count == 0) {
                    _ok = false;
                }
            }
            _stroke_count = _count;
            _have_strokes = true;
        } else if(_tag == KANA_CHUNK_POINTS && !_have_points) {
            const u32 _count  = kana_get_u32(&_c);
            const u32 _record = kana_get_u32(&_c);
            if(!_c.ok || _record < KANA_POINT_RECORD_SIZE || (u64)_count * _record > (u64)(_c.size - _c.pos)) {
                _ok = false;
                break;
            }

            _points = _count > 0 ? rde_malloc((usize)_count * sizeof(kana_ink_point)) : NULL;
            for(u32 _i = 0; _i < _count; _i++) {
                const u32 _start = _c.pos;
                kana_ink_point* _p = &_points[_i];
                _p->position.x = kana_get_f32(&_c);
                _p->position.y = kana_get_f32(&_c);
                _p->pressure   = kana_get_f32(&_c);
                _p->radius     = kana_get_f32(&_c);
                _p->time       = kana_get_f32(&_c);
                _c.pos = _start + _record;

                if(!kana_finite(_p->position.x) || !kana_finite(_p->position.y)) {
                    _ok = false;
                }
                // Cosmetic fields: repaired rather than rejected.
                _p->pressure = kana_finite(_p->pressure) ? rde_math_clamp_f32(_p->pressure, 0.0f, 1.0f) : 0.0f;
                _p->radius   = (kana_finite(_p->radius) && _p->radius > 0.0f) ? _p->radius : KANA_INK_RADIUS_DEFAULT;
                _p->time     = kana_finite(_p->time) ? _p->time : 0.0f;
            }
            _point_count = _count;
            _have_points = true;
        }
        // Anything else: a chunk from a newer build. Skipped.
    }

    // The strokes must account for exactly the points there are.
    if(_ok) {
        u64 _sum = 0;
        for(u32 _i = 0; _i < _stroke_count; _i++) {
            _sum += _strokes[_i].point_count;
        }
        _ok = _r.ok && _have_strokes == _have_points && _sum == _point_count;
    }

    if(_ok) {
        u32 _first = 0;
        for(u32 _i = 0; _i < _stroke_count; _i++) {
            kana_ink_add_loaded_stroke(_ink, &_points[_first], _strokes[_i].point_count, _strokes[_i].color, _strokes[_i].from_pen);
            _first += _strokes[_i].point_count;
        }
        *_view = _v;
    }

    rde_free(_strokes);
    rde_free(_points);
    return _ok;
}

// One file: OK, MISSING, or CORRUPT (and then set aside).
RDE_INTERNAL KANA_LOAD_ kana_load_document_file(const c8* _file, kana_ink* _ink, kana_view* _view) {
    if(!rde_file_exists(_file)) {
        return KANA_LOAD_MISSING;
    }

    u32 _size = 0;
    u8* _data = kana_save_read(_file, &_size);
    const b8 _ok = _data != NULL && kana_parse_document(_data, _size, _ink, _view);
    kana_save_free(_data);

    if(!_ok) {
        kana_save_set_aside(_file);
        return KANA_LOAD_CORRUPT;
    }

    return KANA_LOAD_OK;
}

KANA_LOAD_ kana_load_document(const c8* _path, kana_ink* _ink, kana_view* _view) {
    const KANA_LOAD_ _main = kana_load_document_file(_path, _ink, _view);
    if(_main == KANA_LOAD_OK) {
        return KANA_LOAD_OK;
    }

    // Missing (a kill between a save's two renames) or damaged: the backup is the
    // previous save.
    c8 _bak[RDE_MAX_PATH];
    snprintf(_bak, sizeof(_bak), "%s.bak", _path);
    const KANA_LOAD_ _backup = kana_load_document_file(_bak, _ink, _view);
    if(_backup == KANA_LOAD_OK) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: %s %s, loaded the backup", _path, _main == KANA_LOAD_MISSING ? "missing" : "damaged");
        return KANA_LOAD_RECOVERED;
    }

    return (_main == KANA_LOAD_CORRUPT || _backup == KANA_LOAD_CORRUPT) ? KANA_LOAD_CORRUPT : KANA_LOAD_MISSING;
}

// --- settings --------------------------------------------------------------------

b8 kana_save_settings(const c8* _path, const kana_settings* _settings) {
    kana_bytes _b = kana_bytes_new(64u);
    kana_put_header(&_b, KANA_KIND_SETTINGS);

    const u32 _chunk = kana_chunk_begin(&_b, KANA_CHUNK_PREFS);
    kana_put_u8(&_b, _settings->tool);
    kana_put_u8(&_b, _settings->vertical ? 1u : 0u);
    kana_put_u8(&_b, _settings->show_hud ? 1u : 0u);
    kana_put_u8(&_b, _settings->brush_scale);
    kana_put_u8(&_b, _settings->width_mode);
    kana_put_color(&_b, _settings->color);
    kana_put_f32(&_b, _settings->radius);
    kana_put_f32(&_b, _settings->toolbar_center.x);
    kana_put_f32(&_b, _settings->toolbar_center.y);
    kana_chunk_end(&_b, _chunk);

    return kana_bytes_write_and_free(&_b, _path, NULL);
}

KANA_LOAD_ kana_load_settings(const c8* _path, kana_settings* _settings) {
    // Settings are small and replaceable: the main file, else the backup, and a
    // damaged one is simply set aside.
    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return KANA_LOAD_MISSING;
        }
    }

    u32 _size = 0;
    u8* _data = kana_save_read(_from, &_size);

    kana_reader _r = { .data = _data, .size = _size, .pos = 0, .ok = _data != NULL };
    if(_data == NULL || !kana_read_header(&_r, KANA_KIND_SETTINGS)) {
        kana_save_free(_data);
        kana_save_set_aside(_from);
        return KANA_LOAD_CORRUPT;
    }

    kana_settings _s = *_settings;

    while(_r.ok && _r.pos < _r.size) {
        const u32 _tag = kana_get_u32(&_r);
        const u32 _len = kana_get_u32(&_r);
        if(!_r.ok || _len > _r.size - _r.pos) {
            break;
        }

        kana_reader _c  = { .data = &_r.data[_r.pos], .size = _len, .pos = 0, .ok = true };
        _r.pos         += _len;

        if(_tag != KANA_CHUNK_PREFS) {
            continue;
        }

        // Field by field, in order: a file from an older build simply ends early,
        // and each field it has is taken only if it is in range.
        const u8 _tool = kana_get_u8(&_c);
        if(_c.ok && _tool <= 2u) { _s.tool = _tool; }
        const u8 _vertical = kana_get_u8(&_c);
        if(_c.ok) { _s.vertical = _vertical != 0; }
        const u8 _hud = kana_get_u8(&_c);
        if(_c.ok) { _s.show_hud = _hud != 0; }
        const u8 _scale = kana_get_u8(&_c);
        if(_c.ok && _scale <= 1u) { _s.brush_scale = _scale; }
        const u8 _width = kana_get_u8(&_c);
        if(_c.ok && _width <= 1u) { _s.width_mode = _width; }
        const rde_color _color = kana_get_color(&_c);
        if(_c.ok) { _s.color = _color; }
        const f32 _radius = kana_get_f32(&_c);
        if(_c.ok && kana_finite(_radius) && _radius > 0.0f) { _s.radius = _radius; }
        const f32 _cx = kana_get_f32(&_c);
        const f32 _cy = kana_get_f32(&_c);
        if(_c.ok && kana_finite(_cx) && kana_finite(_cy)) { _s.toolbar_center = (rde_vec_2F){ _cx, _cy }; }
    }

    kana_save_free(_data);
    *_settings = _s;
    return KANA_LOAD_OK;
}

RDE_INTERNAL b8 kana_same_f32(f32 _a, f32 _b) {
    return memcmp(&_a, &_b, sizeof(f32)) == 0;
}

b8 kana_settings_equal(const kana_settings* _a, const kana_settings* _b) {
    return _a->tool == _b->tool && _a->vertical == _b->vertical && _a->show_hud == _b->show_hud &&
           _a->brush_scale == _b->brush_scale && _a->width_mode == _b->width_mode &&
           _a->color.r == _b->color.r && _a->color.g == _b->color.g && _a->color.b == _b->color.b && _a->color.a == _b->color.a &&
           kana_same_f32(_a->radius, _b->radius) &&
           kana_same_f32(_a->toolbar_center.x, _b->toolbar_center.x) && kana_same_f32(_a->toolbar_center.y, _b->toolbar_center.y);
}
